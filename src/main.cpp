#include <Arduino.h>
#include <EEPROM.h>
#include "HFSMEvents.h"
#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"
#include "HAL.h"
#include "BatteryService.h"
#include "game.h"

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1

// ---------------------------------------------------------------------------
// Global networking objects
// ---------------------------------------------------------------------------

// AP IP configuration (must match VALUE used by captive portal)
//IPAddress apIP(192, 168, 144, 1);
//IPAddress apIP(169, 254, 18, 1);
IPAddress apIP(4, 3, 2, 1);

// Networking primitives
AsyncWebServer server(80);
DNSServer       dnsServer;

// API singletons
ApiSystem    apiSystem;
ApiGame      apiGame;

// HFSM wrapper
NetworkingFSM networkingFSM;

// Battery service instance
BatteryService batteryService;

// Persistent device configuration loaded from /network.json at startup
JsonDocument global_configuration;
bool         global_config_loaded = false;   // true only when the file parsed OK

// for LED blinking
bool ledState = false;

bool modeSelected = false;
unsigned long lastHeartbeat = 0;


void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(250);

  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIN_MODE_SELECT, INPUT_PULLUP);

  // Initialize the BatteryService.
  // Calibration values are loaded from EEPROM (addresses 4-20).
  // They can be set via serial commands (B0, B1, B2, B3, B4).
  IFsmDispatcher* batteryFsmListeners[] = { &networkingFSM, &gameFSM };
  batteryService.init(BATTERY_PIN, 1000,
                      batteryFsmListeners, 2,
                      EVT_CORE_LOW_BATTERY);

  // ---------------------------------------------------------------------
  // Mount filesystem (LittleFS) before starting networking
  // ---------------------------------------------------------------------

#ifdef ESP32
  if (!FILESYSTEM.begin(true)) {
    Serial.println("[FS] Failed to mount LittleFS (ESP32)");
    return;
  }
#elif defined(ESP8266)
  if (!LittleFS.begin()) {
    Serial.println("[FS] Failed to mount LittleFS (ESP8266)");
    return;
  }
#endif

  Serial.println("[FS] LittleFS mounted successfully");

  // ---------------------------------------------------------------------
  // Load configuration from /network.json
  // ---------------------------------------------------------------------
  {
    File cfgFile = FILESYSTEM.open("/network.json", "r");
    if (cfgFile) {
      DeserializationError err = deserializeJson(global_configuration, cfgFile);
      cfgFile.close();
      if (err == DeserializationError::Ok) {
        Serial.println("[CFG] Loaded /network.json");
        global_config_loaded = true;
      } else {
        Serial.print("[CFG] Parse error: ");
        Serial.println(err.c_str());
      }
    } else {
      Serial.println("[CFG] /network.json not found, applying defaults");
    }
    if (!global_config_loaded) {
      global_configuration["channel"]     = 1;
      global_configuration["ssid"]        = "BrooklynTactical";
      global_configuration["password"]    = "password";
      global_configuration["net_id"]      = "1";
      global_configuration["mode_id"]     = 0;
      global_configuration["device_id"]   = "0";
      global_configuration["description"] = "";
    }
  }

  // ---------------------------------------------------------------------
  // Dump LittleFS directory tree for debugging
  // ---------------------------------------------------------------------
  Serial.println(F("[FS] --- LittleFS contents ---"));
  std::function<void(const String&, int)> listDir = [&](const String& path, int depth) {
#ifdef ESP32
    File dir = FILESYSTEM.open(path);
    if (!dir || !dir.isDirectory()) {
      Serial.print(F("[FS] Not a directory: ")); Serial.println(path);
      return;
    }
    File entry = dir.openNextFile();
    while (entry) {
      for (int i = 0; i < depth; i++) Serial.print(F("  "));
      if (entry.isDirectory()) {
        Serial.print(F("[DIR]  ")); Serial.println(entry.name());
        listDir(String("/") + entry.name(), depth + 1);
      } else {
        Serial.print(F("[FILE] ")); Serial.print(entry.name());
        Serial.print(F("  (")); Serial.print(entry.size()); Serial.println(F(" bytes)"));
      }
      entry = dir.openNextFile();
    }
#elif defined(ESP8266)
    Dir dir = FILESYSTEM.openDir(path);
    while (dir.next()) {
      for (int i = 0; i < depth; i++) Serial.print(F("  "));
      if (dir.isDirectory()) {
        Serial.print(F("[DIR]  ")); Serial.println(dir.fileName());
        listDir(path + "/" + dir.fileName(), depth + 1);
      } else {
        Serial.print(F("[FILE] ")); Serial.print(dir.fileName());
        Serial.print(F("  (")); Serial.print(dir.fileSize()); Serial.println(F(" bytes)"));
      }
    }
#endif
  };
  listDir("/", 0);
  Serial.println(F("[FS] --- end of listing ---"));

//  server.serveStatic("/", FILESYSTEM, "/web");

  // ---------------------------------------------------------------------
  // Start the FSMs
  // ---------------------------------------------------------------------
  networkingFSM.begin();
  gameFSM.begin();

  // If network.json was missing or unparseable, go straight to admin mode
  // so the user can configure the device on first boot.
  if (!global_config_loaded) {
    Serial.println(F("[CFG] No valid config — auto-entering admin mode"));
    modeSelected = true;
    networkingFSM.dispatch(EVT_NET_SELECTED_ADMIN);
  }
}

// ---------------------------------------------------------------------------
// Serial command processor — called from loop(), non-blocking.
// Accumulates characters until '\n', then dispatches on the 2-char prefix.
//
// Supported commands:
//   HW:<float>   — write hardware version to EEPROM (float) position 0
//                  e.g. "HW:1.5"
//   BR:          - Read the current raw ADC battery level
//   BL:          - Dump the configured battery level calibration points from EEPROM
//   B0:          - Read the current ADC battery level and store it to EEPROM (int) as 0%
//   B1:          - Read the current ADC battery level and store it to EEPROM (int) as 25%
//   B2:          - Read the current ADC battery level and store it to EEPROM (int) as 50%
//   B3:          - Read the current ADC battery level and store it to EEPROM (int) as 75%
//   B4:          - Read the current ADC battery level and store it to EEPROM (int) as 100%
// ---------------------------------------------------------------------------
static void processSerialCommands() {
  static String serialLine;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;          // ignore CR (Windows line endings)
    if (c != '\n') { serialLine += c; continue; }

    // '\n' received — process the accumulated line
    serialLine.trim();

    if (serialLine.length() >= 3) {
      // Encode the 2-char command prefix as a uint16 for switch-case
      uint16_t cmd = ((uint8_t)serialLine[0] << 8) | (uint8_t)serialLine[1];

      switch (cmd) {

        case (('H' << 8) | 'W'): {   // "HW:<float>"
          float hwVal = serialLine.substring(3).toFloat();
          EEPROM.begin(sizeof(float));
          EEPROM.put(0, hwVal);
          EEPROM.commit();
          EEPROM.end();
          Serial.print(F("[CMD] HW version written: "));
          Serial.println(hwVal, 2);
          break;
        }

        case (('B' << 8) | 'R'): {
          int batVal = analogRead(BATTERY_PIN);
          Serial.print(F("[CMD] Battery Level read: "));
          Serial.println(batVal);
          break;
        }

        case (('B' << 8) | 'L'): {   // "BL:" — dump calibration points from EEPROM
          const int base = sizeof(float);
          const int sz   = sizeof(int);
          int addr_b4    = base + 4 * sz;  // highest address needed (addr 20)
          EEPROM.begin(addr_b4 + sz);
          int v0, v25, v50, v75, v100;
          EEPROM.get(base + 0 * sz, v0);
          EEPROM.get(base + 1 * sz, v25);
          EEPROM.get(base + 2 * sz, v50);
          EEPROM.get(base + 3 * sz, v75);
          EEPROM.get(base + 4 * sz, v100);
          EEPROM.end();
          Serial.println(F("[CMD] Battery level calibration (raw ADC):"));
          Serial.print(F("  B0 (  0%): ")); Serial.println(v0);
          Serial.print(F("  B1 ( 25%): ")); Serial.println(v25);
          Serial.print(F("  B2 ( 50%): ")); Serial.println(v50);
          Serial.print(F("  B3 ( 75%): ")); Serial.println(v75);
          Serial.print(F("  B4 (100%): ")); Serial.println(v100);
          break;
        }

        case (('B' << 8) | '0'):
        case (('B' << 8) | '1'):
        case (('B' << 8) | '2'):
        case (('B' << 8) | '3'):
        case (('B' << 8) | '4'): {   // "Bx" where x is 0-4 for battery levels [0%, 25%, 50%, 75%, 100%]
          int index = (cmd & 0xFF) - '0';  // Convert ASCII to 0-4
          int addr = sizeof(float) + index * sizeof(int); // offset by HW version's float entry
          int batVal = analogRead(BATTERY_PIN);
          EEPROM.begin(addr + sizeof(int));
          EEPROM.put(addr, batVal);
          EEPROM.commit();
          EEPROM.end();
          Serial.print(F("[CMD] B")); Serial.print(index); Serial.print(F(" value written: "));
          Serial.println(batVal);
          // update the ranges within the batteryService
          batteryService.updateLevelRanges();
          break;
        }


        default:
          Serial.print(F("[CMD] Unknown command: "));
          Serial.println(serialLine);
          break;
      }
    }

    serialLine = "";   // clear buffer ready for next command
  }
}

void loop() {
  // Handle any pending serial commands
  processSerialCommands();

  // Poll battery service for ADC updates
  batteryService.update();

  // Drive the networking HFSM
  networkingFSM.update();

  // Drive the game HFSM
  gameFSM.update();


  // ================================================================
  // Select Game/Admin mode based on mode-select button being pressed
  // within 5 seconds (for Admin) or not (for Game) after power on.
  // ================================================================
  if (!modeSelected) {
    if (millis() > 5000) {
      // Auto-select game mode after 5 seconds if not already selected
      modeSelected = true;
      networkingFSM.dispatch(EVT_NET_SELECTED_GAME);
    } else if (digitalRead(PIN_MODE_SELECT) == LOW) {
      // Switch to admin mode if the PIN_MODE_SELECT goes low
      modeSelected = true;
      networkingFSM.dispatch(EVT_NET_SELECTED_ADMIN);
    }
  }
  // ================================================================




  // // Heartbeat LED flash every second
  // unsigned long now = millis();
  // if (now - lastHeartbeat >= 1000) {
  //   lastHeartbeat = now;
  //   ledState = true;
  //   digitalWrite(LED_PIN, LOW);
  // }
  // if (ledState && now - lastHeartbeat >= 10) {
  //   ledState = false;
  //   digitalWrite(LED_PIN, HIGH);
  // }

}
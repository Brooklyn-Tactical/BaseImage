#include <Arduino.h>
#include <EEPROM.h>
#include "HFSMEvents.h"
#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"
#include "HAL.h"
#include "BatteryService.h"
#include "game.h"

// --------------------------------------------------- //
// Configuration
#define CONFIG_DEBOUNCE_INTERVAL  10  // in ms
// --------------------------------------------------- //

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


// Hardware
// ============================================================================
// #include <Tone.h>
// Tone piezo[2];

#include <Bounce2.h>
Bounce2::Button global_arming_button = Bounce2::Button();

#include <FadeLed.h>
//#define FADE_LED_MAX_LED 1
FadeLed MainLED(PIN_LIGHT, true);
//FadeLed MainLED[1] = {PIN_LIGHT};


#include <TM1637TinyDisplay.h>
TM1637TinyDisplay global_disp(PIN_TMR_CLK, PIN_TMR_DATA);
const PROGMEM char LEDSTR_DASHES[] = "----";


// ============================================================================



bool modeSelected = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(250);

  Serial.println("");
  Serial.println(F("--------------------------"));
  Serial.print(F("  Bio-Vial Standalone v"));
  Serial.print(FIRMWARE_VERSION_MAJOR);
  Serial.print(F("."));
  Serial.println(FIRMWARE_VERSION_MINOR);
  Serial.println(F("   By Brooklyn Tactical"));
  Serial.println(F("--------------------------"));

  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);

  // Arming Pin button setup
  pinMode(PIN_MODE_SELECT, INPUT_PULLUP);
  global_arming_button.attach(PIN_ARMING, INPUT_PULLUP);
  global_arming_button.interval(CONFIG_DEBOUNCE_INTERVAL);
  global_arming_button.setPressedState(LOW);

  // setup main LED light
  pinMode(PIN_LIGHT, OUTPUT);
  MainLED.set(0); // LED off

  // setup LED display
  global_disp.begin();
  global_disp.setBrightness(1);
  global_disp.showString_P(LEDSTR_DASHES);

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

  // // TODO: Remove This!!!
  // networkingFSM.dispatch(EVT_NET_SELECTED_ADMIN);
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
          EEPROM.begin(sizeof(float) + 5 * sizeof(int));  // full blob size — prevents truncation of calibration data
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
          const int sz   = sizeof(int);  // slot spacing matches write side
          EEPROM.begin(sizeof(float) + 5 * sizeof(int));  // full blob size
          uint16_t v0, v25, v50, v75, v100;               // uint16_t matches write side
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
          uint16_t batVal = (uint16_t)analogRead(BATTERY_PIN);  // uint16_t matches BatteryService reads
          EEPROM.begin(sizeof(float) + 5 * sizeof(int));        // full blob size — prevents truncation of other calibration points
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

  // Drive the HFSMs
  networkingFSM.update();
  gameFSM.update();

  // Do Fading of LED light
  FadeLed::update();

  // do arming pin
  global_arming_button.update();
  if (global_arming_button.pressed()) {
    gameFSM.dispatch(EVT_CORE_PIN_INSERTED);
  }
  if (global_arming_button.released()) {
    gameFSM.dispatch(EVT_CORE_PIN_REMOVED);
  }

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

}
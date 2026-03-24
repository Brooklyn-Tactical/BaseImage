#include <Arduino.h>
#include <EEPROM.h>
#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"
#include "HAL.h"

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1

// ---------------------------------------------------------------------------
// Global networking objects
// ---------------------------------------------------------------------------

// Access Point credentials
const char* ap_ssid     = "ESP32-WebServer";
const char* ap_password = "12345678";  // Minimum 8 characters

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

// Battery level: raw ADC-style value 0-1024 (0% - 100%)
uint16_t global_battery_level = 512;

// Persistent device configuration loaded from /network.json at startup
JsonDocument global_configuration;
bool         global_config_loaded = false;   // true only when the file parsed OK

// for LED blinking
bool ledState = false;

bool modeSelected = false;
unsigned long lastHeartbeat   = 0;
unsigned long lastBatteryUpdate = 0;


void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);

  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);

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
      global_configuration["password"]    = "12345678";
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
  // Start networking HFSM
  // ---------------------------------------------------------------------
  networkingFSM.begin();

  // If network.json was missing or unparseable, go straight to admin mode
  // so the user can configure the device on first boot.
  if (!global_config_loaded) {
    Serial.println(F("[CFG] No valid config — auto-entering admin mode"));
    modeSelected = true;
    networkingFSM.dispatch(EVT_SELECTED_ADMIN);
  }
}

// ---------------------------------------------------------------------------
// Serial command processor — called from loop(), non-blocking.
// Accumulates characters until '\n', then dispatches on the 2-char prefix.
//
// Supported commands:
//   HW:<float>   — write hardware version to EEPROM position 0
//                  e.g. "HW:1.5"
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

        case (('B' << 8) | '1'):
        case (('B' << 8) | '2'):
        case (('B' << 8) | '3'):
        case (('B' << 8) | '4'): {   // "Bx" where x is 1-4
          int addr = 5 + (cmd & 0xFF) * sizeof(int);
          int batVal = analogRead(BATTERY_PIN);
          EEPROM.begin(sizeof(int));
          EEPROM.put(addr, batVal);
          EEPROM.commit();
          EEPROM.end();
          Serial.print(F("[CMD] Bx value written: "));
          Serial.println(batVal);
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

  // Drive the networking HFSM
  networkingFSM.update();

  // Auto-select game mode after 5 seconds if not already selected
  if (millis() > 5000 && !modeSelected) {
    modeSelected = true;
    networkingFSM.dispatch(EVT_SELECTED_ADMIN);
//    networkingFSM.dispatch(EVT_SELECTED_GAME);
  }

  // Update battery level every 250ms with a random walk, clamped 0-1024
  unsigned long now = millis();
  if (now - lastBatteryUpdate >= 250) {
    lastBatteryUpdate = now;
    int32_t newLevel = (int32_t)global_battery_level + random(-250, 251);
    global_battery_level = (uint16_t)constrain(newLevel, 0, 1024);

  }

  // Heartbeat LED flash every second
  if (now - lastHeartbeat >= 1000) {
    lastHeartbeat = now;
    ledState = true;
    digitalWrite(LED_PIN, LOW);
  }
  if (ledState && now - lastHeartbeat >= 10) {
    ledState = false;
    digitalWrite(LED_PIN, HIGH);
  }

}
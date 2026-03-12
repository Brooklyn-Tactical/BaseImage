#include <Arduino.h>
#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"

// ---------------------------------------------------------------------------
// Global networking objects
// ---------------------------------------------------------------------------

// Access Point credentials
const char* ap_ssid     = "ESP32-WebServer";
const char* ap_password = "12345678";  // Minimum 8 characters

// AP IP configuration (must match VALUE used by captive portal)
//IPAddress apIP(192, 168, 144, 1);
IPAddress apIP(169, 254, 18, 1);

// Networking primitives
WebServer  server(80);
DNSServer  dnsServer;

// API singletons
ApiSystem    apiSystem;
ApiGame      apiGame;

// HFSM wrapper
NetworkingFSM networkingFSM;

bool modeSelected = false;
unsigned long lastHeartbeat = 0;


void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);

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
}

void loop() {
  // Drive the networking HFSM
  networkingFSM.update();

  // Auto-select game mode after 5 seconds if not already selected
  if (millis() > 5000 && !modeSelected) {
    modeSelected = true;
    networkingFSM.dispatch(EVT_SELECTED_GAME);
  }

  // Heartbeat message every 5 seconds
  unsigned long now = millis();
  if (now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    Serial.print(F("[HEARTBEAT] "));
    Serial.print(now / 1000);
    Serial.println(F("s"));
  }

}
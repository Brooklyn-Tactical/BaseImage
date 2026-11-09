#include "ApiSystem.h"
#include "networking.h"   // provides FILESYSTEM macro and LittleFS
#include <ArduinoJson.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <FadeLed.h>
#include <HAL.h>
#include "game.h"
#include <TM1637TinyDisplay.h>


// The Update/Updater global object has different header names per platform
#ifdef ESP32
    #include <Update.h>
#else
    #include <Updater.h>
#endif

// Some ESP8266 core versions omit this constant
#ifndef UPDATE_SIZE_UNKNOWN
    #define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

#ifdef ESP32
    #include <WiFi.h>
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
#endif

extern JsonDocument global_configuration;
extern Bounce2::Button global_arming_button;
extern FadeLed MainLED;
extern TM1637TinyDisplay global_disp;

// Constructor
ApiSystem::ApiSystem() {
}

// Register API routes
void ApiSystem::registerRoutes(AsyncWebServer& server) {
    server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getInfo(request);
    });
    server.on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getStatus(request);
    });
    server.on("/api/system/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->restart(request);
    });
    // WiFi config GET
    server.on("/api/system/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("WiFi config GET");
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();
        root["channel"]  = global_configuration["channel"]  | 1;
        root["ssid"]     = global_configuration["ssid"]     | "BrooklynTactical";
        root["password"] = global_configuration["password"] | "";
        response->setCode(200);
        response->setLength();
        request->send(response);
    });

    // WiFi config POST
    static AsyncCallbackJsonWebHandler *wifiPosthandler = new AsyncCallbackJsonWebHandler("/api/system/wifi");
    wifiPosthandler->setMaxContentLength(200);
    wifiPosthandler->setMethod(HTTP_POST | HTTP_PUT);
    wifiPosthandler->onRequest([this](AsyncWebServerRequest *request, JsonVariant &json) {
        Serial.print("[WiFi] Config update: "); Serial.println(json.as<String>().c_str());
        global_configuration["channel"]  = json["channel"]  | (int)(global_configuration["channel"]  | 1);
        global_configuration["ssid"]     = json["ssid"]     | (const char*)(global_configuration["ssid"]     | "");
        global_configuration["password"] = json["password"] | (const char*)(global_configuration["password"] | "");
        saveConfiguration();
        request->send(200, "application/json", "{\"success\":true}");
    });
    server.addHandler(wifiPosthandler);

    // Mesh config GET
    server.on("/api/system/mesh", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("Mesh config GET");
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();
        root["net_id"]      = global_configuration["net_id"]      | 1;
        root["mode_id"]     = global_configuration["mode_id"]     | 0;
        root["device_id"]   = global_configuration["device_id"]   | 0;
        root["description"] = global_configuration["description"] | "";
        response->setCode(200);
        response->setLength();
        request->send(response);
    });

    // Mesh config POST
    static AsyncCallbackJsonWebHandler *meshPostHandler = new AsyncCallbackJsonWebHandler("/api/system/mesh");
    meshPostHandler->setMaxContentLength(200);
    meshPostHandler->setMethod(HTTP_POST | HTTP_PUT);
    meshPostHandler->onRequest([this](AsyncWebServerRequest *request, JsonVariant &json) {
        Serial.print("[Mesh] Config update: "); Serial.println(json.as<String>().c_str());
        global_configuration["net_id"]      = json["net_id"]      | (int)(global_configuration["net_id"]      | 1);
        global_configuration["mode_id"]     = json["mode_id"]     | (int)(global_configuration["mode_id"]     | 0);
        global_configuration["device_id"]   = json["device_id"]   | (int)(global_configuration["device_id"]   | 0);
        global_configuration["description"] = json["description"] | (const char*)(global_configuration["description"] | "");
        saveConfiguration();
        request->send(200, "application/json", "{\"success\":true}");
    });
    server.addHandler(meshPostHandler);

    // Lifted-files manifest
    server.on("/api/system/files", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getFiles(request);
    });

    // Generic file download / upload
    server.on("/api/system/file", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getFile(request);
    });
    server.on("/api/system/file", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->setFile(request, data, len, index, total);
        }
    );

    // Diagnostics: LED bar on/off (POST-only)
    static AsyncCallbackJsonWebHandler *diagLedHandler = new AsyncCallbackJsonWebHandler("/api/system/diag/led");
    diagLedHandler->setMaxContentLength(64);
    diagLedHandler->setMethod(HTTP_POST);
    diagLedHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        bool on = json["on"] | false;
        Serial.print(F("[Diag] LED set: "));
        MainLED.setTime(1000, true);
        if (on) {
            MainLED.set(255);
            Serial.println(F("on"));
        } else {
            MainLED.set(0);
            Serial.println(F("off"));
        }
        request->send(200, "application/json", "{\"success\":true}");
    });
    server.addHandler(diagLedHandler);

    // Diagnostics: LED display on/off (POST-only)
    static AsyncCallbackJsonWebHandler *diagDisplayHandler = new AsyncCallbackJsonWebHandler("/api/system/diag/display");
    diagDisplayHandler->setMaxContentLength(64);
    diagDisplayHandler->setMethod(HTTP_POST);
    diagDisplayHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        uint8_t dots = 0b01010000;
        bool on = json["on"] | false;
        Serial.print(F("[Diag] Display set: "));
        if (on) {
            global_disp.setBrightness(6);
            global_disp.showNumberDec(88, dots, true, 2, 0);
            global_disp.showNumberDec(88, dots, true, 2, 2);
            Serial.println(F("on"));
        } else {
            global_disp.showString("");
            Serial.println(F("off"));
        }
        request->send(200, "application/json", "{\"success\":true}");
    });
    server.addHandler(diagDisplayHandler);

    // Diagnostics: Speaker beep
    server.on("/api/system/diag/beep", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println(F("[Diag] Beep"));
        tone(PIN_PIEZO_A,2500, 250);
        tone(PIN_PIEZO_B,2800, 250);
        request->send(200, "application/json", "{\"success\":true}");
    });

    // Diagnostics: demo routine start/stop (handled on the main loop via flags)
    server.on("/api/system/diag/demo/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println(F("[Diag] Demo start"));
        gameFSM.requestDemoStart();
        request->send(200, "application/json", "{\"success\":true}");
    });
    server.on("/api/system/diag/demo/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println(F("[Diag] Demo stop"));
        gameFSM.requestDemoStop();
        request->send(200, "application/json", "{\"success\":true}");
    });

    // Diagnostics: Arm/Disarm status
    server.on("/api/system/diag/armstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.print(F("[Diag] Read Arm/Disarm -> "));
        if (global_arming_button.isPressed()) {
            Serial.println(F("Detected"));
            request->send(200, "application/json", F("{\"status\":\"Detected\"}"));
        } else {
            Serial.println(F("Not Detected"));
            request->send(200, "application/json", F("{\"status\":\"Not Detected\"}"));
        }
    });

    // Diagnostics: EEPROM read (size matches firmware layout: HW float + 5 calib ints)
    server.on("/api/system/diag/eeprom", HTTP_GET, [](AsyncWebServerRequest *request) {
        // HW version (float) + 5 battery calibration levels (0%, 25%, 50%, 75%, 100%)
        const size_t size = sizeof(float) + 5 * sizeof(int);
        uint8_t buf[size];

        EEPROM.begin(size);
        for (size_t i = 0; i < size; i++) {
            buf[i] = EEPROM.read(i);
        }
        EEPROM.end();

        // Encode raw bytes as uppercase hex string
        char hex[size * 2 + 1];
        for (size_t i = 0; i < size; i++) {
            snprintf(hex + i * 2, 3, "%02X", buf[i]);
        }
        hex[size * 2] = '\0';

        Serial.print(F("[Diag] EEPROM read: ")); Serial.println(hex);

        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();
        root["size"] = (uint32_t)size;
        root["hex"]  = hex;
        response->setCode(200);
        response->setLength();
        request->send(response);
    });

    // Diagnostics: EEPROM write
    static AsyncCallbackJsonWebHandler *diagEepromHandler = new AsyncCallbackJsonWebHandler("/api/system/diag/eeprom");
    diagEepromHandler->setMaxContentLength(256);
    diagEepromHandler->setMethod(HTTP_POST);
    diagEepromHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        const char *hex = json["hex"] | "";
        const size_t size        = sizeof(float) + 5 * sizeof(int);
        const size_t expectedLen = size * 2;

        Serial.print(F("[Diag] EEPROM write: ")); Serial.println(hex);

        if (strlen(hex) != expectedLen) {
            request->send(400, "application/json", "{\"error\":\"Invalid hex length\"}");
            return;
        }

        // Decode hex string into byte buffer
        uint8_t buf[size];
        for (size_t i = 0; i < size; i++) {
            char pair[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
            buf[i] = (uint8_t)strtol(pair, nullptr, 16);
        }

        EEPROM.begin(size);
        for (size_t i = 0; i < size; i++) {
            EEPROM.write(i, buf[i]);
        }
        EEPROM.commit();
        EEPROM.end();

        request->send(200, "application/json", "{\"success\":true}");
    });
    server.addHandler(diagEepromHandler);

    Serial.println("ApiSystem routes registered");
}



// Implementation methods
void ApiSystem::getInfo(AsyncWebServerRequest *request) {
    JsonDocument doc;

    #ifdef ESP32
        doc["device"] = "ESP32";
        doc["chipModel"] = ESP.getChipModel();
        doc["chipRevision"] = ESP.getChipRevision();
        doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        doc["flashSize"] = ESP.getFlashChipSize();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["sdkVersion"] = ESP.getSdkVersion();
    #elif defined(ESP8266)
        doc["device"] = "ESP8266";
        doc["chipId"] = ESP.getChipId();
        doc["coreVersion"] = ESP.getCoreVersion();
        doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        doc["flashSize"] = ESP.getFlashChipSize();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["sdkVersion"] = ESP.getSdkVersion();
    #endif

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiSystem::getStatus(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();

    #ifdef ESP32
        doc["heapSize"] = ESP.getHeapSize();
        doc["minFreeHeap"] = ESP.getMinFreeHeap();
        doc["maxAllocHeap"] = ESP.getMaxAllocHeap();
    #elif defined(ESP8266)
        doc["heapFragmentation"] = ESP.getHeapFragmentation();
        doc["maxFreeBlockSize"] = ESP.getMaxFreeBlockSize();
    #endif

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiSystem::restart(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["message"] = "Restarting device...";
    doc["success"] = true;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    delay(1000);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// Persist global_configuration to /network.json
// ---------------------------------------------------------------------------

void ApiSystem::saveConfiguration() {
    File f = FILESYSTEM.open("/network.json", "w");
    if (!f) {
        Serial.println("[CFG] Failed to open /network.json for writing");
        return;
    }
    serializeJson(global_configuration, f);
    f.close();
    Serial.println("[CFG] /network.json saved");
}

// ---------------------------------------------------------------------------
// Lifted-files manifest  GET /api/system/files
// ---------------------------------------------------------------------------

void ApiSystem::getFiles(AsyncWebServerRequest *request) {
    File f = FILESYSTEM.open("/filesystem.json", "r");
    if (!f) {
        request->send(404, "application/json", "{\"error\":\"filesystem.json not found\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err != DeserializationError::Ok) {
        request->send(500, "application/json", "{\"error\":\"Failed to parse filesystem.json\"}");
        return;
    }
    // Return only the lifted_files array so the web app doesn't need to know
    // the internal structure of filesystem.json.
    JsonDocument out;
    out["lifted_files"] = doc["lifted_files"];
    String response;
    serializeJson(out, response);
    request->send(200, "application/json", response);
}

// ---------------------------------------------------------------------------
// Generic file download  GET /api/system/file?path=/foo
// ---------------------------------------------------------------------------

void ApiSystem::getFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("path")) {
        request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
        return;
    }
    String path = request->getParam("path")->value();
    if (!FILESYSTEM.exists(path)) {
        request->send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }
    // Let ESPAsyncWebServer choose the MIME type from the file extension.
    request->send(FILESYSTEM, path);
}

// ---------------------------------------------------------------------------
// Generic file upload  POST /api/system/file?path=/foo
// ---------------------------------------------------------------------------

void ApiSystem::setFile(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                         size_t index, size_t total) {
    if (!request->hasParam("path")) {
        request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
        return;
    }
    String path = request->getParam("path")->value();

    if (index == 0) {
        Serial.print("[FILE] Writing "); Serial.println(path);
        _uploadFile = FILESYSTEM.open(path, "w");
    }

    if (_uploadFile) {
        _uploadFile.write(data, len);
    }

    if (index + len >= total) {
        if (_uploadFile) {
            _uploadFile.close();
            Serial.print("[FILE] Saved "); Serial.println(path);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Could not open file for writing\"}");
        }
    }
}

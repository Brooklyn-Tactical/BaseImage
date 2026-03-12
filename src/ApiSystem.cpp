#include "ApiSystem.h"

// Initialize static instance pointer
ApiSystem* ApiSystem::_instance = nullptr;

// Constructor
ApiSystem::ApiSystem() : _server(nullptr) {
    _instance = this;
}

// Register API routes
void ApiSystem::registerRoutes(WebServer& server) {
    _server = &server;
    
    // Register system API endpoints
    _server->on("/api/system/info", HTTP_GET, handleGetInfo);
    _server->on("/api/system/status", HTTP_GET, handleGetStatus);
    _server->on("/api/system/restart", HTTP_POST, handleRestart);
    _server->on("/api/system/wifi", HTTP_GET, handleGetWiFiInfo);
    
    Serial.println("ApiSystem routes registered");
}

// Static handlers
void ApiSystem::handleGetInfo() {
    if (_instance) {
        _instance->getInfo();
    }
}

void ApiSystem::handleGetStatus() {
    if (_instance) {
        _instance->getStatus();
    }
}

void ApiSystem::handleRestart() {
    if (_instance) {
        _instance->restart();
    }
}

void ApiSystem::handleGetWiFiInfo() {
    if (_instance) {
        _instance->getWiFiInfo();
    }
}

// Implementation methods
void ApiSystem::getInfo() {
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

    _server->send(200, "application/json", response);
}

void ApiSystem::getStatus() {
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

    _server->send(200, "application/json", response);
}

void ApiSystem::restart() {
    JsonDocument doc;
    doc["message"] = "Restarting device...";
    doc["success"] = true;
    
    String response;
    serializeJson(doc, response);
    
    _server->send(200, "application/json", response);
    
    delay(1000);
    ESP.restart();
}

void ApiSystem::getWiFiInfo() {
    JsonDocument doc;
    
    doc["mode"] = "AP";
    doc["ssid"] = WiFi.softAPSSID();
    doc["ip"] = WiFi.softAPIP().toString();
    doc["mac"] = WiFi.softAPmacAddress();
    doc["connectedClients"] = WiFi.softAPgetStationNum();
    
    String response;
    serializeJson(doc, response);
    
    _server->send(200, "application/json", response);
}


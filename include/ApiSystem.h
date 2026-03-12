#ifndef API_SYSTEM_H
#define API_SYSTEM_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Platform-specific includes
#ifdef ESP32
    #include <WebServer.h>
#elif defined(ESP8266)
    #include <ESP8266WebServer.h>
    #define WebServer ESP8266WebServer
#endif

class ApiSystem {
public:
    // Constructor
    ApiSystem();
    
    // Register API routes with the webserver
    void registerRoutes(WebServer& server);
    
private:
    // API endpoint handlers
    static void handleGetInfo();
    static void handleGetStatus();
    static void handleRestart();
    static void handleGetWiFiInfo();
    
    // Instance pointer for static callbacks
    static ApiSystem* _instance;
    
    // Helper methods
    void getInfo();
    void getStatus();
    void restart();
    void getWiFiInfo();
    
    // Reference to the server (set during registerRoutes)
    WebServer* _server;
};

#endif // API_SYSTEM_H


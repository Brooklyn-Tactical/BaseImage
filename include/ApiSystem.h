#ifndef API_SYSTEM_H
#define API_SYSTEM_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class ApiSystem {
public:
    // Constructor
    ApiSystem();

    // Register API routes with the webserver
    void registerRoutes(AsyncWebServer& server);

private:
    // Helper methods
    void getInfo(AsyncWebServerRequest *request);
    void getStatus(AsyncWebServerRequest *request);
    void restart(AsyncWebServerRequest *request);

    // Configuration persistence helper
    void saveConfiguration();

    // Generic file endpoints (lift/restore + filesystem.json manifest)
    void getFiles(AsyncWebServerRequest *request);
    void getFile(AsyncWebServerRequest *request);
    void setFile(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    // State for chunked file write
    File _uploadFile;
};

#endif // API_SYSTEM_H


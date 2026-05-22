#ifndef API_GAME_H
#define API_GAME_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

class ApiGame {
public:
    // Register API routes with the webserver
    void registerRoutes(AsyncWebServer& server);

private:
    // HTTP handlers for game state plus persisted default settings.
    void getGameState(AsyncWebServerRequest *request);
    void startGame(AsyncWebServerRequest *request, JsonVariant &json);

    void getGameDefaults(AsyncWebServerRequest *request);
    void setGameDefaults(AsyncWebServerRequest *request, JsonVariant &json);
};

#endif // API_GAME_H


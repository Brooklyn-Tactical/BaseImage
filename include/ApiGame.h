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
    // HTTP handler methods — delegate all game logic to gameFSM
    void getGameState(AsyncWebServerRequest *request);
    void setGameState(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void getScore(AsyncWebServerRequest *request);
    void addPoints(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void resetGame(AsyncWebServerRequest *request);
    void pauseResume(AsyncWebServerRequest *request);
    void setGameDuration(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};

#endif // API_GAME_H


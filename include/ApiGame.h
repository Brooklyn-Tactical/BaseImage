#ifndef API_GAME_H
#define API_GAME_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

class ApiGame {
public:
    // Constructor
    ApiGame();

    // Register API routes with the webserver
    void registerRoutes(AsyncWebServer& server);
    
private:
    // Helper methods
    void getGameState(AsyncWebServerRequest *request);
    void setGameState(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void getScore(AsyncWebServerRequest *request);
    void setScore(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void resetGame(AsyncWebServerRequest *request);
    void getBattery(AsyncWebServerRequest *request);

    // Game state variables

    String _gameState;
    int _score;
    unsigned long _gameStartTime;
};

#endif // API_GAME_H


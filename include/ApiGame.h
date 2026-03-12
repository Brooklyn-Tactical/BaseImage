#ifndef API_GAME_H
#define API_GAME_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Platform-specific includes
#ifdef ESP32
    #include <WebServer.h>
#elif defined(ESP8266)
    #include <ESP8266WebServer.h>
    #define WebServer ESP8266WebServer
#endif

class ApiGame {
public:
    // Constructor
    ApiGame();
    
    // Register API routes with the webserver
    void registerRoutes(WebServer& server);
    
private:
    // API endpoint handlers
    static void handleGetGameState();
    static void handleSetGameState();
    static void handleGetScore();
    static void handleSetScore();
    static void handleResetGame();
    
    // Instance pointer for static callbacks
    static ApiGame* _instance;
    
    // Helper methods
    void getGameState();
    void setGameState();
    void getScore();
    void setScore();
    void resetGame();
    
    // Reference to the server (set during registerRoutes)
    WebServer* _server;
    
    // Game state variables
    String _gameState;
    int _score;
    unsigned long _gameStartTime;
};

#endif // API_GAME_H


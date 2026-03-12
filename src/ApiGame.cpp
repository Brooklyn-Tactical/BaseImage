#include "ApiGame.h"

// Initialize static instance pointer
ApiGame* ApiGame::_instance = nullptr;

// Constructor
ApiGame::ApiGame() : _server(nullptr), _gameState("idle"), _score(0), _gameStartTime(0) {
    _instance = this;
}

// Register API routes
void ApiGame::registerRoutes(WebServer& server) {
    _server = &server;

    // Register game API endpoints
    _server->on("/api/game/state", HTTP_GET, handleGetGameState);
    _server->on("/api/game/state", HTTP_POST, handleSetGameState);
    _server->on("/api/game/score", HTTP_GET, handleGetScore);
    _server->on("/api/game/score", HTTP_POST, handleSetScore);
    _server->on("/api/game/reset", HTTP_POST, handleResetGame);

    Serial.println("ApiGame routes registered");
}

// Static handlers
void ApiGame::handleGetGameState() {
    if (_instance) {
        _instance->getGameState();
    }
}

void ApiGame::handleSetGameState() {
    if (_instance) {
        _instance->setGameState();
    }
}

void ApiGame::handleGetScore() {
    if (_instance) {
        _instance->getScore();
    }
}

void ApiGame::handleSetScore() {
    if (_instance) {
        _instance->setScore();
    }
}

void ApiGame::handleResetGame() {
    if (_instance) {
        _instance->resetGame();
    }
}

// Implementation methods
void ApiGame::getGameState() {
    JsonDocument doc;

    doc["state"] = _gameState;
    doc["score"] = _score;
    doc["uptime"] = millis() / 1000;

    if (_gameStartTime > 0) {
        doc["gameTime"] = (millis() - _gameStartTime) / 1000;
    } else {
        doc["gameTime"] = 0;
    }

    String response;
    serializeJson(doc, response);

    _server->send(200, "application/json", response);
}

void ApiGame::setGameState() {
    if (_server->hasArg("plain")) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, _server->arg("plain"));

        if (error) {
            JsonDocument errorDoc;
            errorDoc["error"] = "Invalid JSON";
            errorDoc["success"] = false;

            String response;
            serializeJson(errorDoc, response);
            _server->send(400, "application/json", response);
            return;
        }

        if (doc.containsKey("state")) {
            _gameState = doc["state"].as<String>();

            // If starting the game, record start time
            if (_gameState == "playing" && _gameStartTime == 0) {
                _gameStartTime = millis();
            }

            JsonDocument responseDoc;
            responseDoc["success"] = true;
            responseDoc["state"] = _gameState;

            String response;
            serializeJson(responseDoc, response);
            _server->send(200, "application/json", response);
        } else {
            JsonDocument errorDoc;
            errorDoc["error"] = "Missing 'state' field";
            errorDoc["success"] = false;

            String response;
            serializeJson(errorDoc, response);
            _server->send(400, "application/json", response);
        }
    } else {
        JsonDocument errorDoc;
        errorDoc["error"] = "No data received";
        errorDoc["success"] = false;

        String response;
        serializeJson(errorDoc, response);
        _server->send(400, "application/json", response);
    }
}

void ApiGame::getScore() {
    JsonDocument doc;

    doc["score"] = _score;
    doc["state"] = _gameState;

    String response;
    serializeJson(doc, response);

    _server->send(200, "application/json", response);
}

void ApiGame::setScore() {
    if (_server->hasArg("plain")) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, _server->arg("plain"));

        if (error) {
            JsonDocument errorDoc;
            errorDoc["error"] = "Invalid JSON";
            errorDoc["success"] = false;

            String response;
            serializeJson(errorDoc, response);
            _server->send(400, "application/json", response);
            return;
        }

        if (doc.containsKey("score")) {
            _score = doc["score"].as<int>();

            JsonDocument responseDoc;
            responseDoc["success"] = true;
            responseDoc["score"] = _score;

            String response;
            serializeJson(responseDoc, response);
            _server->send(200, "application/json", response);
        } else {
            JsonDocument errorDoc;
            errorDoc["error"] = "Missing 'score' field";
            errorDoc["success"] = false;

            String response;
            serializeJson(errorDoc, response);
            _server->send(400, "application/json", response);
        }
    } else {
        JsonDocument errorDoc;
        errorDoc["error"] = "No data received";
        errorDoc["success"] = false;

        String response;
        serializeJson(errorDoc, response);
        _server->send(400, "application/json", response);
    }
}

void ApiGame::resetGame() {
    _gameState = "idle";
    _score = 0;
    _gameStartTime = 0;

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Game reset";
    doc["state"] = _gameState;
    doc["score"] = _score;

    String response;
    serializeJson(doc, response);

    _server->send(200, "application/json", response);
}


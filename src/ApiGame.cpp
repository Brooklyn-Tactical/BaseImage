#include "ApiGame.h"

extern uint16_t global_battery_level;

// Constructor
ApiGame::ApiGame() : _gameState("idle"), _score(0), _gameStartTime(0) {
}

// Register API routes
void ApiGame::registerRoutes(AsyncWebServer& server) {
    server.on("/api/game/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getGameState(request);
    });
    // POST with body handler for JSON body
    server.on("/api/game/state", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* response sent in body handler */ },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->setGameState(request, data, len, index, total);
        }
    );
    server.on("/api/game/score", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getScore(request);
    });
    // POST with body handler for JSON body
    server.on("/api/game/score", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* response sent in body handler */ },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->setScore(request, data, len, index, total);
        }
    );
    server.on("/api/game/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->resetGame(request);
    });
    server.on("/api/battery", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getBattery(request);
    });

    Serial.println("ApiGame routes registered");
}



// Implementation methods
void ApiGame::getGameState(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["state"] = _gameState;
    doc["score"] = _score;
    doc["uptime"] = millis() / 1000;
    doc["gameTime"] = (_gameStartTime > 0) ? (millis() - _gameStartTime) / 1000 : 0;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::setGameState(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Only process on the final chunk
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\",\"success\":false}");
        return;
    }

    if (doc["state"].is<String>()) {
        _gameState = doc["state"].as<String>();

        if (_gameState == "playing" && _gameStartTime == 0) {
            _gameStartTime = millis();
        }

        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["state"] = _gameState;

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing 'state' field\",\"success\":false}");
    }
}

void ApiGame::getScore(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["score"] = _score;
    doc["state"] = _gameState;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::setScore(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Only process on the final chunk
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\",\"success\":false}");
        return;
    }

    if (doc["score"].is<int>()) {
        _score = doc["score"].as<int>();

        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["score"] = _score;

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing 'score' field\",\"success\":false}");
    }
}

void ApiGame::getBattery(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["battery"] = (int)constrain((global_battery_level * 100) / 1024, 0, 100);
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::resetGame(AsyncWebServerRequest *request) {
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
    request->send(200, "application/json", response);
}


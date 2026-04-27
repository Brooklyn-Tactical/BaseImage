#include "ApiGame.h"
#include "game.h"

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
    server.on("/api/game/addPoints", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* response sent in body handler */ },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->addPoints(request, data, len, index, total);
        }
    );
    server.on("/api/game/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->resetGame(request);
    });
    server.on("/api/game/pauseResume", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->pauseResume(request);
    });
    // POST with body handler for JSON body
    server.on("/api/game/duration", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* response sent in body handler */ },
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->setGameDuration(request, data, len, index, total);
        }
    );

    Serial.println("ApiGame routes registered");
}



// Implementation methods
void ApiGame::getGameState(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["state"]    = gameFSM.stateName();
    doc["score"]    = gameFSM.score();
    doc["uptime"]   = millis() / 1000;
    doc["gameTime"] = gameFSM.gameTimer();
    doc["duration"] = gameFSM.gameDuration();

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
        String requested = doc["state"].as<String>();

        if (requested == "playing") {
            gameFSM.start();
        } else if (requested == "ended") {
            gameFSM.end();
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid state value\",\"success\":false}");
            return;
        }

        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["state"]   = gameFSM.stateName();

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing 'state' field\",\"success\":false}");
    }
}

void ApiGame::getScore(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["score"] = gameFSM.score();
    doc["state"] = gameFSM.stateName();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::addPoints(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Only process on the final chunk
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\",\"success\":false}");
        return;
    }

    if (doc["points"].is<int>()) {
        gameFSM.addPoints(doc["points"].as<int>());

        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["score"]   = gameFSM.score();

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing 'points' field\",\"success\":false}");
    }
}

void ApiGame::resetGame(AsyncWebServerRequest *request) {
    gameFSM.reset();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Game reset";
    doc["state"]   = gameFSM.stateName();
    doc["score"]   = gameFSM.score();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::pauseResume(AsyncWebServerRequest *request) {
    gameFSM.pauseResume();

    JsonDocument doc;
    doc["success"] = true;
    doc["state"]   = gameFSM.stateName();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void ApiGame::setGameDuration(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Only process on the final chunk
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\",\"success\":false}");
        return;
    }

    if (doc["duration"].is<int>()) {
        bool ok = gameFSM.setGameDuration(doc["duration"].as<int>());

        if (ok) {
            JsonDocument responseDoc;
            responseDoc["success"]  = true;
            responseDoc["duration"] = gameFSM.gameDuration();

            String response;
            serializeJson(responseDoc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(409, "application/json", "{\"error\":\"Duration can only be set while in idle state and must be >= 1 second\",\"success\":false}");
        }
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing 'duration' field\",\"success\":false}");
    }
}


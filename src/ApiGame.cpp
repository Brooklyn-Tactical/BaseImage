#include "ApiGame.h"
#include "game.h"
#include "networking.h"   // provides FILESYSTEM macro and LittleFS

// Device configuration (defined in src/main.cpp) — used to surface the
// operator-configured "description" field in the game-state response so the
// game web UI can display it in its app bar while in game mode.
extern JsonDocument global_configuration;

// Register API routes
void ApiGame::registerRoutes(AsyncWebServer& server) {
    server.on("/api/game/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getGameState(request);
    });
    // POST/PUT {countdownSeconds, armDisarmSeconds, multipleArmings, standbyMinutes, showStandbyTime}
    // Sets all game parameters on gameFSM then dispatches EVT_GAME_API_START.
    static AsyncCallbackJsonWebHandler *gameStartHandler = new AsyncCallbackJsonWebHandler("/api/game/start");
    gameStartHandler->setMaxContentLength(300);
    gameStartHandler->setMethod(HTTP_POST | HTTP_PUT);
    gameStartHandler->onRequest([this](AsyncWebServerRequest *request, JsonVariant &json) {
        this->startGame(request, json);
    });
    server.addHandler(gameStartHandler);

    // GET /api/game/defaults — returns saved default settings from /game_defaults.json
    server.on("/api/game/defaults", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->getGameDefaults(request);
    });
    // POST/PUT — writes the payload verbatim to /game_defaults.json
    static AsyncCallbackJsonWebHandler *gameDefaultsHandler = new AsyncCallbackJsonWebHandler("/api/game/defaults");
    gameDefaultsHandler->setMaxContentLength(300);
    gameDefaultsHandler->setMethod(HTTP_POST | HTTP_PUT);
    gameDefaultsHandler->onRequest([this](AsyncWebServerRequest *request, JsonVariant &json) {
        this->setGameDefaults(request, json);
    });
    server.addHandler(gameDefaultsHandler);

    Serial.println("ApiGame routes registered");
}



// Implementation methods

/// GET /api/game/state — returns current FSM state and countdown config.
void ApiGame::getGameState(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["state"]            = gameFSM.stateName();
    doc["uptime"]           = millis() / 1000;
    doc["countdownSeconds"] = gameFSM._timerCountdownConfigValue / 1000UL;
    doc["armDisarmSeconds"] = gameFSM._armDisarmSeconds;
    doc["multipleArmings"]  = gameFSM._multipleArmings;
    doc["standbyMinutes"]   = gameFSM._standbyTimeMinutes;
    doc["showStandbyTime"]  = gameFSM._showStandbyTime;
    doc["description"]      = global_configuration["description"] | "";

    // gameTime = remaining standby time in whole seconds (0 when disabled / not latched / expired)
    unsigned long gameTimeSecs = 0;
    if (gameFSM._standbyTimeMinutes != 0 && gameFSM._standbyExpire != 0) {
        unsigned long now = millis();
        if (gameFSM._standbyExpire > now) {
            gameTimeSecs = (gameFSM._standbyExpire - now) / 1000UL;
        }
    }
    doc["gameTime"] = gameTimeSecs;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

/// POST|PUT /api/game/start — applies all game settings to gameFSM and dispatches EVT_GAME_API_START.
/// Transitions ConfigureState → AwaitStartState.
/// Body: { "countdownSeconds": <1+>, "armDisarmSeconds": <1-99>,
///         "multipleArmings": <bool>, "standbyMinutes": <0-199>, "showStandbyTime": <bool> }
void ApiGame::startGame(AsyncWebServerRequest *request, JsonVariant &json) {
    // ── Validate all fields ────────────────────────────────────────────────────
    if (!json["countdownSeconds"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'countdownSeconds' field (integer >= 1)\",\"success\":false}");
        return;
    }
    int countdownSeconds = json["countdownSeconds"].as<int>();
    if (countdownSeconds < 1) {
        request->send(400, "application/json", "{\"error\":\"'countdownSeconds' must be >= 1\",\"success\":false}");
        return;
    }

    if (!json["armDisarmSeconds"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'armDisarmSeconds' field (integer 1-99)\",\"success\":false}");
        return;
    }
    int armDisarmSeconds = json["armDisarmSeconds"].as<int>();
    if (armDisarmSeconds < 1 || armDisarmSeconds > 99) {
        request->send(400, "application/json", "{\"error\":\"'armDisarmSeconds' must be between 1 and 99\",\"success\":false}");
        return;
    }

    if (!json["multipleArmings"].is<bool>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'multipleArmings' field (boolean)\",\"success\":false}");
        return;
    }

    if (!json["standbyMinutes"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'standbyMinutes' field (integer 0-199)\",\"success\":false}");
        return;
    }
    int standbyMinutes = json["standbyMinutes"].as<int>();
    if (standbyMinutes < 0 || standbyMinutes > 199) {
        request->send(400, "application/json", "{\"error\":\"'standbyMinutes' must be between 0 and 199\",\"success\":false}");
        return;
    }

    if (!json["showStandbyTime"].is<bool>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'showStandbyTime' field (boolean)\",\"success\":false}");
        return;
    }

    // ── Apply settings to gameFSM ──────────────────────────────────────────────
    gameFSM._timerCountdownConfigValue = (unsigned long)countdownSeconds * 1000UL;
    gameFSM._armDisarmSeconds          = (byte)armDisarmSeconds;
    gameFSM._multipleArmings           = json["multipleArmings"].as<bool>();
    gameFSM._standbyTimeMinutes        = (unsigned int)standbyMinutes;
    gameFSM._showStandbyTime           = json["showStandbyTime"].as<bool>();

    Serial.print(F("[ApiGame] startGame: countdown=")); Serial.print(countdownSeconds); Serial.print(F("s"));
    Serial.print(F(" armDisarm="));  Serial.print(armDisarmSeconds);  Serial.print(F("s"));
    Serial.print(F(" multipleArmings=")); Serial.print(gameFSM._multipleArmings ? F("true") : F("false"));
    Serial.print(F(" standby="));    Serial.print(standbyMinutes);    Serial.print(F("min"));
    Serial.print(F(" showStandby=")); Serial.println(gameFSM._showStandbyTime ? F("true") : F("false"));

    // ── Dispatch EVT_GAME_API_START → ConfigureState → AwaitStartState ────────
    gameFSM.reset();
    gameFSM.start();

    JsonDocument responseDoc;
    responseDoc["success"] = true;
    responseDoc["state"]   = gameFSM.stateName();

    String response;
    serializeJson(responseDoc, response);
    request->send(200, "application/json", response);
}

/// GET /api/game/defaults — returns the contents of /game_defaults.json.
void ApiGame::getGameDefaults(AsyncWebServerRequest *request) {
    File f = FILESYSTEM.open("/game_defaults.json", "r");
    if (!f) {
        request->send(404, "application/json", "{\"error\":\"Defaults not found\",\"success\":false}");
        return;
    }
    String content = f.readString();
    f.close();
    request->send(200, "application/json", content);
}

/// POST|PUT /api/game/defaults — writes the received JSON body to /game_defaults.json.
/// Does NOT modify gameFSM state.
void ApiGame::setGameDefaults(AsyncWebServerRequest *request, JsonVariant &json) {
    File f = FILESYSTEM.open("/game_defaults.json", "w");
    if (!f) {
        request->send(500, "application/json", "{\"error\":\"Failed to open /game_defaults.json for writing\",\"success\":false}");
        return;
    }
    serializeJson(json, f);
    f.close();

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

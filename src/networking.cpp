// Networking HFSM implementation and captive portal helpers

#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"
#include "BatteryService.h"
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <HAL.h>
#include <TM1637TinyDisplay.h>
extern TM1637TinyDisplay global_disp;
const PROGMEM char LEDSTR_SYS[] = "SyS";
const PROGMEM char LEDSTR_CONFIG[] = "conF";

// Global HFSM instance (defined in src/main.cpp)
extern NetworkingFSM networkingFSM;

// Device configuration (defined in src/main.cpp)
extern JsonDocument global_configuration;

// Battery service (defined in src/main.cpp)
extern BatteryService batteryService;

// Helper to dispatch an event into the global networking FSM
namespace {
    inline void dispatchEvent(uint16_t type) {
        networkingFSM.dispatch(hfsm::Event(type));
    }
}

// ---------------------------------------------------------------------------
// State implementations
// ---------------------------------------------------------------------------

InitializeState::InitializeState(const char* name)
    : hfsm::State(nullptr, name) {
}

void InitializeState::enter() {
    Serial.println(F("[Networking] InitializeState: nothing to do, transitioning to Idle"));
    dispatchEvent(EVT_NET_INITIALIZED);
}

IdleState::IdleState(const char* name)
    : hfsm::State(nullptr, name) {
}

void IdleState::enter() {
    Serial.println(F("[Networking] IdleState: waiting for mode selection"));
}

void IdleState::update() {
    // DNS and mDNS are not yet started in this state; nothing to poll.
}

SetupAdminState::SetupAdminState(const char* name)
    : hfsm::State(nullptr, name) {
}

void SetupAdminState::enter() {
    Serial.println(F("[Networking] SetupAdminState: configuring AP and registering admin routes"));

    // NEVER use SSID/password from global_configuration!
    // ALWAYS use a unique chip-ID open AP
    String adminSsid;

#ifdef ESP8266
    uint32_t chipId = ESP.getChipId();
#else
    uint32_t chipId = (uint32_t)ESP.getEfuseMac();
#endif
    adminSsid = String(F("BrooklynTactical-")) + String(chipId & 0xFFF, HEX);

    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(TARGET_HOSTNAME);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    bool apOk = WiFi.softAP(adminSsid.c_str());
    if (!apOk) {
        Serial.println(F("[Networking] SetupAdminState: AP failed to start"));
        return;
    }
    Serial.print(F("[Networking] Admin AP SSID: ")); Serial.println(adminSsid);
    Serial.print(F("[Networking] Admin AP IP:   ")); Serial.println(WiFi.softAPIP());

    // Register API routes first so they take precedence over static file routes
    apiSystem.registerRoutes(server);

    // Serve the admin/system UI from LittleFS at /web/system, rooted at '/'.
    // Restrict the root handler to requests that use the captive-portal host.
    server.serveStatic("/", FILESYSTEM, "/web/system/").setDefaultFile("index.html").setFilter([](AsyncWebServerRequest *request) {
        Serial.print(F("Request host from static[/]: ")); Serial.print(request->host());Serial.println(request->url());
        return request->host() == TARGET_HOSTNAME;
//        return true;
    });

    // Serve shared assets (fonts, images, etc.) from the single shared LittleFS directory
    server.serveStatic("/shared/", FILESYSTEM, "/web/shared/");

    // set LED display to "sys"
    global_disp.setBrightness(1);
    global_disp.showString_P(LEDSTR_SYS);

    // Auto-transition to Run state once setup is done
    dispatchEvent(EVT_NET_SETUP_DONE);
}

SetupGameState::SetupGameState(const char* name)
    : hfsm::State(nullptr, name) {
}

void SetupGameState::enter() {
    Serial.println(F("[Networking] SetupGameState: configuring AP and registering game routes"));

    const char* ssid     = global_configuration["ssid"]     | "BrooklynTactical";
    const char* password = global_configuration["password"] | "password";
    int         channel  = global_configuration["channel"]  | 1;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    bool apOk = WiFi.softAP(ssid, password, channel);
    if (!apOk) {
        Serial.println(F("[Networking] SetupGameState: AP failed to start"));
        return;
    }
    Serial.print(F("[Networking] Game AP SSID: "));    Serial.println(ssid);
    Serial.print(F("[Networking] Game AP channel: ")); Serial.println(channel);
    Serial.print(F("[Networking] Game AP IP:   ")); Serial.println(WiFi.softAPIP());

    // Register API routes first so they take precedence over static file routes
    apiGame.registerRoutes(server);

    // Serve the game UI from LittleFS at /web/game, rooted at '/'
    server.serveStatic("/", FILESYSTEM, "/web/game/").setDefaultFile("index.html").setFilter([](AsyncWebServerRequest *request) {
        Serial.print(F("Request host from static[/]: ")); Serial.print(request->host());Serial.println(request->url());
        return request->host() == TARGET_HOSTNAME;
//        return true;
    });

    // Serve shared assets (fonts, images, etc.) from the single shared LittleFS directory
    server.serveStatic("/shared/", FILESYSTEM, "/web/shared/");

    // set LED display to "cOnF"
    global_disp.setBrightness(1);
    global_disp.showString_P(LEDSTR_CONFIG);

    // Auto-transition to Run state once setup is done
    dispatchEvent(EVT_NET_SETUP_DONE);
}

RunState::RunState(const char* name)
    : hfsm::State(nullptr, name) {
}

void RunState::enter() {
    Serial.println(F("[Networking] RunState: entering main run loop"));

    // audio tone to indicate mode selected
    tone(PIN_PIEZO_A,2500, 250);
    tone(PIN_PIEZO_B,2800, 250);

    // Version endpoint — hw from EEPROM, fw from build defines, fs from filesystem.json
    server.on("/api/versions", HTTP_GET, [](AsyncWebServerRequest *request) {
        // --- hw: float stored at EEPROM position 0 ---
        float hwVersion = 0.0f;
        EEPROM.begin(sizeof(float));
        EEPROM.get(0, hwVersion);
        EEPROM.end();

        // --- fw: assembled from build-time defines ---
        String fwVersion = String(FIRMWARE_VERSION_MAJOR) + "." + String(FIRMWARE_VERSION_MINOR);

        // --- fs: version field from /filesystem.json ---
        String fsVersion = "unknown";
        File fsFile = FILESYSTEM.open("/filesystem.json", "r");
        if (fsFile) {
            JsonDocument doc;
            if (deserializeJson(doc, fsFile) == DeserializationError::Ok) {
                fsVersion = doc["version"] | "unknown";
            }
            fsFile.close();
        }

        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();
        root["hw"] = String(hwVersion, 2);
        root["fw"] = fwVersion;
        root["fs"] = fsVersion;
        response->setCode(200);
        response->setLength();
        request->send(response);
    });

    // Battery level — common to both admin and game modes
    server.on("/api/battery", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["battery"] = batteryService.currentLevel();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // special handlers for redirecting all requests to the captive portal
    // -----------------------------------------------------------------------------------------------------------------
    server.on("/wpad.dat", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.print(F("Request from [/wpad.dat]: ")); Serial.println(request->url());
        request->send(200, "application/x-ns-proxy-autoconfig", "function FindProxyForURL(url, host){return \"PROXY 0.0.0.0:8080\";}");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.print(F("Request from [/connecttest.txt]: ")); Serial.println(request->url());
        request->send(404, "text/plain", "Not found");
    });

    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    server.on("/success", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200);
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    server.on("/startpage", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" TARGET_HOSTNAME "/");
    });
    // -----------------------------------------------------------------------------------------------------------------

    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.print(F("Request onNotFount: ")); Serial.print(request->host()); Serial.println(request->url());

        if (request->host() != TARGET_HOSTNAME && request->host() != apIP.toString()) {
            request->redirect("http://" TARGET_HOSTNAME "/index.html");
        } else {
            request->send(404);
        }
    });


    // Domains to block — answered with 0.0.0.0 instead of the AP IP.
    // OS connectivity checks in this list would otherwise bypass the captive portal.
    static const char* const blocked_domains[] PROGMEM = {
        "wpad"
    };

    // Start DNS server — wildcard (*) redirects everything to our AP IP,
    // except blocked domains which return 0.0.0.0.
    dnsServer.setTTL(240);
    dnsServer.setBlockedDomains(blocked_domains, sizeof(blocked_domains) / sizeof(char*));
    dnsServer.start(53, "*", apIP);
    Serial.println(F("[Networking] DNS server started"));

    // Start mDNS
    if (MDNS.begin(TARGET_HOSTNAME)) {
        Serial.println(F("[Networking] mDNS responder started"));
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println(F("[Networking] mDNS responder failed to start"));
    }

    // Start HTTP server
    server.begin();
    Serial.print(F("[Networking] HTTP server started — connect to http://"));
    Serial.println(F(TARGET_HOSTNAME));
}

void RunState::update() {
    dnsServer.processNextRequest();
    // AsyncWebServer handles requests asynchronously, no handleClient() needed
#ifdef ESP8266
    MDNS.update();
#endif
}

// ---------------------------------------------------------------------------
// NetworkingFSM implementation
// ---------------------------------------------------------------------------

NetworkingFSM::NetworkingFSM()
    : _machine()
    , _initState("Initialize")
    , _idleState("Idle")
    , _setupAdminState("SetupAdmin")
    , _setupGameState("SetupGame")
    , _runState("Run") {
    buildTransitions();
}

void NetworkingFSM::begin() {
    _machine.begin(&_initState);
}

void NetworkingFSM::update() {
    _machine.update();
}

void NetworkingFSM::dispatch(const hfsm::Event& e) {
    _machine.dispatch(e);
}

void NetworkingFSM::dispatch(uint16_t eventId) {
    _machine.dispatch(hfsm::Event(eventId));
}

void NetworkingFSM::buildTransitions() {
    // InitializeState: after EVT_NET_INITIALIZED -> Idle
    _initState._trans[0] = { EVT_NET_INITIALIZED, nullptr, &_idleState };
    _initState._transCount = 1;

    // IdleState: external events select admin or game setup
    _idleState._trans[0] = { EVT_NET_SELECTED_ADMIN, nullptr, &_setupAdminState };
    _idleState._trans[1] = { EVT_NET_SELECTED_GAME,  nullptr, &_setupGameState  };
    _idleState._transCount = 2;

    // Setup states: once setup is done, transition to Run
    _setupAdminState._trans[0] = { EVT_NET_SETUP_DONE, nullptr, &_runState };
    _setupAdminState._transCount = 1;

    _setupGameState._trans[0] = { EVT_NET_SETUP_DONE, nullptr, &_runState };
    _setupGameState._transCount = 1;

    // RunState: terminal, no transitions
}

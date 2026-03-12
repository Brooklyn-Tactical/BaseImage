// Networking HFSM implementation and captive portal helpers

#include "networking.h"
#include "ApiSystem.h"
#include "ApiGame.h"

// Global HFSM instance (defined in src/main.cpp)
extern NetworkingFSM networkingFSM;

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
    Serial.println();
    Serial.println(F("[Networking] InitializeState: starting AP/DNS/mDNS/server"));

    // Configure WiFi AP
    WiFi.mode(WIFI_AP_STA);

    Serial.print(F("Setting soft-AP configuration ... "));
    bool configOk = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    Serial.println(configOk ? F("Ready") : F("Failed!"));
    delay(200);

    Serial.print(F("Setting soft-AP ... "));
    bool apOk = WiFi.softAP(ap_ssid, ap_password);
    Serial.println(apOk ? F("Ready") : F("Failed!"));
    delay(200);

    if (!apOk) {
        Serial.println(F("[Networking] Failed to initialize Access Point"));
        return;  // Stay in InitializeState; no transition
    }

    Serial.print(F("Soft-AP IP address = "));
    Serial.println(WiFi.softAPIP());
    Serial.print(F("AP SSID: "));
    Serial.println(ap_ssid);

    // Start DNS server - hijack all domains to our IP
    dnsServer.start(53, "*", apIP);
    Serial.println(F("DNS server started - hijacking all domains"));

    // Start mDNS
    if (MDNS.begin(TARGET_HOSTNAME)) {
        Serial.println(F("mDNS responder started"));
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println(F("Error setting up mDNS responder"));
    }

    // Start HTTP server
    server.begin();
    Serial.println(F("HTTP server started"));
    Serial.print(F("Connect to WiFi '"));
    Serial.print(ap_ssid);
    Serial.print(F("' and navigate to http://"));
    Serial.println(F(TARGET_HOSTNAME));

    // Auto-transition to Idle state
    dispatchEvent(EVT_INITIALIZED);
}

IdleState::IdleState(const char* name)
    : hfsm::State(nullptr, name) {
}

void IdleState::enter() {
    Serial.println(F("[Networking] IdleState: waiting for mode selection"));
}

void IdleState::update() {
    // Keep DNS and mDNS responsive while waiting for external events
    dnsServer.processNextRequest();
    // ESP8266 mDNS implementation requires explicit update() calls;
    // ESP32's ESPmDNS implementation does not expose update() and
    // runs its housekeeping internally.
#ifdef ESP8266
    MDNS.update();
#endif
}

SetupAdminState::SetupAdminState(const char* name)
    : hfsm::State(nullptr, name) {
}

void SetupAdminState::enter() {
    Serial.println(F("[Networking] SetupAdminState: registering admin routes"));
    // Register API routes first so they take precedence over static file routes
    apiSystem.registerRoutes(server);

    // Serve the admin/system UI from LittleFS at /web/system, rooted at '/'
    // Note: on current cores, serveStatic() returns void and uses its
    // internal default index file (typically "index.html").
    server.serveStatic("/", FILESYSTEM, "/web/system");
//    server.begin();

    // Auto-transition to Run state once setup is done
    dispatchEvent(EVT_SETUP_DONE);
}

SetupGameState::SetupGameState(const char* name)
    : hfsm::State(nullptr, name) {
}

void SetupGameState::enter() {
    Serial.println(F("[Networking] SetupGameState: registering game routes"));
    // Register API routes first so they take precedence over static file routes
    // apiGame.registerRoutes(server);
    //
    // // Serve the game UI from LittleFS at /web/game, rooted at '/'
    // server.serveStatic("/", FILESYSTEM, "/web/game");
//    server.begin();

    // Auto-transition to Run state once setup is done
    dispatchEvent(EVT_SETUP_DONE);
}

RunState::RunState(const char* name)
    : hfsm::State(nullptr, name) {
}

void RunState::enter() {
    Serial.println(F("[Networking] RunState: entering main run loop"));
    server.begin();
}

void RunState::update() {
    dnsServer.processNextRequest();
    server.handleClient();
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

    apiGame.registerRoutes(server);
    // Serve the game UI from LittleFS at /web/game, rooted at '/'
    server.serveStatic("/", FILESYSTEM, "/web/game");
}

void NetworkingFSM::update() {
    _machine.update();
}

void NetworkingFSM::dispatch(const hfsm::Event& e) {
    _machine.dispatch(e);
}

void NetworkingFSM::buildTransitions() {
    // InitializeState: after EVT_INITIALIZED -> Idle
    _initState._trans[0] = { EVT_INITIALIZED, nullptr, &_idleState };
    _initState._transCount = 1;

    // IdleState: external events select admin or game setup
    _idleState._trans[0] = { EVT_SELECTED_ADMIN, nullptr, &_setupAdminState };
    _idleState._trans[1] = { EVT_SELECTED_GAME,  nullptr, &_setupGameState  };
    _idleState._transCount = 2;

    // Setup states: once setup is done, transition to Run
    _setupAdminState._trans[0] = { EVT_SETUP_DONE, nullptr, &_runState };
    _setupAdminState._transCount = 1;

    _setupGameState._trans[0] = { EVT_SETUP_DONE, nullptr, &_runState };
    _setupGameState._transCount = 1;

    // RunState: terminal, no transitions
}

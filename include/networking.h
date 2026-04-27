#ifndef NETWORKING_H
#define NETWORKING_H

#include <Arduino.h>
#include "DNSServer.h"
#include "HFSMEvents.h"
#include <HFSM.h>
#include <ESPAsyncWebServer.h>

// Platform-specific includes
#ifdef ESP32
    #include <WiFi.h>
    #include <ESPmDNS.h>
    #include <LittleFS.h>
    #define FILESYSTEM LittleFS
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
    #include <LittleFS.h>
    #define FILESYSTEM LittleFS
#endif

// Target hostname for captive portal
#define TARGET_HOSTNAME "my.milsim.prop"

// --- Forward declarations for types owned by main.cpp ---
class ApiSystem;
class ApiGame;

// --- Globals defined in main.cpp ---
extern AsyncWebServer server;
extern DNSServer      dnsServer;
extern IPAddress      apIP;
extern ApiSystem      apiSystem;
extern ApiGame        apiGame;

// ============================================================
//  State classes
// ============================================================

/// Sets up WiFi AP, DNS, mDNS, onNotFound handler, and starts the server.
/// Auto-dispatches EVT_INITIALIZED on completion.
class InitializeState : public hfsm::State {
public:
    InitializeState(const char* name = "Initialize");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];
    uint8_t          _transCount = 0;
};

/// Waits for an external event (button press) to choose admin or game mode.
class IdleState : public hfsm::State {
public:
    IdleState(const char* name = "Idle");
    void enter() override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[2];
    uint8_t          _transCount = 0;
};

/// Registers ApiSystem routes, then auto-dispatches EVT_SETUP_DONE.
class SetupAdminState : public hfsm::State {
public:
    SetupAdminState(const char* name = "SetupAdmin");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];
    uint8_t          _transCount = 0;
};

/// Registers ApiGame routes, then auto-dispatches EVT_SETUP_DONE.
class SetupGameState : public hfsm::State {
public:
    SetupGameState(const char* name = "SetupGame");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];
    uint8_t          _transCount = 0;
};

/// Terminal run state — polls DNS and handles HTTP clients.
class RunState : public hfsm::State {
public:
    RunState(const char* name = "Run");
    void enter()  override;
    void update() override;
};

// ============================================================
//  NetworkingFSM wrapper
// ============================================================

class NetworkingFSM : public IFsmDispatcher {
public:
    NetworkingFSM();

    /// Initialise the FSM and enter the InitializeState.
    void begin();

    /// Tick — call from loop().
    void update();

    /// Forward an hfsm::Event into the state machine.
    void dispatch(const hfsm::Event& e);

    /// IFsmDispatcher implementation — wraps dispatch(hfsm::Event).
    void dispatch(uint16_t eventId) override;

private:
    hfsm::StateMachine _machine;

    // State instances (owned by this class)
    InitializeState _initState;
    IdleState       _idleState;
    SetupAdminState _setupAdminState;
    SetupGameState  _setupGameState;
    RunState        _runState;

    /// Wire up the transition tables (called from constructor).
    void buildTransitions();
};

#endif // NETWORKING_H


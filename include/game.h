#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <HFSM.h>
#include "HFSMEvents.h"

// ============================================================
//  Display string constants (PROGMEM)
// ============================================================
// New strings needed by the new states (existing LEDSTR_READY
// and LEDSTR_BATTERY remain defined in game.cpp).

extern const char LEDSTR_AWAIT[];    // "----"  AwaitArming
extern const char LEDSTR_BLANK[];    // "    "
extern const char LEDSTR_GOOD[];    // "good"  Disarmed
extern const char LEDSTR_OFFENSE[]; // "dEAd"  OffenseWin
extern const char LEDSTR_DEFENSE[]; // "SAFE"  DefenseWin
extern const char LEDSTR_DONE[];    // "DonE"  GameOver

// ============================================================
//  Forward declarations
// ============================================================

class GameFSM;

// ============================================================
//  State classes
// ============================================================

// --- Top-level non-game state ---

/// Waiting for EVT_GAME_API_START from the web API before gameplay begins.
class ConfigureState : public hfsm::State {
public:
    ConfigureState(const char* name = "Configure");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_GAME_API_START -> AwaitStart, EVT_CORE_LOW_BATTERY -> LowBattery, EVT_GAME_DEMO_START -> Demo
    uint8_t          _transCount = 0;
};

/// Scripted hardware demo started from the Admin UI. Top-level state; returns to
/// ConfigureState on EVT_GAME_DEMO_END (Stop button). Runs until stopped.
class DemoState : public hfsm::State {
public:
    DemoState(const char* name = "Demo");
    void enter()  override;
    void exit()   override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[2];   // EVT_GAME_DEMO_END -> Configure, EVT_CORE_LOW_BATTERY -> LowBattery
    uint8_t          _transCount = 0;

private:
    void setPhase(uint8_t phase);
    void render(bool on);          // draws _digits honouring _blinkMask / _blankAll when !on

    uint8_t       _phase       = 0;
    unsigned long _phaseStart  = 0;
    unsigned long _tickMs      = 0;   // per-second countdown tick reference
    unsigned long _piezoUntil  = 0;
    unsigned long _expire      = 0;   // countdown end (millis)
    uint8_t       _digits[4]   = {0, 0, 0, 0};
    uint8_t       _blinkMask   = 0;   // bit i = digit i blinks
    bool          _blinkAll    = false;
    unsigned int  _blinkCycle  = 0;   // 0 = no blinking
    unsigned int  _blinkOnMs   = 0;
    bool          _lastOn      = true;
    bool          _fadeDir     = false;
    bool          _blanked     = false;
};

// --- Root of gameplay hierarchy ---

/// Parent of all active gameplay states.
/// Handles EVT_CORE_LOW_BATTERY so all child states automatically transition
/// to LowBatteryState via HFSM event bubbling.
class GameRootState : public hfsm::State {
public:
    GameRootState(const char* name = "GameRoot");

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];   // EVT_CORE_LOW_BATTERY -> LowBattery
    uint8_t          _transCount = 0;
};

// --- Gameplay child states (all children of GameRootState) ---

/// Waiting for the operator to remove the pin and begin the arming flow.
class AwaitStartState : public hfsm::State {
public:
    AwaitStartState(hfsm::State* parent, const char* name = "AwaitStart");
    void enter()  override;
    void exit()   override;   // latches standby deadline on departure
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];   // EVT_CORE_PIN_REMOVED -> AwaitArming
    uint8_t          _transCount = 0;
};

/// Waiting for arming pin to be inserted to begin the arming sequence.
class AwaitArmingState : public hfsm::State {
public:
    AwaitArmingState(hfsm::State* parent, const char* name = "AwaitArming");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[2];   // EVT_CORE_PIN_INSERTED -> Arming, EVT_GAME_STANDBY_END -> DefenseWin
    uint8_t          _transCount = 0;

private:
    unsigned long _lastSecondMs = 0;  // wall-clock reference for per-second standby flash
    bool          _showingTime  = false;
};

/// 10-second arming countdown with variable-pitch dual-piezo tones and LED breathing.
class ArmingState : public hfsm::State {
public:
    ArmingState(hfsm::State* parent, GameFSM* fsm, const char* name = "Arming");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_CORE_PIN_REMOVED -> AwaitArming, EVT_GAME_TIMER_END -> Armed, EVT_GAME_STANDBY_END -> DefenseWin
    uint8_t          _transCount = 0;
private:
    GameFSM* _fsm;
};

/// Fully armed — LED fast-breathing, steady high tone; pin removal starts the countdown.
class ArmedState : public hfsm::State {
public:
    ArmedState(hfsm::State* parent, GameFSM* fsm, const char* name = "Armed");
    void enter()  override;
    void exit()   override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[2];   // EVT_CORE_PIN_REMOVED -> Countdown, EVT_GAME_STANDBY_END -> DefenseWin
    uint8_t          _transCount = 0;
private:
    GameFSM* _fsm;
};

/// Main game countdown — MM:SS display, chirp each second, LED breathing; pin insert to disarm.
class CountdownState : public hfsm::State {
public:
    CountdownState(hfsm::State* parent, GameFSM* fsm, const char* name = "Countdown");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_CORE_PIN_INSERTED -> Disarming, EVT_GAME_TIMER_END -> OffenseWin, EVT_GAME_STANDBY_END -> OffenseWin
    uint8_t          _transCount = 0;
private:
    GameFSM* _fsm;
};

/// 10-second disarm countdown with reversed-pitch tones; pin removal cancels back to Countdown.
class DisarmingState : public hfsm::State {
public:
    DisarmingState(hfsm::State* parent, GameFSM* fsm, const char* name = "Disarming");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_CORE_PIN_REMOVED -> Countdown, EVT_GAME_TIMER_END -> Disarmed, EVT_GAME_STANDBY_END -> OffenseWin
    uint8_t          _transCount = 0;
private:
    GameFSM* _fsm;
};

/// Successfully disarmed — shows "good"; pin removal moves to GameOver (single arming)
/// or back to AwaitArming (multiple armings), selected via guard functions.
class DisarmedState : public hfsm::State {
public:
    DisarmedState(hfsm::State* parent, const char* name = "Disarmed");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_CORE_PIN_REMOVED (guarded) -> GameOver | AwaitArming, EVT_GAME_STANDBY_END -> DefenseWin
    uint8_t          _transCount = 0;
};

/// Countdown expired without disarm — shows "dEAd"; pin removal moves to GameOver.
class OffenseWinState : public hfsm::State {
public:
    OffenseWinState(hfsm::State* parent, const char* name = "OffenseWin");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];   // EVT_CORE_PIN_REMOVED -> GameOver
    uint8_t          _transCount = 0;
};

/// Standby timer expired while prop was in a defense-favourable state — shows "SAFE".
class DefenseWinState : public hfsm::State {
public:
    DefenseWinState(hfsm::State* parent, const char* name = "DefenseWin");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];   // EVT_CORE_PIN_REMOVED -> GameOver
    uint8_t          _transCount = 0;
};

/// End of round — alternates "DonE" / battery display; rapid pin toggle resets to AwaitStart.
class GameOverState : public hfsm::State {
public:
    GameOverState(hfsm::State* parent, GameFSM* fsm, const char* name = "GameOver");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[1];   // EVT_CORE_PIN_TOGGLED -> AwaitStart
    uint8_t          _transCount = 0;
private:
    GameFSM* _fsm;
};

// --- Terminal global state ---

/// Entered from any state (via RootState bubbling) when EVT_CORE_LOW_BATTERY fires.
class LowBatteryState : public hfsm::State {
public:
    LowBatteryState(const char* name = "LowBattery");
    void enter()  override;
    void update() override;
};

// ============================================================
//  GameFSM wrapper
// ============================================================

class GameFSM : public IFsmDispatcher {
public:
    GameFSM();

    /// Initialise the FSM and enter ConfigureState.
    void begin();

    /// Tick — call from loop().
    void update();

    /// IFsmDispatcher override — receives uint16_t events from BatteryService.
    void dispatch(uint16_t eventId) override;

    /// Convenience overload for callers that already hold an hfsm::Event.
    void dispatch(const hfsm::Event& e);

    // ── Game control ─────────────────────────────────────────────────────────

    /// Dispatch EVT_GAME_API_START: moves ConfigureState -> AwaitStartState.
    void start();

    /// Reset runtime state and re-enter ConfigureState without clearing
    /// operator-configured values.
    void reset();

    // ── State accessor ───────────────────────────────────────────────────────

    /// String label of the current state for API responses.
    const char* stateName() const;

    /// Thread-safe requests from web handlers; acted on in update() (main loop).
    void requestDemoStart() { _demoStartRequested = true; }
    void requestDemoStop()  { _demoStopRequested  = true; }

    // ── Data accessible by child states ──────────────────────────────────────

    /// Configured countdown duration in milliseconds (set via API, default 5 min).
    unsigned long _timerCountdownConfigValue;

    /// Absolute millis() timestamp when the main countdown expires.
    unsigned long _masterClockExpire;

    /// Arm/disarm duration in seconds (configurable via API, range 1–99, default 10).
    byte          _armDisarmSeconds;

    /// When true, a successful disarm returns to AwaitArming instead of ending the round.
    bool          _multipleArmings;

    /// Standby wait duration in minutes (configurable via API, range 0–199, default 30).
    unsigned int  _standbyTimeMinutes;

    /// When true, the remaining standby time is shown on the LED display.
    bool          _showStandbyTime;

    /// Absolute millis() timestamp when the standby period expires (0 = not yet latched / disabled).
    unsigned long _standbyExpire;

    /// Arm/disarm sub-countdown: current seconds remaining (display tracking).
    byte          _ArmDisarmSecondsLeft;

    /// General-purpose absolute millis() timestamp used by arm/disarm countdowns
    /// and as a tick reference in CountdownState and GameOverState.
    unsigned long _ArmDisarmTimerMs;

    /// LED fade direction toggle (false = fading up, true = fading down).
    bool          _fadeDirection;

    /// Absolute millis() timestamp after which the piezo is considered free.
    /// Used to simulate isPlaying() — play only when millis() >= _piezoUntil.
    unsigned long _piezoUntil;

private:
    hfsm::StateMachine _machine;

    // State instances (owned by this class, hierarchy wired in constructor)
    ConfigureState   _configureState;
    GameRootState    _gameRootState;
    AwaitStartState  _awaitStartState;
    AwaitArmingState _awaitArmingState;
    ArmingState      _armingState;
    ArmedState       _armedState;
    CountdownState   _countdownState;
    DisarmingState   _disarmingState;
    DisarmedState    _disarmedState;
    OffenseWinState  _offenseWinState;
    DefenseWinState  _defenseWinState;
    GameOverState    _gameOverState;
    LowBatteryState  _lowBatteryState;
    DemoState        _demoState;

    volatile bool    _demoStartRequested = false;
    volatile bool    _demoStopRequested  = false;

    /// Wire up all transition tables (called from constructor).
    void buildTransitions();
};

// Global instance (defined in src/game.cpp)
extern GameFSM gameFSM;

#endif // GAME_H


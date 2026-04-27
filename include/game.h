#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <HFSM.h>
#include "HFSMEvents.h"

// ============================================================
//  Forward declaration (needed by RunningState for back-pointer)
// ============================================================

class GameFSM;

// ============================================================
//  State classes
// ============================================================

/// Waiting for the game to be started via EVT_GAME_API_START.
class ConfigureState : public hfsm::State {
public:
    ConfigureState(const char* name = "Configure");
    void enter() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[2];   // EVT_GAME_API_START, EVT_CORE_LOW_BATTERY
    uint8_t          _transCount = 0;
};

/// Active game loop — transitions to Paused, Ended, or LowBattery.
class RunningState : public hfsm::State {
public:
    RunningState(const char* name, GameFSM* fsm);
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[4];   // EVT_GAME_API_END, EVT_GAME_TIMER_END, EVT_CORE_LOW_BATTERY, EVT_GAME_API_PAUSE_RESUME
    uint8_t          _transCount = 0;

private:
    GameFSM*      _fsm;            // back-pointer for timer check (friend access to private members)
    unsigned long _tickStartTime;  // millis() snapshot at the start of the current 1-second tick
};

/// Game is paused — timer frozen; EVT_GAME_API_PAUSE_RESUME resumes back to RunningState.
class PausedState : public hfsm::State {
public:
    PausedState(const char* name = "Paused");
    void enter()  override;
    void update() override;

    const hfsm::Transition* transitions() const override { return _trans; }
    uint8_t transitionCount()             const override { return _transCount; }

    hfsm::Transition _trans[3];   // EVT_GAME_API_PAUSE_RESUME, EVT_GAME_API_END, EVT_CORE_LOW_BATTERY
    uint8_t          _transCount = 0;
};

/// Game-over state — entered via EVT_GAME_API_END. Terminal until reset() is called.
class EndedState : public hfsm::State {
public:
    EndedState(const char* name = "Ended");
    void enter()  override;
    void update() override;
};

/// Entered from any state when EVT_CORE_LOW_BATTERY is received.
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

    /// Initialise the FSM and enter the ConfigureState.
    void begin();

    /// Tick — call from loop().
    void update();

    /// IFsmDispatcher override — receives uint16_t events from BatteryService.
    void dispatch(uint16_t eventId) override;

    /// Convenience overload for callers that already hold an hfsm::Event.
    void dispatch(const hfsm::Event& e);

    // ── Game control ─────────────────────────────────────────────────────────

    /// Start the game: dispatches EVT_GAME_API_START (tick timer seeded in RunningState::enter).
    void start();

    /// End the game: dispatches EVT_GAME_API_END.
    void end();

    /// Toggle pause/resume: dispatches EVT_GAME_API_PAUSE_RESUME.
    void pauseResume();

    /// Reset the game: clears score and timer, re-enters ConfigureState.
    void reset();

    /// Add delta to the score. Ignored (with debug output) if not in RunningState.
    void addPoints(int delta);

    /// Set game duration in seconds. Rejected if < 1 or if not in ConfigureState.
    /// Returns true on success, false on failure.
    bool setGameDuration(int seconds);

    // ── Game state accessors ─────────────────────────────────────────────────

    /// Current score.
    int score() const;

    /// Returns the number of whole seconds elapsed since the game started (frozen when not in RunningState).
    int gameTimer() const;

    /// Game duration in seconds (default 60).
    int gameDuration() const;

    /// String label of the current state for API responses.
    /// Returns: "idle" | "playing" | "paused" | "ended" | "lowbattery"
    const char* stateName() const;

private:
    friend class RunningState;  // grants access to _gameTimer and _gameDuration

    hfsm::StateMachine _machine;

    // State instances (owned by this class)
    ConfigureState  _configureState;
    RunningState    _runningState;
    PausedState     _pausedState;
    EndedState      _endedState;
    LowBatteryState _lowBatteryState;

    // Game data
    int           _score;
    int           _gameTimer;     // elapsed seconds; increments once per second while in RunningState
    int           _gameDuration;  // seconds; default 60, minimum 1

    /// Wire up the transition tables (called from constructor).
    void buildTransitions();
};

// Global instance (defined in src/game.cpp)
extern GameFSM gameFSM;

#endif // GAME_H


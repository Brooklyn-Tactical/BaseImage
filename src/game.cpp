// Game HFSM implementation

#include "game.h"
#include "HAL.h"

// ---------------------------------------------------------------------------
// Global GameFSM instance
// ---------------------------------------------------------------------------

GameFSM gameFSM;

// ---------------------------------------------------------------------------
// Helper — dispatch an event into the global game FSM
// ---------------------------------------------------------------------------

namespace {
    inline void dispatchEvent(uint16_t type) {
        gameFSM.dispatch(hfsm::Event(type));
    }
}

// ---------------------------------------------------------------------------
// State implementations
// ---------------------------------------------------------------------------

ConfigureState::ConfigureState(const char* name)
    : hfsm::State(nullptr, name) {
}

void ConfigureState::enter() {
    Serial.println(F("[Game] ConfigureState: waiting for EVT_GAME_API_START"));
    digitalWrite(LED_PIN, HIGH); // LED off (active-LOW)
}

RunningState::RunningState(const char* name, GameFSM* fsm)
    : hfsm::State(nullptr, name), _fsm(fsm) {
}

void RunningState::enter() {
    Serial.println(F("[Game] RunningState: game is now running"));
    digitalWrite(LED_PIN, LOW); // LED on (active-LOW)
    _tickStartTime = millis();  // seed the first tick
}

void RunningState::update() {
    // Advance the game timer by one second per tick
    if (millis() - _tickStartTime >= 1000) {
        _fsm->_gameTimer++;
        _tickStartTime += 1000;  // drift-correcting: shift window, not anchored to now
    }

    // Check whether the configured game duration has elapsed
    if (_fsm->_gameTimer >= _fsm->_gameDuration) {
        Serial.println(F("[Game] Timer expired — dispatching EVT_GAME_TIMER_END"));
        _fsm->dispatch(EVT_GAME_TIMER_END);
    }
}

PausedState::PausedState(const char* name)
    : hfsm::State(nullptr, name) {
}

void PausedState::enter() {
    Serial.println(F("[Game] PausedState: game paused — timer frozen"));
    digitalWrite(LED_PIN, HIGH); // LED off (active-LOW) while paused
}

void PausedState::update() {
    // Timer is frozen; nothing to do while paused
}

EndedState::EndedState(const char* name)
    : hfsm::State(nullptr, name) {
}

void EndedState::enter() {
    Serial.println(F("[Game] EndedState: game over"));
    digitalWrite(LED_PIN, HIGH); // LED off (active-LOW)
}

void EndedState::update() {
    // Game-over display logic goes here
}

LowBatteryState::LowBatteryState(const char* name)
    : hfsm::State(nullptr, name) {
}

void LowBatteryState::enter() {
    Serial.println(F("[Game] LowBatteryState: battery critical — game suspended"));
    digitalWrite(LED_PIN, HIGH); // LED off (active-LOW)
}

void LowBatteryState::update() {
    // Low-battery handling logic goes here (e.g. alert LED, pause timers)
}

// ---------------------------------------------------------------------------
// GameFSM implementation
// ---------------------------------------------------------------------------

GameFSM::GameFSM()
    : _machine()
    , _configureState("Configure")
    , _runningState("Running", this)
    , _pausedState("Paused")
    , _endedState("Ended")
    , _lowBatteryState("LowBattery")
    , _score(0)
    , _gameTimer(0)
    , _gameDuration(60) {
    buildTransitions();
}


void GameFSM::begin() {
    _machine.begin(&_configureState);
}

void GameFSM::update() {
    _machine.update();
}

void GameFSM::dispatch(uint16_t eventId) {
    _machine.dispatch(hfsm::Event(eventId));
}

void GameFSM::dispatch(const hfsm::Event& e) {
    _machine.dispatch(e);
}

void GameFSM::buildTransitions() {
    // ConfigureState: EVT_GAME_API_START -> Running, EVT_CORE_LOW_BATTERY -> LowBattery
    _configureState._trans[0] = { EVT_GAME_API_START,   nullptr, &_runningState    };
    _configureState._trans[1] = { EVT_CORE_LOW_BATTERY, nullptr, &_lowBatteryState };
    _configureState._transCount = 2;

    // RunningState: end/timer -> Ended, low-battery -> LowBattery, pause -> Paused
    _runningState._trans[0] = { EVT_GAME_API_END,          nullptr, &_endedState      };
    _runningState._trans[1] = { EVT_GAME_TIMER_END,        nullptr, &_endedState      };
    _runningState._trans[2] = { EVT_CORE_LOW_BATTERY,      nullptr, &_lowBatteryState };
    _runningState._trans[3] = { EVT_GAME_API_PAUSE_RESUME, nullptr, &_pausedState     };
    _runningState._transCount = 4;

    // PausedState: resume -> Running, end -> Ended, low-battery -> LowBattery
    _pausedState._trans[0] = { EVT_GAME_API_PAUSE_RESUME, nullptr, &_runningState    };
    _pausedState._trans[1] = { EVT_GAME_API_END,          nullptr, &_endedState      };
    _pausedState._trans[2] = { EVT_CORE_LOW_BATTERY,      nullptr, &_lowBatteryState };
    _pausedState._transCount = 3;

    // EndedState: terminal (reset() calls begin() to return to ConfigureState)

    // LowBatteryState: terminal, no transitions
}

// ── Game control ──────────────────────────────────────────────────────────────

void GameFSM::start() {
    _machine.dispatch(hfsm::Event(EVT_GAME_API_START));
}

void GameFSM::end() {
    _machine.dispatch(hfsm::Event(EVT_GAME_API_END));
}

void GameFSM::pauseResume() {
    _machine.dispatch(hfsm::Event(EVT_GAME_API_PAUSE_RESUME));
}

void GameFSM::reset() {
    _score = 0;
    _gameTimer = 0;
    _machine.begin(&_configureState);
}

void GameFSM::addPoints(int delta) {
    if (_machine.current() != &_runningState) {
        Serial.println(F("[Game] addPoints ignored — game not in RunningState"));
        return;
    }
    _score += delta;
    Serial.print(F("[Game] addPoints: +")); Serial.print(delta);
    Serial.print(F(" → total: "));         Serial.println(_score);
}

bool GameFSM::setGameDuration(int seconds) {
    if (_machine.current() != &_configureState) {
        Serial.println(F("[Game] setGameDuration ignored — not in ConfigureState"));
        return false;
    }
    if (seconds < 1) {
        Serial.println(F("[Game] setGameDuration rejected — duration must be >= 1 second"));
        return false;
    }
    _gameDuration = seconds;
    Serial.print(F("[Game] setGameDuration: ")); Serial.print(_gameDuration); Serial.println(F("s"));
    return true;
}

// ── Game state accessors ──────────────────────────────────────────────────────

int GameFSM::score() const {
    return _score;
}

int GameFSM::gameTimer() const {
    return _gameTimer;
}

int GameFSM::gameDuration() const {
    return _gameDuration;
}

const char* GameFSM::stateName() const {
    const hfsm::State* cur = _machine.current();
    if (cur == &_runningState)    return "playing";
    if (cur == &_pausedState)     return "paused";
    if (cur == &_endedState)      return "ended";
    if (cur == &_lowBatteryState) return "lowbattery";
    return "idle"; // ConfigureState or not yet begun
}


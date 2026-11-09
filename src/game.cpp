// Game HFSM implementation

#include "game.h"
#include "HAL.h"
#include "BatteryService.h"

#include <TM1637TinyDisplay.h>
#include <FadeLed.h>
#include <Bounce2.h>

extern TM1637TinyDisplay   global_disp;
extern FadeLed             MainLED;
extern BatteryService      batteryService;
extern Bounce2::Button     global_arming_button;

// ── Display string constants (PROGMEM) ────────────────────────────────────────

const PROGMEM char LEDSTR_BATTERY[] = "bAtt";  // LowBattery
const PROGMEM char LEDSTR_READY[]   = "rEdY";  // AwaitStart
const PROGMEM char LEDSTR_AWAIT[]   = "----";  // AwaitArming
const PROGMEM char LEDSTR_BLANK[]    = "    ";
const PROGMEM char LEDSTR_GOOD[]    = "good";  // Disarmed
const PROGMEM char LEDSTR_OFFENSE[] = "dEAd";  // OffenseWin
const PROGMEM char LEDSTR_DEFENSE[] = "SAFE";  // DefenseWin
const PROGMEM char LEDSTR_DONE[]    = "DonE";  // GameOver


// ── Global GameFSM instance ────────────────────────────────────────────────────

GameFSM gameFSM;

// ── Helpers ────────────────────────────────────────────────────────────────────

namespace {
    inline void dispatchEvent(uint16_t type) {
        gameFSM.dispatch(hfsm::Event(type));
    }

    // Guard functions for DisarmedState EVT_CORE_PIN_REMOVED transitions.
    // The first matching guard wins, so order in the transition table matters.
    bool guardSingleArming  (const hfsm::Event&) { return !gameFSM._multipleArmings; }
    bool guardMultipleArmings(const hfsm::Event&) { return  gameFSM._multipleArmings; }
}

/// Display "-XX-" arm/disarm sub-countdown on the 4-digit display.
static void showArmCountdown(byte seconds) {
    char buf[5];
    buf[0] = '-';
    buf[1] = '0' + (seconds / 10);
    buf[2] = '0' + (seconds % 10);
    buf[3] = '-';
    buf[4] = '\0';
    global_disp.showString(buf);
}

// ── State implementations ──────────────────────────────────────────────────────

// ---------------------------------------------------------------------------
// ConfigureState — idle; waits for /api/game/start (EVT_GAME_API_START)
// ---------------------------------------------------------------------------

ConfigureState::ConfigureState(const char* name)
    : hfsm::State(nullptr, name) {}

void ConfigureState::enter() {
    Serial.println(F("[Game] ConfigureState: waiting for /api/game/start"));
    MainLED.set(0);
}

// ---------------------------------------------------------------------------
// GameRootState — parent of all gameplay states; handles EVT_CORE_LOW_BATTERY
// via HFSM event bubbling so every child transitions to LowBatteryState.
// ---------------------------------------------------------------------------

GameRootState::GameRootState(const char* name)
    : hfsm::State(nullptr, name) {}

// ---------------------------------------------------------------------------
// AwaitStartState — shows "rEdY"; pin removed → AwaitArming
// ---------------------------------------------------------------------------

AwaitStartState::AwaitStartState(hfsm::State* parent, const char* name)
    : hfsm::State(parent, name) {}

void AwaitStartState::enter() {
    Serial.println(F("[Game] AwaitStartState: remove pin to begin"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_READY);
    MainLED.set(0);
}

void AwaitStartState::update() {
    // Periodic display refresh in case of transient corruption
    static unsigned long lastRefresh = 0;
    if (millis() - lastRefresh >= 250UL) {
        lastRefresh = millis();
        global_disp.showString_P(LEDSTR_READY);
    }
}

void AwaitStartState::exit() {
    // Latch the standby deadline the moment the operator removes the pin.
    // Uses the global gameFSM (matches DisarmedState guard pattern).
    if (gameFSM._standbyTimeMinutes != 0) {
        gameFSM._standbyExpire = millis() + (unsigned long)gameFSM._standbyTimeMinutes * 60UL * 1000UL;
        Serial.print(F("[Game] AwaitStartState::exit — standby expires in "));
        Serial.print(gameFSM._standbyTimeMinutes);
        Serial.println(F(" min"));
    }
}

// ---------------------------------------------------------------------------
// AwaitArmingState — shows "----", LED on; pin inserted → Arming
//                    optionally flashes standby MM:SS for 300 ms each second
// ---------------------------------------------------------------------------

AwaitArmingState::AwaitArmingState(hfsm::State* parent, const char* name)
    : hfsm::State(parent, name) {}

void AwaitArmingState::enter() {
    Serial.println(F("[Game] AwaitArmingState: insert pin to arm"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_AWAIT);
    MainLED.on();
    MainLED.setTime(1500);
    MainLED.set(255);
    _lastSecondMs = millis();
    _showingTime  = false;
}

void AwaitArmingState::update() {
    if (!gameFSM._showStandbyTime
        || gameFSM._standbyTimeMinutes == 0
        || gameFSM._standbyExpire == 0) {
        return;  // standby display disabled — enter() already showed "----"
    }

    unsigned long now = millis();

    // Rising edge of each new wall-clock second → flash the remaining time
    if (now - _lastSecondMs >= 1000UL) {
        _lastSecondMs += 1000UL;   // keep aligned; don't drift
        _showingTime = true;

        unsigned long remaining = (gameFSM._standbyExpire > now)
                                  ? (gameFSM._standbyExpire - now) : 0UL;
        uint32_t totalSecs = (uint32_t)(remaining / 1000UL);
        uint16_t mins      = (uint16_t)(totalSecs / 60UL);
        uint8_t  secs      = (uint8_t) (totalSecs % 60UL);

        // Show MM:SS — minutes shown as full integer even when >99
        uint8_t dots = 0b01010000;  // colon between digit 1 and 2
        global_disp.showNumberDec(mins, dots, true, 2, 0);
        global_disp.showNumberDec(secs, dots, true, 2, 2);
    }

    // 500 ms after the flash started, revert to "    "
    if (_showingTime && (now - _lastSecondMs >= 500UL)) {
        _showingTime = false;
        global_disp.showString_P(LEDSTR_BLANK, 4, 0, 0b01010000);
    }
}

// ---------------------------------------------------------------------------
// ArmingState — 10-second countdown, variable-pitch dual-piezo, LED breathing
// ---------------------------------------------------------------------------

ArmingState::ArmingState(hfsm::State* parent, GameFSM* fsm, const char* name)
    : hfsm::State(parent, name), _fsm(fsm) {}

void ArmingState::enter() {
    Serial.println(F("[Game] ArmingState: arming countdown started"));
    _fsm->_ArmDisarmSecondsLeft = _fsm->_armDisarmSeconds + 1;
    _fsm->_ArmDisarmTimerMs     = millis() + (unsigned long)(_fsm->_armDisarmSeconds + 1) * 1000UL;
    _fsm->_piezoUntil           = 0;
    MainLED.setTime(750);
}

void ArmingState::update() {
    unsigned long now = millis();

    // Seconds remaining in arming countdown
    byte current_seconds = (_fsm->_ArmDisarmTimerMs > now)
        ? (byte)((_fsm->_ArmDisarmTimerMs - now) / 1000UL)
        : 0;

    if (current_seconds != _fsm->_ArmDisarmSecondsLeft) {
        _fsm->_ArmDisarmSecondsLeft = current_seconds;
        showArmCountdown(current_seconds);
        if (current_seconds == 0) {
            dispatchEvent(EVT_GAME_TIMER_END);
            return;
        }
        _fsm->_piezoUntil = 0; // reset so a new tone fires on next iteration
    }

    // Variable-pitch dual-piezo: tone rises (LOW→HIGH) as arming completes
    if (now >= _fsm->_piezoUntil) {
        unsigned long timeLeft  = (_fsm->_ArmDisarmTimerMs > now) ? (_fsm->_ArmDisarmTimerMs - now) : 0;
        unsigned long totalMs   = (unsigned long)_fsm->_armDisarmSeconds * 1000UL;
        unsigned int  complete  = (totalMs == 0 || timeLeft >= totalMs) ? 100
                                  : (unsigned int)(timeLeft * 100UL / totalMs); // 0–100
        unsigned int  freq      = (unsigned int)map(complete, 0, 100, ARM_DISARM_TONE_HIGH, ARM_DISARM_TONE_LOW);
        unsigned int  interbeat = (unsigned int)map(complete, 0, 100, 300, 100);
        unsigned int  beep_dur  = (unsigned int)map(complete, 0, 100, 10, 250);
        tone(PIN_PIEZO_A, freq, beep_dur);
        tone(PIN_PIEZO_B, freq + interbeat, beep_dur);
        _fsm->_piezoUntil = now + beep_dur;
    }

    // LED breathing (750 ms fade time)
    if (MainLED.done()) {
        MainLED.set(_fsm->_fadeDirection ? 32 : 255);
        _fsm->_fadeDirection = !_fsm->_fadeDirection;
    }
}

// ---------------------------------------------------------------------------
// ArmedState — fast LED breathing, steady high tone; pin removed → Countdown
// ---------------------------------------------------------------------------

ArmedState::ArmedState(hfsm::State* parent, GameFSM* fsm, const char* name)
    : hfsm::State(parent, name), _fsm(fsm) {}

void ArmedState::enter() {
    Serial.println(F("[Game] ArmedState: ARMED — remove pin to start countdown"));
    _fsm->_piezoUntil = 0;
    MainLED.setTime(250);
}

void ArmedState::exit() {
    // Latch the countdown expiry when we leave for Countdown
    _fsm->_masterClockExpire = millis() + _fsm->_timerCountdownConfigValue;
    Serial.print(F("[Game] ArmedState::exit — countdown expires in "));
    Serial.print(_fsm->_timerCountdownConfigValue / 1000UL);
    Serial.println(F("s"));
}

void ArmedState::update() {
    unsigned long now = millis();

    // Steady high-pitched dual tone
    if (now >= _fsm->_piezoUntil) {
        tone(PIN_PIEZO_A, ARM_DISARM_TONE_HIGH,       100);
        tone(PIN_PIEZO_B, ARM_DISARM_TONE_HIGH + 300, 100);
        _fsm->_piezoUntil = now + 110UL;
    }

    // Fast LED breathing (250 ms fade time)
    if (MainLED.done()) {
        MainLED.set(_fsm->_fadeDirection ? 16 : 255);
        _fsm->_fadeDirection = !_fsm->_fadeDirection;
    }
}

// ---------------------------------------------------------------------------
// CountdownState — MM:SS display, per-second chirp, LED breathing
//                  pin inserted → Disarming | timer expires → OffenseWin | standby expires → OffenseWin
// ---------------------------------------------------------------------------

CountdownState::CountdownState(hfsm::State* parent, GameFSM* fsm, const char* name)
    : hfsm::State(parent, name), _fsm(fsm) {}

void CountdownState::enter() {
    Serial.println(F("[Game] CountdownState: countdown running"));
    MainLED.setTime(2000);
    _fsm->_ArmDisarmTimerMs = 0; // 0 triggers immediate first tick in update()
}

void CountdownState::update() {
    unsigned long now = millis();

    // Check expiry
    if (now >= _fsm->_masterClockExpire) {
        Serial.println(F("[Game] CountdownState: expired — EVT_GAME_TIMER_END"));
        dispatchEvent(EVT_GAME_TIMER_END);
        return;
    }

    // Per-second tick: chirp + redraw MM:SS
    if (now - _fsm->_ArmDisarmTimerMs >= 1000UL) {
        _fsm->_ArmDisarmTimerMs = now;

        tone(PIN_PIEZO_A, 3100, 100);
        tone(PIN_PIEZO_B, 3800, 100);
        _fsm->_piezoUntil = now + 100UL;

        unsigned long remaining = _fsm->_masterClockExpire - now;
        uint16_t totalSecs = (uint16_t)(remaining / 1000UL);
        byte minutes = (byte)(totalSecs / 60);
        byte secs    = (byte)(totalSecs % 60);
        uint8_t dots = 0b01010000;
        global_disp.showNumberDec(minutes, dots, true, 2, 0);
        global_disp.showNumberDec(secs,    dots, true, 2, 2);
    }

    // 925 ms after the flash started, revert to "    "
    if (now - _fsm->_ArmDisarmTimerMs >= 925UL) {
        global_disp.showString_P(LEDSTR_BLANK);
    }


    // Slow LED breathing (2000 ms fade time)
    if (MainLED.done()) {
        MainLED.set(_fsm->_fadeDirection ? 32 : 255);
        _fsm->_fadeDirection = !_fsm->_fadeDirection;
    }
}

// ---------------------------------------------------------------------------
// DisarmingState — 10-second countdown, reversed-pitch dual-piezo, LED breathing
//                  pin removed → Countdown | timer expires → Disarmed
// ---------------------------------------------------------------------------

DisarmingState::DisarmingState(hfsm::State* parent, GameFSM* fsm, const char* name)
    : hfsm::State(parent, name), _fsm(fsm) {}

void DisarmingState::enter() {
    Serial.println(F("[Game] DisarmingState: disarming countdown started"));
    _fsm->_ArmDisarmSecondsLeft = _fsm->_armDisarmSeconds + 1;
    _fsm->_ArmDisarmTimerMs     = millis() + (unsigned long)(_fsm->_armDisarmSeconds + 1) * 1000UL;
    _fsm->_piezoUntil           = 0;
    MainLED.setTime(750);
}

void DisarmingState::update() {
    unsigned long now = millis();

    byte current_seconds = (_fsm->_ArmDisarmTimerMs > now)
        ? (byte)((_fsm->_ArmDisarmTimerMs - now) / 1000UL)
        : 0;

    if (current_seconds != _fsm->_ArmDisarmSecondsLeft) {
        _fsm->_ArmDisarmSecondsLeft = current_seconds;
        if (current_seconds == 0) {
            dispatchEvent(EVT_GAME_TIMER_END);
            return;
        }
        showArmCountdown(current_seconds);
        _fsm->_piezoUntil = 0; // reset so a new tone fires on next iteration
    }

    // Variable-pitch dual-piezo: tone falls (HIGH→LOW) as disarming completes
    if (now >= _fsm->_piezoUntil) {
        unsigned long timeLeft  = (_fsm->_ArmDisarmTimerMs > now) ? (_fsm->_ArmDisarmTimerMs - now) : 0;
        unsigned long totalMs   = (unsigned long)_fsm->_armDisarmSeconds * 1000UL;
        unsigned int  complete  = (totalMs == 0 || timeLeft >= totalMs) ? 100
                                  : (unsigned int)(timeLeft * 100UL / totalMs); // 0–100
        unsigned int  freq      = (unsigned int)map(complete, 0, 100, ARM_DISARM_TONE_LOW, ARM_DISARM_TONE_HIGH);
        unsigned int  interbeat = (unsigned int)map(complete, 0, 100, 100, 300);
        unsigned int  beep_dur  = (unsigned int)map(complete, 0, 100, 250, 10);
        tone(PIN_PIEZO_A, freq,             beep_dur);
        tone(PIN_PIEZO_B, freq + interbeat, beep_dur);
        _fsm->_piezoUntil = now + beep_dur;
    }

    // LED breathing (750 ms fade time)
    if (MainLED.done()) {
        MainLED.set(_fsm->_fadeDirection ? 32 : 255);
        _fsm->_fadeDirection = !_fsm->_fadeDirection;
    }
}

// ---------------------------------------------------------------------------
// DisarmedState — shows "good", LED off;
//   EVT_CORE_PIN_REMOVED + guardSingleArming   → GameOver
//   EVT_CORE_PIN_REMOVED + guardMultipleArmings → AwaitArming (re-arm for another disarm)
// ---------------------------------------------------------------------------

DisarmedState::DisarmedState(hfsm::State* parent, const char* name)
    : hfsm::State(parent, name) {}

void DisarmedState::enter() {
    Serial.println(F("[Game] DisarmedState: vial disarmed successfully"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_GOOD);
    if (!gameFSM._multipleArmings) {
        MainLED.setTime(1000);
        MainLED.off();
    }
}

// ---------------------------------------------------------------------------
// OffenseWinState — shows "dEAd", LED off; pin removed → GameOver
// ---------------------------------------------------------------------------

OffenseWinState::OffenseWinState(hfsm::State* parent, const char* name)
    : hfsm::State(parent, name) {}

void OffenseWinState::enter() {
    Serial.println(F("[Game] OffenseWinState: countdown expired — offense wins"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_OFFENSE);
    MainLED.setTime(1000);
    MainLED.off();
}

// ---------------------------------------------------------------------------
// DefenseWinState — shows "SAFE", LED off; pin removed → GameOver
// ---------------------------------------------------------------------------

DefenseWinState::DefenseWinState(hfsm::State* parent, const char* name)
    : hfsm::State(parent, name) {}

void DefenseWinState::enter() {
    Serial.println(F("[Game] DefenseWinState: standby expired — defense wins"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_DEFENSE);
    MainLED.setTime(1000);
    MainLED.off();
}

// ---------------------------------------------------------------------------
// GameOverState — alternates "DonE" / battery%; rapid pin toggle → AwaitStart
// ---------------------------------------------------------------------------

GameOverState::GameOverState(hfsm::State* parent, GameFSM* fsm, const char* name)
    : hfsm::State(parent, name), _fsm(fsm) {}

void GameOverState::enter() {
    Serial.println(F("[Game] GameOverState: round complete — rapid toggle to reset"));
    global_disp.clear();
    global_disp.showString_P(LEDSTR_DONE);
    MainLED.off();
    _fsm->_ArmDisarmTimerMs     = millis() + 4000UL; // first display toggle in 4 s
    _fsm->_fadeDirection        = true;               // true = showing DONE
    _fsm->_ArmDisarmSecondsLeft = 0;                  // reset toggle counter
    _fsm->_piezoUntil           = 0;
}

void GameOverState::update() {
    unsigned long now = millis();
    static byte resetToggles = 0;

    // Alternate between "DonE" and "b- XX%" every 4 seconds
    if (now >= _fsm->_ArmDisarmTimerMs) {
        _fsm->_ArmDisarmTimerMs   = now + 4000UL;
        _fsm->_fadeDirection = !_fsm->_fadeDirection;

        if (_fsm->_fadeDirection) {
            global_disp.showString_P(LEDSTR_DONE);
        } else {
            int batLevel = constrain(batteryService.currentLevel(), 0, 99);
            global_disp.showString("b-  ");
            global_disp.showNumber(batLevel, false, 2, 2);
        }
    }

    if (global_arming_button.changed()) {
        if (global_arming_button.previousDuration() < 1000) {
            resetToggles++;
            if (resetToggles > 3) {
                resetToggles = 0;
                gameFSM.reset();
                gameFSM.start();
            }
        }
    }

}

// ---------------------------------------------------------------------------
// LowBatteryState — terminal; shows "bAtt"
// ---------------------------------------------------------------------------

LowBatteryState::LowBatteryState(const char* name)
    : hfsm::State(nullptr, name) {}

void LowBatteryState::enter() {
    Serial.println(F("[Game] LowBatteryState: battery critical — game suspended"));
    MainLED.set(0);
    global_disp.setBrightness(1);
    global_disp.showString_P(LEDSTR_BATTERY);
}

void LowBatteryState::update() {
    // Terminal state — nothing to do
}

// ---------------------------------------------------------------------------
// DemoState — scripted display/audio/LED demo started from the Admin UI.
// Runs until the Stop button (EVT_GAME_DEMO_END). Tone code is copied from the
// Arming/Armed/Countdown states so those remain untouched.
// ---------------------------------------------------------------------------

namespace {
    enum DemoPhase : uint8_t {
        DP_IDLE, DP_BEEP1, DP_SET0, DP_SET1, DP_BEEP2, DP_COUNT,
        DP_HOLD, DP_BLINK, DP_ARMING, DP_ARMED, DP_COUNTDOWN
    };
    const unsigned int FAST_CYCLE = 330, FAST_ON = 250;   // 250 on / 80 off
    const unsigned int SLOW_CYCLE = 1000, SLOW_ON = 500;  // 500 on / 500 off
    const unsigned long DEMO_COUNTDOWN_MS = 15UL * 60UL * 1000UL;
}

DemoState::DemoState(const char* name)
    : hfsm::State(nullptr, name) {}

void DemoState::enter() {
    Serial.println(F("[Game] DemoState: demo started"));
    MainLED.set(0);
    setPhase(DP_IDLE);
}

void DemoState::exit() {
    Serial.println(F("[Game] DemoState: demo ended"));
    noTone(PIN_PIEZO_A);
    noTone(PIN_PIEZO_B);
    global_disp.showString_P(LEDSTR_BLANK);
    MainLED.set(0);
}

void DemoState::render(bool on) {
    uint8_t seg[4];
    for (uint8_t i = 0; i < 4; i++) {
        bool blank = !on && (_blinkAll || (_blinkMask & (1 << i)));
        // Colon (0x80) lives on digit 1; keep it lit when only a single digit blinks.
        uint8_t colon = (i == 1) ? 0x80 : 0;
        if (blank) seg[i] = (_blinkAll ? 0 : colon);
        else       seg[i] = global_disp.encodeDigit(_digits[i]) | colon;
    }
    global_disp.setSegments(seg, 4, 0);
}

void DemoState::setPhase(uint8_t phase) {
    unsigned long now = millis();
    _phase      = phase;
    _phaseStart = now;
    _blinkMask  = 0;
    _blinkAll   = false;
    _blinkCycle = 0;
    _lastOn     = true;
    _piezoUntil = 0;

    switch (phase) {
    case DP_IDLE:
        global_disp.showString_P(LEDSTR_AWAIT);
        break;
    case DP_BEEP1:
        tone(PIN_PIEZO_A, 2500, 500);
        tone(PIN_PIEZO_B, 2800, 500);
        break;
    case DP_SET0:
        _digits[0] = _digits[1] = _digits[2] = _digits[3] = 0;
        _blinkMask = 0x01; _blinkCycle = FAST_CYCLE; _blinkOnMs = FAST_ON;
        render(true);
        break;
    case DP_SET1:
        _digits[0] = 1;
        _blinkMask = 0x01; _blinkCycle = FAST_CYCLE; _blinkOnMs = FAST_ON;
        render(true);
        break;
    case DP_BEEP2:
        tone(PIN_PIEZO_A, 2500, 100);
        tone(PIN_PIEZO_B, 2800, 100);
        render(true);
        break;
    case DP_COUNT:
        _digits[1] = 1;
        _blinkMask = 0x02; _blinkCycle = FAST_CYCLE; _blinkOnMs = FAST_ON;
        render(true);
        break;
    case DP_HOLD:
        _digits[1] = 5;
        _blinkMask = 0x02; _blinkCycle = FAST_CYCLE; _blinkOnMs = FAST_ON;
        render(true);
        break;
    case DP_BLINK:
        _blinkAll = true; _blinkCycle = SLOW_CYCLE; _blinkOnMs = SLOW_ON;
        render(true);
        break;
    case DP_ARMING:
        render(true);
        MainLED.setTime(750);
        MainLED.set(32);
        _fadeDir = false;
        break;
    case DP_ARMED:
        _blinkAll = true; _blinkCycle = FAST_CYCLE; _blinkOnMs = FAST_ON;
        render(true);
        MainLED.setTime(250);
        break;
    case DP_COUNTDOWN:
        _expire  = now + DEMO_COUNTDOWN_MS;
        _tickMs  = 0;       // immediate first tick
        _blanked = false;
        MainLED.setTime(2000);
        break;
    }
}

void DemoState::update() {
    unsigned long now     = millis();
    unsigned long elapsed = now - _phaseStart;

    // Generic blinking for phases that request it
    if (_blinkCycle) {
        bool on = (elapsed % _blinkCycle) < _blinkOnMs;
        if (on != _lastOn) { _lastOn = on; render(on); }
    }

    switch (_phase) {
    case DP_IDLE:   if (elapsed >= 5000) setPhase(DP_BEEP1); break;
    case DP_BEEP1:  if (elapsed >= 500)  setPhase(DP_SET0);  break;
    case DP_SET0:   if (elapsed >= 3000) setPhase(DP_SET1);  break;
    case DP_SET1:   if (elapsed >= 600)  setPhase(DP_BEEP2); break;
    case DP_BEEP2:  if (elapsed >= 100)  setPhase(DP_COUNT); break;
    case DP_COUNT: {
        if (elapsed >= 6000) { setPhase(DP_HOLD); break; }
        uint8_t d = 1 + (uint8_t)(elapsed / 1500UL);   // 1,2,3,4,5
        if (d != _digits[1]) { _digits[1] = d; render(_lastOn); }
        break;
    }
    case DP_HOLD:   if (elapsed >= 5000) setPhase(DP_BLINK); break;
    case DP_BLINK:  if (elapsed >= 3000) setPhase(DP_ARMING); break;

    case DP_ARMING: {
        if (elapsed >= 10000) { setPhase(DP_ARMED); break; }
        if (now >= _piezoUntil) {
            unsigned long timeLeft = 10000UL - elapsed;
            unsigned int complete  = (unsigned int)(timeLeft * 100UL / 10000UL);  // 100..0
            unsigned int freq      = (unsigned int)map(complete, 0, 100, ARM_DISARM_TONE_HIGH, ARM_DISARM_TONE_LOW);
            unsigned int interbeat = (unsigned int)map(complete, 0, 100, 300, 100);
            unsigned int beep_dur  = (unsigned int)map(complete, 0, 100, 10, 250);
            tone(PIN_PIEZO_A, freq, beep_dur);
            tone(PIN_PIEZO_B, freq + interbeat, beep_dur);
            _piezoUntil = now + beep_dur;
        }
        if (MainLED.done()) {
            MainLED.set(_fadeDir ? 32 : 255);
            _fadeDir = !_fadeDir;
        }
        break;
    }

    case DP_ARMED:
        if (elapsed >= 4000) { setPhase(DP_COUNTDOWN); break; }
        if (now >= _piezoUntil) {
            tone(PIN_PIEZO_A, ARM_DISARM_TONE_HIGH,       100);
            tone(PIN_PIEZO_B, ARM_DISARM_TONE_HIGH + 300, 100);
            _piezoUntil = now + 110UL;
        }
        if (MainLED.done()) {
            MainLED.set(_fadeDir ? 16 : 255);
            _fadeDir = !_fadeDir;
        }
        break;

    case DP_COUNTDOWN: {
        if (now >= _expire) { dispatchEvent(EVT_GAME_DEMO_END); return; }
        if (now - _tickMs >= 1000UL) {
            _tickMs  = now;
            _blanked = false;
            tone(PIN_PIEZO_A, 3100, 100);
            tone(PIN_PIEZO_B, 3800, 100);
            uint16_t totalSecs = (uint16_t)((_expire - now) / 1000UL);
            byte minutes = (byte)(totalSecs / 60);
            byte secs    = (byte)(totalSecs % 60);
            uint8_t dots = 0b01010000;
            global_disp.showNumberDec(minutes, dots, true, 2, 0);
            global_disp.showNumberDec(secs,    dots, true, 2, 2);
        }
        if (!_blanked && now - _tickMs >= 925UL) {
            _blanked = true;
            global_disp.showString_P(LEDSTR_BLANK);
        }
        if (MainLED.done()) {
            MainLED.set(_fadeDir ? 32 : 255);
            _fadeDir = !_fadeDir;
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// GameFSM implementation
// ---------------------------------------------------------------------------

GameFSM::GameFSM()
    : _timerCountdownConfigValue(5UL * 60UL * 1000UL)  // default: 5 minutes
    , _masterClockExpire(0)
    , _armDisarmSeconds(10)        // default: 10 s (range 1–99)
    , _multipleArmings(false)      // default: single arming per round
    , _standbyTimeMinutes(30)      // default: 30 min (range 0–199; 0 disables standby countdown)
    , _showStandbyTime(false)      // default: do not show standby time on display
    , _standbyExpire(0)            // latched in AwaitStartState::exit() when game begins
    , _ArmDisarmSecondsLeft(0)
    , _ArmDisarmTimerMs(0)
    , _fadeDirection(false)
    , _piezoUntil(0)
    , _machine()
    , _configureState("Configure")
    , _gameRootState("GameRoot")
    , _awaitStartState(&_gameRootState, "AwaitStart")
    , _awaitArmingState(&_gameRootState, "AwaitArming")
    , _armingState(&_gameRootState, this, "Arming")
    , _armedState(&_gameRootState, this, "Armed")
    , _countdownState(&_gameRootState, this, "Countdown")
    , _disarmingState(&_gameRootState, this, "Disarming")
    , _disarmedState(&_gameRootState, "Disarmed")  // no _fsm needed — guards read gameFSM directly
    , _offenseWinState(&_gameRootState, "OffenseWin")
    , _defenseWinState(&_gameRootState, "DefenseWin")
    , _gameOverState(&_gameRootState, this, "GameOver")
    , _lowBatteryState("LowBattery")
    , _demoState("Demo") {
    buildTransitions();
}

void GameFSM::begin() {
    _machine.begin(&_configureState);
}

void GameFSM::update() {
    // Demo requests from web handlers are applied here, on the main loop.
    if (_demoStartRequested) {
        _demoStartRequested = false;
        _machine.dispatch(hfsm::Event(EVT_GAME_DEMO_START));  // only acts in ConfigureState
    }
    if (_demoStopRequested) {
        _demoStopRequested = false;
        _machine.dispatch(hfsm::Event(EVT_GAME_DEMO_END));    // only acts in DemoState
    }
    // Standby countdown tick — fires EVT_GAME_STANDBY_END once when the deadline passes.
    // Guard: disabled when _standbyTimeMinutes == 0 or deadline not yet latched.
    if (_standbyTimeMinutes != 0 && _standbyExpire != 0 && millis() >= _standbyExpire) {
        _standbyExpire = 0;  // clear to prevent re-fire on subsequent ticks
        Serial.println(F("[Game] GameFSM: standby expired — EVT_GAME_STANDBY_END"));
        _machine.dispatch(hfsm::Event(EVT_GAME_STANDBY_END));
    }
    _machine.update();
}

void GameFSM::dispatch(uint16_t eventId) {
    _machine.dispatch(hfsm::Event(eventId));
}

void GameFSM::dispatch(const hfsm::Event& e) {
    _machine.dispatch(e);
}

void GameFSM::buildTransitions() {
    // ConfigureState: API start -> AwaitStart, low-battery -> LowBattery
    _configureState._trans[0] = { EVT_GAME_API_START,   nullptr, &_awaitStartState };
    _configureState._trans[1] = { EVT_CORE_LOW_BATTERY, nullptr, &_lowBatteryState };
    _configureState._trans[2] = { EVT_GAME_DEMO_START,  nullptr, &_demoState };
    _configureState._transCount = 3;

    // DemoState: Stop -> Configure, low-battery -> LowBattery
    _demoState._trans[0] = { EVT_GAME_DEMO_END,    nullptr, &_configureState  };
    _demoState._trans[1] = { EVT_CORE_LOW_BATTERY, nullptr, &_lowBatteryState };
    _demoState._transCount = 2;

    // GameRootState: low-battery bubbles up to LowBattery for all children
    _gameRootState._trans[0] = { EVT_CORE_LOW_BATTERY, nullptr, &_lowBatteryState };
    _gameRootState._transCount = 1;

    // AwaitStartState: pin removed -> AwaitArming (exit() latches standby deadline)
    _awaitStartState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_awaitArmingState };
    _awaitStartState._transCount = 1;

    // AwaitArmingState: pin inserted -> Arming, standby expires -> DefenseWin
    _awaitArmingState._trans[0] = { EVT_CORE_PIN_INSERTED, nullptr, &_armingState     };
    _awaitArmingState._trans[1] = { EVT_GAME_STANDBY_END,  nullptr, &_defenseWinState };
    _awaitArmingState._transCount = 2;

    // ArmingState: pin removed cancels -> AwaitArming, timer done -> Armed, standby expires -> DefenseWin
    _armingState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_awaitArmingState };
    _armingState._trans[1] = { EVT_GAME_TIMER_END,   nullptr, &_armedState       };
    _armingState._trans[2] = { EVT_GAME_STANDBY_END, nullptr, &_defenseWinState  };
    _armingState._transCount = 3;

    // ArmedState: pin removed starts countdown -> Countdown, standby expires -> DefenseWin
    _armedState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_countdownState  };
    _armedState._trans[1] = { EVT_GAME_STANDBY_END, nullptr, &_defenseWinState };
    _armedState._transCount = 2;

    // CountdownState: pin inserted -> Disarming, timer expires -> OffenseWin, standby expires -> OffenseWin
    _countdownState._trans[0] = { EVT_CORE_PIN_INSERTED, nullptr, &_disarmingState  };
    _countdownState._trans[1] = { EVT_GAME_TIMER_END,    nullptr, &_offenseWinState };
    _countdownState._trans[2] = { EVT_GAME_STANDBY_END,  nullptr, &_offenseWinState };
    _countdownState._transCount = 3;

    // DisarmingState: pin removed -> Countdown, timer done -> Disarmed, standby expires -> OffenseWin
    _disarmingState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_countdownState  };
    _disarmingState._trans[1] = { EVT_GAME_TIMER_END,   nullptr, &_disarmedState   };
    _disarmingState._trans[2] = { EVT_GAME_STANDBY_END, nullptr, &_offenseWinState };
    _disarmingState._transCount = 3;

    // DisarmedState: pin removed → GameOver (single) or AwaitArming (multiple), standby expires -> DefenseWin
    // First matching guard wins; order is significant.
    _disarmedState._trans[0] = { EVT_CORE_PIN_REMOVED, guardSingleArming,    &_gameOverState    };
    _disarmedState._trans[1] = { EVT_CORE_PIN_REMOVED, guardMultipleArmings, &_awaitArmingState };
    _disarmedState._trans[2] = { EVT_GAME_STANDBY_END, nullptr,              &_defenseWinState  };
    _disarmedState._transCount = 3;

    // OffenseWinState: pin removed -> GameOver
    _offenseWinState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_gameOverState };
    _offenseWinState._transCount = 1;

    // DefenseWinState: pin removed -> GameOver
    _defenseWinState._trans[0] = { EVT_CORE_PIN_REMOVED, nullptr, &_gameOverState };
    _defenseWinState._transCount = 1;

    // GameOverState: rapid pin toggle -> AwaitStart (new round)
    _gameOverState._trans[0] = { EVT_CORE_PIN_TOGGLED, nullptr, &_awaitStartState };
    _gameOverState._transCount = 1;

    // LowBatteryState: terminal — no transitions
}

// ── Game control ──────────────────────────────────────────────────────────────

void GameFSM::start() {
    _machine.dispatch(hfsm::Event(EVT_GAME_API_START));
}

void GameFSM::reset() {
    _masterClockExpire         = 0;
    _standbyExpire             = 0;
    _ArmDisarmSecondsLeft      = 0;
    _ArmDisarmTimerMs          = 0;
    _fadeDirection             = false;
    _piezoUntil                = 0;
    // Operator-configured values are intentionally NOT reset:
    // _timerCountdownConfigValue, _armDisarmSeconds, _multipleArmings, _standbyTimeMinutes, _showStandbyTime
    _machine.begin(&_configureState);
}

// ── Game state accessor ───────────────────────────────────────────────────────

const char* GameFSM::stateName() const {
    const hfsm::State* cur = _machine.current();
    if (cur == &_awaitStartState)  return "await_start";
    if (cur == &_awaitArmingState) return "await_arming";
    if (cur == &_armingState)      return "arming";
    if (cur == &_armedState)       return "armed";
    if (cur == &_countdownState)   return "countdown";
    if (cur == &_disarmingState)   return "disarming";
    if (cur == &_disarmedState)    return "disarmed";
    if (cur == &_offenseWinState)  return "offense_win";
    if (cur == &_defenseWinState)  return "defense_win";
    if (cur == &_gameOverState)    return "gameover";
    if (cur == &_lowBatteryState)  return "lowbattery";
    if (cur == &_demoState)        return "demo";
    return "configure"; // ConfigureState or not yet begun
}


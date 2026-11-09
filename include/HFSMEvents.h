#ifndef HFSM_EVENTS_H
#define HFSM_EVENTS_H

#include <cstdint>

// ============================================================
// FSM Dispatcher Interface
// ============================================================
// Implement this on any FSM wrapper class that should be
// eligible to receive events dispatched by other services
// (e.g. BatteryService dispatching EVT_CORE_LOW_BATTERY).

class IFsmDispatcher {
public:
  virtual void dispatch(uint16_t eventId) = 0;
  virtual ~IFsmDispatcher() = default;
};

// ============================================================
// Cross-FSM Events (0-99)
// ============================================================
// Events that can be dispatched to multiple state machines
// or represent system-wide conditions

static const uint16_t EVT_CORE_LOW_BATTERY  = 1;
static const uint16_t EVT_CORE_PIN_INSERTED = 2;
static const uint16_t EVT_CORE_PIN_REMOVED  = 3;
static const uint16_t EVT_CORE_PIN_TOGGLED  = 4;  // rapid toggle sequence detected (GameOver reset)

// ============================================================
// Networking FSM Events (100-199)
// ============================================================
// Events specific to the networking state machine

static const uint16_t EVT_NET_INITIALIZED    = 100;
static const uint16_t EVT_NET_SELECTED_ADMIN = 101;
static const uint16_t EVT_NET_SELECTED_GAME  = 102;
static const uint16_t EVT_NET_SETUP_DONE     = 103;

// ============================================================
// Game FSM Events (200-299)
// ============================================================
// Events specific to the game state machine

static const uint16_t EVT_GAME_API_START     = 200;  // API requested game start
static const uint16_t EVT_GAME_TIMER_END     = 202;  // Game duration timer expired
static const uint16_t EVT_GAME_STANDBY_END   = 203;  // Standby duration timer expired
static const uint16_t EVT_GAME_DEMO_START    = 204;  // Admin UI requested the demo routine
static const uint16_t EVT_GAME_DEMO_END      = 205;  // Demo finished or Stop pressed

#endif // HFSM_EVENTS_H


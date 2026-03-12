# Networking FSM Documentation

## Overview

The Networking Finite State Machine (FSM) manages the device's networking lifecycle, from initialization through idle waiting to setup and finally running. It uses the `jonblack/arduino-fsm` library to implement a robust, event-driven state machine that handles WiFi, DNS, mDNS, and webserver operations.

## Architecture

The FSM follows a linear progression through five states:

```
initialize → idle → setup_admin/setup_game → run
```

Each state has specific responsibilities and transitions are triggered by events rather than time-based or polling mechanisms.

## States

### 1. Initialize State

**Responsibility**: One-time subsystem initialization

**Entry Function**:
- Initializes the WiFi subsystem (AP + STA mode)
- Configures DNS hijacking server
- Registers mDNS service (`my.milsim.prop.local`)
- Sets up filesystem (LittleFS)

**Update Function**:
- Triggers the `initialized` event to transition to the `idle` state

**Transitions**:
- `initialized` event → `idle` state

### 2. Idle State

**Responsibility**: Wait for external configuration events

**Entry Function**:
- No initialization needed (passive state)

**Update Function**:
- Calls DNS server handler: `_dnsServer.processNextRequest()`
- Calls mDNS handler: `MDNS.update()` (ESP32) or `MDNS.update()` (ESP8266)
- Waits for external events without blocking

**Transitions**:
- `selected_admin` event → `setup_admin` state
- `selected_game` event → `setup_game` state

### 3. Setup Admin State

**Responsibility**: Configure administrative interface routes

**Entry Function**:
- Sets up HTTP API routes for administrative functions
- Registers handlers for admin endpoints (e.g., `/api/system/*`, `/api/config/*`)
- Prepares webserver for admin mode

**Update Function**:
- Triggers the `setup_done` event to transition to the `run` state

**Transitions**:
- `setup_done` event → `run` state

### 4. Setup Game State

**Responsibility**: Configure game interface routes

**Entry Function**:
- Sets up HTTP API routes for game functions
- Registers handlers for game endpoints (e.g., `/api/game/*`, `/api/players/*`)
- Prepares webserver for game mode

**Update Function**:
- Triggers the `setup_done` event to transition to the `run` state

**Transitions**:
- `setup_done` event → `run` state

### 5. Run State

**Responsibility**: Operate and handle all networking requests

**Entry Function**:
- No additional setup (routes already configured in setup states)

**Update Function**:
- Calls webserver handler: `_server->handleClient()`
- Calls DNS server handler: `_dnsServer.processNextRequest()`
- Calls mDNS handler: `MDNS.update()`
- Continuously processes incoming requests

**Transitions**:
- None (terminal state for normal operation)

## Events

The FSM is driven by four events:

| Event | Trigger | Source | Target State |
|-------|---------|--------|--------------|
| `initialized` | Initialization complete | `initialize` state update | `idle` |
| `selected_admin` | Admin mode selected | External (e.g., user input, API call) | `setup_admin` |
| `selected_game` | Game mode selected | External (e.g., user input, API call) | `setup_game` |
| `setup_done` | Setup complete | `setup_admin` or `setup_game` update | `run` |

## State Transition Diagram

```
┌─────────────┐
│ initialize  │
│  (entry)    │
│  - Init WiFi│
│  - Init DNS │
│  - Init mDNS│
└──────┬──────┘
       │ initialized event
       ▼
┌─────────────┐
│   idle      │
│  (update)   │
│  - DNS poll │
│  - mDNS poll│
└──┬──────┬───┘
   │      │
   │ selected_admin
   │      │ selected_game
   │      │
   ▼      ▼
┌──────────────┐  ┌──────────────┐
│ setup_admin  │  │ setup_game   │
│  (entry)     │  │  (entry)     │
│  - Add admin │  │  - Add game  │
│    routes    │  │    routes    │
└──────┬───────┘  └──────┬───────┘
       │                 │
       │ setup_done      │ setup_done
       │                 │
       └────────┬────────┘
                ▼
         ┌─────────────┐
         │    run      │
         │  (update)   │
         │  - Handle   │
         │    clients  │
         │  - DNS poll │
         │  - mDNS poll│
         └─────────────┘
```

## Implementation Notes

### Lambda Capture

All lambda functions in the FSM must capture `this` to access member variables:

```cpp
[this]() {
    _inSystemMode = true;  // Access member variable
    _server->handleClient();  // Call member function
}
```

### Handler Functions

The `update` functions for `idle` and `run` states must continuously call:
- `_dnsServer.processNextRequest()` - Process DNS queries
- `MDNS.update()` - Handle mDNS announcements
- `_server->handleClient()` - Process HTTP requests (run state only)

### Webserver Routes

Routes are registered in the entry functions of `setup_admin` and `setup_game` states using:
```cpp
_server->on("/api/endpoint", HTTP_GET, [this]() { /* handler */ });
```

### Captive Portal

The webserver implements a captive portal by:
1. Redirecting all DNS queries to `192.168.4.1` (AP IP)
2. Redirecting HTTP requests with non-target hostnames to `my.milsim.prop`
3. Serving static content from `/web/` directory for valid hostname requests

## Lifecycle Example

1. **Device boots** → FSM enters `initialize` state
2. **Initialization completes** → `initialized` event triggers
3. **FSM enters `idle` state** → Waits for user selection
4. **User selects admin mode** → `selected_admin` event triggers
5. **FSM enters `setup_admin` state** → Admin routes registered
6. **Setup completes** → `setup_done` event triggers
7. **FSM enters `run` state** → Handles all requests indefinitely

Alternatively, user could select game mode in step 3, leading to `setup_game` instead.

## Integration with DeviceWebserver

The FSM is initialized in `DeviceWebserver::initFSM()` and updated in `DeviceWebserver::handleClient()`:

```cpp
void DeviceWebserver::handleClient() {
    _fsm->update();  // Update FSM state machine
}
```

The FSM manages the complete networking lifecycle while the `DeviceWebserver` class provides the underlying infrastructure (webserver, DNS, mDNS, filesystem).

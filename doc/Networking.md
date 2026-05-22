# Networking Documentation

## Overview

The firmware still uses a networking finite state machine, but the old document no longer matches the current implementation.

The current networking stack is built from:

- `NetworkingFSM` in `include/networking.h` and `src/networking.cpp`
- `ESPAsyncWebServer` for HTTP
- a custom `DNSServer` in `src/DNSServer.cpp`
- LittleFS for static assets and persisted configuration
- `ApiSystem` and `ApiGame` for mode-specific routes

The captive-portal hostname is `my.milsim.prop` and the access-point IP is fixed at `4.3.2.1`.

## Key Files

- `src/main.cpp` - boot flow, config load, mode selection, and per-loop FSM updates
- `include/networking.h` - networking FSM declarations
- `src/networking.cpp` - state behavior, AP setup, shared routes, captive portal
- `src/ApiSystem.cpp` - admin/system API routes
- `src/ApiGame.cpp` - game API routes
- `src/DNSServer.cpp` - wildcard DNS with a small blocklist

## Architecture

The current FSM flow is:

```text
Initialize -> Idle -> SetupAdmin or SetupGame -> Run
```

Important corrections versus the previous document:

- the code uses `HFSM`, not `jonblack/arduino-fsm`
- `InitializeState` does not start Wi-Fi, DNS, mDNS, or HTTP
- DNS, mDNS, and the web server are started in `RunState::enter()`
- HTTP uses `ESPAsyncWebServer`, so there is no `_server->handleClient()` loop
- `IdleState` does not poll DNS or mDNS because those services are not running yet

## Boot and Mode Selection

`setup()` in `src/main.cpp` performs networking-related startup in this order:

1. Mount LittleFS.
2. Load `/network.json` into `global_configuration`.
3. Start `networkingFSM.begin()`.
4. Choose admin or game mode.

Mode selection behavior:

- If `/network.json` is missing or invalid, firmware immediately dispatches `EVT_NET_SELECTED_ADMIN`.
- Otherwise, the user has 5 seconds after boot to press `PIN_MODE_SELECT` for admin mode.
- If no button press occurs in that window, firmware dispatches `EVT_NET_SELECTED_GAME`.

This means first boot and recovery paths always land in admin mode.

## Events

Networking events are defined in `include/HFSMEvents.h`:

| Event | ID | Purpose |
|---|---:|---|
| `EVT_NET_INITIALIZED` | 100 | leave `Initialize` and enter `Idle` |
| `EVT_NET_SELECTED_ADMIN` | 101 | enter admin setup |
| `EVT_NET_SELECTED_GAME` | 102 | enter game setup |
| `EVT_NET_SETUP_DONE` | 103 | leave setup and enter `Run` |

## States

### Initialize

`InitializeState::enter()` only logs and immediately dispatches `EVT_NET_INITIALIZED`.

Current responsibility:

- no AP startup
- no DNS startup
- no mDNS startup
- no HTTP startup

### Idle

`IdleState` waits for the mode-selection event.

Current responsibility:

- no DNS or mDNS polling
- no HTTP handling
- wait for `EVT_NET_SELECTED_ADMIN` or `EVT_NET_SELECTED_GAME`

### SetupAdmin

`SetupAdminState::enter()` prepares the provisioning/admin network.

Behavior:

- sets `WiFi.mode(WIFI_AP_STA)`
- sets hostname to `my.milsim.prop`
- configures the AP IP/subnet using `apIP`
- starts an open AP named `BrooklynTactical-XXX`
  - `XXX` comes from the chip ID / eFuse MAC low bits
- registers admin routes with `apiSystem.registerRoutes(server)`
- serves static content:
  - `/` -> `/web/system/`
  - `/shared/` -> `/web/shared/`
- shows `SyS` on the LED display
- dispatches `EVT_NET_SETUP_DONE`

Admin mode intentionally ignores the configured SSID/password so the device remains recoverable even if saved settings are bad.

### SetupGame

`SetupGameState::enter()` prepares the gameplay network.

Behavior:

- sets `WiFi.mode(WIFI_AP_STA)`
- configures the AP IP/subnet using `apIP`
- starts the AP from `/network.json` values:
  - `ssid`, fallback `BrooklynTactical`
  - `password`, fallback `password`
  - `channel`, fallback `1`
- registers game routes with `apiGame.registerRoutes(server)`
- serves static content:
  - `/` -> `/web/game/`
  - `/shared/` -> `/web/shared/`
- shows `conF` on the LED display
- dispatches `EVT_NET_SETUP_DONE`

### Run

`RunState::enter()` activates the runtime services.

Behavior:

- plays a short mode-selected tone
- registers shared endpoints:
  - `GET /api/versions`
  - `GET /api/battery`
- registers captive-portal helper routes:
  - `/generate_204`
  - `/redirect`
  - `/hotspot-detect.html`
  - `/canonical.html`
  - `/ncsi.txt`
  - `/startpage`
  - `/connecttest.txt`
  - `/wpad.dat`
  - `/success`
- installs `server.onNotFound(...)`
- starts the DNS server on port 53
- starts mDNS for `my.milsim.prop`
- starts the async HTTP server on port 80

`RunState::update()` performs only periodic service maintenance:

- `dnsServer.processNextRequest()`
- `MDNS.update()` on ESP8266

Because `ESPAsyncWebServer` is asynchronous, there is no request-processing call equivalent to `_server->handleClient()`.

## HTTP Surface

### Shared Endpoints

These are registered in `RunState` and available regardless of whether the device entered admin or game mode:

- `GET /api/versions`
- `GET /api/battery`

### Admin/System Routes

Registered by `ApiSystem::registerRoutes()`:

- `GET /api/system/info`
- `GET /api/system/status`
- `POST /api/system/restart`
- `GET /api/system/wifi`
- `POST|PUT /api/system/wifi`
- `GET /api/system/mesh`
- `POST|PUT /api/system/mesh`
- `GET /api/system/files`
- `GET /api/system/file?path=/...`
- `POST /api/system/file?path=/...`
- `POST /api/system/ota/filesystem`
- `POST /api/system/diag/led`
- `POST /api/system/diag/display`
- `POST /api/system/diag/beep`
- `GET /api/system/diag/armstatus`
- `GET /api/system/diag/eeprom`
- `POST /api/system/diag/eeprom`

Notable admin persistence behavior:

- Wi-Fi and mesh settings are written back to `/network.json`
- lifted-file metadata comes from `/filesystem.json`
- generic file upload/download works directly against LittleFS
- filesystem OTA remounts LittleFS after flashing

### Game Routes

Registered by `ApiGame::registerRoutes()`:

- `GET /api/game/state`
- `POST|PUT /api/game/start`
- `GET /api/game/defaults`
- `POST|PUT /api/game/defaults`

Notable game behavior:

- `/api/game/state` exposes the current game FSM state and active timing/config values
- `/api/game/start` validates and applies countdown/game settings, then resets and starts `gameFSM`
- `/api/game/defaults` reads and writes `/game_defaults.json`

## Captive Portal Behavior

The captive portal relies on DNS, host filtering, and redirect handlers.

### DNS

The custom DNS server is started as a wildcard responder:

- port `53`
- domain `*`
- resolved IP `4.3.2.1`

Blocked domains are answered with `0.0.0.0` instead of the AP IP. The current blocklist contains:

- `wpad`

### Host Filtering and Redirects

Static `/` handlers in both admin and game modes use a host filter so content is only served when the request host equals `my.milsim.prop`.

For unmatched requests:

- if the host is neither `my.milsim.prop` nor `4.3.2.1`, the request is redirected to `http://my.milsim.prop/index.html`
- otherwise, the request receives `404`

This keeps browsers and captive-portal probes moving toward the intended hostname instead of the original requested domain.

## Static Content Layout

LittleFS content is exposed as follows:

- admin UI root: `/web/system/`
- game UI root: `/web/game/`
- shared assets: `/web/shared/`

Both mode-specific roots use `index.html` as the default file.

## Persistent Files Used by Networking

- `/network.json` - saved Wi-Fi and mesh/device configuration
- `/game_defaults.json` - saved defaults for the game UI/API
- `/filesystem.json` - manifest used by `/api/system/files`

If `/network.json` cannot be loaded, the firmware seeds in-memory defaults and immediately enters admin mode.

## Main Loop Integration

The `loop()` function in `src/main.cpp` drives networking with:

- `networkingFSM.update()`

It also handles one-time mode selection during the first 5 seconds after boot. After the FSM reaches `Run`, HTTP traffic is handled asynchronously by `ESPAsyncWebServer` while DNS requests continue to be serviced from `RunState::update()`.

## Summary

The current implementation is a small HFSM-driven boot-and-mode-selection layer around an async captive-portal web stack:

- `Initialize` immediately hands off to `Idle`
- `Idle` waits for admin/game selection
- `SetupAdmin` or `SetupGame` configures AP, routes, and static files
- `Run` starts DNS, mDNS, and HTTP and exposes shared runtime endpoints

If you need to trace behavior in code, start with `src/main.cpp` for boot/mode selection and `src/networking.cpp` for the actual network lifecycle.
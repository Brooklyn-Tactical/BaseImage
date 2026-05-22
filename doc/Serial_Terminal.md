# Serial Terminal Interface

## Overview

The Bio-Vial firmware exposes a small serial terminal interface over USB/UART.

It is currently used for two jobs:

1. setting a few EEPROM-backed values used by the firmware
2. watching boot and runtime log output so you can tell what state the prop is in

The serial interface is implemented in `src/main.cpp` by `processSerialCommands()`.

## Connection Settings

- Baud rate: `115200`
- Commands are processed only after a newline is received
- Carriage return (`\r`) is ignored, so Windows-style `CRLF` line endings work fine
- Commands are case-sensitive and are expected in uppercase

On boot, the prop prints a banner similar to:

- `Bio-Vial Standalone v<major>.<minor>`
- filesystem/configuration logs such as `[FS] ...` and `[CFG] ...`

## Supported Serial Commands

The current firmware supports these commands:

| Command | Purpose | EEPROM? |
|---|---|---|
| `HW:<float>` | Write the hardware revision value | Yes |
| `BR:` | Read the current raw ADC battery value | No |
| `BL:` | Read the stored battery calibration points | Yes |
| `B0:` | Store the current ADC reading as the 0% battery point | Yes |
| `B1:` | Store the current ADC reading as the 25% battery point | Yes |
| `B2:` | Store the current ADC reading as the 50% battery point | Yes |
| `B3:` | Store the current ADC reading as the 75% battery point | Yes |
| `B4:` | Store the current ADC reading as the 100% battery point | Yes |

If the command is not recognized, the prop responds with:

- `[CMD] Unknown command: <your text>`

## EEPROM Layout

The current EEPROM layout used by the serial terminal and battery service is:

| Address | Value | Meaning |
|---:|---|---|
| `0` | `float` | Hardware revision |
| `4` | battery `B0` slot | Raw ADC value representing 0% battery |
| `8` | battery `B1` slot | Raw ADC value representing 25% battery |
| `12` | battery `B2` slot | Raw ADC value representing 50% battery |
| `16` | battery `B3` slot | Raw ADC value representing 75% battery |
| `20` | battery `B4` slot | Raw ADC value representing 100% battery |

Notes:

- The battery values are stored in slots spaced by `sizeof(int)`.
- The code writes/reads them as raw ADC numbers and uses them for interpolation in `BatteryService`.
- After writing any `B0:` through `B4:` command, the firmware immediately reloads the battery calibration into `BatteryService`.

## Hardware Revision Command

### `HW:<float>`

Writes the hardware version to EEPROM address `0`.

Example:

- `HW:1.5`

Typical response:

- `[CMD] HW version written: 1.50`

What it is used for:

- The hardware revision is later exposed by the web endpoint `GET /api/versions` as the `hw` field.
- It is informational metadata and is not used by the current game/network state machines.

Important limitation:

- There is currently **no serial command** to read this value back directly.

## Battery Commands

### `BR:`

Reads the live battery ADC value with `analogRead(BATTERY_PIN)`.

Typical response:

- `[CMD] Battery Level read: 742`

What it is used for:

- This is a live measurement helper for calibration.
- It does **not** write anything to EEPROM.

### `BL:`

Reads the stored battery calibration points from EEPROM.

Typical response:

- `[CMD] Battery level calibration (raw ADC):`
- `B0 (  0%): ...`
- `B1 ( 25%): ...`
- `B2 ( 50%): ...`
- `B3 ( 75%): ...`
- `B4 (100%): ...`

What it is used for:

- Verifies what battery calibration is currently stored.
- Lets you confirm that `B0:` through `B4:` were written correctly.

### `B0:` through `B4:`

Each of these commands samples the current battery ADC value and stores it into EEPROM:

- `B0:` -> 0%
- `B1:` -> 25%
- `B2:` -> 50%
- `B3:` -> 75%
- `B4:` -> 100%

Typical response:

- `[CMD] B0 value written: 612`
- `[CMD] B4 value written: 824`

What they are used for:

- `BatteryService::currentLevel()` converts raw ADC readings into a percentage using these five calibration points.
- The calculated battery percentage is used for runtime battery display and low-battery behavior.
- When the calculated battery level reaches `0%`, `BatteryService` dispatches the low-battery event into the FSMs.

### Battery calibration procedure

The firmware does not guide the process automatically; it only stores the reading taken at the moment each command is sent.

A typical calibration workflow is:

1. Put the prop at the battery condition you want to represent.
2. Send `BR:` to see the raw ADC number.
3. Send the matching calibration command such as `B0:` or `B4:`.
4. Repeat for the remaining calibration points.
5. Send `BL:` to verify the stored values.

## How the Stored Battery Values Are Used

The stored `B0` through `B4` values are used by `BatteryService` like this:

- `B0` to `B1` maps to `0%` to `25%`
- `B1` to `B2` maps to `25%` to `50%`
- `B2` to `B3` maps to `50%` to `75%`
- `B3` to `B4` maps to `75%` to `100%`

The firmware interpolates linearly between those points.

If the battery level reaches `0%`, the game FSM can enter the low-battery terminal state.

## Reading the Prop's State Over Serial

### Important limitation

There is currently **no dedicated serial command** like `STATE:` or `GETSTATE:`.

So, reading the prop's state over serial means watching the normal runtime log messages.

## Boot and system logs

At startup you can watch these prefixes:

- `[FS]` - filesystem mount and file listing
- `[CFG]` - configuration load/default behavior
- `[BAT]` - battery calibration load
- `[Networking]` - networking FSM progress
- `[Game]` - game FSM state changes
- `[CMD]` - serial command responses

Examples of useful boot lines:

- `[FS] LittleFS mounted successfully`
- `[CFG] Loaded /network.json`
- `[CFG] /network.json not found, applying defaults`
- `[CFG] No valid config — auto-entering admin mode`

## Networking state logs

The networking FSM prints state transitions as they happen. Useful examples include:

- `[Networking] InitializeState: nothing to do, transitioning to Idle`
- `[Networking] IdleState: waiting for mode selection`
- `[Networking] SetupAdminState: configuring AP and registering admin routes`
- `[Networking] SetupGameState: configuring AP and registering game routes`
- `[Networking] RunState: entering main run loop`

Additional mode details are also printed, for example:

- admin AP SSID and IP
- game AP SSID, channel, and IP
- DNS startup
- mDNS startup result

These logs let you tell whether the prop booted into admin mode or game mode.

## Game state logs

The game FSM prints state entry messages. These are the main prop-state indicators you can read over serial:

| Game state | Typical serial line | Meaning |
|---|---|---|
| `configure` | `[Game] ConfigureState: waiting for /api/game/start` | Waiting for the game to be started through the web API |
| `await_start` | `[Game] AwaitStartState: remove pin to begin` | Game is armed for setup and waiting for pin removal |
| `await_arming` | `[Game] AwaitArmingState: insert pin to arm` | Waiting for the pin to be inserted |
| `arming` | `[Game] ArmingState: arming countdown started` | Arming countdown is running |
| `armed` | `[Game] ArmedState: ARMED — remove pin to start countdown` | Device is armed and ready to start the main timer |
| `countdown` | `[Game] CountdownState: countdown running` | Main game timer is running |
| `disarming` | `[Game] DisarmingState: disarming countdown started` | Disarm countdown is running |
| `disarmed` | `[Game] DisarmedState: vial disarmed successfully` | Objective was disarmed |
| `released` | `[Game] ReleasedState: countdown expired — vial released` | Countdown expired without disarm |
| `gameover` | `[Game] GameOverState: round complete — rapid toggle to reset` | Round finished |
| `lowbattery` | `[Game] LowBatteryState: battery critical — game suspended` | Battery is critically low |

## Practical usage examples

### Example: set hardware revision

1. Open the terminal at `115200` baud.
2. Send `HW:2.1`
3. Confirm the reply shows the written version.

### Example: read and store a battery calibration point

1. Put the prop in the battery condition you want to capture.
2. Send `BR:` and note the raw ADC value.
3. Send the matching calibration command, for example `B2:`.
4. Send `BL:` to confirm the EEPROM values.

### Example: determine the prop's current runtime state

1. Reboot or power-cycle the prop with the serial terminal open.
2. Watch `[Networking] ...` lines to see whether it entered admin or game mode.
3. Watch `[Game] ...` lines to see the active game-state transitions.

## Current limitations of the serial interface

At the moment, the serial terminal can:

- write the hardware version
- read/write battery calibration values
- show runtime state through log output

At the moment, it cannot:

- directly query the current state with a dedicated command
- directly read back the stored hardware version with a serial command
- change the game settings stored in `/network.json` or `/game_defaults.json`

## Source References

- `src/main.cpp` - serial command parser and command responses
- `src/BatteryService.cpp` - EEPROM calibration usage and percentage mapping
- `src/game.cpp` - game-state log messages
- `src/networking.cpp` - networking-state log messages
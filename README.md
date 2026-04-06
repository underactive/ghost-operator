# Ghost Operator - BLE HID Device

## Version 2.5.4
A wireless Bluetooth device that prevents screen lock and idle timeout. Masquerades as a keyboard and mouse, sending periodic keystrokes and movements. What you do with it is your own business.

---

## Features

- **BLE + USB HID Keyboard + Mouse + Consumer Control** - USB and BLE are both supported; by default, BLE stops when USB is connected. Enable **BT while USB** in settings to use both transports at once.
- **Encoder Menu System** - Adjust all settings with single rotary encoder
- **Flash Storage** - Settings survive sleep and power off (1MB onboard)
- **Software Power Control** - Deep sleep mode (~3µA)
- **OLED Display** - Real-time countdown bars and uptime
- **USB-C Charging** - Charge while operating
- **~120+ hours runtime** on 2000mAh battery
- **Scroll Wheel** - Optional random scroll events during mouse jiggle
- **Web Dashboard** - Configure via USB serial in Chrome (Web Serial API)
- **Web Serial DFU** - Update firmware from the web dashboard via USB serial
- **OTA DFU Mode** - Update firmware over Bluetooth via nRF Connect mobile app
- **Build Automation** - Local build/flash via Makefile, GitHub Actions CI/CD for releases

---

## Building

**Prerequisites:** Python 3 with pip (used by PlatformIO), and USB access for flashing. Run `make setup` once to install PlatformIO Core and `adafruit-nrfutil` into the project tooling.

From a fresh clone:

1. `make setup` — install PlatformIO + nrfutil (first time)
2. `make build` — compile the default nRF52 firmware (`seeed_xiao_nrf52840`)
3. `make flash` — build and flash over USB serial DFU

Other Makefile targets (e.g. `make monitor`, `make release`) wrap `build.sh` and PlatformIO; see `Makefile` and `build.sh` for details. Optional ESP32 LCD environments `c6lcd` and `s3lcd` are defined in `platformio.ini`.

**Dashboard (local dev):** `cd dashboard && npm install && npm run dev` — Vite dev server for the Vue 3 Web Serial dashboard.

Release notes and version history: **[CHANGELOG.md](CHANGELOG.md)**.

---

## Screenshots

| ![Boot Splash](docs/images/splash.png) | ![Normal Mode](docs/images/normal_mode.png) | ![Mouse Muted](docs/images/normal_mode-muted.png) | ![Screensaver](docs/images/screensaver.png) |
|:---:|:---:|:---:|:---:|
| **Boot splash** | **Normal mode** | **Mouse muted** | **Screensaver** |

| ![Settings Menu](docs/images/menu.png) | ![Settings More](docs/images/settings-more.png) | ![Key Slots](docs/images/menu-slots.png) | ![Sleep](docs/images/sleep.png) |
|:---:|:---:|:---:|:---:|
| **Settings menu** | **Settings (more)** | **Key slot editor** | **Sleep** |

---

## Bill of Materials

### Components

| Ref | Qty | Part | Description | Price |
|-----|-----|------|-------------|-------|
| U1 | 1 | Seeed XIAO nRF52840 | BLE 5.0 MCU, USB-C, LiPo charger | $9.90 |
| DISP1 | 1 | SSD1306 OLED 0.96" | 128x64 I2C Display | $3.50 |
| ENC1 | 1 | Rotary Encoder | EC11 with pushbutton (internal pull-ups) | $1.50 |
| SW1 | 1 | Tactile Button | 6x6mm momentary | $0.10 |
| BT1 | 1 | LiPo Battery | 3.7V 2000mAh, JST-SH 1.25mm | $6.00 |
| C1 | 1 | Ceramic Capacitor | 100nF | $0.05 |
| C2 | 1 | Electrolytic Capacitor | 10µF | $0.10 |
| J1 | 1 | JST-SH Header | 2-pin, through-hole | $0.30 |
| - | 1 | Prototype PCB | 5x7cm | $0.50 |
| - | - | Wire, headers, enclosure | Misc | $3.00 |

### Cost Summary

| Category | Cost |
|----------|------|
| Components | $24.95 |
| Shipping | ~$3.00 |
| **Total** | **~$28** |

---

## Pinout

| XIAO Pin | Function |
|----------|----------|
| BAT+ | Battery + (direct) |
| BAT- | Battery - |
| 3V3 | Power out to peripherals |
| GND | Ground rail |
| D0 | Encoder CLK (A) |
| D1 | Encoder DT (B) |
| D2 | Encoder SW (button) |
| D3 | Function Button |
| D4 | I2C SDA (OLED) |
| D5 | I2C SCL (OLED) |
| D6 | Piezo buzzer |
| D7 | Mute button (Simple/Sim) / Play-Pause (Volume Control) |
| D8-D10 | Unused (available) |

---

## Controls

### UI Modes

Ten UI modes, accessed via function button and menu actions (separate from **operation modes** — Simple/Simulation/Volume Control and built-in games — chosen in **MODE**):

| Mode | Purpose |
|------|---------|
| **NORMAL** | Live status display; behavior depends on operation mode (Simple/Simulation/Volume Control) |
| **MENU** | Scrollable settings menu; encoder navigates/edits, button selects/confirms |
| **SLOTS** | 8-key slot editor; encoder cycles key, button advances slot |
| **NAME** | Device name editor; encoder cycles character, button advances position |
| **DECOY** | BLE identity picker; choose from 10 commercial device presets |
| **SCHEDULE** | Schedule editor; set mode, start/end times |
| **MODE** | Operation mode picker (horizontal carousel); Simple/Simulation/Volume Control (and games where supported) |
| **SET CLOCK** | Manual clock editor; set hours and minutes |
| **CAROUSEL** | Full-screen carousel editors for selected settings (named options) |
| **CLICK SLOTS** | 7-slot phantom click action editor (Left/Middle/Right/Btn4/Btn5/Wheel Up/Down/NONE) |

### Control Actions

| Control | NORMAL Mode | MENU Mode | SLOTS Mode | NAME Mode |
|---------|-------------|-----------|------------|-----------|
| Encoder Turn | Switch profile (LAZY/NORMAL/BUSY) | Navigate items / adjust value | Cycle key for active slot | Cycle character / toggle Yes-No |
| Encoder Button | Cycle KB/MS combos | Select item / confirm edit | Advance slot cursor (1-8) | Advance position / confirm |
| Func Short | Open menu | Close menu (save) | Back to menu (save) | Save name (reboot prompt) |
| Func Hold (5s) | Sleep (countdown) | Sleep (countdown) | Sleep (countdown) | Sleep (countdown) |
| Func (sleeping) | Wake up | - | - | - |

### Menu Items

Settings organized under headings in the scrollable menu:

| Heading | Setting | Range |
|---------|---------|-------|
| **Keyboard** | Key min | 0.5s - 30s (0.5s steps) |
| | Key max | 0.5s - 30s (0.5s steps) |
| | Key slots | → opens SLOTS mode |
| **Mouse** | Move duration | 0.5s - 90s (0.5s steps) |
| | Idle duration | 0.5s - 90s (0.5s steps) |
| | Move style | Bezier / Brownian |
| | Move size | 1px - 5px (1px steps, Brownian only) |
| | Scroll | Off / On (random scroll wheel during jiggle) |
| **Profiles** | Lazy adjust | -50% to 0% (5% steps) |
| | Busy adjust | 0% to 50% (5% steps) |
| **Display** | Brightness | 10% - 100% (10% steps) |
| | Saver bright | 10% - 100% (10% steps) |
| | Saver time | Never / 1 / 5 / 10 / 15 / 30 min |
| | Animation | ECG / EQ / Ghost / Matrix / Radar / None |
| **Device** | Device name | → opens NAME mode (14 char max) |
| | BT while USB | Off / On (keep BLE active when USB plugged in) |
| | Reset defaults | → confirmation prompt (restores factory settings) |
| | Reboot | → confirmation prompt (restarts device) |
| **About** | Version | Read-only firmware version |

### Timing Profiles

Rotate the encoder in NORMAL mode to switch between **LAZY**, **NORMAL**, and **BUSY** profiles:

| Profile | Key Intervals | Mouse Move | Mouse Idle |
|---------|--------------|--------------|------------|
| LAZY | Longer (+%) | Shorter (−%) | Longer (+%) |
| NORMAL | Base values | Base values | Base values |
| BUSY | Shorter (−%) | Longer (+%) | Shorter (−%) |

Encoder rotation is clamped: turning left past LAZY stays at LAZY, turning right past BUSY stays at BUSY. Profile percentages are adjustable (0-50% in 5% steps, default 15%) via the LAZY % and BUSY % settings modes. Profiles do not modify stored base values — they apply at scheduling time.

### Timing Behavior

- **Keyboard**: Random interval between MIN and MAX each keypress (profile-adjusted)
- **Mouse**: Move for set duration → Idle for set duration → repeat (profile-adjusted)
  - Inertial movement: sine ease-in-out ramps speed from zero → peak → zero each jiggle
  - ±20% randomness applied to movement and idle durations
  - Minimum clamp: 0.5s

---

## Display

### Normal Mode

```
┌────────────────────────────────┐
│ GHOST Operator          ᛒ 85%  │
├────────────────────────────────┤
│ KB [F15] 2.0-6.5s            ↑ │
│ ████████████░░░░░░░░░░░░  3.2s │
├────────────────────────────────┤
│ MS [MOV]  15s/30s            ↑ │
│ ██████░░░░░░░░░░░░░░░░░░  8.5s │
├────────────────────────────────┤
│ Up: 2h 34m             ~^~_~^~ │  ← or profile name for 3s
└────────────────────────────────┘
```

- **KB [key]**: Shows the pre-picked next key that will fire when the countdown reaches zero
- **2.0-6.5s**: Profile-adjusted key interval range (MIN-MAX)
- **Bluetooth icon** (solid) = BLE connected, (flashing) = Scanning; **USB icon** shown when wired
- ↑ = enabled, ✕ = disabled; mouse idle bar counts up, move bar counts down
- **[MOV]** = Mouse moving, **[IDL]** = Mouse idle, **[RTN]** = Returning to origin
- Each keystroke cycle randomly picks from populated (non-NONE) slots
- **Uptime line**: Shows profile name (LAZY/NORMAL/BUSY) for 3 seconds after switching, then reverts to uptime (compact format: `2h 34m`, `1d 5h`, `45s`)
- **Status animation**: Configurable animation in the footer area (default: Ghost). Options: ECG, EQ, Ghost, Matrix, Radar, None — changeable via Display → Animation in the menu. Animation speed is activity-aware: full speed with both KB/MS enabled, half speed with one muted, frozen with both muted.

### Menu Mode

```
┌────────────────────────────────┐
│ MENU                    ᛒ 85%  │  ← "MENU" inverted when title selected
├────────────────────────────────┤
│       - Keyboard -             │  ← Heading (not selectable)
│ ▌Key min          < 2.0s >  ▐  │  ← Selected item (inverted row)
│  Key max          < 6.5s >     │  ← Unselected item
│  Key slots                 >   │  ← Action item
│       - Mouse -                │  ← Heading
├────────────────────────────────┤
│ Minimum delay between keys     │  ← Context help (scrolls if long)
└────────────────────────────────┘
```

- 5-row scrollable viewport with headings, value items, and action items
- `< value >` arrows show available range; arrows hidden at bounds
- Selected item shown with inverted colors; editing inverts only the value portion
- Help bar at bottom shows context text for the selected item

### Slots Mode

```
┌────────────────────────────────┐
│ MODE: SLOTS              [3/8] │
├────────────────────────────────┤
│   F15 F14 --- ---              │
│   --- --- --- ---              │
├────────────────────────────────┤
│ Turn=key  Press=slot           │
│ Func=back                      │
└────────────────────────────────┘
```

- 8 configurable slots (2 rows × 4), active slot shown with inverted colors
- Turn encoder to cycle key for active slot, press encoder to advance slot cursor
- Function button returns to menu at "Key slots" item

### Name Mode

```
┌────────────────────────────┐
│ DEVICE NAME          [3/14] │
├────────────────────────────┤
│   G h o s t O p             │
│   e r a t o r · ·           │
├────────────────────────────┤
│ Turn=char  Press=next       │
│ Func=save                   │
└────────────────────────────┘
```

- 14 character positions (2 rows × 7), active position shown with inverted colors
- Turn encoder to cycle through A-Z, a-z, 0-9, space, dash, underscore, END (66 chars, wrapping)
- END positions shown as `·` (middle dot)
- Press encoder to advance to next position (wraps at 14)
- Function button saves; if name changed, shows reboot confirmation (Yes/No)

---

## Wiring

### Power (Direct - No Switch)

```
BT1 (+) → J1 Pin 1 → XIAO BAT+
BT1 (-) → J1 Pin 2 → XIAO BAT-
```

### OLED Display

```
VCC → 3V3 Rail
GND → GND Rail
SCL → D5
SDA → D4
```

### Rotary Encoder (EC11)

```
GND → GND Rail
SW  → D2 (internal pull-up)
DT  → D1 (internal pull-up)
CLK → D0 (internal pull-up)
```

### Function Button

```
Pin 1 → D3
Pin 2 → GND Rail
```

### Decoupling Capacitors

Place C1 (100nF) and C2 (10µF) between 3V3 and GND, close to XIAO.

---

## Flash Storage

Settings are automatically saved to the XIAO's internal flash when:
- Leaving a settings mode (function button press)
- 30 second timeout in menu/slots mode
- Entering sleep mode

Settings persist through sleep and power off.

---

## Serial Commands

Connect via USB at 115200 baud:

| Key | Command |
|-----|---------|
| h | Help |
| s | Status report |
| d | Dump settings |
| z | Enter sleep mode |
| p | PNG screenshot (base64-encoded) |
| v | Activate screensaver |
| e | Easter egg animation |
| f | Enter OTA DFU mode (BLE) |
| u | Enter Serial DFU mode (USB) |

---

## Pairing

1. Connect battery (device powers on automatically)
2. Open Bluetooth settings on your computer
3. Search for "GhostOperator" (or your custom device name)
4. Pair and connect
5. Display shows Bluetooth icon when connected

---

## Files

PlatformIO splits firmware by target: **`src/common/`** (shared portable code), **`src/nrf52/`** (Seeed XIAO nRF52840 — `env:seeed_xiao_nrf52840`; BLE, OLED, encoder, sleep, and related drivers are nRF-only and not built for ESP32 envs), and optional ESP32 LCD ports under **`src/esp32-c6-lcd-1.47/`** (`env:c6lcd`) and **`src/esp32-s3-lcd-1.47/`** (`env:s3lcd`).

| File | Description |
|------|-------------|
| `src/common/config.h` | Constants, enums, structs (header-only) |
| `src/common/keys.h`, `keys.cpp` | Const data tables (keys, menu items, names) |
| `src/common/timing.h`, `timing.cpp` | Profiles, scheduling, formatting |
| `src/common/mouse.h`, `mouse_pure.h`, `mouse.cpp` | Mouse movement logic (shared) |
| `src/common/orchestrator.h`, `orchestrator.cpp` | Simulation activity orchestrator (phase cycling, mutual exclusion) |
| `src/common/sim_data.h`, `sim_data.cpp` | Simulation data tables (job templates, work modes, phase timing) |
| `src/common/schedule.h`, `schedule.cpp` | Timed schedule (auto-sleep/full auto, light/deep sleep, time sync) |
| `src/common/settings.h`, `settings_pure.h`, `settings_common.cpp` | Settings model + shared accessors |
| `src/common/state.h` | Portable shared state (settings, UI, orchestrator, etc.) |
| `src/common/hid_keycodes.h` | HID keycode definitions |
| `src/common/platform_hal.h` | Platform hooks (implemented per target) |
| `src/nrf52/ghost_operator.cpp` | Main entry point: setup(), loop(), BLE + USB HID callbacks |
| `src/nrf52/state.h`, `state.cpp` | nRF52 globals and hardware handles (includes `common/state.h`) |
| `src/nrf52/icons.h`, `icons.cpp` | PROGMEM bitmaps (splash, BT/USB icons, arrows) |
| `src/nrf52/settings_nrf52.cpp` | Flash persistence + nRF52 value accessors |
| `src/nrf52/schedule_nrf52.cpp` | Schedule integration on nRF52 |
| `src/nrf52/sim_data_flash.cpp` | Simulation data persistence (flash) |
| `src/nrf52/encoder.h`, `encoder.cpp` | ISR + polling quadrature decode |
| `src/nrf52/battery.h`, `battery.cpp` | ADC battery reading |
| `src/nrf52/hid.h`, `hid.cpp` | Keystroke + mouse + scroll (BLE + USB) |
| `src/nrf52/sleep.h`, `sleep.cpp` | Deep sleep sequence |
| `src/nrf52/screenshot.h`, `screenshot.cpp` | PNG encoder + base64 serial output |
| `src/nrf52/serial_cmd.h`, `serial_cmd.cpp` | Serial debug commands + status |
| `src/nrf52/input.h`, `input.cpp` | Encoder dispatch, buttons, name editor |
| `src/nrf52/display.h`, `display.cpp` | All rendering (~2900 lines) |
| `src/nrf52/ble_uart.h`, `ble_uart.cpp` | BLE UART (NUS) + transport-agnostic config protocol |
| `src/nrf52/sound.h`, `sound.cpp` | Piezo buzzer keyboard sounds |
| `src/nrf52/protocol.h`, `protocol.cpp` | Config protocol helpers |
| `src/nrf52/breakout.h`, `breakout.cpp` | Breakout game |
| `src/nrf52/snake.h`, `snake.cpp` | Snake game |
| `src/nrf52/racer.h`, `racer.cpp` | Ghost Racer game |
| `src/esp32-c6-lcd-1.47/`, `src/esp32-s3-lcd-1.47/` | Optional ESP32 LCD firmware (LVGL, NimBLE); see `platformio.ini` `c6lcd` / `s3lcd` |
| `platformio.ini` | PlatformIO build configuration (board, deps, flags) |
| `boards/` | Custom board definition + variant files for Seeed XIAO nRF52840 |
| `dashboard/` | Vue 3 web dashboard (USB serial config + Web Serial DFU) |
| `schematic_v8.svg` | Circuit schematic |
| `schematic_interactive_v3.html` | Interactive documentation |
| `README.md` | Technical documentation (this file) |
| `docs/USER_MANUAL.md` | End-user guide |
| `CHANGELOG.md` | Version history (semver) |
| `CLAUDE.md` | Project context for AI assistants |
| `build.sh` | Build automation: compile, setup, release, flash |
| `Makefile` | Convenience targets wrapping build.sh + PlatformIO |
| `.github/workflows/release.yml` | CI/CD: build + GitHub Release on `v*` tag push |

---

## Version History

Per-release notes are maintained in **[CHANGELOG.md](CHANGELOG.md)** (canonical version history).

---

## License

This project is licensed under the **PolyForm Noncommercial License 1.0.0**. You may use and share it for **noncommercial** purposes; **commercial** use requires a separate agreement. You must preserve **copyright and license notices** when distributing copies. The full terms are in **[LICENSE](LICENSE)**; the official license text is at [polyformproject.org/licenses/noncommercial/1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0).

*"Fewer parts, more flash"*

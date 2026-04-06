# CLAUDE.md - Ghost Operator Project Context

## Project Overview

**Ghost Operator** is a BLE keyboard/mouse hardware device built on the Seeed XIAO nRF52840. It prevents screen lock and idle timeout by sending periodic keystrokes and mouse movements over Bluetooth.

**Current Version:** 2.5.3
**Status:** Production-ready

---

## Hardware

### Microcontroller
- **Seeed XIAO nRF52840**
- ARM Cortex-M4 @ 64MHz
- BLE 5.0
- 1MB Flash, 256KB RAM
- Built-in LiPo charger (USB-C)
- Deep sleep: ~3µA

### Components (8 total)
| Ref | Component | Purpose |
|-----|-----------|---------|
| U1 | XIAO nRF52840 | MCU + BLE |
| DISP1 | SSD1306 OLED 128x64 | Display (I2C) |
| ENC1 | EC11 Rotary Encoder | Input + key selection |
| SW1 | Tactile button 6x6mm | Mode cycle / sleep |
| SW2 | Tactile button 6x6mm | Mute (KB/MS toggle) |
| BT1 | LiPo 3.7V 2000mAh | Power |
| C1 | 100nF ceramic | Decoupling |
| C2 | 10µF electrolytic | Decoupling |

### Pin Assignments
| Pin | Function | Notes |
|-----|----------|-------|
| D0 | Encoder CLK (A) | Interrupt-driven |
| D1 | Encoder DT (B) | Interrupt-driven |
| D2 | Encoder SW | Push button |
| D3 | Function button | Mode/sleep |
| D4 | I2C SDA | OLED |
| D5 | I2C SCL | OLED |
| D6 | Piezo buzzer | Keyboard sound output |
| D7 | Mute button | SPST momentary, active LOW |
| D8-D10 | Unused | Available for expansion |
| BAT+/- | Battery | Direct to LiPo |

---

## Architecture

### Core Files
Firmware is **not** a flat `src/` layout and there is no root `*.ino`; each PlatformIO environment in `platformio.ini` selects its own trees and entrypoint.

- **`seeed_xiao_nrf52840`:** `src/common/` + `src/nrf52/` (`build_src_filter = +<common/> +<nrf52/>`, `-Isrc/common`, `-Isrc/nrf52`). Entry: **`src/nrf52/ghost_operator.cpp`** (`setup()`, `loop()`).
- **`s3lcd` / `c6lcd`:** `src/common/` plus **`src/esp32-s3-lcd-1.47/`** or **`src/esp32-c6-lcd-1.47/`** (filters `+<common/> +<esp32-s3-lcd-1.47/>` and `+<common/> +<esp32-c6-lcd-1.47/>`). Entry: **`main.cpp`** in that platform directory.
- **`native`:** Host Unity tests using `src/common/` and `test/` (see `env:native`).

**Portable (`src/common/`):** `config.h` — `#define` constants, enums, structs; `keys.h` / `keys.cpp` — AVAILABLE_KEYS, MENU_ITEMS, names; `timing.h` / `timing.cpp` — profiles, scheduling, formatting; `mouse.h` / `mouse.cpp` — mouse state machine with sine easing; `schedule.h` / `schedule.cpp` — timed schedule logic; `orchestrator.h` / `orchestrator.cpp` — simulation activity orchestrator; `sim_data.h` / `sim_data.cpp` — job templates, work modes, phase timing; `settings.h`, `settings_pure.h` / `settings_common.cpp` — `loadDefaults()`, `getSettingValue()`, `setSettingValue()`, `formatMenuValue()`; `state.h` — portable globals; `hid_keycodes.h`; `platform_hal.h`.

**nRF52 (`src/nrf52/`):** `settings_nrf52.cpp` — `loadSettings()` / `saveSettings()` and flash I/O; `schedule_nrf52.cpp` — platform hooks; `sim_data_flash.cpp` — flash-backed sim data; `state.h` / `state.cpp` — includes portable `state.h`, adds hardware/BLE/USB objects and game macros; paired `.h` / `.cpp` modules — `icons` (PROGMEM bitmaps including splash), `encoder`, `battery`, `hid`, `sleep`, `screenshot`, `serial_cmd`, `input`, `display` (~2900 lines, largest module), `ble_uart` (NUS + config protocol), `sound`, `protocol`, `breakout`, `snake`, `racer`.

**ESP32 LCD (`src/esp32-s3-lcd-1.47/`, `src/esp32-c6-lcd-1.47/`):** `main.cpp` entry; platform `state`, `display`, `ble` / `ble_uart`, `hid`, `settings_*`, `sim_data_nvs`, and related modules (LVGL UI; NimBLE).

### Dependencies
- Adafruit Bluefruit (built into Seeed nRF52 core)
- Adafruit GFX Library
- Adafruit SSD1306
- Adafruit LittleFS (built into core)

### Key Subsystems

#### 1. HID (BLE + USB)
- Presents as composite keyboard + mouse + consumer control device over both BLE and USB
- BLE: Adafruit Bluefruit HID; USB: TinyUSB composite HID (`setupUSBHID()` registers descriptor before USB stack starts)
- USB descriptors: manufacturer "TARS Industrial", product "Ghost Operator" (set via `TinyUSBDevice.setManufacturerDescriptor/setProductDescriptor` before stack init)
- `dualKeyboardReport()` sends to both transports; `sendMouseMove()` and `sendMouseScroll()` likewise
- Device name: "GhostOperator" (customizable)
- Auto-reconnects to last paired BLE host
- "BT while USB" setting (default Off): when Off, BLE stops when USB is connected; when On, both transports active simultaneously
- Display shows USB trident icon when wired, BLE icon when wireless, lightning bolt icon when charging
- Jiggler runs when either BLE or USB is connected (disabled in Volume Control mode)

#### 2. UI Modes
```cpp
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_DECOY, MODE_SCHEDULE, MODE_MODE, MODE_SET_CLOCK, MODE_CAROUSEL, MODE_CLICK_SLOTS, MODE_COUNT };
```

#### 2b. Operation Modes
```cpp
enum OperationMode { OP_SIMPLE, OP_SIMULATION, OP_VOLUME, OP_BREAKOUT, OP_SNAKE, OP_RACER, OP_MODE_COUNT };
```
- **NORMAL**: Live status; encoder switches profile (Simple), adjusts job performance (Simulation), or sends volume up/down (Volume Control); button cycles KB/MS combos (Simple/Simulation); D2 configurable (Play/Pause or Mute), D7 configurable (Next/Mute/Play) in Volume Control
- **MENU**: Scrollable settings menu; encoder navigates/edits, button selects/confirms
- **SLOTS**: 8-key slot editor; encoder cycles key, button advances slot
- **NAME**: Device name editor; encoder cycles character, button advances position
- **DECOY**: BLE identity picker; encoder navigates presets (with active marker `*`), button selects, reboot confirmation on change
- **SCHEDULE**: Schedule editor; 3-row layout (Mode/Start/End) with contextual help; rows hidden based on mode selection
- **MODE**: Operation mode picker (Simple/Simulation/Volume Control) as horizontal carousel with smooth scrolling and reboot confirmation
- **SET_CLOCK**: Manual time editor; encoder cycles hour/minute values, button advances field, function button confirms and calls `syncTime()`; pre-fills from current time if synced
- **CLICK_SLOTS**: 7-slot click action editor; encoder cycles action (Left/Middle/Right/Btn4/Btn5/Wheel Up/Wheel Down/NONE), button advances slot
- Function button toggles NORMAL ↔ MENU; from SLOTS/NAME/DECOY/SCHEDULE/MODE/SET_CLOCK/CLICK_SLOTS returns to MENU
- 30-second timeout returns to NORMAL from MENU, SLOTS, CLICK_SLOTS, or NAME

#### 2a. Menu System
Data-driven architecture using `MenuItem` struct array (62 entries):
```cpp
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME, FMT_MOUSE_STYLE, FMT_ON_OFF, FMT_SCHEDULE_MODE, FMT_TIME_5MIN, FMT_UPTIME, FMT_DIE_TEMP, FMT_OP_MODE, FMT_JOB_SIM, FMT_SWITCH_KEYS, FMT_HEADER_DISP, FMT_CLICK_TYPE, FMT_KEY_SOUND, FMT_PERF_LEVEL, FMT_VOLUME_THEME, FMT_ENC_BTN_ACTION, FMT_SIDE_BTN_ACTION };
```
- `getSettingValue(settingId)` / `setSettingValue(settingId, value)` — generic accessors (with key min/max cross-constraint)
- `formatMenuValue(settingId, format)` — formats for display using `formatDuration()`, `N%`, `-N%`, `SAVER_NAMES[]`, `Npx`, `ANIM_NAMES[]`, `MOUSE_STYLE_NAMES[]`, or `SWITCH_KEYS_NAMES[]`
- `moveCursor(direction)` — skips headings, clamps at bounds, manages viewport scroll
- `FMT_PERCENT_NEG` items: encoder direction and arrow bounds are inverted so displayed value direction matches encoder rotation
- Conditional visibility: some items are hidden based on other settings (e.g., "Switch keys" in the Keyboard section is only visible when `windowSwitching` is enabled; "Move size" hidden when mouse style is Bezier; Volume heading/Theme only visible in Volume Control mode; Animation, Schedule, and Sound sections hidden in Volume Control mode)

#### 3. Settings Storage
```cpp
#define NUM_SLOTS 8

struct Settings {
  uint32_t magic;              // 0x50524F61 (bumped: added racer settings)
  uint32_t keyIntervalMin;     // ms
  uint32_t keyIntervalMax;     // ms
  uint32_t mouseJiggleDuration; // ms
  uint32_t mouseIdleDuration;   // ms
  uint8_t keySlots[NUM_SLOTS]; // index into AVAILABLE_KEYS per slot
  uint8_t lazyPercent;         // 0-50, step 5, default 15
  uint8_t busyPercent;         // 0-50, step 5, default 15
  uint8_t saverTimeout;        // index into SAVER_MINUTES[] (0=Never..5=30min)
  uint8_t saverBrightness;     // 10-100, step 10, default 20
  uint8_t displayBrightness;   // 10-100, step 10, default 80
  uint8_t mouseAmplitude;      // 1-5, step 1, default 1 (pixels per movement step)
  uint8_t mouseStyle;          // 0=Bezier, 1=Brownian (default 0)
  uint8_t animStyle;           // 0-5 index into ANIM_NAMES[] (default 2 = Ghost)
  char    deviceName[15];      // 14 chars + null terminator (BLE device name)
  uint8_t btWhileUsb;          // 0=Off (default), 1=On — keep BLE active when USB connected
  uint8_t scrollEnabled;       // 0=Off (default), 1=On — random scroll wheel during mouse jiggle
  uint8_t dashboardEnabled;    // 1=On (default), 0=Off — WebUSB landing page for Chrome
  uint8_t dashboardBootCount;  // 0-2=boot count (auto-disable after 3), 0xFF=user pinned
  uint8_t decoyIndex;          // 0=Custom/default, 1-10=preset index into DECOY_NAMES[]
  uint8_t scheduleMode;        // 0=Off, 1=Auto-sleep, 2=Full auto
  uint16_t scheduleStart;      // 0-287 (5-min slots), default 108 (9:00)
  uint16_t scheduleEnd;        // 0-287 (5-min slots), default 204 (17:00)
  uint8_t invertDial;          // 0=Off (default), 1=On — reverse encoder rotation
  // Simulation mode settings
  uint8_t operationMode;       // OP_SIMPLE=0 (default), OP_SIMULATION=1, OP_VOLUME=2, OP_BREAKOUT=3, OP_SNAKE=4, OP_RACER=5
  uint8_t jobSimulation;       // 0=Staff, 1=Developer, 2=Designer (default: 0)
  uint8_t jobPerformance;      // 0-11, default 5 (level*10 = percentage, 5=baseline)
  uint16_t jobStartTime;       // 0-287 (5-min slots), default 96 (8:00)
  uint8_t phantomClicks;       // 0=Off (default), 1=On
  uint8_t clickSlots[NUM_CLICK_SLOTS]; // 7 slots, each 0-7 index into CLICK_TYPE_NAMES (default: [1,7,7,7,7,7,7])
  uint8_t windowSwitching;     // 0=Off (default), 1=On
  uint8_t switchKeys;          // 0=Alt-Tab (default), 1=Cmd-Tab
  uint8_t headerDisplay;       // 0=Job sim name (default), 1=Device name
  uint8_t soundEnabled;        // 0=Off (default), 1=On
  uint8_t soundType;           // 0=MX Blue, 1=MX Brown, 2=Membrane, 3=Buckling, 4=Thock
  uint8_t systemSoundEnabled;  // 0=Off (default), 1=On — BLE connect/disconnect alert tones
  // Volume control settings
  uint8_t volumeTheme;         // 0=Basic (default), 1=Retro, 2=Futuristic
  uint8_t encButtonAction;     // 0=Play/Pause (default), 1=Mute
  uint8_t sideButtonAction;    // 0=Next (default), 1=Mute, 2=Play/Pause
  // Shift/lunch settings (dashboard-only)
  uint16_t shiftDuration;      // 240-720 min, step 30, default 480 (8h)
  uint8_t lunchDuration;       // 15-120 min, step 5, default 30 (30m)
  uint8_t checksum;            // must remain last
};

enum SwitchKeys { SWITCH_KEYS_ALT_TAB, SWITCH_KEYS_CMD_TAB, SWITCH_KEYS_COUNT };
```
Saved to `/settings.dat` via LittleFS. Survives sleep and power-off.
Default: slot 0 = F16 (index 3), slots 1-7 = NONE (index 28), lazy/busy = 15%, screensaver = Never, saver brightness = 20%, display brightness = 80%, mouse amplitude = 1px, mouse style = Bezier, animation = Ghost, device name = "GhostOperator", BT while USB = Off, scroll = Off, dashboard = On (smart default: auto-disables after 3 boots if user never touches it; any explicit toggle pins it permanently), invert dial = Off, operation mode = Simple, job simulation = Staff, job performance = 5 (baseline), job start time = 8:00 (96), phantom clicks = Off, click slots = [Middle, NONE x6], window switching = Off, switchKeys = Alt-Tab (0), header display = Job sim name, sound = Off, sound type = MX Blue, volume theme = Basic, knob button = Play/Pause, side button = Next, shift duration = 480 (8h), lunch duration = 30 (30m).

#### 4. Timing Profiles
```cpp
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
```
- Profiles scale base timing values by a configurable percentage at scheduling time
- BUSY: shorter KB intervals (−%), longer mouse movement (+%), shorter mouse idle (−%)
- LAZY: longer KB intervals (+%), shorter mouse movement (−%), longer mouse idle (+%)
- NORMAL: passes base values through unchanged (default)
- `applyProfile()` helper applies directional scaling; `effective*()` accessors wrap it
- Profile resets to NORMAL on sleep/wake; lazy% and busy% persist in flash
- Selected via encoder rotation in NORMAL mode (clamped, not wrapping); name shown on uptime line for 3s

#### 5. Timing Behavior
- **Keyboard:** Random interval between effective MIN and MAX each cycle (profile-adjusted)
- **Mouse:** Effective movement then idle durations, ±20% randomness on both (profile-adjusted)

#### 6. Mouse State Machine
```cpp
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };
enum MouseStyle { MOUSE_BEZIER, MOUSE_BROWNIAN, MOUSE_STYLE_COUNT };
```
- Two movement styles selectable via "Move style" menu item:
  - **Bezier** (default): Smooth curved sweeps with random radius; `mouseAmplitude` has no effect (firmware hides "Move size" in menu)
  - **Brownian**: Classic jiggle with sine ease-in-out velocity profile; `mouseAmplitude` controls peak step size
- Tracks net displacement during movement
- Non-blocking return to approximate origin via MOUSE_RETURNING state
- Brownian JIGGLING uses sine ease-in-out velocity profile: `amp = mouseAmplitude * sin(π × progress)` where progress goes 0→1 over the jiggle duration. Movement ramps from zero → peak → zero. Steps with zero amplitude are skipped (natural pause at start/end). `pickNewDirection()` stores unit vectors (-1/0/+1); amplitude is applied per-step with easing.
- Display shows `[RTN]` during MOUSE_RETURNING state; progress bar stays at 0% (empty)
- **Scroll wheel** (optional): When `scrollEnabled`, random ±1 scroll ticks injected at 2-5s intervals during MOUSE_JIGGLING (both Bezier and Brownian). Uses `sendMouseScroll()` which sends to both BLE and USB.

#### 7. Power Management
- `sd_power_system_off()` for deep sleep
- Wake via GPIO sense on function button (P0.29)
- **Two-stage sleep**: after 500ms hold, overlay shows "Hold to sleep..." with 6-second segmented countdown bar
  - **Light sleep** (0–3s): Release enters light sleep — display shows breathing circle animation (radius 2→6px, 4s sinusoidal cycle), BLE advertising stopped
  - **Deep sleep** (3–6s): Release or completion enters deep sleep via `sd_power_system_off()` (~3µA)
  - Bar has two labeled segments ("Light" and "Deep") with dynamic hint: "Release to cancel" → "Release = light sleep" at 3s midpoint
- Release before 500ms threshold cancels sleep ("Cancelled" shown for 400ms)
- Input suppressed during confirmation and cancellation overlays
- Works from any mode and from screensaver
- **Hardware watchdog (WDT)**: 8-second timeout with I2C bus recovery on restart

#### 8. Screensaver
- Overlay state (`screensaverActive` flag), not a UIMode — gates display rendering
- Activates only on top of `MODE_NORMAL` after configurable timeout (Never/1/5/10/15/30 min)
- Minimal display: centered key label + 1px progress bar, mouse state + 1px bar, battery only if <15%
- OLED contrast dimmed to configurable brightness (10-100%, default 20%) via `SSD1306_SETCONTRAST`
- Any input (encoder, buttons) wakes screensaver — input is consumed, not passed through
- Long-press sleep still works from screensaver (not consumed)
- Timeout and brightness configurable via MENU items ("Saver time" and "Saver bright")

#### 8a. Display Rendering
- **Dirty flag**: `markDisplayDirty()` called from all state-change sites (BLE connect/disconnect, keystroke, mouse transition, encoder, buttons, settings, battery, orchestrator); static modes skip I2C entirely when idle
- **20 Hz refresh**: `DISPLAY_UPDATE_MS` = 50ms with dirty flag gating; time-based modes (NORMAL, MENU, screensaver, overlays) redraw every frame; screensaver runs at 5 Hz (`DISPLAY_UPDATE_SAVER_MS` = 200ms)
- **Shadow buffer**: 1024-byte page cache compares each render via `memcmp`, sends only contiguous dirty page ranges via SSD1306 page addressing (~60–75% I2C savings on typical NORMAL frames)
- `invalidateDisplayShadow()` for code paths that bypass the shadow (e.g., scheduled light sleep screen)

#### 8b. Sound
- **Piezo buzzer** on D6 (transistor-driven: Q1 2N2222, R1 1kΩ, R2 10kΩ)
- 5 keyboard sound profiles: MX Blue, MX Brown, Membrane, Buckling Spring, Deep Thock
- `playKeySound()`: plays on key-down when `soundEnabled` AND connected AND in `MODE_NORMAL`; per-call frequency variation for realism
- Uses direct `digitalWrite()` GPIO pulses with microsecond timing (not `tone()`) for sharp percussive transients
- **Live preview**: non-blocking state machine plays continuous typing (80–180ms intervals) while editing sound type in menu; `startSoundPreview()`/`stopSoundPreview()`/`updateSoundPreview()` (called from main loop)
- Menu items: "Sound" (On/Off toggle), "Key sound" (profile selector, hidden when sound disabled)

#### 9. Config Protocol (BLE UART + USB Serial)
```
WEB → DEVICE                    DEVICE → WEB
────────────────────────────────────────────────
?status                     →   !status|connected=1|kb=1|ms=1|bat=85|...
?settings                   →   !settings|keyMin=2000|keyMax=6500|...
?keys                       →   !keys|F13|F14|F15|...|NONE
=keyMin:2000                →   +ok
=slots:2,28,28,28,28,28,28,28 → +ok
=mouseStyle:1               →   +ok
=btWhileUsb:1               →   +ok
=scroll:1                   →   +ok
=dashboard:1                →   +ok
=invertDial:1               →   +ok
=switchKeys:N               →   +ok
=jobStart:N                 →   +ok
=jobPerf:N                  →   +ok
=clickSlots:1,7,7,7,7,7,7  →   +ok
=sound:1                    →   +ok
=soundType:N                →   +ok
=encButton:N                →   +ok
=sideButton:N               →   +ok
=shiftDur:N                 →   +ok
=lunchDur:N                 →   +ok
=totalKeys:N                →   +ok
=totalMousePx:N             →   +ok
=totalClicks:N              →   +ok
=statusPush:1               →   +ok
=name:MyDevice              →   +ok
!save                       →   +ok (saves settings, sim data, and stats)
!defaults                   →   +ok
!reboot                     →   (device reboots)
!dfu                        →   +ok:dfu (then reboots into OTA DFU bootloader)
!serialdfu                  →   +ok:serialdfu (then reboots into Serial DFU bootloader)
```
- Transport-agnostic `processCommand(line, writer)` — accepts `ResponseWriter` function pointer
- Same text protocol works over BLE UART (NUS) and USB serial
- Text protocol: `?` = query, `=` = set, `!` = action, pipe-delimited responses
- Settings changes apply to in-memory struct immediately (like encoder); flash save on `!save`
- BLE path: 20-byte chunked writes via `bleWrite()` for default MTU compatibility
- Serial path: `serialWrite()` in `src/nrf52/serial_cmd.cpp` — line-buffered, dispatches `?/=/!` prefixed lines to `processCommand()`
- `pushSerialStatus()` proactively sends `?status` response on state changes (key sent, profile/mode toggle, mouse transition) — guarded by `serialStatusPush` flag (default OFF; dashboard enables via `=statusPush:1` on connect; serial `t` command toggles manually)
- NUS not added to advertising packet (would overflow 31 bytes); discovered via `optionalServices`
- Web dashboard (Vue 3 + Vite) in `dashboard/` connects via Chrome Web Serial API (USB)
- **DFU backup/restore:** The Seeed bootloader may erase the LittleFS region during DFU, wiping both `/settings.dat` and `/stats.dat`. The dashboard snapshots stats and settings to localStorage before sending `!serialdfu`, and auto-restores them on the first reconnection after DFU. Firmware also flushes stats/settings to flash before entering DFU mode. Stats restore uses `max(device, backup)` to avoid losing any data. Settings restore only triggers if device values match factory defaults.

#### 10. DFU (Firmware Updates)
- **IMPORTANT:** Must use `sd_power_gpregret_set()` (not `NRF_POWER->GPREGRET` directly) — SoftDevice owns the register; direct access causes a hard fault
- Bootloader checks `GPREGRET` on boot — magic value determines DFU mode
- Bootloader has NO timeout — stays in DFU mode until transfer completes or device is power cycled
- DFU packages generated by `adafruit-nrfutil dfu genpkg` (ZIP with manifest.json, .dat, .bin)

**Serial DFU (USB) — preferred method:**
- `!serialdfu` command (over BLE UART or USB serial) writes `0x4E` to `GPREGRET`, resets into USB CDC serial DFU mode
- Web dashboard "Firmware Update" section orchestrates the full flow: USB serial command → reboot → Web Serial API transfer
- Uses Nordic SDK 11 legacy HCI/SLIP serial DFU protocol (SLIP framing, 4-byte HCI header, CRC16-CCITT)
- Dashboard DFU library: `dashboard/src/lib/dfu/` (slip.js, crc16.js, serial.js, dfu.js, zip.js)
- OLED shows "USB DFU / Connect USB cable" screen before reboot
- Serial `u` command provides same Serial DFU entry for development/testing
- Also works via CLI: `adafruit-nrfutil dfu serial -pkg firmware.zip -p /dev/tty.usbmodem* -b 115200`

**OTA DFU (BLE) — alternative method:**
- `!dfu` BLE UART command writes `0xA8` to `GPREGRET`, resets into OTA BLE DFU mode
- Bootloader advertises as "AdaDFU" with legacy DFU service (`00001530-...`)
- OLED shows "OTA DFU / Waiting for update" screen before reboot
- Serial `f` command provides same OTA DFU entry for development/testing
- **Web Bluetooth not supported:** Chrome blocklists legacy DFU UUID — use nRF Connect mobile app or `adafruit-nrfutil dfu ble` instead

### Encoder Handling
- **EC11 bare encoder** — no onboard pull-ups; uses nRF52840 internal pull-ups via `INPUT_PULLUP` on D0, D1, D2 (configured in `setupPins()`)
- Hybrid ISR + polling architecture:
  - **Primary:** GPIOTE interrupt (`encoderISR`) catches every pin edge in real-time, including during blocking I2C display transfers
  - **Fallback:** `pollEncoder()` in main loop catches edges missed during SoftDevice radio blackouts
  - `noInterrupts()` around polling prevents race conditions with ISR (~300ns critical section, SoftDevice-safe)
- Interrupts attached AFTER SoftDevice init (`setupBLE()`) to prevent GPIOTE channel invalidation
- Atomic `NRF_P0->IN` register read samples both pins simultaneously (no race window)
- 2-step jump recovery: when intermediate quadrature states are missed, direction is inferred from last known movement
- 4 transitions per detent (divide by 4 for clicks)
- **IMPORTANT:** Do NOT use `analogRead()` on A0 or A1 — these share P0.02/P0.03 with encoder CLK/DT. `analogRead()` disconnects the digital input buffer, breaking encoder reads.

### Display Layout

See [docs/CLAUDE.md/display-layout.md](docs/CLAUDE.md/display-layout.md) for ASCII mockups of all UI modes (Normal, Menu, Slots, Name, Screensaver).

### Version History

See [docs/CLAUDE.md/version-history.md](docs/CLAUDE.md/version-history.md) for full changelog (v1.0.0 through v2.3.2).

---

## Build Configuration

### PlatformIO Configuration
- **Platform:** `nordicnrf52` with Seeed framework override (S140 v7.3.0)
- **Config file:** `platformio.ini` — declarative deps, board, and build flags
- **Custom board:** `boards/seeed_xiao_nrf52840.json` — not in PlatformIO registry; defines SoftDevice FWID `0x0123`, linker script `nrf52840_s140_v7.ld`, VID/PID `0x2886`/`0x8044`
- **Variant files:** `boards/variants/Seeed_XIAO_nRF52840/variant.h` + `variant.cpp` — pin mappings copied from the Seeed Arduino core v1.1.12
- **Framework override:** `platform_packages` points to `Seeed-Studio/Adafruit_nRF52_Arduino.git#1.1.12` — PlatformIO's built-in Adafruit core has S140 v6.1.1 (wrong SoftDevice)
- **Optimization:** `build_unflags = -Ofast` overrides PlatformIO's default; uses `-Os` to match Arduino IDE behavior. `-Ofast` can cause subtle issues with USB/BLE init.
- **`lib_archive = no`:** Critical — prevents PlatformIO from archiving libraries into `.a` files. TinyUSB uses indirect function pointer driver tables that the linker strips from archives. Without this, USB CDC/HID doesn't work.
- **`extra_script.py`:** Adds the CC310 crypto library path (`libnrf_cc310_0.9.13-no-interrupts.a`) that PlatformIO's framework builder doesn't include.
- **Settings magic number:** `SETTINGS_MAGIC` in `src/common/config.h` encodes the settings struct schema version. Bump it when the `Settings` struct layout changes to trigger a safe `loadDefaults()` reset instead of reading corrupt data from flash.
- **DFU ZIP generation:** The Seeed nRF52 toolchain automatically generates a `.zip` DFU package alongside the `.hex` during compilation (via `adafruit-nrfutil`). No extra build step required.
- **CI release:** Release workflow builds DFU firmware and creates a GitHub Release with the artifact.

### PlatformIO Board JSON Gotchas
The custom board JSON (`boards/seeed_xiao_nrf52840.json`) requires several fields that PlatformIO's `nordicnrf52` platform doesn't auto-derive from the framework. Missing any of these breaks the build or firmware:
- **`build.board`:** Must be `"Seeed_XIAO_nRF52840"` — generates `-DARDUINO_Seeed_XIAO_nRF52840`. The nordicnrf52 platform builder does NOT auto-generate this define (unlike other platforms). Without it, the Seeed Wire library fails with `#error "Unsupported board"`.
- **`build.extra_flags`:** Must include `-DUSE_TINYUSB` — PlatformIO's builder already adds this, but it also needs `-DARDUINO_Seeed_XIAO_nRF52840` here.
- **`build.bsp.name`:** Must be `"adafruit"` — routes to the Adafruit framework builder instead of the generic nRF5 builder.
- **`build.softdevice.sd_flags`:** Must be `"-DS140"` — SoftDevice preprocessor define for BLE API headers.
- **`build.usb_product`:** Must be present — triggers PlatformIO's USB VID/PID/manufacturer define injection. Without it, USB uses default Adafruit identifiers.
- **`upload.speed`:** Must be `115200` — PlatformIO passes this as `-b` to `adafruit-nrfutil`. If missing, the baudrate argument is empty and `--singlebank` gets parsed as the baudrate value.

### Environment Variables

No environment variables are used in local builds. Configuration is sourced entirely from `src/common/config.h` preprocessor defines and git tags.

**CI/CD (GitHub Actions) secrets:**

| Secret | Purpose |
|--------|---------|
| `GITHUB_TOKEN` | Default token for repo access and release creation (auto-provided by GitHub Actions) |

---

## Code Style

No formal linting or formatting tools are configured (no `.clang-format`, `.editorconfig`, `.eslintrc`, or `.prettierrc`). Style is maintained by following patterns in existing code.

**Cross-language conventions (C++ firmware and JS/Vue dashboard):**

| Aspect | Convention |
|--------|-----------|
| Indentation | 2 spaces (no tabs) |
| Runtime values/functions | `camelCase` |
| Constants/enums | `UPPER_SNAKE_CASE` |
| Struct/type names | `PascalCase` |
| Brace style | Opening brace on same line (K&R) |
| Line length | Soft limit ~100 chars |
| Comments | Pragmatic — explain *why*, not *what* |

**C++ firmware specifics:**
- Header guards: `#ifndef GHOST_[FILENAME]_H` / `#define GHOST_[FILENAME]_H` / `#endif`
- Include ordering: project headers first (quoted), then standard library, then third-party
- Prefer `uint8_t`/`uint16_t`/`uint32_t` over `int` for sized data
- `volatile` for ISR-accessed variables; `const` for immutable data
- Struct-based data (C idiom), not C++ classes
- `extern` declarations in headers; definitions in `.cpp`
- Game state macros: `gBrk` (breakout), `gSnk` (snake), `gRcr` (racer) — defined in `src/nrf52/state.h` as `#define gBrk (gameState.brk)` etc., accessing the shared `GameState` union

**Vue/JS dashboard specifics:**
- Vue 3 Composition API with `<script setup>` syntax
- Named exports preferred
- Scoped styles with `<style scoped>`
- CSS custom properties at `:root` for theming

For new contributions, follow the patterns in the largest modules (`src/nrf52/display.cpp`, `src/common/settings_common.cpp`, `src/nrf52/settings_nrf52.cpp`, `src/common/keys.cpp`, `App.vue`, `store.js`).

---

## External Integrations

### Web Bluetooth API (Dashboard)
- **What:** BLE connection to the nRF52840 via Nordic UART Service (NUS)
- **Loaded via:** Browser `navigator.bluetooth` API in `dashboard/src/lib/ble.js`
- **Lifecycle:** User-initiated device picker → GATT connection → characteristic notifications for receive, chunked writes for send
- **Gotchas:** NUS UUID not in BLE advertising packet (would overflow 31 bytes) — must be listed in `optionalServices` during `requestDevice()`. Chrome blocklists legacy DFU UUID, so Web Bluetooth cannot be used for OTA DFU.

### Web Serial API (Dashboard)
- **What:** USB CDC serial connection for configuration and DFU firmware updates
- **Loaded via:** Browser `navigator.serial` API in `dashboard/src/lib/serial.js` and `dashboard/src/lib/dfu/serial.js`
- **Lifecycle:** User-initiated port picker → 115200 baud connection → async read loop with line buffering
- **Gotchas:** DTR toggle is required after opening serial port for DFU mode — without it, the bootloader's USB CDC serial processing isn't initialized and larger packets will crash. Same USB port path is reused between app mode and bootloader mode.

### WebUSB (Firmware)
- **What:** Auto-launches the web dashboard when the device is plugged in via USB
- **Loaded via:** `Adafruit_USBD_WebUSB` class in `src/nrf52/ghost_operator.cpp`
- **Lifecycle:** Registered before USB stack init; Chrome navigates to landing URL on device connect
- **Environment gating:** Only active when `dashboardEnabled` setting is on. Smart default: auto-disables after 3 boots if user never interacts with it; any explicit toggle pins it permanently.
- **Landing URL:** `tarsindustrial.tech/ghost-operator/dashboard?welcome`

### fflate (NPM)
- **What:** ZIP decompression for parsing DFU firmware packages (`.zip` → `manifest.json` + `.dat` + `.bin`)
- **Loaded via:** `import { unzipSync } from 'fflate'` in `dashboard/src/lib/dfu/zip.js`
- **Gotchas:** Only production dependency besides Vue. Chosen for small bundle size and synchronous API.

---

## Known Issues / Limitations

1. **Battery reading approximate** - ADC calibration varies per chip
2. **No hardware power switch** - Sleep mode is the off switch
3. **Encoder must use Seeed nRF52 core** - mbed core incompatible with Bluefruit
4. **Custom board definition required** - The Seeed XIAO nRF52840 is not in PlatformIO's board registry. The custom board JSON in `boards/` and framework override in `platformio.ini` must be kept in sync with the Seeed Arduino core version. See "PlatformIO Board JSON Gotchas" in Build Configuration for required fields.
5. **`lib_archive = no` is mandatory** - PlatformIO's default library archiving strips TinyUSB's indirect driver table references, breaking USB CDC/HID. Do not remove this setting from `platformio.ini`.
5. **D0/D1 (A0/A1) are analog-capable** - `analogRead()` on these pins disconnects the digital input buffer, breaking encoder reads. Use `NRF_FICR->DEVICEADDR` or other sources for entropy.
6. **SoftDevice register access** - When the SoftDevice (BLE stack) is active, do NOT write directly to `NRF_POWER->GPREGRET` or other SoftDevice-owned registers. Use the `sd_power_*()` SVCall API instead. Direct access causes a hard fault. This affects `enterOTADfu()` from `wiring.h` — use `resetToDfu()` from `src/nrf52/ble_uart.h` instead.

---

## Development Rules (QA Hardening)

These rules exist to prevent classes of bugs found during firmware QA. Follow them for all new code and modifications.

### 1. Validate all external input at the storage boundary
Every value arriving over BLE UART or USB serial (`=key:value` commands) passes through `setSettingValue()`, which clamps to valid bounds using `clampVal()`. **Never** assign a protocol-supplied value to a setting field without bounds checking — the encoder UI path uses `MENU_ITEMS[].minVal/maxVal`, and the protocol path must be equally strict. When adding a new setting to `setSettingValue()`, always wrap the assignment with `clampVal(value, MIN, MAX)`.

### 2. Guard all array-indexed format lookups
`formatMenuValue()` uses setting values as direct indices into name arrays (`ANIM_NAMES[]`, `SAVER_NAMES[]`, `MOUSE_STYLE_NAMES[]`, etc.). Every such lookup must have a bounds check: `(val < COUNT) ? NAMES[val] : "???"`. This is defense-in-depth in case a corrupt or unvalidated value reaches the display path.

### 3. Reset connection-scoped state on disconnect
BLE UART and USB serial maintain line buffers (`uartBuf`, `serialBuf`) that accumulate bytes across reads. On BLE disconnect, `resetBleUartBuffer()` clears the buffer and overflow flag. Any new connection-scoped state (buffers, flags, session variables) must be reset in `disconnect_callback()` to prevent cross-session corruption.

### 4. Avoid Arduino `String` concatenation in hot paths
On the nRF52's 256KB RAM with no heap compaction, repeated `String +=` causes heap fragmentation that accumulates over weeks of uptime. Use `snprintf()` into stack-allocated `char[]` buffers for protocol responses (`cmdQueryStatus`, `cmdQuerySettings`) and any function called more than once per second. Reserve `String` for one-shot or display-only code where allocation lifetime is short.

### 5. Use symbolic constants for menu indices
Never hardcode `menuCursor = N` — use the `MENU_IDX_*` defines from `src/common/keys.h` (`MENU_IDX_KEY_SLOTS`, `MENU_IDX_SCHEDULE`, `MENU_IDX_BLE_IDENTITY`). These must match `MENU_ITEMS[]` order in `src/common/keys.cpp`. When reordering or inserting menu items, update both the array and the index defines together.

### 6. Throttle event-driven output
`pushSerialStatus()` has a 200ms minimum interval guard to prevent BLE stack saturation. Any new function that sends data in response to frequent events (keystroke, mouse transition, timer tick) must implement similar throttling. The BLE UART's 20-byte chunked writes amplify the cost — a single 200-byte status response requires ~10 BLE write operations.

### 7. Use `snprintf` over `sprintf`
Always use `snprintf(buf, sizeof(buf), ...)` instead of `sprintf(buf, ...)`, even when the buffer appears large enough. This prevents silent overflow if format arguments change in the future.

### 8. Report errors, don't silently truncate
Command buffers track an overflow flag (`uartBufOverflow`, `serialBufOverflow`). When a line exceeds the buffer, respond with `-err:cmd too long` instead of silently truncating and processing a partial command. Apply the same principle to any new protocol handler: always give the client actionable error feedback.

---

## Plan Pre-Implementation

Before planning, check `docs/CLAUDE.md/plans/` for prior plans that touched the same areas. Scan the **Files changed** lists in both `implementation.md` and `audit.md` files to find relevant plans without reading every file — then read the full `plan.md` only for matches. This keeps context window usage low while preserving access to project history.

When a plan is finalized and about to be implemented, write the full plan to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/plan.md`, where `{epoch}` is the Unix timestamp at the time of writing and `{plan_name}` is a short kebab-case description of the plan (e.g., `1709142000-add-user-auth/plan.md`).

The epoch prefix ensures chronological ordering — newer plans visibly supersede earlier ones at a glance based on directory name ordering.

The plan document should include:
- **Objective** — what is being implemented and why
- **Changes** — files to modify/create, with descriptions of each change
- **Dependencies** — any prerequisites or ordering constraints between changes
- **Risks / open questions** — anything flagged during planning that needs attention

---

## Plan Post-Implementation

After a plan has been fully implemented, write the completed implementation record to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/implementation.md`, using the same directory as the corresponding `plan.md`.

The implementation document **must** include:
- **Files changed** — list of all files created, modified, or deleted. This section is **required** — it serves as a lightweight index for future planning, allowing prior plans to be found by scanning file lists without reading full plan contents.
- **Summary** — what was actually implemented (noting any deviations from the plan)
- **Verification** — steps taken to verify the implementation is correct (tests run, manual checks, build confirmation)
- **Follow-ups** — any remaining work, known limitations, or future improvements identified during implementation

If the implementation added or changed user-facing behavior (new settings, UI modes, protocol commands, or display changes), add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

---

## Post-Implementation Audit

After finishing implementation of a plan, run the following subagents **in parallel** to audit all changed files.

> **Scope directive for all subagents:** Only flag issues in the changed code and its immediate dependents. Do not audit the entire codebase.

> **Output directive:** After all subagents complete, write a single consolidated audit report to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/audit.md`, using the same directory as the corresponding `plan.md`. The audit report **must** include a **Files changed** section listing all files where findings were flagged. This section is **required** — it serves as a lightweight index for future planning, covering files affected by audit findings (including immediate dependents not in the original implementation).

### 1. QA Audit (subagent)
Review changes for:
- **Functional correctness**: broken workflows, missing error/loading states, unreachable code paths, logic that doesn't match spec
- **Edge cases**: empty/null/undefined inputs, zero-length collections, off-by-one errors, race conditions, boundary values (min/max/overflow)
- **Infinite loops**: unbounded `while`/recursive calls, callbacks triggering themselves, retry logic without max attempts or backoff
- **Performance**: unnecessary computation in hot paths, O(n²) or worse in loops over growing data, unthrottled event handlers, expensive operations blocking main thread or interrupt context

### 2. Security Audit (subagent)
Review changes for:
- **Injection / input trust**: unsanitized external input used in commands, queries, or output rendering; format string vulnerabilities; untrusted data used in control flow
- **Overflows**: unbounded buffer writes, unguarded index access, integer overflow/underflow in arithmetic, unchecked size parameters
- **Memory leaks**: allocated resources not freed on all exit paths, event/interrupt handlers not deregistered on cleanup, growing caches or buffers without eviction or bounds
- **Hard crashes**: null/undefined dereferences without guards, unhandled exceptions in async or interrupt context, uncaught error propagation across module boundaries

### 3. Interface Contract Audit (subagent)
Review changes for:
- **Data shape mismatches**: caller assumptions that diverge from actual API/protocol schema, missing fields treated as present, incorrect type coercion or endianness
- **Error handling**: no distinction between recoverable and fatal errors, swallowed failures, missing retry/backoff on transient faults, no timeout or watchdog configuration
- **Auth / privilege flows**: credential or token lifecycle issues, missing permission checks, race conditions during handshake or session refresh
- **Data consistency**: optimistic state updates without rollback on failure, stale cache served after mutation, sequence counters or cursors not invalidated after writes

### 4. State Management Audit (subagent)
Review changes for:
- **Mutation discipline**: shared state modified outside designated update paths, state transitions that skip validation, side effects hidden inside getters or read operations
- **Reactivity / observation pitfalls**: mutable updates that bypass change detection or notification mechanisms, deeply nested state triggering unnecessary cascading updates
- **Data flow**: excessive pass-through of context across layers where a shared store or service belongs, sibling modules communicating via parent state mutation, event/signal spaghetti without cleanup
- **Sync issues**: local copies shadowing canonical state, multiple sources of truth for the same entity, concurrent writers without arbitration (locks, atomics, or message ordering)

### 5. Resource & Concurrency Audit (subagent)
Review changes for:
- **Concurrency**: data races on shared memory, missing locks/mutexes/atomics around critical sections, deadlock potential from lock ordering, priority inversion in RTOS or threaded contexts
- **Resource lifecycle**: file handles, sockets, DMA channels, or peripherals not released on error paths; double-free or use-after-free; resource exhaustion under sustained load
- **Timing**: assumptions about execution order without synchronization, spin-waits without yield or timeout, interrupt latency not accounted for in real-time constraints
- **Power & hardware**: peripherals left in active state after use, missing clock gating or sleep transitions, watchdog not fed on long operations, register access without volatile or memory barriers

### 6. Testing Coverage Audit (subagent)
Review changes for:
- **Missing tests**: new public functions/modules without corresponding unit tests, modified branching logic without updated assertions, deleted tests not replaced
- **Test quality**: assertions on implementation details instead of behavior, tests coupled to internal structure, mocked so heavily the test proves nothing
- **Integration gaps**: cross-module flows tested only with mocks and never with integration or contract tests, initialization/shutdown sequences untested, error injection paths uncovered
- **Flakiness risks**: tests dependent on timing or sleep, shared mutable state between test cases, non-deterministic data (random IDs, timestamps), hardware-dependent tests without abstraction layer

### 7. DX & Maintainability Audit (subagent)
Review changes for:
- **Readability**: functions exceeding ~50 lines, boolean parameters without named constants, magic numbers/strings without explanation, nested ternaries or conditionals deeper than one level
- **Dead code**: unused includes/imports, unreachable branches behind stale feature flags, commented-out blocks with no context, exported symbols with zero consumers
- **Naming & structure**: inconsistent naming conventions, business/domain logic buried in UI or driver layers, utility functions duplicated across modules
- **Documentation**: public API changes without updated doc comments, non-obvious workarounds missing a `// WHY:` comment, breaking changes without migration notes

---

## Audit Post-Implementation

After audit findings have been addressed, update the `implementation.md` file in the corresponding `docs/CLAUDE.md/plans/{epoch}-{plan_name}/` directory:

1. **Flag fixed items** — In the audit report (`docs/CLAUDE.md/plans/{epoch}-{plan_name}/audit.md`), mark each finding that was fixed with a `[FIXED]` prefix so it is visually distinct from unresolved items.

2. **Append a fixes summary** — Add an `## Audit Fixes` section at the end of `implementation.md` containing:
   - **Fixes applied** — a numbered list of each fix, referencing the audit finding it addresses (e.g., "Fixed unchecked index access flagged by Security Audit §2")
   - **Verification checklist** — a `- [ ]` checkbox list of specific tests or manual checks to confirm each fix is correct (e.g., "Verify bounds check on `configIndex` with out-of-range input returns fallback")

3. **Leave unresolved items as-is** — Any audit findings intentionally deferred or accepted as-is should remain unmarked in the audit report. Add a brief note in the fixes summary explaining why they were not addressed.

4. **Update testing checklist** — If any audit fixes changed user-facing behavior, add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

---

## Common Modifications

### Version bumps
Version string appears in 8 files: `src/common/config.h` (firmware `VERSION` define), `CLAUDE.md`, `CHANGELOG.md`, `README.md`, `App.vue`, `USER_MANUAL.md` (x2 — header + footer), `dashboard/package.json`, and `dashboard/package-lock.json` (x2).

**Dashboard version is kept in sync with the firmware version.** Always bump `App.vue`, `dashboard/package.json`, and `dashboard/package-lock.json` alongside the other version files during any version bump.

### Add a new keystroke option
1. Add entry to `AVAILABLE_KEYS[]` array in `src/common/keys.cpp`
2. Include HID keycode and display name
3. Set `isModifier` flag appropriately
4. Update `NUM_KEYS` in `src/common/config.h`
5. Add 3-char short name to `SHORT_NAMES[]` in `slotName()` function in `src/nrf52/display.cpp`

### Change timing range
Modify these defines in `src/common/config.h`:
```cpp
#define VALUE_MIN_MS          500UL    // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL  // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL  // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL    // 0.5 second steps
```

### Change mouse randomness
In `src/common/config.h`:
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

### Change mouse amplitude range
Modify `MENU_ITEMS[]` entry for `SET_MOUSE_AMP` in `src/common/keys.cpp` (minVal/maxVal currently 1–5). Only applies to Brownian mode — Bezier uses random sweep radius and ignores `mouseAmplitude`. The JIGGLING case in `src/common/mouse.cpp` applies `mouseAmplitude` per-step via sine easing (`amp = mouseAmplitude * sin(π × progress)`). `pickNewDirection()` stores unit vectors only (-1/0/+1). The return phase clamps at `min(5, remaining)` per axis, so amplitudes above 5 would require updating the return logic.

### Add new menu setting
1. Add `SettingId` enum entry in `src/common/config.h`
2. Add field to `Settings` struct in `src/common/config.h` (before `checksum`)
3. Set default in `loadDefaults()` in `src/common/settings_common.cpp`, add bounds check in `loadSettings()` in `src/nrf52/settings_nrf52.cpp`
4. Add `MenuItem` entry to `MENU_ITEMS[]` array in `src/common/keys.cpp` (update `MENU_ITEM_COUNT` in `src/common/config.h`)
5. Add cases to `getSettingValue()` and `setSettingValue()` in `src/common/settings_common.cpp` — **use `clampVal()` in `setSettingValue()`** to enforce bounds
6. If the setting uses an array-indexed format (e.g., `FMT_ANIM_NAME`), add a bounds guard in `formatMenuValue()`: `(val < COUNT) ? NAMES[val] : "???"`
7. If the new item changes existing `MENU_ITEMS[]` positions, update `MENU_IDX_*` defines in `src/common/keys.h`
8. If adding a protocol `=key:value` command, add the case in `cmdSetValue()` in `src/nrf52/ble_uart.cpp`
9. `calcChecksum()` auto-adapts (loops `sizeof(Settings) - 1`)

### Change device name
Configurable via menu: Device → "Device name" action item opens a character editor (MODE_NAME). Max 14 characters, A-Z, a-z, 0-9, space, dash, underscore. Requires reboot to apply. The compile-time default is:
```cpp
#define DEVICE_NAME "GhostOperator"
```

---

## File Inventory

| File | Purpose |
|------|---------|
| `src/common/config.h` | Constants, enums, structs (header-only) |
| `src/common/keys.h` / `keys.cpp` | Const data tables (keys, menu items, names) |
| `src/common/timing.h` / `timing.cpp` | Profiles, scheduling, formatting |
| `src/common/mouse.h` / `mouse.cpp` | Mouse state machine with sine easing |
| `src/common/schedule.h` / `schedule.cpp` | Timed schedule (shared logic) |
| `src/common/orchestrator.h` / `orchestrator.cpp` | Simulation activity orchestrator |
| `src/common/sim_data.h` / `sim_data.cpp` | Simulation data tables (job templates, work modes, phase timing) |
| `src/common/settings.h`, `settings_pure.h` / `settings_common.cpp` | Shared settings API (`loadDefaults()`, getters/setters, `formatMenuValue()`) |
| `src/common/state.h` | Portable globals |
| `src/nrf52/ghost_operator.cpp` | Entry point: setup(), loop(), BLE + USB HID setup/callbacks |
| `src/nrf52/settings_nrf52.cpp` | Flash-backed `loadSettings()` / `saveSettings()` |
| `src/nrf52/schedule_nrf52.cpp` | nRF52 schedule hooks |
| `src/nrf52/sim_data_flash.cpp` | Flash-backed simulation data |
| `src/nrf52/state.h` / `state.cpp` | nRF52 globals, hardware handles, game macros |
| `src/nrf52/icons.h` / `icons.cpp` | PROGMEM bitmaps (splash, BT icon, USB icon, arrows) |
| `src/nrf52/encoder.h` / `encoder.cpp` | ISR + polling quadrature decode |
| `src/nrf52/battery.h` / `battery.cpp` | ADC battery reading |
| `src/nrf52/hid.h` / `hid.cpp` | Keystroke + mouse + scroll sending (BLE + USB dual-transport) |
| `src/nrf52/sleep.h` / `sleep.cpp` | Deep sleep sequence |
| `src/nrf52/screenshot.h` / `screenshot.cpp` | PNG encoder + base64 serial output |
| `src/nrf52/serial_cmd.h` / `serial_cmd.cpp` | Serial debug commands + status |
| `src/nrf52/input.h` / `input.cpp` | Encoder dispatch, buttons, name editor |
| `src/nrf52/display.h` / `display.cpp` | All rendering (~2900 lines, largest module) |
| `src/nrf52/ble_uart.h` / `ble_uart.cpp` | BLE UART (NUS) + transport-agnostic config protocol |
| `src/nrf52/sound.h` / `sound.cpp` | Piezo buzzer keyboard sound profiles (5 types) |
| `src/nrf52/protocol.h` / `protocol.cpp` | JSON config protocol (ArduinoJson; `processCommand` lines starting with `{`) |
| `src/nrf52/breakout.h` / `breakout.cpp` | Breakout arcade game |
| `src/nrf52/snake.h` / `snake.cpp` | Classic snake game |
| `src/nrf52/racer.h` / `racer.cpp` | Ghost Racer side-scrolling racing game |
| `src/esp32-s3-lcd-1.47/` | ESP32-S3 + LCD firmware (`s3lcd` env); entry `main.cpp` |
| `src/esp32-c6-lcd-1.47/` | ESP32-C6 + LCD firmware (`c6lcd` env); entry `main.cpp` |
| `platformio.ini` | PlatformIO build configuration (board, deps, flags) |
| `extra_script.py` | PlatformIO build hook (adds CC310 crypto library path) |
| `boards/seeed_xiao_nrf52840.json` | Custom PlatformIO board definition (SoftDevice S140 v7.3.0) |
| `boards/variants/Seeed_XIAO_nRF52840/` | Board variant files (pin mappings, init) |
| `dashboard/` | Vue 3 web dashboard (Vite + Web Serial config + Web Serial DFU) |
| `dashboard/src/lib/serial.js` | Web Serial config transport (same API as `ble.js`) |
| `dashboard/src/lib/dfu/` | Web Serial DFU library (SLIP, CRC16, serial transport, protocol, ZIP parser) |
| `schematic_v8.svg` | Circuit schematic |
| `schematic_interactive_v3.html` | Interactive documentation |
| `README.md` | Technical documentation |
| `docs/USER_MANUAL.md` | End-user guide |
| `CHANGELOG.md` | Version history (semver) |
| `CLAUDE.md` | This file |
| `docs/CLAUDE.md/` | Reference docs (display-layout, testing-checklist, future-improvements, version-history) |
| `docs/CLAUDE.md/plans/` | Plan, implementation, and audit records (epoch-prefixed directories for chronological ordering) |
| `docs/images/` | OLED screenshot PNGs for README |
| `build.sh` | Build automation: compile, setup, release (DFU ZIP), flash |
| `Makefile` | Convenience targets wrapping build.sh + PlatformIO |
| `.github/workflows/release.yml` | CI/CD: build + GitHub Release on `v*` tag push |

---

## Build Instructions

### PlatformIO CLI (recommended)
```bash
make setup    # Install PlatformIO + adafruit-nrfutil
make build    # Compile firmware (auto-installs board + libs on first build)
make flash    # Compile + flash via USB serial DFU
make release  # Compile + create versioned DFU ZIP in releases/
make monitor  # Open serial monitor at 115200 baud
make clean    # Remove build artifacts
```

PlatformIO auto-downloads the Seeed nRF52 framework and library dependencies on first build. No manual board/library installation needed.

### PlatformIO IDE
1. Install the PlatformIO IDE extension for VS Code
2. Open the project folder — PlatformIO detects `platformio.ini` automatically
3. Use the PlatformIO toolbar to build, upload, and monitor

### Troubleshooting Build
- **First build slow** → PlatformIO downloads the framework + toolchain (~200MB); subsequent builds are cached
- **"Board not found"** → Ensure `boards/seeed_xiao_nrf52840.json` exists; PlatformIO searches `boards/` automatically
- **SoftDevice mismatch** → Verify `platform_packages` in `platformio.ini` points to Seeed fork `#1.1.12`, not Adafruit upstream
- **USB not enumerating after flash** → Verify `lib_archive = no` in `platformio.ini`; without it, TinyUSB driver code gets stripped. Recovery: double-reset into UF2 bootloader, flash known-good firmware
- **`#error "Unsupported board"`** → Board JSON missing `build.board` or `extra_flags` missing `-DARDUINO_Seeed_XIAO_nRF52840`
- **Linker: `cannot find -lnrf_cc310`** → `extra_scripts = extra_script.py` missing from `platformio.ini`
- **Upload: `--singlebank is not a valid integer`** → Board JSON missing `upload.speed: 115200`

---

## Testing

See [docs/CLAUDE.md/testing-checklist.md](docs/CLAUDE.md/testing-checklist.md) for the full QA testing checklist (~100 items covering all UI modes, settings, BLE, display, and serial commands).

---

## Future Improvements (Ideas)

See [docs/CLAUDE.md/future-improvements.md](docs/CLAUDE.md/future-improvements.md) for the ideas backlog and completed items.

---

## Serial Debug Commands

At 115200 baud:
| Key | Action |
|-----|--------|
| h | Help |
| s | Status report |
| d | Dump settings |
| z | Sleep |
| p | PNG screenshot (base64-encoded between `--- PNG START ---` / `--- PNG END ---` markers) |
| v | Screensaver (activate instantly, forces NORMAL mode first) |
| t | Toggle status push (real-time `!status` lines on state changes, default OFF) |
| e | Easter egg (trigger animation immediately) |
| f | Enter OTA DFU bootloader mode (writes 0xA8 to GPREGRET, resets) |
| u | Enter Serial DFU bootloader mode (writes 0x4E to GPREGRET, resets — USB CDC) |

---

## Maintaining This File

### When to update CLAUDE.md
- **Adding a new subsystem or module** — add it to Architecture and File Inventory
- **Adding a new setting or config field** — update the Settings section and Common Modifications
- **Discovering a new bug class** — add a Development Rule to prevent recurrence
- **Changing the build process** — update Build Instructions and/or Build Configuration
- **Changing linting or style rules** — update Code Style
- **Integrating a new third-party service or SDK** — add to External Integrations
- **Bumping the version** — update the version in Project Overview
- **Adding/removing files** — update File Inventory
- **Finding a new limitation** — add to Known Issues

### Supplementary docs
For sections that grow large (display layouts, testing checklists, changelogs), move them to separate files under `docs/CLAUDE.md/` and link from here. This keeps the main CLAUDE.md scannable while preserving detail.

### Future improvements tracking
When a new feature is added and related enhancements or follow-up ideas are suggested but declined, add them as `- [ ]` items to `docs/CLAUDE.md/future-improvements.md`. This preserves good ideas for later without cluttering the current task.

### Version history maintenance
When making changes that are committed to the repository, add a row to the version history table in `docs/CLAUDE.md/version-history.md`. Each entry should include:

- **Ver** — A semantic version identifier (e.g., `v2.4.0`). Follow semver: MAJOR.MINOR.PATCH. Use the most recent entry in the table to determine the next version number.
- **Changes** — A brief summary of what changed.

Append new rows to the bottom of the table. Do not remove or rewrite existing entries.

### Testing checklist maintenance
When adding or modifying user-facing behavior (new settings, UI modes, protocol commands, or display changes), add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

### What belongs here vs. in code comments
- **Here:** Architecture decisions, cross-cutting concerns, "how things fit together," gotchas, recipes
- **In code:** Implementation details, function-level docs, inline explanations of tricky logic

---

## Origin

Created with Claude (Anthropic)

*"Fewer parts, more flash"*

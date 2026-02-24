# CLAUDE.md - Ghost Operator Project Context

## Project Overview

**Ghost Operator** is a BLE keyboard/mouse hardware device built on the Seeed XIAO nRF52840. It prevents screen lock and idle timeout by sending periodic keystrokes and mouse movements over Bluetooth.

**Current Version:** 2.2.0
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
| BT1 | LiPo 3.7V 1000mAh | Power |
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

## Firmware Architecture

### Core Files
Modular architecture — 20 `.h/.cpp` module pairs + lean `.ino` entry point:

- `ghost_operator.ino` - Entry point: setup(), loop(), BLE + USB HID setup/callbacks
- `config.h` - All `#define` constants, enums, structs (header-only)
- `keys.h/.cpp` - Const data tables (AVAILABLE_KEYS, MENU_ITEMS, names)
- `icons.h/.cpp` - PROGMEM bitmaps (splash, BT icon, USB icon, arrows)
- `state.h/.cpp` - All ~60 mutable globals as `extern` declarations
- `settings.h/.cpp` - Flash persistence + value accessors
- `timing.h/.cpp` - Profiles, scheduling, formatting
- `encoder.h/.cpp` - ISR + polling quadrature decode
- `battery.h/.cpp` - ADC battery reading
- `hid.h/.cpp` - Keystroke + mouse + scroll sending (BLE + USB dual-transport)
- `mouse.h/.cpp` - Mouse state machine with sine easing
- `sleep.h/.cpp` - Deep sleep sequence
- `screenshot.h/.cpp` - PNG encoder + base64 serial output
- `serial_cmd.h/.cpp` - Serial debug commands + status
- `input.h/.cpp` - Encoder dispatch, buttons, name editor
- `display.h/.cpp` - All rendering (dirty flag, shadow buffer page redraw, ~800 lines, largest module)
- `ble_uart.h/.cpp` - BLE UART (NUS) + transport-agnostic config protocol
- `sound.h/.cpp` - Piezo buzzer keyboard sound profiles (5 types) with live preview
- `orchestrator.h/.cpp` - Simulation activity orchestrator (phase cycling, mutual exclusion, job performance scaling)
- `sim_data.h/.cpp` - Simulation data tables (job templates, work modes, phase timing)
- `schedule.h/.cpp` - Timed schedule (auto-sleep/full auto, light/deep sleep, time sync)

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
- Display shows USB trident icon when wired, BLE icon when wireless
- Jiggler runs when either BLE or USB is connected (disabled in Volume Control mode)

#### 2. UI Modes
```cpp
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_DECOY, MODE_SCHEDULE, MODE_MODE, MODE_SET_CLOCK, MODE_COUNT };
```
- **NORMAL**: Live status; encoder switches profile (Simple), adjusts job performance (Simulation), or sends volume up/down (Volume Control); button cycles KB/MS combos (Simple/Simulation) or toggles mute (Volume Control)
- **MENU**: Scrollable settings menu; encoder navigates/edits, button selects/confirms
- **SLOTS**: 8-key slot editor; encoder cycles key, button advances slot
- **NAME**: Device name editor; encoder cycles character, button advances position
- **DECOY**: BLE identity picker; encoder navigates presets (with active marker `*`), button selects, reboot confirmation on change
- **SCHEDULE**: Schedule editor; 3-row layout (Mode/Start/End) with contextual help; rows hidden based on mode selection
- **MODE**: Operation mode picker (Simple/Simulation/Volume Control) as horizontal carousel with smooth scrolling and reboot confirmation
- **SET_CLOCK**: Manual time editor; encoder cycles hour/minute values, button advances field, function button confirms and calls `syncTime()`; pre-fills from current time if synced
- Function button toggles NORMAL ↔ MENU; from SLOTS/NAME/DECOY/SCHEDULE/MODE/SET_CLOCK returns to MENU
- 30-second timeout returns to NORMAL from MENU, SLOTS, or NAME

#### 2a. Menu System
Data-driven architecture using `MenuItem` struct array (47 entries: 10 headings + 37 items):
```cpp
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME, FMT_MOUSE_STYLE, FMT_ON_OFF, FMT_SCHEDULE_MODE, FMT_TIME_5MIN, FMT_UPTIME, FMT_DIE_TEMP, FMT_OP_MODE, FMT_JOB_SIM, FMT_SWITCH_KEYS, FMT_HEADER_DISP, FMT_CLICK_TYPE, FMT_KEY_SOUND, FMT_PERF_LEVEL, FMT_VOLUME_THEME };
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
  uint32_t magic;              // 0x50524F57 (bumped: +volumeTheme)
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
  uint8_t operationMode;       // 0=Simple (default), 1=Simulation, 2=Volume Control
  uint8_t jobSimulation;       // 0=Staff, 1=Developer, 2=Designer (default: 0)
  uint8_t jobPerformance;      // 0-11, default 5 (level*10 = percentage, 5=baseline)
  uint16_t jobStartTime;       // 0-287 (5-min slots), default 96 (8:00)
  uint8_t phantomClicks;       // 0=Off (default), 1=On
  uint8_t clickType;           // 0=Middle (default), 1=Left
  uint8_t windowSwitching;     // 0=Off (default), 1=On
  uint8_t switchKeys;          // 0=Alt-Tab (default), 1=Cmd-Tab
  uint8_t headerDisplay;       // 0=Job sim name (default), 1=Device name
  uint8_t soundEnabled;        // 0=Off (default), 1=On
  uint8_t soundType;           // 0=MX Blue, 1=MX Brown, 2=Membrane, 3=Buckling, 4=Thock
  // Volume control settings
  uint8_t volumeTheme;         // 0=Basic (default), 1=Retro, 2=Futuristic
  uint8_t checksum;            // must remain last
};

enum SwitchKeys { SWITCH_KEYS_ALT_TAB, SWITCH_KEYS_CMD_TAB, SWITCH_KEYS_COUNT };
```
Saved to `/settings.dat` via LittleFS. Survives sleep and power-off.
Default: slot 0 = F16 (index 3), slots 1-7 = NONE (index 28), lazy/busy = 15%, screensaver = Never, saver brightness = 20%, display brightness = 80%, mouse amplitude = 1px, mouse style = Bezier, animation = Ghost, device name = "GhostOperator", BT while USB = Off, scroll = Off, dashboard = On (smart default: auto-disables after 3 boots if user never touches it; any explicit toggle pins it permanently), invert dial = Off, operation mode = Simple, job simulation = Staff, job performance = 5 (baseline), job start time = 8:00 (96), phantom clicks = Off, click type = Middle, window switching = Off, switchKeys = Alt-Tab (0), header display = Job sim name, sound = Off, sound type = MX Blue, volume theme = Basic.

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
=clickType:N                →   +ok
=sound:1                    →   +ok
=soundType:N                →   +ok
=statusPush:1               →   +ok
=name:MyDevice              →   +ok
!save                       →   +ok
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
- Serial path: `serialWrite()` in `serial_cmd.cpp` — line-buffered, dispatches `?/=/!` prefixed lines to `processCommand()`
- `pushSerialStatus()` proactively sends `?status` response on state changes (key sent, profile/mode toggle, mouse transition) — guarded by `serialStatusPush` flag (default OFF; dashboard enables via `=statusPush:1` on connect; serial `t` command toggles manually)
- NUS not added to advertising packet (would overflow 31 bytes); discovered via `optionalServices`
- Web dashboard (Vue 3 + Vite) in `dashboard/` connects via Chrome Web Serial API (USB)

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

---

## Display Layout

See [docs/CLAUDE.md/display-layout.md](docs/CLAUDE.md/display-layout.md) for ASCII mockups of all UI modes (Normal, Menu, Slots, Name, Screensaver).

---

## Version History

See [docs/CLAUDE.md/version-history.md](docs/CLAUDE.md/version-history.md) for full changelog (v1.0.0 through v2.2.0).

---

## Known Issues / Limitations

1. **Battery reading approximate** - ADC calibration varies per chip
2. **No hardware power switch** - Sleep mode is the off switch
3. **Encoder must use Seeed nRF52 core** - mbed core incompatible with Bluefruit
4. **Python required for compilation** - Seeed toolchain dependency
5. **AR_INTERNAL_3_6 not available** - Use AR_INTERNAL_3_0 on Seeed core
6. **D0/D1 (A0/A1) are analog-capable** - `analogRead()` on these pins disconnects the digital input buffer, breaking encoder reads. Use `NRF_FICR->DEVICEADDR` or other sources for entropy.
7. **SoftDevice register access** - When the SoftDevice (BLE stack) is active, do NOT write directly to `NRF_POWER->GPREGRET` or other SoftDevice-owned registers. Use the `sd_power_*()` SVCall API instead. Direct access causes a hard fault. This affects `enterOTADfu()` from `wiring.h` — use `resetToDfu()` from `ble_uart.h` instead.

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
Never hardcode `menuCursor = N` — use the `MENU_IDX_*` defines from `keys.h` (`MENU_IDX_KEY_SLOTS`, `MENU_IDX_SCHEDULE`, `MENU_IDX_BLE_IDENTITY`). These must match `MENU_ITEMS[]` order in `keys.cpp`. When reordering or inserting menu items, update both the array and the index defines together.

### 6. Throttle event-driven output
`pushSerialStatus()` has a 200ms minimum interval guard to prevent BLE stack saturation. Any new function that sends data in response to frequent events (keystroke, mouse transition, timer tick) must implement similar throttling. The BLE UART's 20-byte chunked writes amplify the cost — a single 200-byte status response requires ~10 BLE write operations.

### 7. Use `snprintf` over `sprintf`
Always use `snprintf(buf, sizeof(buf), ...)` instead of `sprintf(buf, ...)`, even when the buffer appears large enough. This prevents silent overflow if format arguments change in the future.

### 8. Report errors, don't silently truncate
Command buffers track an overflow flag (`uartBufOverflow`, `serialBufOverflow`). When a line exceeds the buffer, respond with `-err:cmd too long` instead of silently truncating and processing a partial command. Apply the same principle to any new protocol handler: always give the client actionable error feedback.

---

## Common Modifications

### Version bumps
Version string appears in 8 files: `config.h` (firmware `VERSION` define), `CLAUDE.md`, `CHANGELOG.md`, `README.md`, `App.vue`, `USER_MANUAL.md` (x2 — header + footer), `dashboard/package.json`, and `dashboard/package-lock.json` (x2).

**Dashboard version is kept in sync with the firmware version.** Always bump `App.vue`, `dashboard/package.json`, and `dashboard/package-lock.json` alongside the other version files during any version bump.

### Add a new keystroke option
1. Add entry to `AVAILABLE_KEYS[]` array in `keys.cpp`
2. Include HID keycode and display name
3. Set `isModifier` flag appropriately
4. Update `NUM_KEYS` in `config.h`
5. Add 3-char short name to `SHORT_NAMES[]` in `slotName()` function in `display.cpp`

### Change timing range
Modify these defines in `config.h`:
```cpp
#define VALUE_MIN_MS          500UL    // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL  // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL  // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL    // 0.5 second steps
```

### Change mouse randomness
In `config.h`:
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

### Change mouse amplitude range
Modify `MENU_ITEMS[]` entry for `SET_MOUSE_AMP` in `keys.cpp` (minVal/maxVal currently 1–5). Only applies to Brownian mode — Bezier uses random sweep radius and ignores `mouseAmplitude`. The JIGGLING case in `mouse.cpp` applies `mouseAmplitude` per-step via sine easing (`amp = mouseAmplitude * sin(π × progress)`). `pickNewDirection()` stores unit vectors only (-1/0/+1). The return phase clamps at `min(5, remaining)` per axis, so amplitudes above 5 would require updating the return logic.

### Add new menu setting
1. Add `SettingId` enum entry in `config.h`
2. Add field to `Settings` struct in `config.h` (before `checksum`)
3. Set default in `loadDefaults()` in `settings.cpp`, add bounds check in `loadSettings()`
4. Add `MenuItem` entry to `MENU_ITEMS[]` array in `keys.cpp` (update `MENU_ITEM_COUNT` in `config.h`)
5. Add cases to `getSettingValue()` and `setSettingValue()` in `settings.cpp` — **use `clampVal()` in `setSettingValue()`** to enforce bounds
6. If the setting uses an array-indexed format (e.g., `FMT_ANIM_NAME`), add a bounds guard in `formatMenuValue()`: `(val < COUNT) ? NAMES[val] : "???"`
7. If the new item changes existing `MENU_ITEMS[]` positions, update `MENU_IDX_*` defines in `keys.h`
8. If adding a protocol `=key:value` command, add the case in `cmdSetValue()` in `ble_uart.cpp`
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
| `ghost_operator.ino` | Entry point: setup(), loop(), BLE + USB HID setup/callbacks |
| `config.h` | Constants, enums, structs (header-only) |
| `keys.h/.cpp` | Const data tables (keys, menu items, names) |
| `icons.h/.cpp` | PROGMEM bitmaps (splash, BT icon, USB icon, arrows) |
| `state.h/.cpp` | All mutable globals (extern declarations) |
| `settings.h/.cpp` | Flash persistence + value accessors |
| `timing.h/.cpp` | Profiles, scheduling, formatting |
| `encoder.h/.cpp` | ISR + polling quadrature decode |
| `battery.h/.cpp` | ADC battery reading |
| `hid.h/.cpp` | Keystroke + mouse + scroll sending (BLE + USB dual-transport) |
| `mouse.h/.cpp` | Mouse state machine with sine easing |
| `sleep.h/.cpp` | Deep sleep sequence |
| `screenshot.h/.cpp` | PNG encoder + base64 serial output |
| `serial_cmd.h/.cpp` | Serial debug commands + status |
| `input.h/.cpp` | Encoder dispatch, buttons, name editor |
| `display.h/.cpp` | All rendering (~800 lines, largest module) |
| `ble_uart.h/.cpp` | BLE UART (NUS) + transport-agnostic config protocol |
| `sound.h/.cpp` | Piezo buzzer keyboard sound profiles (5 types) |
| `orchestrator.h/.cpp` | Simulation activity orchestrator (phase cycling, mutual exclusion) |
| `sim_data.h/.cpp` | Simulation data tables (job templates, work modes, phase timing) |
| `schedule.h/.cpp` | Timed schedule (auto-sleep/full auto, light/deep sleep, time sync) |
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
| `docs/images/` | OLED screenshot PNGs for README |
| `build.sh` | Build automation: compile, setup, release (DFU ZIP), flash |
| `Makefile` | Convenience targets wrapping build.sh |
| `.github/workflows/release.yml` | CI/CD: build + GitHub Release on `v*` tag push |
| `ghost_operator_splash.bin` | Splash screen bitmap (128x64, 1-bit raw) |

---

## Build Instructions

### Arduino IDE
1. Add board URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
2. Install "Seeed nRF52 Boards" from Board Manager
3. Install libraries: Adafruit GFX, Adafruit SSD1306
4. Select board: Seeed nRF52 Boards → Seeed XIAO nRF52840
5. Select port and upload

### Command Line (build.sh / Makefile)
```bash
make setup    # Install arduino-cli, Seeed nRF52 board, required libraries
make build    # Compile firmware
make flash    # Compile + flash via USB serial DFU
make release  # Compile + create versioned DFU ZIP in releases/
```

### Building in a Git Worktree
Arduino CLI requires the sketch directory name to match the `.ino` filename. Worktrees (e.g., `.claude/worktrees/bugfixes/`) have the wrong directory name, so `make build` and `./build.sh` will fail with `main file missing from sketch`. Use a symlink instead:
```bash
ln -sfn "$(pwd)" /tmp/ghost_operator && arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 /tmp/ghost_operator
```
**Do NOT** attempt `make build` in a worktree — go straight to the symlink approach.

### Troubleshooting Build
- **"python not found"** → `sudo ln -s $(which python3) /usr/local/bin/python`
- **"AR_INTERNAL_3_6 not declared"** → Change to `AR_INTERNAL_3_0`
- **Folder name has spaces** → Rename to use underscores

---

## Testing Checklist

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

## Origin

Created with Claude (Anthropic)

*"Fewer parts, more flash"*

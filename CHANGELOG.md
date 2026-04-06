# Changelog

All notable changes to Ghost Operator will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.5.4] - 2026-04-06

### Added

- **ESP32 OTA firmware updates** — In-browser firmware updates for ESP32 C6 and S3 via web dashboard USB serial. Uses Arduino `Update` library with A/B OTA partitions for safe rollback. No port re-selection needed (transfers on existing serial connection)
- **ESP32 `!ota` serial command** — Triggers OTA receive mode over USB serial; rejects BLE with clear error. Old `!dfu`/`!serialdfu` commands updated to suggest `!ota`

### Fixed

- **S3 platform detection** — Dashboard now correctly detects ESP32-S3 as `'s3'` platform (was falling through to `'nrf52'`). S3 now uses JSON protocol as intended

## [2.5.3] - 2026-04-05

### Added

- **ESP32-C6 LCD 1.47 port** — Full firmware port to Waveshare ESP32-C6-LCD-1.47 with LVGL color UI, NimBLE BLE stack, animated ghost splash screen with shimmer effect, serial BMP screenshot support, and USB CDC serial
- **ESP32-S3 LCD 1.47 port** — Full firmware port to Waveshare ESP32-S3-LCD-1.47 with dual-transport USB+BLE HID (USB-A OTG), LVGL display, NimBLE
- **JSON protocol (nRF52)** — ArduinoJson-based config protocol alongside existing text protocol for ESP32 LCD platforms
- **DFU backup/restore** — Dashboard snapshots stats and settings to localStorage before Serial DFU; auto-restores on first reconnection after update. Firmware flushes stats/settings to flash before entering DFU mode
- **Testing infrastructure** — PlatformIO `native` test environment with Unity framework; unit tests for mouse (Bezier/Brownian drift, return-to-origin), settings (clamp, checksum), DFU (CRC16, HCI packets, ZIP parsing), and protocol (store `handleLine`, `buildSet`)
- **CI test runner** — GitHub Actions workflow runs native tests and multi-environment builds (nRF52, C6, S3)
- **Screenshot tooling** — `tools/screenshot-c6.sh` for capturing ESP32-C6 serial BMP screenshots; `tools/gen_splash_sprite.py` for generating splash screen sprite data

### Changed

- **Multi-platform architecture** — Refactored firmware into `src/common/` (portable logic), `src/nrf52/` (nRF52-specific), `src/esp32-c6-lcd-1.47/`, and `src/esp32-s3-lcd-1.47/`. PlatformIO `build_src_filter` selects per-environment source trees
- **Build system** — Updated `build.sh` and `Makefile` for new `src/common/` + `src/nrf52/` layout; fixed VERSION path extraction for release script

### Fixed

- **nRF52 build errors** — Forward declarations for `dualKeyboardReport()`/`trackBleNotify()` in `hid.cpp`; removed non-existent `BLESecurity::setSecureConn()` call (SoftDevice negotiates LESC automatically)
- **C6 build errors** — Missing `volatile` on `mouseState` definition; implicit narrowing in BMP header writes
- **Dashboard dev server** — Installed missing `vitest` dependency breaking `vite.config.js` import
- **DFU integrity hardening** — CRC16 validation no longer skippable; ZIP manifest filename validation; HCI ACK sequence/integrity checks; `sendPacket()` error propagation instead of silent reset
- **Protocol input validation** — `parseInt()` with explicit radix 10; `buildSet()` rejects empty/space-only input; name field rejects non-printable ASCII and pipe characters; JSON parse errors surfaced instead of swallowed
- **Stats monotonic enforcement** — `=totalKeys`/`=totalMousePx`/`=totalClicks` protocol commands enforce `max(current, incoming)` to prevent rollback
- **Settings checksum** — Replaced XOR checksum with CRC8 for better flash corruption detection
- **BLE security** — Protocol commands require BLE pairing; serial debug output no longer echoes full client-controlled BLE UART input; device name sanitized in logs
- **Flash write safety** — LittleFS write-then-rename pattern to prevent data loss on power failure during settings/stats save
- **Dashboard connection state** — Fixed stale "Saving..." on disconnect; proper port close on serial disconnect callback; transport object cleanup on reconnect; connection timeout handling
- **Dashboard store.js** — Fixed `parseInt` without radix; `displayFlip` reactive default for ESP JSON protocol; 4 additional state management fixes
- **Mouse state** — Removed dead `mouseReturnTotal` variable; `MouseState` marked volatile for ISR safety across all platforms
- **BLE UART** — Buffer overflow tracking on disconnect; 3 additional protocol handler fixes
- **Simulation timing** — `jsonQueryWorkMode` correct timing data generation; work mode parameter cross-field validation
- **Sound module** — `systemSoundEnabled` bounds check on flash load to prevent corrupt values
- **npm audit** — Updated dashboard dependencies to resolve known vulnerabilities

## [2.5.2] - 2026-04-01

### Fixed

- **Mouse clicks lifetime stat undercounted** — `sendMouseScroll()` was not incrementing `totalMouseClicks`, causing all scroll actions (simple mode random scroll and simulation mode phantom scroll slots) to go uncounted. Only actual button-press clicks via `sendMouseClick()` were tracked

## [2.5.1] - 2026-03-26

### Changed

- **Dual-unit distance display** — Mouse travel distance now shows both imperial and metric (e.g., `42ft/12m`, `1.2mi/1.9km`) on the OLED and dashboard. Units auto-scale from feet/meters to miles/km at the 1-mile threshold
- **Travel label** — Shortened "Mouse dist" to "Travel" on the OLED menu to fit dual-unit values

## [2.5.0] - 2026-03-26

### Added

- **Lifetime stats** — Persistent counters for total keystrokes, mouse distance (miles), and mouse clicks. Displayed as read-only items in the About menu section. Stats are stored in a separate `/stats.dat` file so they survive firmware updates and settings resets
- **Dashboard stats view** — Lifetime stats shown in a new card in the web dashboard, updated live via `?status` polling
- **Stats protocol fields** — `totalKeys`, `totalMousePx`, `totalClicks` added to both `?status` and `?settings` responses

### Fixed

- **Read-only menu arrow bug** — Spurious `<` arrow on read-only menu items with non-zero values (e.g., High score) now suppressed when `minVal == maxVal`
- **Light sleep encoder wake** — Fixed spurious wake from encoder noise accumulation during light sleep

## [2.4.3] - 2026-03-20

### Fixed

- **Deep sleep BLE cleanup** — `enterDeepSleep()` now sets `restartOnDisconnect(false)` and disconnects BLE before calling `sd_power_system_off()`. Previously, a pending BLE disconnect event (from `trackBleNotify` forced reconnect) would silently restart advertising during `delay()` calls in the sleep sequence, leaving SoftDevice events pending that prevented system-off from succeeding — causing an immediate WDT reboot instead of sleep
- **WDT safety in sleep button wait** — Feed watchdog during the button-release wait loop in `enterDeepSleep()` to prevent WDT reset if the user holds the button after the countdown completes

## [2.4.2] - 2026-03-20

### Fixed

- **BLE stale link detection** — `blehid.keyboardReport()` and all BLE HID send functions (`mouseMove`, `mouseScroll`, `mouseButtonPress/Release`) now check their `bool` return value. After 5 consecutive GATT notify failures, the device force-disconnects to trigger a clean BLE reconnect. Previously, a stale macOS HID session would silently swallow all reports indefinitely while the device appeared to operate normally
- **Unmute BLE refresh** — Toggling KB/MS back on after mute now immediately requests active BLE connection parameters (`BLE_INTERVAL_ACTIVE`). Previously, unmute left the link in idle mode (60ms interval, slave latency 4), which could prevent macOS from re-engaging the HID service after a long mute period

## [2.4.1] - 2026-03-18

### Added

- **Activity LEDs** — Blue LED flashes on keyboard HID activity, green LED flashes on mouse movement/clicks. Configurable via Display → "Activity LEDs" menu setting (default: On)

### Fixed

- **Orchestrator day-rollover drift** — Work block selection drifted from wall clock over multi-day uptime (e.g., showing "Lunch Break" at 7:30 AM after a weekend). `tickOrchestrator()` now re-syncs to wall clock via `syncOrchestratorTime()` on every day rollover when time is synced, instead of cycling blocks purely on `millis()` timers with accumulated jitter
- **Pre-job-start sync crash** — `syncOrchestratorTime()` returned early without starting a block when current time was before job start, leaving the orchestrator in a tight loop (expired timer, no new block). Now starts a fresh day at block 0

## [2.4.0] - 2026-03-12

### Added

- **Charging indicator** — Lightning bolt icon appears in OLED header (between BT/USB icon and battery %) when USB-charging; game and screensaver modes show `+N%` text prefix. Reads BQ25100 `~CHG` pin (D23/P0.17) with fast polling in main loop
- **Battery spec update** — Documentation updated from 1000mAh to 2000mAh LiPo, runtime estimate ~60h → ~120h

### Changed

- **Build system: Arduino CLI → PlatformIO** — Declarative `platformio.ini` replaces `arduino-cli` wrapper scripts. Eliminates sketch directory naming constraint (no more worktree symlinks or CI checkout path hacks). Dependencies auto-installed on first build.
- **Firmware sources moved to `src/`** — All 48 firmware files (`.ino`, `.h`, `.cpp`, assets) moved from project root to `src/` directory per PlatformIO convention
- **Custom board definition** — `boards/seeed_xiao_nrf52840.json` with Seeed-specific SoftDevice S140 v7.3.0 (FWID `0x0123`), variant files committed to repo for self-contained builds
- **CI simplified** — GitHub Actions release workflow no longer needs `path: ghost_operator` checkout hack or `working-directory` override; uses PlatformIO cache for faster builds
- **`build.sh` rewritten** — Uses `pio run` for compilation; adds 1200bps touch for automatic DFU mode entry during `--flash`
- **`Makefile` updated** — Added `monitor` and `clean` targets

### Fixed

- **Display warning** — Bounds-check `carouselCursor` index into `cellCenterX[]` array to silence `-Wmaybe-uninitialized` warning

## [2.3.2] - 2026-03-08

### Fixed

- **ISR safety**: Defer BLE callback sound to main loop via volatile flags; add `volatile` to `deviceConnected`/`bleConnHandle` for correct ISR visibility
- **Display race condition**: Clear-before-render pattern for `displayDirty` flag prevents missed updates
- **Modifier key bug**: Save `nextKeyIndex` before `pickNextKey()` to prevent modifier release targeting wrong key
- **Encoder TOCTOU**: Use local snapshot of pin register to eliminate double-read race
- **Settings buffer overflow**: Increase `cmdQuerySettings` buffer from 640 to 768 bytes
- **BLE name validation**: Reject non-printable ASCII in device name protocol command
- **Dashboard parse gaps**: Add 13 missing settings fields to `parseSettings`; fix `||0` → `??0` for fields where 0 is valid
- **Dashboard mode arrays**: Fix `OP_MODE_NAMES`/`MODE_NAMES` array completeness for all operation modes
- **Serial/BLE rx overflow**: Add receive buffer overflow guard in both `serial.js` and `ble.js`
- **DFU lock release**: Add `releaseLockOnce` guard to prevent double-release in DFU serial transport
- **Export/import completeness**: Fix `clickSlots` handling and `EXPORTABLE_KEYS` list for settings backup

### Changed

- **OperationMode enum**: Replace ~50 magic number comparisons with named `OP_SIMPLE`/`OP_SIMULATION`/etc. constants across firmware
- **Game state macros**: Rename `brk`/`snk`/`rcr` → `gBrk`/`gSnk`/`gRcr` to avoid reserved identifier conflicts (~248 sites)
- **Display refactor**: Extract `drawScrollingText()` helper replacing 5 duplicated text-scrolling patterns
- **Serial command queue**: Replace single `pendingResolve` with FIFO queue for concurrent protocol commands
- **Shared game sound**: Extract `canPlayGameSound()` to `sound.h/.cpp`, shared by Breakout, Snake, and Racer modules
- **Bounds hardening**: Add guards on 11+ array-indexed lookups across display, settings, and orchestrator

## [2.3.1] - 2026-02-26

### Added

- **Simulation tuning dashboard**: Per-mode parameter editing UI with sliders for profile weights, phase timing, and durations across all 11 work modes
- **Sim tuning backup/restore**: Settings export now includes sim tuning data (version 2 format); import restores all work mode parameters; backward compatible with version 1 files
- **Compressed job performance curve**: Level 11 now caps at 1.4× max (was higher), creating a more usable scaling range

### Fixed

- **Sim tuning sliders respond on input**: Sliders now fire on `input` instead of `change` for immediate feedback while dragging
- **Stack overflow on sim data save**: Made File object static in `saveSimData()` to prevent stack overflow from large stack allocation
- **Unified save flow**: `!save` command now persists both settings and sim data in a single operation

## [2.3.0] - 2026-02-26

### Added

- **7-slot click action system**: Replace single click type with 7 configurable slots — each slot can be Left, Middle, Right, Button 4, Button 5, Wheel Up, Wheel Down, or NONE; random non-NONE slot chosen at runtime (same pattern as keyboard slots); new MODE_CLICK_SLOTS UI with 4+3 grid editor; comma-separated BLE/USB protocol (`clickSlots=1,7,7,7,7,7,7`); dashboard 7-slot grid UI replaces single dropdown
- **Expanded function keys (F1–F20)**: Add F1–F12 to available keystroke options, remove unused F21–F24; all function keys from F1 through F20 now accessible from key slot editor
- **Snake game mode**: Fourth operation mode (operationMode = 4) — classic snake game played via rotary encoder on the OLED display
- **Game-specific boot splashes**: Breakout and Snake modes show dedicated pixel art splash screens on boot instead of the default ghost splash
- **Configurable shift/lunch duration**: Shift duration (4–12h, step 30m, default 8h) and lunch duration (15–120m, step 5m, default 30m) configurable via dashboard; used by Simulation mode day schedule calculations
- **Dashboard settings export/import**: Back up all device settings to a JSON file and restore them after firmware updates; uses named protocol keys for forward/backward compatibility; unknown keys skipped on import, missing keys keep current values

### Changed

- **Redesigned pixel art icons**: Keycap pressed icon changed from 9×9 to 10×8 (same width as normal, vertical-only depression effect); mouse icons redesigned with wavy cord top and flat base across all three states (normal, click, scroll)
- **Clearer menu labels**: "Sys. sounds" → "System sounds", "Sound" → "Keybd sounds", "BLE identity" → "Bluetooth name"

### Fixed

- **Work mode timer stuck at 0s**: Mode timer check was chained as `else if` to block/lunch logic — when lunch force-jump outer condition was true but inner was false, mode timer was never evaluated; moved to independent `if` after skipBlockTransition label
- **K+M and M+K phase bugs**: Resolved 5 QA bugs in combined keyboard+mouse phase implementation
- **Breakout mode hardening**: Non-blocking sounds, physics guards, and display fixes for the Breakout game mode

## [2.2.2] - 2026-02-25

### Added

- **Lunch enforcement**: Simulation day schedule gates the lunch block until the 4-hour mark, preventing pre-lunch blocks from running past the target time; force-jumps to lunch if already overdue
- **Keystroke keepalive floor**: Background burst of 2–5 keystrokes every 2 minutes during non-typing phases ensures non-zero keyboard activity in activity monitors
- **Configurable Volume Control buttons**: Encoder button (D2) action: Play/Pause or Mute; side button (D7) action: Next, Mute, or Play/Pause — configurable via menu and BLE/serial protocol (`=encButton:N`, `=sideButton:N`)

### Changed

- **Volume Control D3 unification**: Function button (D3) uses the same handler across all operation modes — short press opens menu, hold enters sleep; eliminates the separate D2-hold-to-sleep path in Volume Control
- **Settings struct**: +2 bytes (`encButtonAction`, `sideButtonAction`); `SETTINGS_MAGIC` bumped to `0x50524F5A`; menu items 48→50

### Fixed

- **Carousel callback flash write**: Moved `onCursorChange` callback from `const` flash struct to RAM global — writes to flash-placed `static const` data are undefined behavior on ARM Cortex-M4 (silently fail), so the callback was never reliably set
- **Orchestrator time sync**: Correct work block and phase selection when time is synced mid-session via `syncOrchestratorTime()`
- **Keepalive sound suppression**: Added `silent` flag to `sendKeyDown()` to prevent buzzer clicks during background keystroke bursts in non-typing phases
- **Keepalive HID hold time**: Added 30–60ms hold between key-down/up for robust HID host recognition (zero-duration press/release pairs can be dropped by some hosts)
- **Volume Control D7 state leak**: Reset `volD7ClickCount` on screensaver activation and light sleep entry to prevent spurious next-track command on wake
- **Lunch gate clarity**: Replaced accumulative `+=` with absolute assignment in lunch block extension calculation

## [2.2.1] - 2026-02-24

### Added

- **System sounds**: BLE connect/disconnect alert tones (lo-hi on connect, hi-lo on disconnect) provide immediate audible feedback when the connection drops — the most likely cause of unexplained activity stoppage
  - New "Sys. sounds" menu setting under Sound heading (default: Off)
  - Tones play through the existing piezo buzzer circuit
- **Carousel settings page**: 9 named-option menu settings (Animation, Move style, Saver T.O., Key sound, Click type, Switch keys, Header txt, Volume theme, Job type) now open as full-screen horizontal carousels with smooth scrolling and per-option help text, matching the MODE_MODE UX
  - Boolean On/Off toggles and numeric settings remain as inline edits
- **Dashboard Bluetooth transport**: Web dashboard can now connect via Bluetooth (BLE) in addition to USB serial; `ble.js` with `getDevices()` auto-reconnect after first Chrome pairing; DFU remains USB-only with a BLE guard message

### Changed

- **Carousel pages snap on open**: Selected option is centered immediately when entering a carousel page instead of easing slowly from position 0; MODE_CAROUSEL added to time-based display refresh for continuous 20 Hz redraws
- **Mute button scope**: D7 mute button now only responds in MODE_NORMAL, preventing accidental KB/MS state changes while navigating menus
- **Orchestrator idle cap**: PHASE_IDLE capped at 60 seconds to prevent extended silence at low job performance levels
- **ResponseWriter zero-alloc**: Typedef changed from `const String&` to `const char*`, eliminating heap allocation per protocol response

### Fixed

- **ISR state volatility**: Added `volatile` qualifier to `encoderPrevState` and `lastEncoderDir` — compiler could cache stale values for ISR-shared state, causing missed encoder transitions
- **Sleep button stuck GPIO**: Added 30-second `millis()` timeout to sleep button-release polling loop to prevent battery drain if GPIO is held low by hardware fault
- **Heap fragmentation in protocol handlers**: Replaced `String +=` loops in `cmdQueryKeys()`/`cmdQueryDecoys()` with `snprintf()` into stack buffers; removed `String(buf)` wrappers in status/settings responses; rewrote `bleWrite()` to send newline as separate BLE write instead of String concatenation
- **Array bounds safety**: Added compile-time `MENU_IDX` array size check and runtime index validation in `setup()`; bounds guard on `nextKeyIndex` before `AVAILABLE_KEYS[]` access in status response; defensive `<= DECOY_COUNT` upper bound on decoy iteration; time sync value clamped at protocol boundary before `syncTime()`

## [2.2.0] - 2026-02-23

### Added

- **Volume Control operation mode**: Third operation mode that transforms the device into a BLE/USB media controller with HID consumer control
  - Encoder sends volume up/down; D2 toggles mute (hold for sleep); D3 skips tracks (double-click for previous, hold 3s for menu); D7 toggles play/pause
  - Three display themes selectable via menu: Basic (segmented bar), Retro (VU meter with needle), Futuristic (horizontal slider with thumb)
  - Consumer control HID report (`RID_CONSUMER`) always present in the USB descriptor across all modes
  - Jiggler fully disabled in Volume Control mode; Animation, Schedule, and Sound menu sections hidden
  - Screensaver dims the active theme display instead of showing a separate screen
- **Mode-specific splash screens**: Boot screen selects bitmap based on operation mode; Volume Control has its own splash art; all splash screens use white background

### Changed

- **Mode picker redesign**: Replaced vertical list with horizontal carousel — all mode names laid out as an animated strip with the selected item inverted and adjacent items visible at screen edges; smooth ease-out lerp scrolling at 20 Hz; scrolling description and footer text
- **Menu items**: 47 entries (10 headings + 37 items) — added Volume heading + Theme at the top (visible only in Volume Control mode); new `FMT_VOLUME_THEME` value format
- **Settings struct**: +1 byte (`volumeTheme`); `SETTINGS_MAGIC` bumped to `0x50524F57`
- **USB HID descriptor**: Added `RID_CONSUMER` report ID for consumer control (volume, media keys)

### Fixed

- **Mode carousel text wrap**: Disabled text wrap on mode carousel to prevent long mode names from overflowing to the next line

## [2.1.0] - 2026-02-23

### Added

- **Piezo buzzer keyboard sounds**: 5 mechanical keyboard sound profiles (MX Blue, MX Brown, Membrane, Buckling Spring, Deep Thock) with per-keystroke frequency variation for realism
  - New "Sound" heading in menu with "Sound" (On/Off) and "Key sound" (profile selector) items
  - Sound plays on key-down in both Simple and Simulation modes; auto-muted outside NORMAL mode
  - Live preview: continuous typing plays while cycling sound types in menu, switches profile instantly on encoder rotation
  - Protocol support: `=sound:N`, `=soundType:N`
  - Dashboard: new Sound section with toggle and type dropdown
- **Job performance scaling**: 0–11 "Performance" knob in Simulation menu globally scales activity intensity
  - Level 5 = baseline (current behavior); higher = more keystrokes, shorter idle; lower = near-idle
  - Encoder adjusts job performance directly from NORMAL screen in Simulation mode (replaces profile switching), with OLED overlay showing fill-bar gauge
  - Dashboard: performance slider in Simulation section
  - Protocol: `=jobPerf:N`, `jobPerf` field in `?settings` response
- **Job start time**: Separate `jobStartTime` setting (default 8:00 AM) decouples simulation day-block anchoring from the sleep schedule's start time
  - Dashboard: job start time slider in Simulation section
  - Protocol: `=jobStart:N` (0–287 five-minute slots)
- **Manual clock setting (MODE_SET_CLOCK)**: On-device time editor accessible via "Set clock" menu action in Schedule section — set hours (0–23) and minutes (0–59) without USB connection
  - Pre-fills from current time if already synced; encoder cycles values, button advances field, function button confirms

### Changed

- **Display optimization**: Three-layer rendering overhaul minimizes I2C traffic to SSD1306 OLED
  - Dirty flag: `markDisplayDirty()` called from all state-change sites; static modes skip I2C entirely when idle
  - 20 Hz refresh (50ms): time-based modes redraw every frame, others only when dirty
  - Shadow buffer: 1024-byte page cache sends only changed pages via SSD1306 page addressing (~60–75% I2C savings on typical NORMAL frames)
  - `invalidateDisplayShadow()` added for code paths that bypass the shadow (e.g., light sleep screen)
- **Settings struct**: +3 bytes (`jobPerformance`, `jobStartTime` as uint16, clock editor state); `SETTINGS_MAGIC` bumped to `0x50524F56`
- **Menu items**: 45 entries (9 headings + 36 items) — added Performance, Job start, Set clock, Sound, Key sound; new `FMT_PERF_LEVEL` value format
- **Schematic**: Updated encoder from KY-040 breakout to bare EC11 with internal pull-ups; added transistor-driven buzzer circuit (Q1 2N2222, R1 1kΩ, R2 10kΩ)

### Fixed

- **Piezo chirping**: Replaced `tone()` (sustained square wave causing piezo resonance) with direct GPIO pulses using microsecond timing for sharp percussive transients

## [2.0.0] - 2026-02-22

### Added

- **Simulation mode**: New operation mode that replaces flat keystroke/mouse timing with realistic human work patterns — keystroke bursting with humanized press durations, mutual exclusion between keyboard and mouse activity, and auto-profiling (LAZY/NORMAL/BUSY) driven by work mode weights
- **Job simulations**: Three pre-built daily schedule templates (Staff, Developer, Designer) with 9 time blocks each, containing weighted pools of 11 micro-modes (Email Compose, Programming, Browsing, Video Conference, etc.)
- **Keystroke bursting**: Non-blocking burst state machine sends rapid key sequences (5-100 keys per burst) with variable inter-key delays and humanized hold durations, replacing uniform single-key timing
- **Phantom clicks**: Optional mouse button clicks injected at transition points during mouse phases (25% chance per transition), simulating tab switching behavior
- **Click type setting**: Configurable phantom click button — Middle (default) or Left
- **Window switching**: Optional Alt-Tab or Cmd-Tab keystrokes at configurable intervals, gated behind `windowSwitching` toggle
- **Activity orchestrator**: Unified `tickOrchestrator()` replaces independent KB and mouse timers, cycling through PHASE_TYPING → PHASE_SWITCHING → PHASE_MOUSING → PHASE_IDLE with mutual exclusion
- **Mute button (D7)**: New SPST momentary button on pin D7 cycles KB/MS enable combos in Simple mode; cycles clock/uptime/version/die temp footer info in Simulation mode
- **Mode picker (MODE_MODE)**: Dedicated sub-page for switching between Simple and Simulation with descriptions ("Direct timing" / "Human work patterns") and reboot confirmation prompt
- **Two-stage sleep**: Hold-to-sleep now has two thresholds — light sleep at 3s, deep sleep at 6s; segmented progress bar with "Light" and "Deep" labels; dynamic hint changes from "Release to cancel" → "Release = light sleep" at midpoint
- **Light sleep breathing animation**: Manual light sleep shows a breathing circle (radius 2→6px, 4s cycle) in the lower-right corner, evoking MacBook sleep LED
- **Hardware watchdog (WDT)**: 8-second timeout with I2C bus recovery on restart — prevents permanent hang from I2C lockup
- **Pixel art icons**: 10×10 keycap and mouse bitmaps in simulation footer and screensaver, with distinct states for key press, mouse click, and mouse scroll
- **Footer info cycling**: Mute button (SW2) cycles through clock/uptime/version/die temp in simulation mode footer
- **Dashboard simulation support**: New SimulationSection component for job type and simulation-specific settings; StatusBar shows mode selector with reboot warning
- **8 new settings**: Operation mode (Simple/Simulation), Job type (Staff/Dev/Designer), Auto-clicks (On/Off), Click type (Middle/Left), Window switch (On/Off), Switch keys (Alt-Tab/Cmd-Tab), Header txt (Job name/Device name), and Invert dial (Off/On)
- **8 new BLE/serial commands**: `=opMode`, `=jobSim`, `=phantom`, `=clickType`, `=winSwitch`, `=switchKeys`, `=headerDisp`, `=invertDial`
- **Non-blocking HID functions**: `sendKeyDown()` / `sendKeyUp()` for burst state machine, `sendMouseClick()` for phantom clicks, `sendWindowSwitch()` for Alt/Cmd-Tab
- **Conditional menu visibility**: `isMenuItemHidden()` dynamically shows/hides menu sections based on operation mode (Keyboard min/max and Profiles hidden in Simulation, Simulation heading hidden in Simple)

### Changed

- **Settings struct**: +7 bytes (operationMode, jobSimulation, phantomClicks, clickType, windowSwitching, switchKeys, headerDisplay); SETTINGS_MAGIC bumped to `0x50524F54`
- **Host OS → Switch Keys**: `hostOS` (4-value: Disabled/Win/Mac/Linux) replaced with `switchKeys` (2-value: Alt-Tab/Cmd-Tab); window switching is now gated only by `windowSwitching` toggle
- **Menu items**: 39 entries (8 headings + 31 items) — Simulation heading at top; "Mode" moved to Device section as action item; renames: "Phantom clicks" → "Auto-clicks", "Job profile" → "Job type", "Header display" → "Header txt"
- **Menu viewport**: Now skips hidden items during rendering, properly tracking visible row count for scroll offset calculation
- **Heap fragmentation elimination**: All `String`-returning functions converted to `snprintf()` with caller-provided buffers — `formatDuration()`, `formatUptime()`, `formatCurrentTime()`, `formatMenuValue()`
- **Simulation display redesign**: 4-row hierarchy (block → mode → profile stint → activity) with inline progress bars and scrolling long names; pixel art keycap/mouse icons in footer replace text indicators
- **Simulation screensaver**: 3× scaled centered keycap/mouse icons with state feedback (press, click, scroll) — replaces text-and-bars layout
- **Phantom clicks**: Transition-based 25% chance per mouse phase transition (was timer-based fixed interval)
- **Screensaver refresh**: 5 Hz (was 2 Hz) with keycap visual hold for short key presses
- **Sleep confirmation**: 6s total with segmented bar showing Light (0-3s) and Deep (3-6s) zones (was 5s single bar)
- **Protocol responses**: `?settings` and `?status` extended with simulation-specific fields
- **Serial dump**: `d` command now prints orchestrator state (block, mode, phase, profile) when in Simulation mode
- **BLE model string**: Now uses `VERSION` define instead of hardcoded string
- **Build**: `-Wall` warnings enabled and resolved
- **CI**: `CHANGELOG.md` used for release notes instead of auto-generated notes

### Notes

- Simple mode (default) is 100% unchanged from v1.10.2 — zero behavior change on upgrade
- First boot after upgrade resets settings due to SETTINGS_MAGIC bump (operationMode defaults to Simple)
- New files: `orchestrator.h/.cpp`, `sim_data.h/.cpp`

## [1.10.2] - 2026-02-22

## [1.10.1] - 2026-02-21

### Added

- **nRF52840 die temperature**: New "Die temp" read-only item in About menu section, displays internal temperature in both Celsius and Fahrenheit via `sd_temp_get()`
- **Dashboard battery chart**: Real-time SVG chart showing battery percentage and voltage history over time, with localStorage persistence across sessions
- **Battery voltage in status protocol**: `?status` response now includes `batMv` field for millivolt-level battery monitoring

### Changed

- **LiPo discharge curve**: Replaced linear voltage-to-percentage mapping with 11-point lookup table matching real LiPo characteristics (plateau at 3.7-3.8V, steep dropoff below 3.5V)
- **BLE idle power management**: Connection interval negotiated from 15ms (active HID) to 60ms with slave latency 4 (idle) after 5 seconds of no HID activity; reverts on next keystroke or mouse move
- **Adaptive display refresh**: Screensaver runs at 2 Hz (was 10 Hz) to reduce I2C bus power draw
- **Protocol input validation**: All `setSettingValue()` paths now use `clampVal()` for bounds enforcement; array-indexed format lookups guarded against out-of-bounds access
- **Heap fragmentation reduction**: `cmdQueryStatus()` and `cmdQuerySettings()` rewritten with `snprintf()` into stack buffers instead of `String` concatenation
- **Symbolic menu indices**: Hardcoded `menuCursor = N` replaced with `MENU_IDX_*` defines for position-independent menu navigation
- **BLE UART reliability**: Buffer overflow tracking with `-err:cmd too long` error response; `resetBleUartBuffer()` called on disconnect to prevent cross-session corruption; partial slot commands fill remaining slots with NONE
- **Serial status push throttle**: 200ms minimum interval guard on `pushSerialStatus()` to prevent BLE stack saturation
- **Copyright reference decoupled**: `COPYRIGHT_TEXT` constant replaces position-dependent `MENU_ITEMS[MENU_ITEM_COUNT - 1].helpText` references

### Fixed

- **Menu timeout on About items**: Read-only items (Uptime, Die temp, Version) no longer trigger 30-second mode timeout, so users can monitor uptime without being kicked back to NORMAL mode
- **`sprintf` → `snprintf`**: Remaining `sprintf` calls in `schedule.cpp` and `settings.cpp` converted to `snprintf` for buffer safety

## [1.10.0] - 2026-02-20

### Added

- **BLE identity presets (decoy masquerade)**: Device can impersonate common commercial Bluetooth peripherals to blend into corporate BLE scans
  - 10 built-in presets: Apple Magic Keyboard/Mouse/Trackpad, Logitech MX Master 3S/MX Keys S/MX Anywhere 3S/K380/ERGO K860, Keychron K2/V1
  - New `MODE_DECOY` UI: scrollable picker with active marker (`*`), encoder navigates, button selects, reboot confirmation on change
  - BLE device name and USB descriptors updated to match selected identity
  - "Custom" mode (index 0) preserves manual device name editing via `MODE_NAME`
  - Menu item "BLE identity" in Device section (action item, opens picker)
  - Config protocol: `=decoy:N` (0=Custom, 1-10=preset), `?decoys` returns preset list
  - Dashboard: identity dropdown in Device section, disables custom name field when preset selected
  - Serial `d` command prints current BLE identity
- **Timed schedule (auto-sleep / full auto)**: Automated sleep and wake tied to wall clock time, synced via USB dashboard
  - Two scheduling modes: **Auto-sleep** (deep sleep at end time, manual wake) and **Full auto** (light sleep at end time, auto-wake at start time)
  - 5-minute time slot granularity (288 slots per day), configurable start and end times (default 9:00–17:00)
  - New `MODE_SCHEDULE` UI: 3-row editor (Mode, Start, End) with contextual help text; rows hidden based on mode selection
  - Wall clock synced from dashboard on USB connect (`=time:SECS` command, seconds since midnight)
  - Schedule editor locked with "Sync clock via USB dashboard" message until time is synced
  - Light sleep state: display dimmed, BLE advertising stopped, "Scheduled Sleep" + wake time shown; encoder button wakes manually
  - Auto-sleep mode disables schedule before entering deep sleep to prevent boot loop
  - New `schedule.h/.cpp` module (core logic: time sync, schedule check, light/deep sleep transitions)
  - Config protocol: `=schedMode:0-2`, `=schedStart:N`, `=schedEnd:N`, `=time:SECS`
  - Dashboard: new Schedule section with mode dropdown, start/end time sliders, sync indicator and sleeping badge
  - Serial `d` command prints schedule mode, times, sync status, and current time

### Changed

- **Settings struct**: Added `decoyIndex` (uint8), `scheduleMode` (uint8), `scheduleStart` (uint16), `scheduleEnd` (uint16); bumped `SETTINGS_MAGIC` to `0x50524F50` (existing settings auto-reset to defaults on first boot)
- **Menu**: 2 new action items (BLE identity, Schedule) — `MENU_ITEM_COUNT` updated; new `FMT_SCHEDULE_MODE` and `FMT_TIME_5MIN` value formats
- **Config protocol**: `?settings` response includes `decoy`, `schedMode`, `schedStart`, `schedEnd` fields; `?status` includes `timeSynced`, `schedSleeping`, `daySecs`
- **Dashboard**: Device section includes BLE identity dropdown; new Schedule section component

### Fixed

- **Dashboard port close**: Awaits serial port close on disconnect to prevent stale connection state

## [1.9.1] - 2026-02-20

### Added

- **"Dashboard" setting with smart default**: WebUSB landing page shows a Chrome notification linking to the web dashboard when plugged in via USB (default On)
  - New menu item "Dashboard" in Device section (On/Off toggle, help text: "reboot to apply")
  - Setting takes effect at boot — Chrome reads the URL during USB enumeration
  - **Smart auto-disable**: Notification shows for 3 boots; auto-disables on boot 4 if the user never interacts with the setting. Any explicit toggle (menu or config protocol) pins the setting permanently (`dashboardBootCount = 0xFF`)
  - Pin-on-change: only pins when the value actually changes (writing same value is a no-op for the boot counter)
  - Settings struct: added `dashboardEnabled` and `dashboardBootCount` fields; bumped `SETTINGS_MAGIC` to `0x50524F4D` (existing settings auto-reset to defaults on first boot)
  - Config protocol: `?settings` includes `dashboard` field; `=dashboard:N` set command added
  - Dashboard web app: "Dashboard Link" dropdown in Device section
  - Serial `d` command prints `Dashboard: On/Off (boot: N/3)` or `(boot: pinned)`
- **USB descriptor customization**: Chrome shows "Ghost Operator" by "TARS Industrial" instead of the board's default name in the WebUSB notification
  - `TinyUSBDevice.setManufacturerDescriptor("TARS Industrial")` and `TinyUSBDevice.setProductDescriptor("Ghost Operator")` set before USB stack init

### Changed

- **Settings load moved before USB init**: `loadSettings()` now runs before `usb_web.begin()` and `Serial.begin()` so that `dashboardEnabled` is known before the TinyUSB stack starts and Chrome enumerates the device. Debug output from `loadSettings()` is silent (Serial not yet initialized).
- **Deferred flash save**: Boot-count `saveSettings()` is deferred until after `Serial.begin()` — LittleFS page erase (~85ms with interrupts disabled) before USB init was preventing WebUSB descriptor enumeration
- **WebUSB landing page URL**: Changed from `tarsindustrial.tech/ghost-operator/landing` to `tarsindustrial.tech/ghost-operator/dashboard?welcome`

## [1.9.0] - 2026-02-19

### Added

- **USB HID (wired)**: TinyUSB composite keyboard + mouse over USB alongside BLE
  - Keystrokes and mouse movements sent to both USB and BLE transports simultaneously
  - Display shows USB trident icon in header when wired; progress bars and jiggler run when either BLE or USB is connected
  - LED blink rate reflects combined connection state (either transport counts as connected)
- **"BT while USB" setting**: Control whether Bluetooth stays active when USB is plugged in (Off / On, default Off)
  - When Off, BLE advertising stops and any active BLE connection is disconnected while USB is connected
  - When On, both transports operate simultaneously
  - New menu item in Device section; exposed in BLE UART protocol and dashboard
- **Scroll wheel**: Optional random scroll wheel events during mouse jiggle (Off / On, default Off)
  - When enabled, injects ±1 scroll ticks at random 2-5 second intervals during MOUSE_JIGGLING
  - Works with both Bezier and Brownian movement styles
  - New `sendMouseScroll()` sends to both BLE and USB transports
  - New menu item "Scroll" in Mouse section; exposed in BLE UART protocol and dashboard
- **Build automation**: Local build tooling and CI/CD for firmware releases
  - `build.sh`: compile, setup (arduino-cli + board + libs), release (versioned DFU ZIP), flash (USB serial)
  - `Makefile`: convenient targets wrapping build.sh (build, release, flash, setup, clean, help)
  - `.github/workflows/release.yml`: GitHub Actions workflow — on `v*` tag push, validates version against `config.h`, builds firmware, creates GitHub Release with DFU ZIP attached
- **Real-time serial status push**: `pushSerialStatus()` proactively sends `?status` response over USB serial on state changes (key sent, profile/mode/KB-MS toggles, mouse state transitions) — dashboard reflects device state without polling
- **Serial `e` command**: Trigger easter egg animation immediately from serial debug interface

### Changed

- **Settings struct**: Added `btWhileUsb` and `scrollEnabled` fields; bumped `SETTINGS_MAGIC` to `0x50524F4B` (existing settings auto-reset to defaults on first boot)
- **Menu**: 2 new items (Scroll, BT while USB) — `MENU_ITEM_COUNT` increased from 23 to 25; new `FMT_ON_OFF` value format
- **Config protocol**: `?settings` response includes `btWhileUsb` and `scroll` fields; `?status` includes `usb` field; `=btWhileUsb:N` and `=scroll:N` set commands added
- **Dashboard**: Status bar shows USB connection state; Mouse section has Scroll control; Device section has BT while USB control

### Fixed

- **Dashboard profile display**: LAZY profile (index 0) was silently displayed as NORMAL due to JavaScript `parseInt(0) || 1` falsy-value coercion — fixed with nullish coalescing (`??`). Same fix applied to `animStyle` (ECG → Ghost) and `saverTimeout` (Never → 30min)

## [1.8.2] - 2026-02-19

### Changed

- **Mute indicator**: Show "mute" instead of countdown time next to KB/mouse progress bars when respective output is disabled
- **Custom device name in header**: Normal mode header shows custom device name instead of "GHOST Operator" when name differs from default
- **Screensaver progress bars**: Narrowed to 65% width (83px centered) with 3px tall end caps as visual bookends; reduces lit pixels for burn-in prevention

## [1.8.1] - 2026-02-18

### Changed

- **Pac-Man easter egg redesign**: Full power pellet narrative — dots on screen, ghost chases Pac-Man, energizer power-up, ghost becomes frightened, Pac-Man reverses and eats ghost, then turns back to eat remaining dots
  - Seamless "tunnel" transition: corner ghost animation syncs to right edge before easter egg starts
  - New sprites: left-facing Pac-Man, right-facing ghost (chasing), frightened ghost (zigzag mouth)
  - 5-phase sequence across 53 animation frames (~5.3s)
- **Default key slot**: Changed from F15 to F16 (same invisible ghost key category)
- **Default screensaver timeout**: Changed from 30 minutes to Never (screensaver disabled by default)
- **Dashboard label**: "Jiggle Duration" renamed to "Move Duration" in Mouse section

## [1.8.0] - 2026-02-18

### Added

- **Compact uptime format**: Footer uptime display uses compact `d/h/m/s` units instead of `HH:MM:SS` (e.g., "2h 34m", "1d 5h", "45s"); seconds hidden once uptime exceeds 1 day. Dashboard status bar matches the same format.
- **Activity-aware animation**: Footer animation speed responds to KB/mouse enabled state
  - Both enabled: full speed
  - One muted: half speed
  - Both muted: animation freezes on current frame
- **Mouse movement styles**: New "Move style" setting with two movement algorithms
  - **Bezier** (default): Smooth curved sweeps with random radius — natural-looking arcs
  - **Brownian**: Classic jiggle with sine ease-in-out velocity profile
  - "Move size" setting only applies to Brownian mode; hidden in OLED menu and disabled in dashboard when Bezier is active
  - Settings struct: added `mouseStyle` field — bumped `SETTINGS_MAGIC` (existing settings auto-reset to defaults on first boot)
  - Dashboard: "Move Style" dropdown in Mouse section, Move Size conditionally disabled in Bezier mode
  - Serial `d` command prints mouse style name

## [1.7.2] - 2026-02-18

### Added

- **Web Serial DFU**: Browser-based firmware updates via USB serial, integrated into the web dashboard
  - New `!serialdfu` BLE UART command reboots device into USB CDC serial DFU bootloader mode
  - New `u` serial command for entering Serial DFU mode during development/testing
  - Dashboard "Firmware Update" section with full DFU workflow: select ZIP → confirm → USB reboot → Web Serial transfer → progress bar
  - Uses GPREGRET magic `0x4E` (DFU_MAGIC_SERIAL_ONLY_RESET) via SoftDevice-safe API
  - OLED shows "USB DFU / Connect USB cable" screen before reboot
  - DFU library (`dashboard/src/lib/dfu/`): Nordic SDK 11 legacy HCI/SLIP serial DFU protocol
    - SLIP byte-stuffing + HCI packet framing with sequence numbers and CRC16-CCITT
    - 512-byte chunked transfer with flash page-write delays
    - ZIP parser using `fflate` (~8KB tree-shaken) for `adafruit-nrfutil` packages
  - `dfuActive` flag keeps dashboard UI visible during DFU transfer (after serial disconnects)
  - Web Serial API with no VID filter (bootloader reuses Seeed's VID, not Adafruit's)
  - **Requires Chrome/Edge on desktop** (Web Serial API not available in Firefox/Safari)

### Changed

- **Dashboard switched from BLE to USB serial**: Web dashboard now connects via Web Serial API instead of Web Bluetooth
  - Eliminates the BLE pairing conflict (no more "forget device" workaround in OS Bluetooth settings)
  - USB serial works alongside BLE HID without interference — device stays paired as keyboard/mouse
  - Firmware `processCommand()` made transport-agnostic via `ResponseWriter` function pointer
  - Serial command handler (`serial_cmd.cpp`) now buffers `?/=/!` protocol lines alongside single-char debug commands
  - New `dashboard/src/lib/serial.js` — Web Serial config transport (same API as `ble.js`)
  - DFU flow sends `!serialdfu` over USB serial (was: over BLE), then opens separate DFU serial port
  - Device name populated from `?settings` response (serial doesn't provide it at connect time like BLE GATT did)

## [1.7.1] - 2026-02-18

### Added

- **OTA DFU mode**: Firmware commands to reboot into DFU bootloader for over-the-air updates via nRF Connect mobile or `adafruit-nrfutil`
  - New `!dfu` BLE UART command reboots device into DFU bootloader mode
  - New `f` serial command for entering DFU mode during development/testing
  - Uses SoftDevice-safe GPREGRET API (`sd_power_gpregret_set`) — direct register access hard-faults
  - OLED shows "OTA DFU" screen during bootloader mode (persists through reboot)
  - Bootloader stays in DFU mode indefinitely; power cycle to exit without completing transfer
  - **Note:** Web Bluetooth DFU not supported — Adafruit bootloader uses legacy DFU protocol (`00001530-...`) which Chrome blocklists. Use nRF Connect mobile app or `adafruit-nrfutil dfu ble` instead.

## [1.7.0] - 2026-02-18

### Added

- **BLE UART wireless configuration**: New `ble_uart.h/.cpp` module adds Nordic UART Service (NUS) for wireless settings management
  - Text-based protocol: `?` queries, `=` sets, `!` actions, pipe-delimited key=value responses
  - `?status` — battery, profile, mode, mouse state, uptime, next key (polled by dashboard)
  - `?settings` — all persistent settings in one response
  - `?keys` — available keystroke names for populating UI dropdowns
  - `=key:value` — change any setting in memory (like encoder editing)
  - `!save` — persist to flash, `!defaults` — factory reset, `!reboot` — restart device
  - 20-byte chunked writes for default MTU compatibility
  - Does NOT modify BLE advertising (NUS discovered via `optionalServices` after connection)
- **Web dashboard** (`dashboard/`): Vue 3 + Vite single-page app for device configuration via Chrome Web Serial API (USB)
  - Connect/disconnect with auto-reconnect handling
  - Real-time status bar (battery, profile, mode, KB/MS state, uptime, next key)
  - Slider controls for all timing settings with cross-constraint enforcement
  - 8-slot key editor with dropdown populated from device
  - Profile adjustment sliders, display settings, animation/screensaver dropdowns
  - Device name editor, Reset Defaults and Reboot with confirmation dialogs
  - Explicit Save button (dirty flag tracking) — avoids flash wear from rapid adjustments
  - Dark theme, responsive layout, zero runtime dependencies beyond Vue

## [1.6.1] - 2026-02-17

### Changed

- About menu copyright updated to full company name
- Mouse RETURNING display: empty bar (0%) instead of sweeping animation

## [1.6.0] - 2026-02-17

### Changed

- **Modular codebase architecture**: Split monolithic `ghost_operator.ino` (2,602 lines) into 15 focused `.h/.cpp` module pairs + lean `.ino` (~297 lines). Zero logic changes — purely structural refactor for maintainability.
  - `config.h` — all `#define` constants, enums, structs (header-only)
  - `keys.h/.cpp` — const data tables (`AVAILABLE_KEYS`, `MENU_ITEMS`, etc.)
  - `icons.h/.cpp` — PROGMEM bitmaps (splash, BT icon, arrows)
  - `state.h/.cpp` — all mutable globals as `extern` declarations
  - `settings.h/.cpp` — flash persistence + value accessors
  - `timing.h/.cpp` — profiles, scheduling, formatting
  - `encoder.h/.cpp` — ISR + polling quadrature decode
  - `battery.h/.cpp` — ADC battery reading
  - `hid.h/.cpp` — keystroke sending + key selection
  - `mouse.h/.cpp` — mouse state machine
  - `sleep.h/.cpp` — deep sleep sequence
  - `screenshot.h/.cpp` — PNG encoder + base64 serial output
  - `serial_cmd.h/.cpp` — serial debug commands + status
  - `input.h/.cpp` — encoder dispatch, buttons, name editor
  - `display.h/.cpp` — all rendering (~800 lines, largest module)
- **Mouse RETURNING progress bar**: Shows empty bar (0%) during return-to-origin phase — no animation, just a static empty state
- Removed redundant "RTN" text from mouse timer area during MOUSE_RETURNING state — `[RTN]` in the status label is sufficient
- **Configurable status animation**: New "Animation" setting in the Display menu section with 6 styles
  - ECG (scrolling heartbeat trace), EQ (equalizer bars), Ghost (animated ghost character), Matrix (falling characters), Radar (rotating sweep), None (disabled)
  - Default animation: Ghost; persists in flash
  - Settings struct: added `animStyle` field — bumped `SETTINGS_MAGIC` (existing settings auto-reset to defaults on first boot)
- **Menu section reorganization**: Split combined "Display/Device" heading into separate "Display" and "Device" sections
  - Display section: Brightness, Saver bright, Saver T.O., Animation
  - Device section: Device name, Reset defaults, Reboot
  - `MENU_ITEM_COUNT` increased from 20 to 22 (6 headings + 16 items)
- Name editor: END positions drawn as 2×2 filled rectangle instead of middle-dot glyph (no suitable glyph in Adafruit GFX default font)
- Serial `d` command prints current animation style name

## [1.5.0] - 2026-02-17

### Added

- **Adjustable mouse movement amplitude**: New "Move size" setting in the Mouse menu section (1-5px, step 1, default 1px)
  - Amplitude represents peak pixels-per-step, scaled by sine easing curve during each jiggle
  - At amplitude 1, subtle pauses at start/end of each jiggle with 1px movement in the middle
  - At amplitude 5, smooth visible ramp-up and ramp-down with 5px peak movement
  - Return phase naturally handles all amplitudes ≤ 5 (existing `min(5, remaining)` clamp)
- **Inertial mouse movement**: Sine ease-in-out velocity profile during JIGGLING state
  - Movement ramps from zero → peak → zero using `sin(π × progress)` curve
  - Creates natural-looking acceleration and deceleration, like a human moving the mouse
  - Steps with zero amplitude are skipped (natural pause at start/end of each jiggle)
- **Configurable BLE device name**: New "Device name" action item in the Device menu section opens a character-by-character editor (MODE_NAME)
  - 66-character set: A-Z, a-z, 0-9, space, dash, underscore, plus END marker
  - Max 14 characters (fits BLE scan response; default "GhostOperator" is 13)
  - Pre-initialized from current saved name — only change what's different
  - END marker (`·`) terminates the name; trailing ENDs trimmed on save
  - Encoder rotates through characters (wrapping), encoder button advances position
  - Function button saves and shows reboot confirmation if name changed
  - `NVIC_SystemReset()` applies new name immediately; "No" returns to menu
  - If name unchanged, func button returns to menu directly (no reboot prompt)
  - Empty name guard: falls back to "GhostOperator" if all positions are END
- New `FMT_PIXELS` menu value format — renders as `"Npx"`
- Dynamic help text for "Device name" menu item shows `Current: <name>` instead of static text
- **Reset defaults**: New "Reset defaults" action item in Device menu section restores all settings to factory defaults
  - Yes/No confirmation overlay (defaults to "No" for safety)
  - Resets all values including key slots, timing, profiles, brightness, screensaver, device name
  - Profile resets to NORMAL, next key re-picked from default slot
- **Reboot**: New "Reboot" action item in Device menu section for restarting the device
  - Yes/No confirmation overlay (defaults to "No" for safety)
  - Useful for applying pending BLE name changes without a full power cycle
- Serial `d` command prints mouse amplitude and device name
- **Sleep confirmation**: Long-press sleep replaced with a hold-to-confirm flow
  - After 500ms hold, an overlay appears with "Hold to sleep..." and a 5-second countdown bar
  - Keep holding to complete the countdown and enter deep sleep
  - Release during countdown to cancel — shows "Cancelled" briefly and returns to previous screen
  - Encoder and button input suppressed during confirmation and cancellation overlays
- **Serial PNG screenshot**: New `p` serial command dumps the current OLED display as a base64-encoded PNG
  - Minimal on-device PNG encoder — no external library, uses uncompressed deflate (stored blocks)
  - Output delimited by `--- PNG START ---` / `--- PNG END ---` text markers for easy capture
  - Produces valid 128×64 1-bit grayscale PNG (~1156 bytes raw, ~1544 chars base64)
  - Converts SSD1306 page-format framebuffer to PNG row-major format on the fly
  - CRC32 (bit-by-bit) and Adler32 computed inline — zero extra RAM for lookup tables

### Changed

- Menu heading renamed from "Display" to "Device" (now contains brightness, screensaver, and device name settings)
- Settings struct: added `mouseAmplitude` and `deviceName[15]` fields — existing settings auto-reset to defaults on first boot (checksum mismatch)
- `MENU_ITEM_COUNT` increased from 16 to 20
- BLE `Bluefruit.setName()` now reads from `settings.deviceName` instead of compile-time `DEVICE_NAME` constant
- Mode timeout (30s) now handles MODE_NAME: saves name, skips reboot prompt, returns to NORMAL
- Sleep entry changed from immediate 3-second long-press to hold-to-confirm with 5-second countdown bar (cancellable by release)
- Pre-sleep "SLEEPING..." display shortened to 500ms

## [1.4.0] - 2026-02-16

### Added

- **Scrollable settings menu**: Function button opens a scrollable menu with all settings organized under headings (Keyboard, Mouse, Profiles, Display)
  - Encoder navigates menu items (headings auto-skipped), encoder press selects/confirms
  - Two-press interaction: select item → enter edit mode → turn to adjust → press to confirm
  - `< value >` arrows show available range, hidden at bounds
  - Context-sensitive help bar at bottom; long text auto-scrolls with pauses at each end
- **Display brightness setting**: Adjustable OLED brightness for normal mode (10-100% in 10% steps, default 80%)
  - Live contrast update while editing in menu for instant visual feedback
- **Menu data architecture**: Data-driven `MenuItem` struct array with type, label, help text, format, range, and step — adding a new setting requires only array entry + accessor cases

### Changed

- **UI model**: Replaced 10-mode flat cycle (function button) with 3 modes: NORMAL, MENU, SLOTS
  - Function button short press: NORMAL ↔ MENU toggle (was: cycle through all modes)
  - Function button from SLOTS: returns to MENU at "Key slots" item (was: cycle to next mode)
  - Encoder button in MENU: select/confirm (was: cycle KB/MS combos in all modes)
- **Slots mode**: Bottom instruction changed from "Func=exit" to "Func=back"
- **Lazy adj display**: Shows `0%` instead of `-0%` at zero; encoder direction inverted so CW → toward 0% and CCW → toward -50% (matches displayed value direction)
- Settings struct: added `displayBrightness` field — existing settings auto-reset to defaults on first boot
- Serial `d` command now prints display brightness

### Removed

- `drawSettingsMode()` — replaced by data-driven `drawMenuMode()` + `drawHelpBar()`
- `adjustValue()` helper — replaced by generic menu value adjustment via `getSettingValue()`/`setSettingValue()`
- Individual mode cases in `handleEncoder()` (KEY_MIN, KEY_MAX, MOUSE_JIG, etc.) — replaced by single menu editing path
- Linear mode cycling in `handleButtons()` — replaced by NORMAL ↔ MENU toggle

## [1.3.1] - 2026-02-16

### Fixed

- **Encoder unresponsive after boot** — `analogRead(A0)` for RNG seeding disconnected the digital input buffer on P0.02 (encoder CLK), making rotation produce net-zero position changes until `readBattery()` incidentally restored the pin ~60 seconds later
- **Rapid encoder rotation missing detents** — switched from interrupt-only to hybrid ISR + polling architecture; added 2-step jump recovery to infer direction when intermediate quadrature states are missed during I2C display transfers or BLE radio events

### Changed

- Encoder reading: hybrid GPIOTE interrupt (primary, catches edges during blocking I2C) + main-loop polling (fallback, catches edges during SoftDevice radio blackouts)
- Encoder ISR uses atomic `NRF_P0->IN` register read for both pins simultaneously (eliminates race window between two `digitalRead()` calls)
- RNG seeding uses `NRF_FICR->DEVICEADDR` (factory-unique device ID) + `micros()` instead of `analogRead(A0)` to avoid GPIO side effects on encoder pin
- Encoder interrupts attached after SoftDevice initialization to prevent GPIOTE channel invalidation
- Main loop delay reduced from 5ms to 1ms for improved encoder polling rate
- Custom bitmap splash screen replaces text-based splash (loaded from `ghost_operator_splash.bin`)
- Splash screen displays version number in lower-right corner
- Splash screen duration increased from 1.5s to 3s

## [1.3.0] - 2026-02-16

### Added

- **Screensaver mode**: Minimal OLED display activates after configurable idle timeout to prevent burn-in
  - Centered key label + 1px progress bar, mouse state + 1px progress bar
  - Battery percentage shown only when below 15%
  - OLED contrast dimmed to configurable brightness level
  - Dramatically reduces lit pixels and power compared to normal display
- **SCREENSAVER settings page**: Configure timeout via encoder (Never / 1 / 5 / 10 / 15 / 30 min, default 30 min)
  - Position dots UI (filled = selected, hollow = unselected)
- **SAVER BRIGHT settings page**: Configure screensaver OLED brightness (10-100% in 10% steps, default 20%)
  - Progress bar UI consistent with other percentage settings

### Changed

- Mode cycle: NORMAL → KEY MIN → KEY MAX → SLOTS → MOUSE JIG → MOUSE IDLE → LAZY % → BUSY % → SCREENSAVER → SAVER BRIGHT
- Settings struct: added `saverTimeout` and `saverBrightness` fields — existing settings auto-reset to defaults on first boot
- Serial `d` command now prints screensaver timeout name, brightness, and active state
- Any user input (encoder turn, encoder press, function button short press) wakes screensaver; input is consumed to prevent accidental changes
- Long-press sleep still works from screensaver (not consumed)
- BLE reconnection now resets keyboard/mouse timers and mouse state, preventing stale progress bars
- Encoder interrupts attached after SoftDevice initialization to prevent intermittent GPIOTE disruption

## [1.2.1] - 2026-02-16

### Fixed

- Encoder not responding on first rotation after boot — ISR state machine initialized to `0` instead of actual pin state, causing first few transitions to be swallowed

## [1.2.0] - 2026-02-16

### Added

- **Multi-key slots**: 8 configurable key slots instead of a single keystroke selection
  - Each cycle randomly picks from populated (non-NONE) slots, reducing pattern detectability
  - Default: slot 1 = F15, slots 2-8 = NONE (backwards-compatible behavior)
- **Pre-picked next key**: NORMAL mode display shows which key will fire next
- **Dedicated SLOTS settings mode**: 2-row slot grid with active slot highlight, encoder rotates key, button advances slot
- **Timing profiles**: LAZY / NORMAL / BUSY profiles scale all timing values by a configurable percentage
  - BUSY: shorter key intervals, longer mouse jiggle, shorter mouse idle (more active)
  - LAZY: longer key intervals, shorter mouse jiggle, longer mouse idle (less active)
  - NORMAL: passes base values through unchanged (default)
- **Profile switching**: Encoder rotation in NORMAL mode switches between LAZY ← NORMAL → BUSY (clamped)
- **Profile display**: Selected profile name temporarily replaces uptime line for 3 seconds after switching
- **LAZY % settings mode**: Adjust lazy profile scaling (0-50% in 5% steps, default 15%)
- **BUSY % settings mode**: Adjust busy profile scaling (0-50% in 5% steps, default 15%)

### Changed

- NORMAL mode display restored to original single-line `KB [key]` layout with interval range
- Encoder rotation in NORMAL mode switches timing profiles (LAZY/NORMAL/BUSY, clamped)
- Encoder button in NORMAL mode restored to KB/MS enable combo cycling
- Slot configuration moved to new MODE_SLOTS (accessed via function button cycle)
- Mode cycle: NORMAL → KEY MIN → KEY MAX → SLOTS → MOUSE JIG → MOUSE IDLE → LAZY % → BUSY %
- NORMAL mode display shows profile-adjusted effective values for KB intervals and mouse timing
- `sendKeystroke()` uses pre-picked `nextKeyIndex` instead of random-on-the-fly
- Settings struct: `selectedKeyIndex` replaced with `keySlots[8]` array + `lazyPercent` + `busyPercent`
- Settings magic changed (0x4A494747 → 0x50524F46) — existing settings auto-reset to defaults
- Scheduling functions use profile-adjusted effective values instead of raw settings
- Serial debug output now shows all 8 slot names, profile, lazy%, busy%, and effective values
- Key interval range display: `2.0-6.5s` (removed redundant "s" on min value)

### Fixed

- Display freeze when mouse returns to origin — `returnMouseToOrigin()` was a blocking loop; replaced with non-blocking `MOUSE_RETURNING` state

## [1.1.1] - 2026-02-16

### Changed

- Encoder button now cycles through all KB/MS enable combinations (both ON → KB only → MS only → both OFF) regardless of UI mode
- ON/OFF text labels replaced with bitmap icons (↑ arrow = enabled, ✕ cross = disabled)
- Mouse idle progress bar counts up (charging) instead of down, counts down when moving (draining)
- Activity spinner replaced with scrolling ECG heartbeat pulse trace

## [1.1.0] - 2026-02-16

### Added

- Bluetooth icon bitmap on status bar (solid when connected, 1Hz flash when scanning)

### Changed

- Status bar layout: "GHOST Operator" left-aligned, BT icon + battery right-aligned
- Display labels: "KEY:" → "KB [key]", "MOUSE" → "MS" to prevent text overlap
- Mouse state indicators: "[JIG]" → "[MOV]", "[---]" → "[IDL]"
- Settings page value display: ">>> x.xs <<<" → "> x.xs <" to fit 128px width
- Settings page instruction: "Turn encoder to adjust" → "Turn dial to adjust"

### Fixed

- HID keystrokes sending wrong characters (F15 sent "j") — `keyPress()` expects ASCII but was receiving HID keycodes; switched to `keyboardReport()` for raw HID output

## [1.0.0] - 2026-02-16

First hardware release.

### Added

- BLE HID composite device (keyboard + mouse) via Adafruit Bluefruit
- Rotary encoder menu system with 5 UI modes (Normal, Key Min, Key Max, Mouse Jig, Mouse Idle)
- Persistent settings storage via LittleFS with magic number validation and checksum
- SSD1306 OLED display with real-time countdown bars, uptime, and battery percentage
- 29 selectable keystroke options (F13-F24, ScrLk, Pause, NumLk, L/R Shift, L/R Ctrl, L/R Alt, Esc, Space, Enter, arrow keys, NONE)
- Independent key/mouse enable toggles via encoder button
- Configurable key interval range (0.5s-30s) with random timing between min and max
- Mouse jiggle state machine with configurable jiggle and idle durations
- Mouse return-to-origin after each jiggle phase
- ±20% randomness on mouse timing durations
- Deep sleep via `sd_power_system_off()` (~3µA) with GPIO wake on function button
- Settings auto-save on mode timeout (10s) and before sleep
- Battery voltage monitoring with 8-sample averaging on 3.0V internal reference
- Activity LED with connection-state-dependent blink rate
- Serial debug interface at 115200 baud (commands: h, s, d, z)
- Interrupt-driven encoder with 4-transition-per-detent state machine
- Splash screen on boot with version display
- BLE auto-reconnect to last paired host

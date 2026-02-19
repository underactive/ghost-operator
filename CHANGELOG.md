# Changelog

All notable changes to Ghost Operator will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

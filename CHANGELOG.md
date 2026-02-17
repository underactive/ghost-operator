# Changelog

All notable changes to Ghost Operator will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- **SCREENSAVER settings page**: Configure timeout via encoder (Never / 1 / 5 / 10 / 15 / 30 min, default 10 min)
  - Position dots UI (filled = selected, hollow = unselected)
- **SAVER BRIGHT settings page**: Configure screensaver OLED brightness (10-100% in 10% steps, default 30%)
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
- 10 selectable keystroke options (F13-F15, ScrLk, Pause, NumLk, LShift, LCtrl, LAlt, NONE)
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

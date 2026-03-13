# Plan: CYD Port Phase 1 — Directory Restructure

## Objective
Restructure `src/` into `src/common/`, `src/nrf52/`, and `src/cyd/` directories to support multi-platform builds. Phase 1 focuses on the directory split and ensuring the existing nRF52 build compiles cleanly. No CYD code is written yet — only the skeleton directory.

## Changes

### New files created
- `src/common/hid_keycodes.h` — Standard USB HID keycode constants, platform-gated (empty on nRF52, defined on CYD)
- `src/common/platform_hal.h` — Cross-platform function signatures (HID, display, settings, sleep, sim data)
- `src/nrf52/state.h` — nRF52-specific state (BLE, USB, display hardware objects, game state, encoder ISR)
- `src/nrf52/state.cpp` — nRF52-specific state definitions
- `src/nrf52/settings_nrf52.cpp` — LittleFS load/save + die temperature (extracted from settings.cpp)
- `src/nrf52/sim_data_flash.cpp` — LittleFS persistence for sim data (extracted from sim_data.cpp)
- `src/nrf52/schedule_platform.cpp` — enterLightSleep/exitLightSleep (extracted from schedule.cpp)

### Files moved to src/common/ (portable)
- config.h, keys.h/.cpp, timing.h/.cpp, mouse.h/.cpp, orchestrator.h/.cpp
- sim_data.h/.cpp (data tables only), schedule.h/.cpp (time math only)
- settings.h, state.h/.cpp (portable state only)

### Files moved to src/nrf52/ (platform-specific)
- ghost_operator.ino → ghost_operator.cpp (renamed for PlatformIO subdirectory support)
- hid.h/.cpp, display.h/.cpp, input.h/.cpp, encoder.h/.cpp, battery.h/.cpp
- sleep.h/.cpp, ble_uart.h/.cpp, serial_cmd.h/.cpp, sound.h/.cpp
- icons.h/.cpp, screenshot.h/.cpp, breakout.h/.cpp, snake.h/.cpp, racer.h/.cpp
- ghost_operator_splash.bin/.png

### Modified files
- `platformio.ini` — Added `src_filter`, `-Isrc/common -Isrc/nrf52`, `-DGHOST_PLATFORM_NRF52=1`
- `src/common/keys.h` — Replaced `<bluefruit.h>` with `hid_keycodes.h` + conditional `<bluefruit.h>` on nRF52
- `src/common/mouse.cpp` — Replaced hid.h/display.h/serial_cmd.h with platform_hal.h
- `src/common/orchestrator.cpp` — Replaced hid.h/display.h/serial_cmd.h with platform_hal.h
- `src/common/schedule.cpp` — Replaced platform headers with platform_hal.h, removed enterLightSleep/exitLightSleep
- `src/common/settings_common.cpp` — Removed platform-specific functions (saveSettings, loadSettings, getDieTempCelsius)
- `src/common/state.h` — Removed platform-specific includes and declarations
- `src/common/sim_data.cpp` — Removed LittleFS persistence section
- `src/nrf52/ghost_operator.cpp` — Added `#include <Arduino.h>` and forward declarations

## Dependencies
- Phase 2 (CYD skeleton) depends on this restructure being complete
- No external dependency changes

## Risks / open questions
- `keys.h` conditionally includes `<bluefruit.h>` on nRF52 — this pulls BLE types into common headers but is necessary for HID constant availability
- Common modules still use `Serial.print()` for debug output (Arduino standard, available on both platforms)
- Future work: may want to extract `processCommand()` from ble_uart.cpp to common for config protocol sharing

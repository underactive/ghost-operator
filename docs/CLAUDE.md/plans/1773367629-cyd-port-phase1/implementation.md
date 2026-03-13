# Implementation: CYD Port Phase 1 — Directory Restructure

## Files changed

### Created
- `src/common/hid_keycodes.h`
- `src/common/platform_hal.h`
- `src/nrf52/state.h`
- `src/nrf52/state.cpp`
- `src/nrf52/settings_nrf52.cpp`
- `src/nrf52/sim_data_flash.cpp`
- `src/nrf52/schedule_platform.cpp`

### Moved (git mv) to src/common/
- `config.h`, `keys.h`, `keys.cpp`, `timing.h`, `timing.cpp`
- `mouse.h`, `mouse.cpp`, `orchestrator.h`, `orchestrator.cpp`
- `sim_data.h`, `schedule.h`, `settings.h`
- `state.h` → `common/state.h` (then rewritten to remove platform-specific parts)
- `state.cpp` → `common/state.cpp` (then rewritten to remove platform-specific definitions)
- `settings.cpp` → `common/settings_common.cpp` (then rewritten to remove platform functions)
- `sim_data.cpp` → `common/sim_data.cpp` (then trimmed to data tables only)
- `schedule.cpp` → `common/schedule.cpp` (then trimmed to remove platform sleep functions)

### Moved (git mv) to src/nrf52/
- `ghost_operator.ino` → `ghost_operator.cpp` (renamed)
- `hid.h`, `hid.cpp`, `display.h`, `display.cpp`, `input.h`, `input.cpp`
- `encoder.h`, `encoder.cpp`, `battery.h`, `battery.cpp`
- `sleep.h`, `sleep.cpp`, `ble_uart.h`, `ble_uart.cpp`
- `serial_cmd.h`, `serial_cmd.cpp`, `sound.h`, `sound.cpp`
- `icons.h`, `icons.cpp`, `screenshot.h`, `screenshot.cpp`
- `breakout.h`, `breakout.cpp`, `snake.h`, `snake.cpp`, `racer.h`, `racer.cpp`
- `ghost_operator_splash.bin`, `ghost_operator_splash.png`

### Modified
- `platformio.ini` — Added src_filter, include paths, platform define
- `src/common/keys.h` — hid_keycodes.h + conditional bluefruit.h
- `src/common/mouse.cpp` — platform_hal.h includes
- `src/common/orchestrator.cpp` — platform_hal.h includes
- `src/nrf52/ghost_operator.cpp` — Arduino.h include, forward declarations

## Summary
Restructured the flat `src/` directory into `src/common/` (19 files), `src/nrf52/` (36 files), and `src/cyd/` (empty, ready for Phase 2). The nRF52 build compiles cleanly with the same Flash/RAM footprint (189KB / 21KB). Key architectural decisions:

1. **Include path resolution**: `-Isrc/common -Isrc/nrf52` in build_flags lets `#include "file.h"` resolve correctly via directory-local lookup + fallback to `-I` paths.
2. **Split state.h pattern**: Both `src/common/state.h` and `src/nrf52/state.h` exist; nrf52's version includes common's via `#include "../common/state.h"`. Same-directory lookup resolves the right version automatically.
3. **platform_hal.h**: Common modules include this instead of platform-specific headers (hid.h, display.h, serial_cmd.h). Function signatures declared here, implemented per-platform.
4. **hid_keycodes.h**: Platform-gated — empty on nRF52 (TinyUSB enums would clash with macros), provides constants on CYD.

## Verification
- `pio run -e seeed_xiao_nrf52840` compiles successfully
- RAM: 8.9% (21,036 / 237,568 bytes)
- Flash: 23.3% (189,364 / 811,008 bytes)
- DFU ZIP generated at `.pio/build/seeed_xiao_nrf52840/firmware.zip`

## Follow-ups
- Phase 2: CYD skeleton (main.cpp, NVS settings, serial, stubs)
- Phase 3: TFT display + touchscreen
- Phase 4: NimBLE HID
- Phase 5: Full integration
- Phase 6: Polish
- Consider extracting `processCommand()` from ble_uart.cpp to common for config protocol sharing between platforms

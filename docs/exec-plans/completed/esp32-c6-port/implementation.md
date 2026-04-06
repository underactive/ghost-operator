# ESP32-C6-LCD-1.47 Port — Implementation Record

## Summary

Ported Ghost Operator firmware from the Seeed XIAO nRF52840 to the Waveshare ESP32-C6-LCD-1.47 across all 6 planned phases. The nRF52 build remains byte-identical to the pre-restructure version (no regressions). The C6 target compiles successfully with NimBLE 2.x BLE HID, LVGL v9 color TFT display, JSON config protocol, and NVS persistence.

### Deviations from plan

- **Phase 1:** `sim_data.cpp` was split into `src/common/sim_data.cpp` (const data tables) and platform-specific persistence files (`src/nrf52/sim_data_flash.cpp`, `src/c6/sim_data_nvs.cpp`), as planned. `schedule.cpp` needed a platform-specific companion `schedule_nrf52.cpp` for nRF52 flash operations.
- **Phase 2:** 21 files created in `src/c6/` (plan estimated 12) — `config.h` forwarding header was added, and `state.cpp` was added alongside `state.h`.
- **Phase 4:** Display buffer allocation uses two 20KB partial buffers (320x32x2 bytes each) instead of the planned 10KB (320x16x2) buffers, for smoother rendering. `display.cpp` is 927 lines (plan didn't estimate size). LVGL MIPI generic + ST7789 drivers used together.
- **Phase 6:** Dashboard JSON protocol file named `protocol_json.js` (plan called it `ble_json.js`). Platform auto-detection implemented in `store.js` from the status response rather than requiring manual selection. Modifications were broader than planned — `DeviceSection.vue` and `StatusBar.vue` also needed platform guards.

---

## Files Changed

### New files created

**`src/common/` (18 files):**
- `platform_hal.h` — HAL function declarations (~20 cross-platform functions)
- `hid_keycodes.h` — portable HID key defines (for non-nRF52 platforms)
- `config.h` — shared constants, enums, structs with `#ifdef GHOST_PLATFORM_*` blocks
- `state.h` — platform-independent state extern declarations
- `settings.h` — settings interface declarations
- `settings_common.cpp` — portable settings logic (`loadDefaults()`, `calcChecksum()`, `get/setSettingValue()`, `formatMenuValue()`)
- `sim_data.h` / `sim_data.cpp` — const simulation data tables
- `keys.h` / `keys.cpp` — key data tables, menu items, names
- `mouse.h` / `mouse.cpp` — mouse state machine
- `orchestrator.h` / `orchestrator.cpp` — simulation activity orchestrator
- `timing.h` / `timing.cpp` — profiles, scheduling, formatting
- `schedule.h` / `schedule.cpp` — timed schedule logic

**`src/nrf52/` (36 files):**
- `ghost_operator.cpp` — entry point (renamed from `.ino`)
- `state.h` / `state.cpp` — nRF52-specific state (SSD1306, Bluefruit, TinyUSB objects)
- `settings_nrf52.cpp` — LittleFS settings persistence
- `sim_data_flash.cpp` — LittleFS sim data persistence
- `schedule_nrf52.cpp` — nRF52-specific schedule operations
- `hid.h` / `hid.cpp` — BLE + USB dual-transport HID
- `display.h` / `display.cpp` — SSD1306 OLED rendering (~2900 lines)
- `ble_uart.h` / `ble_uart.cpp` — Bluefruit NUS + text protocol
- `battery.h` / `battery.cpp` — ADC battery reading
- `encoder.h` / `encoder.cpp` — ISR + polling quadrature decode
- `input.h` / `input.cpp` — encoder dispatch, buttons, name editor
- `sleep.h` / `sleep.cpp` — deep sleep sequence
- `sound.h` / `sound.cpp` — piezo buzzer sound profiles
- `serial_cmd.h` / `serial_cmd.cpp` — serial debug commands
- `screenshot.h` / `screenshot.cpp` — PNG encoder + base64 output
- `icons.h` / `icons.cpp` — PROGMEM bitmaps
- `breakout.h` / `breakout.cpp` — Breakout game
- `snake.h` / `snake.cpp` — Snake game
- `racer.h` / `racer.cpp` — Ghost Racer game
- `ghost_operator_splash.bin` / `ghost_operator_splash.png` — splash bitmap

**`src/c6/` (21 files):**
- `main.cpp` — C6 entry point (`setup()` + `loop()`)
- `config.h` — forwarding header to `common/config.h`
- `ble.h` / `ble.cpp` — NimBLE 2.x BLE HID server with composite keyboard+mouse+consumer descriptor
- `ble_uart.h` / `ble_uart.cpp` — NUS service + text protocol + JSON routing
- `hid.cpp` — BLE-only HID HAL (`sendKeystroke()`, `sendMouseMove()`, `sendMouseScroll()`)
- `state.h` / `state.cpp` — C6-specific state (NimBLE characteristic pointers)
- `settings_c6.cpp` — ESP32 Preferences (NVS) settings persistence
- `sim_data_nvs.cpp` — NVS sim data persistence
- `protocol.h` / `protocol.cpp` — JSON command parser + response builder (549 lines)
- `display.h` / `display.cpp` — LVGL v9 ST7789 display (927 lines)
- `led.h` / `led.cpp` — NeoPixel activity LED (blue=KB, green=mouse, breathing=advertising)
- `sleep.h` / `sleep.cpp` — backlight control, schedule-driven sleep stubs
- `serial_cmd.h` / `serial_cmd.cpp` — serial debug commands

**Project root / config:**
- `include/lv_conf.h` — LVGL v9 configuration (ST7789, MIPI drivers, Montserrat fonts, 48KB heap)

**Dashboard:**
- `dashboard/src/lib/protocol_json.js` — JSON protocol query/set/command builders for C6

### Files moved (git mv)

All files from `src/` relocated to `src/common/` or `src/nrf52/`:
- `src/ghost_operator.ino` → `src/nrf52/ghost_operator.cpp` (renamed)
- Platform-independent modules → `src/common/`
- nRF52-specific modules → `src/nrf52/`

### Files modified

- `platformio.ini` — added `[env:c6lcd]` environment (espressif32, NimBLE, LVGL, ArduinoJson, NeoPixel); updated `[env:seeed_xiao_nrf52840]` with `-DGHOST_PLATFORM_NRF52=1`, `-Isrc/common`, `-Isrc/nrf52`, `build_src_filter`
- `src/common/config.h` — added `GHOST_PLATFORM_*` conditional blocks for pins, display dimensions, feature flags (`HAS_BATTERY`, `HAS_SOUND`, `HAS_USB_HID`, `HAS_ENCODER`, `HAS_TOUCH`, `HAS_NEOPIXEL`), `OP_MODE_MAX` define
- `src/common/settings_common.cpp` — uses `OP_MODE_MAX` for operation mode bounds clamping
- `src/nrf52/state.h` — includes `../common/state.h`
- `src/nrf52/ghost_operator.cpp` — added `Arduino.h` include (needed after `.ino` → `.cpp` rename)
- `dashboard/src/lib/store.js` — platform auto-detection from status response, `buildQuery()`/`buildSet()`/`buildAction()` helpers that branch on detected platform (text vs JSON)
- `dashboard/src/App.vue` — platform-aware section visibility (hide battery, sound, DFU for C6)
- `dashboard/src/components/DeviceSection.vue` — C6 platform guards for device-specific fields
- `dashboard/src/components/StatusBar.vue` — hide battery indicator for C6

---

## Verification

| Check | Result |
|-------|--------|
| `pio run -e seeed_xiao_nrf52840` | SUCCESS — 21KB RAM, 192KB Flash (unchanged from pre-restructure) |
| `pio run -e c6lcd` | SUCCESS — 118KB RAM, 989KB Flash |
| `cd dashboard && npm run build` | SUCCESS — 133KB JS, 16KB CSS |
| nRF52 build byte-identical | Confirmed — no regressions from directory restructure |

---

## Follow-ups

- **Hardware testing:** Flash to actual Waveshare ESP32-C6-LCD-1.47 board and verify: display orientation/colors, BLE HID pairing with macOS/Windows/Linux, NeoPixel LED colors, backlight PWM brightness levels
- **ST7789 tuning:** Rotation flags (`MIRROR_X`|`MIRROR_Y`) and gap offset (`y=34`) may need adjustment on actual hardware — currently based on datasheet values
- **DMA SPI transfers:** Display currently uses blocking SPI writes; switch to DMA if frame rate is insufficient on hardware
- **LVGL font subsetting:** Full Montserrat 14/20/28 fonts increase Flash usage; subset to ASCII-only to reduce footprint
- **WiFi OTA updates:** ESP32-C6 has WiFi 6 capability (unused) — could enable OTA firmware updates without BLE DFU
- **macOS companion menubar app:** Planned to use the same JSON protocol over BLE for native configuration
- **Web Bluetooth dashboard path:** Phase 6 modified the USB serial dashboard path; a dedicated Web Bluetooth connection mode for the C6 (bypassing serial entirely) is a natural next step

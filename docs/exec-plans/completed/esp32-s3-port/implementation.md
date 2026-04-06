# ESP32-S3-LCD-1.47 Port — Implementation Record

## Files changed

### New files (src/s3/) — 24 files
- `src/s3/config.h` — S3 platform config wrapper
- `src/s3/main.cpp` — Entry point with USB HID + BLE init, dual-transport detection loop
- `src/s3/ble.h` / `src/s3/ble.cpp` — NimBLE BLE HID (forked from C6)
- `src/s3/ble_uart.h` / `src/s3/ble_uart.cpp` — BLE UART NUS + text/JSON protocol (forked from C6, platform=s3)
- `src/s3/protocol.h` / `src/s3/protocol.cpp` — JSON config protocol (forked from C6, platform=s3)
- `src/s3/hid.cpp` — **Dual-transport HID** (USB via Arduino USBHIDKeyboard/Mouse/Consumer + BLE via NimBLE raw reports)
- `src/s3/display.h` / `src/s3/display.cpp` — LVGL display driver (forked from C6, HSPI bus, S3 pin mapping)
- `src/s3/led.h` / `src/s3/led.cpp` — NeoPixel activity LED on GPIO38 (dim white = USB connected status)
- `src/s3/sleep.h` / `src/s3/sleep.cpp` — Sleep management (light sleep = backlight off + BLE stop)
- `src/s3/settings_s3.cpp` — NVS settings/stats persistence (forked from C6)
- `src/s3/sim_data_nvs.cpp` — Sim data NVS persistence (copy from C6)
- `src/s3/state.h` / `src/s3/state.cpp` — State with `usbHostConnected` flag
- `src/s3/serial_cmd.h` / `src/s3/serial_cmd.cpp` — Serial debug commands (platform=S3, USB host status display)
- `src/s3/ghost_splash.h` — Splash screen bitmap (copy from C6)
- `src/s3/ghost_sprite.h` — Ghost sprite bitmap (copy from C6)
- `src/s3/kbm_icons.h` — Activity icons (copy from C6)

### Modified files
- `platformio.ini` — Added `[env:s3lcd]` environment (ESP32-S3, HSPI, NimBLE, LVGL, NeoPixel, ArduinoJson)
- `src/common/config.h` — Added `GHOST_PLATFORM_S3` feature flags (HAS_USB_HID=1), display config, pin definitions (GPIO 38-48), OP_MODE_MAX guard

## Summary

Ported Ghost Operator to the Waveshare ESP32-S3-LCD-1.47 board as a fourth hardware platform. The key new capability is **dual-transport HID**: USB HID via the board's USB-A male plug (primary when host detected) + BLE HID via NimBLE (fallback when on USB power bank).

The implementation:
1. Created `src/s3/` directory with 24 source files, mostly forked from the existing C6 port
2. `hid.cpp` is the key new file — implements `useUsb()` and `useBle()` transport routing based on `usbHostConnected` state and `btWhileUsb` setting
3. `main.cpp` adds USB HID init via Arduino `USB.h` / `USBHIDKeyboard` / `USBHIDMouse` / `USBHIDConsumerControl`, plus periodic USB host detection polling
4. Display uses HSPI (SPI2) instead of C6's FSPI, with S3-specific pin mapping (MOSI=45, SCLK=40, CS=42, DC=41, RST=39, BL=48)
5. NeoPixel on GPIO38 (vs C6's GPIO8)

No deviations from the plan.

## Verification

- `pio run -e s3lcd` — **SUCCESS** (0 warnings, 29.8% RAM, 32.6% Flash)
- `pio run -e c6lcd` — **SUCCESS** (no regressions)
- `pio run -e seeed_xiao_nrf52840` — **SUCCESS** (no regressions)
- Hardware testing pending (requires physical ESP32-S3-LCD-1.47 board)

## Follow-ups

- [ ] Hardware testing on physical board (USB HID enumeration, BLE pairing, display rendering)
- [ ] USB host detection may need tuning (TinyUSB `tud_connected()` vs Arduino `USB` operator bool)
- [ ] PSRAM display buffer upgrade (currently uses same 32-line partial buffers as C6; S3 has 8MB PSRAM for full-frame)
- [ ] Web Serial dashboard testing over USB-A connection
- [ ] Consider adding USB trident icon to display when wired (currently same icon set as C6)

# ESP32-S3 USB CDC Fix — Implementation Record

## Files changed

- `boards/ghost_operator_esp32_s3_usbcdc.json` — custom ESP32-S3 board definition with the project’s USB defaults
- `platformio.ini` — point `env:s3lcd` at the custom board and keep TinyUSB CDC + device-mode flags explicit
- `src/esp32-s3-lcd-1.47/main.cpp` — compile-time USB config guards, start CDC before `USB.begin()`, update startup log text
- `docs/references/testing-checklist.md` — add manual QA for macOS serial enumeration and Chrome Web Serial visibility
- `docs/PLANS.md` — move the work from active to completed

## Summary

Fixed the ESP32-S3 USB configuration path so the firmware is built for TinyUSB device mode with CDC enabled instead of inheriting the upstream DevKit board’s `ARDUINO_USB_MODE=1` default.

The implementation does three things:
1. Introduces a repo-local S3 board definition that removes the upstream hardcoded USB mode override.
2. Keeps the required USB macros explicit in the S3 env so the build is deterministic and self-documenting.
3. Aligns S3 startup with the Arduino USB examples by opening `Serial` before starting the composite USB stack, then adds compile-time guards so future regressions fail at build time instead of silently shipping HID-only firmware.

## Verification

- `pio run -e s3lcd` — **SUCCESS** (RAM 29.9%, Flash 32.7%)
- `pio test -e native` — **FAILED** on this machine with `Undefined symbols for architecture arm64: "_main"` during link

## Follow-ups

- [ ] Flash the S3 board and confirm macOS exposes a `/dev/cu.usb*` device after boot
- [ ] Confirm Chrome Web Serial can see and select the new S3 serial port
- [ ] Investigate the pre-existing `native` test environment link failure separately if host-side tests are expected to run on this machine

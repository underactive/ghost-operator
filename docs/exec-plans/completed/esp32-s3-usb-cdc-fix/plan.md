# Plan: ESP32-S3 USB CDC Fix

**Goal:** Restore ESP32-S3 USB CDC enumeration so the board appears as a serial device for the Chrome dashboard while keeping USB HID working.
**Status:** Completed
**Started:** 2026-04-07

## Context
The ESP32-S3 firmware was enumerating as a USB HID device, but macOS was not exposing a `/dev/cu.usb*` serial device. Without USB CDC, Chrome could not attach to the board over Web Serial for dashboard configuration and DFU flows.

Prior S3 port work already touched the relevant areas:
- `platformio.ini`
- `src/esp32-s3-lcd-1.47/main.cpp`
- `src/esp32-s3-lcd-1.47/hid.cpp`

Local Arduino ESP32 core headers and examples showed that TinyUSB HID + CDC composite mode on ESP32-S3 requires:
- `ARDUINO_USB_MODE=0`
- `ARDUINO_USB_CDC_ON_BOOT=1`

The upstream PlatformIO `esp32-s3-devkitc-1` board definition hardcodes `ARDUINO_USB_MODE=1`, so a repo-local board definition was needed to avoid fighting that default on every build.

## Objective
Lock the S3 build to the correct USB device mode, make startup order match the working composite-device pattern, and document the manual verification needed for serial enumeration.

## Changes
- `boards/ghost_operator_esp32_s3_usbcdc.json` — repo-local S3 board definition without the upstream HWCDC default
- `platformio.ini` — switch S3 env to the custom board and explicitly define TinyUSB CDC + device-mode flags
- `src/esp32-s3-lcd-1.47/main.cpp` — add compile-time guards for the expected USB mode and align startup order for CDC + HID composite enumeration
- `docs/references/testing-checklist.md` — add a manual S3 serial-enumeration verification item
- `docs/PLANS.md` — track the completed plan state

## Dependencies
- The build flag fix had to land before validation because the wrong USB mode compiles but suppresses the CDC interface.
- Startup changes needed to preserve the user’s in-progress S3 edits and avoid touching unrelated files.

## Risks / open questions
- Hardware confirmation is still required to prove the board enumerates on macOS as a serial device after flashing.
- The repo currently has unrelated uncommitted S3 changes in `hid.cpp`; the implementation avoided overwriting them.

## Steps
- [x] Add the active plan and update the plans index
- [x] Patch the S3 USB mode/startup code
- [x] Build the S3 firmware and run applicable checks
- [x] Move the plan to completed with implementation and audit notes

## Decisions
- 2026-04-07: Use the local Arduino core as the source of truth for ESP32-S3 USB mode semantics instead of relying on prior repo comments.
- 2026-04-07: Add a repo-local S3 board definition so the project no longer inherits the upstream `ARDUINO_USB_MODE=1` default.

# Plan: Migrate Build System from Arduino CLI to PlatformIO

## Objective

Replace the Arduino CLI build system (`build.sh` + `Makefile` wrapping `arduino-cli`) with PlatformIO. This eliminates the sketch directory naming constraint (causing worktree hacks and CI checkout tricks), provides declarative dependency management, and simplifies CI.

**Key challenge:** PlatformIO does not include the Seeed XIAO nRF52840 in its board registry. The Seeed core is a fork of Adafruit's nRF52 Arduino core with a different SoftDevice version (S140 v7.3.0 vs v6.1.1). Requires a custom board definition and framework override.

## Changes

1. **Custom board definition** — `boards/seeed_xiao_nrf52840.json` with correct SoftDevice FWID `0x0123`, linker script, VID/PID
2. **Variant files** — `boards/variants/Seeed_XIAO_nRF52840/variant.h` + `variant.cpp` copied from Seeed core v1.1.12
3. **`platformio.ini`** — declarative config with Seeed framework override, library deps, custom board
4. **Move firmware to `src/`** — all 48 source files (23 .h + 22 .cpp + 1 .ino + 2 assets) via `git mv`
5. **Rewrite `build.sh`** — replace arduino-cli with `pio run`; update artifact paths
6. **Update `Makefile`** — add `monitor` and `clean` targets; delegate to PlatformIO
7. **Update CI workflow** — replace arduino/setup-arduino-cli with Python + PlatformIO; add cache; remove checkout path hack
8. **Update `.gitignore`** — remove Arduino-specific entries, keep PlatformIO entries
9. **Update `CLAUDE.md`** — all file references updated to `src/` prefix; build docs rewritten

## Dependencies

- Steps 1-3 (board + platformio.ini) must exist before step 4 (move files) to avoid a broken intermediate state
- Step 5-9 can proceed after step 4

## Risks / Open Questions

- PlatformIO's nordicnrf52 platform may handle Seeed's SoftDevice S140 v7.3.0 linker script differently than the original Arduino toolchain
- DFU ZIP auto-generation depends on the `nrfutil` upload protocol being correctly triggered by the custom board definition
- First build requires ~200MB download of framework + toolchain

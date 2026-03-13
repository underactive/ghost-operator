# Implementation: Migrate Build System from Arduino CLI to PlatformIO

## Files changed

### New files
- `platformio.ini` — PlatformIO build configuration
- `boards/seeed_xiao_nrf52840.json` — custom board definition (SoftDevice S140 v7.3.0, FWID 0x0123)
- `boards/variants/Seeed_XIAO_nRF52840/variant.h` — pin mappings (copied from Seeed core v1.1.12)
- `boards/variants/Seeed_XIAO_nRF52840/variant.cpp` — pin init (copied from Seeed core v1.1.12)
- `docs/CLAUDE.md/plans/1773351139-platformio-migration/plan.md`
- `docs/CLAUDE.md/plans/1773351139-platformio-migration/implementation.md`

### Moved files (git mv, 48 files)
- All firmware sources moved from project root to `src/`:
  - `ghost_operator.ino` → `src/ghost_operator.ino`
  - 23 `.h` files → `src/*.h`
  - 22 `.cpp` files → `src/*.cpp`
  - `ghost_operator_splash.bin` → `src/ghost_operator_splash.bin`
  - `ghost_operator_splash.png` → `src/ghost_operator_splash.png`

### Modified files
- `build.sh` — rewritten: arduino-cli → `pio run`, updated paths (`src/config.h`, `.pio/build/`)
- `Makefile` — added `monitor` and `clean` targets
- `README.md` — file inventory updated to `src/` paths, added PlatformIO files
- `.github/workflows/release.yml` — removed checkout path hack + working-directory; replaced arduino/setup-arduino-cli with Python + PlatformIO; added cache for `~/.platformio`; updated artifact paths
- `.gitignore` — removed Arduino-specific entries (`*.ino.cpp`, etc.); kept PlatformIO entries
- `CLAUDE.md` — all firmware file references updated to `src/` prefix; Build Configuration rewritten for PlatformIO; Build Instructions rewritten; Known Issues updated (removed Python/folder-name issues, added custom board note); File Inventory updated with new files

## Summary

Full migration from Arduino CLI to PlatformIO as the build system. The key technical decisions:

1. **Custom board JSON** rather than using PlatformIO's built-in nRF52840 boards, because the Seeed XIAO uses SoftDevice S140 v7.3.0 (not v6.1.1) with a different FWID and linker script
2. **Framework override via `platform_packages`** pointing to Seeed's GitHub fork at tag `#1.1.12` to get the correct SoftDevice binary
3. **Variant files committed to repo** so the build is self-contained (no dependency on locally installed Arduino board packages)
4. **No `#include` changes needed** — all firmware files use bare filenames, and PlatformIO adds `src/` to the include search path automatically

## Verification

- [ ] `pio run` — compiles with no errors (requires network for first build to download platform)
- [ ] `.pio/build/seeed_xiao_nrf52840/firmware.zip` exists and is a valid DFU package
- [ ] `./build.sh` — compiles and reports version/size correctly
- [ ] `./build.sh --release` — creates versioned ZIP in `releases/`
- [ ] `make build` — works as expected
- [ ] `make flash` — flashes device successfully (requires device connected)
- [ ] BLE + USB HID functionality confirmed after flashing
- [ ] CI workflow builds correctly on tag push

## Follow-ups

- Monitor PlatformIO's nordicnrf52 platform updates — if they add native Seeed XIAO support, the custom board JSON and framework override could be simplified
- Consider adding `compile_commands.json` generation (`pio run -t compiledb`) for better IDE/LSP support in `src/`
- Add a compile-on-PR CI workflow to catch build breakages before release
- Pin `pip install` versions in CI for reproducible builds

## Audit Fixes

Fixes applied after the 7-subagent post-implementation audit:

1. **[IFC-1] Added `build.bsp.name: "adafruit"` to board JSON** — Without this, PlatformIO loads the nRF5 framework builder instead of Adafruit, causing wrong include paths and compiler flags. Critical fix.
2. **[IFC-2] Added `sd_flags: "-DS140"` to board JSON softdevice section** — Ensures the SoftDevice preprocessor define is present for BLE API headers.
3. **[IFC-3] Added `bootloader.settings_addr: "0xFF000"` to board JSON** — Correct address for nRF52840 (default `0x7F000` is for nRF52832).
4. **[SEC-1] Pinned `lib_deps` versions in `platformio.ini`** — GFX `^1.12.4`, SSD1306 `^2.5.16`, BusIO `^1.17.4`. Prevents silent dependency drift.
5. **[SM-1] Broadened CI cache key to include `boards/**`** — Board definition changes now invalidate the cache.
6. **[SM-2] Removed `.pio/` from CI cache path** — Only `~/.platformio` (platform store) is cached; build output is always fresh for releases.
7. **[SM-3] Removed `restore-keys` fallback** — Prevents stale cache restoration from mismatched `platformio.ini` versions.
8. **[QA-1] Removed misleading CI comment from `platformio.ini`** — Comment claimed CI overrides build_flags but it doesn't.
9. **[QA-2/DX-1] Updated README.md file inventory** — All firmware paths updated to `src/` prefix, added PlatformIO config files.
10. **[DX-2] Removed redundant `pio run --target clean` from Makefile** — `rm -rf .pio/` is sufficient and works without PlatformIO installed.

### Verification checklist for fixes
- [ ] Board JSON `build.bsp.name` set to `"adafruit"` — verify `pio run` uses Adafruit framework builder (check for `adafruit.py` in verbose output)
- [ ] Board JSON `sd_flags` set to `"-DS140"` — verify `-DS140` appears in compiler flags during build
- [ ] `lib_deps` versions resolve correctly on first build
- [ ] CI cache invalidates when `boards/seeed_xiao_nrf52840.json` changes
- [ ] `make clean` works without PlatformIO installed
- [ ] README.md file inventory matches actual `src/` layout

### Unresolved items
- **SEC-2** (mutable git tag): Accepted — pinning to SHA would hurt readability; Seeed-Studio org compromise is low probability
- **SEC-3** (unpinned pip install): Deferred — low risk for this project's release cadence
- **SEC-4** (expression injection): Accepted — tags controlled by collaborators, constrained to `v*` pattern
- **TC-1** (no PR CI trigger): Deferred — separate workflow improvement, not part of this migration
- **DX-3** (no CHANGELOG migration note): Will be addressed in next version bump

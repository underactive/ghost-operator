# Audit Report: PlatformIO Migration

## Files changed

Files where findings were flagged (including immediate dependents not in the original implementation):

- `boards/seeed_xiao_nrf52840.json`
- `platformio.ini`
- `.github/workflows/release.yml`
- `Makefile`
- `build.sh`
- `README.md`
- `CLAUDE.md`

---

## 1. QA Audit

### [FIXED] QA-1: Misleading CI comment in `platformio.ini`
Comment claimed "CI uses build_flags override" but CI runs bare `pio run`. Removed misleading comment.

### [FIXED] QA-2: README.md has stale file paths
File inventory in README.md still used root-level paths. Updated all to `src/` prefix with new PlatformIO files added.

### QA-3: `build.sh --flash` assumes macOS-only port paths (pre-existing)
`find /dev -name "tty.usbmodem*"` is macOS-specific. Not a regression from old build.sh.

### QA-4: DFU ZIP generation depends on undeclared platform behavior
`build.sh` asserts `firmware.zip` exists after `pio run`. This depends on `nrfutil` upload protocol triggering DFU package generation. The `die` check catches failure. Low risk.

---

## 2. Security Audit

### [FIXED] SEC-1: `lib_deps` not version-pinned
Libraries resolved to "latest" on every fresh build. Pinned to `^major.minor.patch` using installed versions (GFX 1.12.4, SSD1306 2.5.16, BusIO 1.17.4).

### SEC-2: `platform_packages` git tag is mutable reference
`#1.1.12` is a git tag that could theoretically be force-pushed. Acceptable risk given it requires compromising the Seeed-Studio GitHub org. Pinning to full SHA would be safer but less readable.

### SEC-3: `pip install` without version pins in CI
`pip install platformio adafruit-nrfutil` uses latest. Low risk for this project; pinning deferred.

### SEC-4: Expression injection via `steps.version.outputs.version`
Version from git tag interpolated into shell. Low risk since tags are controlled by collaborators and constrained to `v*` pattern.

---

## 3. Interface Contract Audit

### [FIXED] IFC-1 (CRITICAL): Missing `build.bsp.name` in board JSON
Without `"bsp": {"name": "adafruit"}`, PlatformIO defaults to the nRF5 framework builder instead of the Adafruit builder, causing completely wrong include paths and compiler flags. Added.

### [FIXED] IFC-2: Missing `sd_flags` in board JSON softdevice section
Without `"sd_flags": "-DS140"`, the SoftDevice preprocessor define may be missing, breaking BLE API headers. Added.

### [FIXED] IFC-3: Missing `bootloader.settings_addr` in board JSON
Default `0x7F000` is wrong for nRF52840 (correct: `0xFF000`). Only affects bootloader settings merge; added for correctness.

### [FIXED] IFC-4: CI comment claims build_flags override (same as QA-1)
Addressed by removing misleading comment.

---

## 4. State Management Audit

### [FIXED] SM-1: CI cache key too narrow
Cache key only hashed `platformio.ini`, missing `boards/**` changes. Broadened to `hashFiles('platformio.ini', 'boards/**')`.

### [FIXED] SM-2: CI cache included `.pio/` build output
Caching `.pio/` risks stale object files in release builds. Changed to cache only `~/.platformio` (platform/tool store).

### SM-3: `restore-keys` prefix match could restore stale packages
Removed `restore-keys: pio-` fallback to avoid stale cache restoration.

### SM-4: Stale `build/` directory on disk (informational)
Old Arduino CLI build artifacts persist locally. Doesn't affect new builds. Clean up manually with `rm -rf build/`.

---

## 5. Resource & Concurrency Audit

No findings. Variant files confirmed byte-for-byte identical to Seeed core originals. Board JSON memory sizes, clock frequency, and pin mappings all correct.

---

## 6. Testing Coverage Audit

### TC-1: No CI trigger on PR/push-to-branch
Build breakages only caught at release time. Pre-existing limitation; adding a separate compile-on-PR workflow deferred.

### TC-2: CI calls `pio run` directly, bypassing `build.sh` existence check
If DFU ZIP generation fails silently, CI `cp` would fail. Low risk since PlatformIO errors are loud.

### TC-3: Unpinned `pip install platformio` in CI
Could break on major PlatformIO releases. Low risk; pinning deferred.

---

## 7. DX & Maintainability Audit

### [FIXED] DX-1: README.md stale file inventory (same as QA-2)
Addressed with full path updates and new PlatformIO file entries.

### [FIXED] DX-2: Redundant `pio run --target clean` in Makefile
Removed the `pio run --target clean` call since `rm -rf .pio/` is sufficient and doesn't require PlatformIO to be installed.

### DX-3: No migration note in CHANGELOG
Contributors who `git pull` and run old `make build` will get `pio: command not found`. Error is clear enough to guide them to `make setup`. Will be addressed in version bump.

# Build Instructions & Configuration

## PlatformIO CLI (recommended)

```bash
make setup    # Install PlatformIO + adafruit-nrfutil
make build    # Compile firmware (auto-installs board + libs on first build)
make flash    # Compile + flash via USB serial DFU
make release  # Compile + create versioned DFU ZIP in releases/
make monitor  # Open serial monitor at 115200 baud
make clean    # Remove build artifacts
make test     # Run native Unity tests
```

PlatformIO auto-downloads the Seeed nRF52 framework and library dependencies on first build.

## PlatformIO IDE

1. Install the PlatformIO IDE extension for VS Code
2. Open the project folder — PlatformIO detects `platformio.ini` automatically
3. Use the PlatformIO toolbar to build, upload, and monitor

## Build configuration

### PlatformIO settings
- **Platform:** `nordicnrf52` with Seeed framework override (S140 v7.3.0)
- **Config file:** `platformio.ini` — declarative deps, board, and build flags
- **Custom board:** `boards/seeed_xiao_nrf52840.json` — not in PlatformIO registry
- **Variant files:** `boards/variants/Seeed_XIAO_nRF52840/variant.h` + `variant.cpp`
- **Framework override:** `platform_packages` points to `Seeed-Studio/Adafruit_nRF52_Arduino.git#1.1.12`
- **Optimization:** `build_unflags = -Ofast`, uses `-Os` to match Arduino IDE behavior
- **`lib_archive = no`:** Critical — prevents TinyUSB driver table stripping
- **`extra_script.py`:** Adds CC310 crypto library path
- **DFU ZIP generation:** Auto-generated alongside `.hex` during compilation

### Board JSON gotchas

The custom board JSON requires these fields (missing any breaks the build):
- **`build.board`:** `"Seeed_XIAO_nRF52840"` — generates the `-DARDUINO_*` define
- **`build.extra_flags`:** Must include `-DUSE_TINYUSB` and `-DARDUINO_Seeed_XIAO_nRF52840`
- **`build.bsp.name`:** `"adafruit"` — routes to Adafruit framework builder
- **`build.softdevice.sd_flags`:** `"-DS140"`
- **`build.usb_product`:** Must be present — triggers USB VID/PID injection
- **`upload.speed`:** `115200` — passed as `-b` to `adafruit-nrfutil`

### Settings magic number

`SETTINGS_MAGIC` in `src/common/config.h` encodes the settings struct schema version. Bump it when the `Settings` struct layout changes to trigger safe `loadDefaults()` instead of reading corrupt data.

### Environment variables

No environment variables in local builds. Configuration from `src/common/config.h` defines and git tags.

**CI/CD secrets:** Only `GITHUB_TOKEN` (auto-provided by GitHub Actions) for release creation.

## Troubleshooting

- **First build slow** → PlatformIO downloads framework + toolchain (~200MB)
- **"Board not found"** → Ensure `boards/seeed_xiao_nrf52840.json` exists
- **SoftDevice mismatch** → Verify `platform_packages` points to Seeed fork `#1.1.12`
- **USB not enumerating** → Verify `lib_archive = no` in `platformio.ini`
- **`#error "Unsupported board"`** → Board JSON missing `build.board` or `-DARDUINO_*` flag
- **`cannot find -lnrf_cc310`** → `extra_scripts = extra_script.py` missing
- **`--singlebank is not a valid integer`** → Board JSON missing `upload.speed: 115200`

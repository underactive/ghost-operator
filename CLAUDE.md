# CLAUDE.md - Ghost Operator Project Context

## Project Overview

**Ghost Operator** is a BLE keyboard/mouse hardware device built on the Seeed XIAO nRF52840. It prevents screen lock and idle timeout by sending periodic keystrokes and mouse movements over Bluetooth.

**Current Version:** 1.1.1
**Status:** Production-ready

---

## Hardware

### Microcontroller
- **Seeed XIAO nRF52840**
- ARM Cortex-M4 @ 64MHz
- BLE 5.0
- 1MB Flash, 256KB RAM
- Built-in LiPo charger (USB-C)
- Deep sleep: ~3µA

### Components (7 total)
| Ref | Component | Purpose |
|-----|-----------|---------|
| U1 | XIAO nRF52840 | MCU + BLE |
| DISP1 | SSD1306 OLED 128x64 | Display (I2C) |
| ENC1 | KY-040 Rotary Encoder | Input + key selection |
| SW1 | Tactile button 6x6mm | Mode cycle / sleep |
| BT1 | LiPo 3.7V 1000mAh | Power |
| C1 | 100nF ceramic | Decoupling |
| C2 | 10µF electrolytic | Decoupling |

### Pin Assignments
| Pin | Function | Notes |
|-----|----------|-------|
| D0 | Encoder CLK (A) | Interrupt-driven |
| D1 | Encoder DT (B) | Interrupt-driven |
| D2 | Encoder SW | Push button |
| D3 | Function button | Mode/sleep |
| D4 | I2C SDA | OLED |
| D5 | I2C SCL | OLED |
| D6 | LED (optional) | Activity indicator |
| D7-D10 | Unused | Available for expansion |
| BAT+/- | Battery | Direct to LiPo |

---

## Firmware Architecture

### Core Files
- `ghost_operator.ino` - Main firmware (single file)

### Dependencies
- Adafruit Bluefruit (built into Seeed nRF52 core)
- Adafruit GFX Library
- Adafruit SSD1306
- Adafruit LittleFS (built into core)

### Key Subsystems

#### 1. BLE HID
- Presents as composite keyboard + mouse device
- Device name: "GhostOperator"
- Auto-reconnects to last paired host

#### 2. UI Modes
```
enum UIMode {
  MODE_NORMAL,      // Main display, encoder selects key
  MODE_KEY_MIN,     // Adjust minimum key interval
  MODE_KEY_MAX,     // Adjust maximum key interval
  MODE_MOUSE_JIG,   // Adjust jiggle duration
  MODE_MOUSE_IDLE   // Adjust idle duration
};
```
Function button cycles through modes. 10-second timeout returns to NORMAL.

#### 3. Settings Storage
```cpp
struct Settings {
  uint32_t magic;              // 0x4A494747 "JIGG"
  uint32_t keyIntervalMin;     // ms
  uint32_t keyIntervalMax;     // ms
  uint32_t mouseJiggleDuration; // ms
  uint32_t mouseIdleDuration;   // ms
  uint8_t selectedKeyIndex;
  uint8_t checksum;
};
```
Saved to `/settings.dat` via LittleFS. Survives sleep and power-off.

#### 4. Timing Behavior
- **Keyboard:** Random interval between MIN and MAX each cycle (no additional randomness)
- **Mouse:** Jiggle duration then idle duration, ±20% randomness on both

#### 5. Mouse State Machine
```cpp
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING };
```
- Tracks net displacement during jiggle
- Returns to approximate origin after each jiggle phase

#### 6. Power Management
- `sd_power_system_off()` for deep sleep
- Wake via GPIO sense on function button (P0.29)
- Sleep triggered by 3-second long press

### Encoder Handling
- Interrupt-driven state machine
- 4 transitions per detent (divide by 4 for clicks)
- Exponential smoothing not used (digital precision)

---

## Display Layout

### Normal Mode
```
GHOST Operator          ᛒ 85%
─────────────────────────────
KB [F15]  2.0s-6.5s        ↑
████████████░░░░░░░░░░░  3.2s
─────────────────────────────
MS [MOV]  15s/30s          ↑
██████░░░░░░░░░░░░░░░░░  8.5s
─────────────────────────────
Up: 02:34:15          ~^~_~^~
```

### Settings Mode
```
MODE: KEY MIN           [K]
─────────────────────────────

      > 2.0s <

0.5s ████████░░░░░░░░░░░ 30s
─────────────────────────────
Turn dial to adjust
```

---

## Version History

| Ver | Changes |
|-----|---------|
| 1.0.0 | Initial hardware release - encoder menu, flash storage, BLE HID |
| 1.1.0 | Display overhaul, BT icon, HID keycode fix |
| 1.1.1 | Icon-based status, ECG pulse, KB/MS combo cycling |

---

## Known Issues / Limitations

1. **Battery reading approximate** - ADC calibration varies per chip
2. **No hardware power switch** - Sleep mode is the off switch
3. **Encoder must use Seeed nRF52 core** - mbed core incompatible with Bluefruit
4. **Python required for compilation** - Seeed toolchain dependency
5. **AR_INTERNAL_3_6 not available** - Use AR_INTERNAL_3_0 on Seeed core

---

## Common Modifications

### Add a new keystroke option
1. Add entry to `AVAILABLE_KEYS[]` array
2. Include HID keycode and display name
3. Set `isModifier` flag appropriately

### Change timing range
Modify these defines:
```cpp
#define VALUE_MIN_MS      500UL    // 0.5 seconds
#define VALUE_MAX_MS      30000UL  // 30 seconds
#define VALUE_STEP_MS     500UL    // 0.5 second steps
```

### Change mouse randomness
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

### Add new UI mode
1. Add to `UIMode` enum
2. Add name to `MODE_NAMES[]`
3. Handle in `handleEncoder()` switch
4. Handle in `drawSettingsMode()` if needed
5. Update `MODE_COUNT`

### Change BLE device name
```cpp
#define DEVICE_NAME "GhostOperator"
```

---

## File Inventory

| File | Purpose |
|------|---------|
| `ghost_operator.ino` | Firmware source |
| `schematic_v8.svg` | Circuit schematic |
| `schematic_interactive_v3.html` | Interactive documentation |
| `README.md` | Technical documentation |
| `USER_MANUAL.md` | End-user guide |
| `CHANGELOG.md` | Version history (semver) |
| `CLAUDE.md` | This file |

---

## Build Instructions

### Arduino IDE
1. Add board URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
2. Install "Seeed nRF52 Boards" from Board Manager
3. Install libraries: Adafruit GFX, Adafruit SSD1306
4. Select board: Seeed nRF52 Boards → Seeed XIAO nRF52840
5. Select port and upload

### PlatformIO
```bash
pio run -t upload
```

### Troubleshooting Build
- **"python not found"** → `sudo ln -s $(which python3) /usr/local/bin/python`
- **"AR_INTERNAL_3_6 not declared"** → Change to `AR_INTERNAL_3_0`
- **Folder name has spaces** → Rename to use underscores

---

## Testing Checklist

- [ ] Display initializes and shows splash screen
- [ ] BLE advertises as "GhostOperator"
- [ ] Pairs with computer
- [ ] Encoder rotates through keystrokes (NORMAL mode)
- [ ] Encoder button cycles KB/MS enable combos
- [ ] Function button cycles modes
- [ ] Settings values change in settings modes
- [ ] Settings persist after sleep
- [ ] Long press enters sleep (display off)
- [ ] Button press wakes from sleep
- [ ] Keystrokes detected on host (use key test website)
- [ ] Mouse movement visible on host
- [ ] Battery percentage displays (with battery connected)

---

## Future Improvements (Ideas)

- [ ] RGB LED for status (uses D7)
- [ ] Buzzer feedback on mode change
- [ ] Multiple profiles (save/load different configs)
- [ ] Adjustable mouse movement amplitude
- [ ] Configurable BLE device name
- [ ] OTA firmware updates
- [ ] Web-based configuration (BLE UART)
- [ ] Scheduled on/off times
- [ ] Activity logging to flash

---

## Serial Debug Commands

At 115200 baud:
| Key | Action |
|-----|--------|
| h | Help |
| s | Status report |
| d | Dump settings |
| z | Sleep |

---

## Contact / Origin

Created with Claude (Anthropic)  
TARS Settings: Humor 80%, Honesty 100%

*"Fewer parts, more flash"*

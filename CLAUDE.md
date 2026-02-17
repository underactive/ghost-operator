# CLAUDE.md - Ghost Operator Project Context

## Project Overview

**Ghost Operator** is a BLE keyboard/mouse hardware device built on the Seeed XIAO nRF52840. It prevents screen lock and idle timeout by sending periodic keystrokes and mouse movements over Bluetooth.

**Current Version:** 1.3.0
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
  MODE_NORMAL,      // Shows next key; encoder switches profile (LAZY/NORMAL/BUSY)
  MODE_KEY_MIN,     // Adjust minimum key interval
  MODE_KEY_MAX,     // Adjust maximum key interval
  MODE_SLOTS,       // Configure 8 key slots (encoder=key, button=next slot)
  MODE_MOUSE_JIG,   // Adjust jiggle duration
  MODE_MOUSE_IDLE,  // Adjust idle duration
  MODE_LAZY_PCT,    // Adjust lazy profile percentage (0-50%)
  MODE_BUSY_PCT,    // Adjust busy profile percentage (0-50%)
  MODE_SAVER_TIMEOUT, // Adjust screensaver timeout (Never/1/5/10/15/30 min)
  MODE_SAVER_BRIGHT  // Adjust screensaver brightness (10-100%, default 30%)
};
```
Function button cycles through modes. 10-second timeout returns to NORMAL.

#### 3. Settings Storage
```cpp
#define NUM_SLOTS 8

struct Settings {
  uint32_t magic;              // 0x50524F46 "PROF"
  uint32_t keyIntervalMin;     // ms
  uint32_t keyIntervalMax;     // ms
  uint32_t mouseJiggleDuration; // ms
  uint32_t mouseIdleDuration;   // ms
  uint8_t keySlots[NUM_SLOTS]; // index into AVAILABLE_KEYS per slot
  uint8_t lazyPercent;         // 0-50, step 5, default 15
  uint8_t busyPercent;         // 0-50, step 5, default 15
  uint8_t saverTimeout;        // index into SAVER_MINUTES[] (0=Never..5=30min)
  uint8_t saverBrightness;     // 10-100, step 10, default 30
  uint8_t checksum;            // must remain last
};
```
Saved to `/settings.dat` via LittleFS. Survives sleep and power-off.
Default: slot 0 = F15 (index 0), slots 1-7 = NONE (index 9), lazy/busy = 15%, screensaver = 10 min, brightness = 30%.

#### 4. Timing Profiles
```cpp
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
```
- Profiles scale base timing values by a configurable percentage at scheduling time
- BUSY: shorter KB intervals (−%), longer mouse jiggle (+%), shorter mouse idle (−%)
- LAZY: longer KB intervals (+%), shorter mouse jiggle (−%), longer mouse idle (+%)
- NORMAL: passes base values through unchanged (default)
- `applyProfile()` helper applies directional scaling; `effective*()` accessors wrap it
- Profile resets to NORMAL on sleep/wake; lazy% and busy% persist in flash
- Selected via encoder rotation in NORMAL mode (clamped, not wrapping); name shown on uptime line for 3s

#### 5. Timing Behavior
- **Keyboard:** Random interval between effective MIN and MAX each cycle (profile-adjusted)
- **Mouse:** Effective jiggle then idle durations, ±20% randomness on both (profile-adjusted)

#### 6. Mouse State Machine
```cpp
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };
```
- Tracks net displacement during jiggle
- Non-blocking return to approximate origin via MOUSE_RETURNING state

#### 7. Power Management
- `sd_power_system_off()` for deep sleep
- Wake via GPIO sense on function button (P0.29)
- Sleep triggered by 3-second long press

#### 8. Screensaver
- Overlay state (`screensaverActive` flag), not a UIMode — gates display rendering
- Activates only on top of `MODE_NORMAL` after configurable timeout (Never/1/5/10/15/30 min)
- Minimal display: centered key label + 1px progress bar, mouse state + 1px bar, battery only if <15%
- OLED contrast dimmed to configurable brightness (10-100%, default 30%) via `SSD1306_SETCONTRAST`
- Any input (encoder, buttons) wakes screensaver — input is consumed, not passed through
- Long-press sleep still works from screensaver (not consumed)
- `MODE_SAVER_TIMEOUT` is the settings page for configuring timeout
- `MODE_SAVER_BRIGHT` is the settings page for configuring screensaver OLED brightness

### Encoder Handling
- Interrupt-driven state machine
- Interrupts attached AFTER SoftDevice init (`setupBLE()`) to prevent GPIOTE disruption
- 4 transitions per detent (divide by 4 for clicks)
- Exponential smoothing not used (digital precision)

---

## Display Layout

### Normal Mode
```
GHOST Operator          ᛒ 85%
─────────────────────────────
KB [F15] 1.7-5.5s        ↑     ← effective (profile-adjusted) range
████████████░░░░░░░░░░░  3.2s
─────────────────────────────
MS [MOV]  17s/25s          ↑   ← effective durations
██████░░░░░░░░░░░░░░░░░  8.5s
─────────────────────────────
BUSY                  ~^~_~^~  ← profile name (3s) or "Up: HH:MM:SS"
```
`KB [F15]` shows the pre-picked next key. Changes after each keypress.
Footer shows profile name for 3 seconds after switching, then reverts to uptime.

### Slots Mode
```
MODE: SLOTS             [3/8]
─────────────────────────────
  F15 F14 --- ---
  --- --- --- ---
─────────────────────────────
Turn=key  Press=slot
Func=exit
```
Active slot rendered with inverted colors (white rect, black text).

### Settings Mode
```
MODE: KEY MIN           [K]
─────────────────────────────

      > 2.0s <

0.5s ████████░░░░░░░░░░░ 30s
─────────────────────────────
Turn dial to adjust
```

### Screensaver Mode (overlay)
```
                              (blank)

             [F15]            ← next key label, centered
  ████████████████░░░░░░░░░░  ← 1px high KB progress bar (full width)

             [IDL]            ← mouse state, centered
  ██████░░░░░░░░░░░░░░░░░░░░  ← 1px high MS progress bar (full width)

              12%             ← battery % (ONLY if <15%)
                              (blank)
```

---

## Version History

| Ver | Changes |
|-----|---------|
| 1.0.0 | Initial hardware release - encoder menu, flash storage, BLE HID |
| 1.1.0 | Display overhaul, BT icon, HID keycode fix |
| 1.1.1 | Icon-based status, ECG pulse, KB/MS combo cycling |
| 1.2.0 | Multi-key slots, timing profiles (LAZY/NORMAL/BUSY), SLOTS mode |
| 1.2.1 | Fix encoder initial state sync bug |
| 1.3.0 | Screensaver mode for OLED burn-in prevention |

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
4. Add 3-char short name to `SHORT_NAMES[]` in `slotName()` function

### Change timing range
Modify these defines:
```cpp
#define VALUE_MIN_MS          500UL    // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL  // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL  // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL    // 0.5 second steps
```

### Change mouse randomness
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

### Add new UI mode
1. Add to `UIMode` enum
2. Add name to `MODE_NAMES[]`
3. Handle in `handleEncoder()` switch
4. Add draw function or handle in `drawSettingsMode()`
5. Update `updateDisplay()` routing if using custom draw function
6. `MODE_COUNT` auto-updates (last enum value)

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
- [ ] NORMAL mode encoder rotation switches profiles (LAZY/NORMAL/BUSY, clamped)
- [ ] Profile name appears on uptime line for 3 seconds after switching
- [ ] NORMAL mode KB/MS values update to reflect active profile
- [ ] Encoder button cycles KB/MS enable combos in NORMAL mode
- [ ] Function button cycles modes (NORMAL → KEY MIN → KEY MAX → SLOTS → MOUSE JIG → MOUSE IDLE → LAZY % → BUSY % → SCREENSAVER → SAVER BRIGHT)
- [ ] SLOTS mode: encoder rotates key for active slot, button advances slot cursor
- [ ] Settings values change in settings modes
- [ ] Settings persist after sleep
- [ ] Long press enters sleep (display off)
- [ ] Button press wakes from sleep
- [ ] Keystrokes detected on host (use key test website)
- [ ] Mouse movement visible on host
- [ ] Battery percentage displays (with battery connected)
- [ ] LAZY %: encoder adjusts 0-50% in 5% steps
- [ ] BUSY %: encoder adjusts 0-50% in 5% steps
- [ ] Scheduling uses profile-adjusted values (verify via serial `s`)
- [ ] Profile does NOT modify base settings (enter KEY MIN → shows original, not adjusted)
- [ ] Sleep + wake → lazy% and busy% persist, profile resets to NORMAL
- [ ] Serial `d` → prints profile, lazy%, busy%, effective values
- [ ] SCREENSAVER timeout: encoder adjusts Never/1/5/10/15/30 min with position dots
- [ ] Screensaver activates after configured timeout with no input (minimal display)
- [ ] Screensaver shows centered key label + 1px bar, mouse state + 1px bar
- [ ] Screensaver battery warning appears only when <15%
- [ ] Any input wakes screensaver without side effects (consumed)
- [ ] Long-press sleep works from screensaver
- [ ] "Never" disables screensaver
- [ ] SAVER BRIGHT: encoder adjusts 10-100% in 10% steps with progress bar
- [ ] Screensaver dims OLED to configured brightness, restores on wake
- [ ] Serial `d` → prints screensaver timeout, brightness, and active state
- [ ] Encoder responsive immediately after boot (ISR attached after SoftDevice init)
- [ ] BLE reconnect resets progress bars (no stale countdown at 0% or 100%)

---

## Future Improvements (Ideas)

- [ ] RGB LED for status (uses D7)
- [ ] Buzzer feedback on mode change
- [x] ~~Multiple profiles~~ → Implemented as timing profiles (LAZY/NORMAL/BUSY) in v1.2.0
- [ ] Adjustable mouse movement amplitude
- [ ] Configurable BLE device name
- [ ] OTA firmware updates
- [ ] Web-based configuration (BLE UART)
- [ ] Scheduled on/off times
- [ ] Activity logging to flash
- [x] ~~Display idle timeout / dimming~~ → Implemented as screensaver mode in v1.3.0 (minimal pixel display after configurable timeout)

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

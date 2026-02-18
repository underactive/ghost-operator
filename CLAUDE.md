# CLAUDE.md - Ghost Operator Project Context

## Project Overview

**Ghost Operator** is a BLE keyboard/mouse hardware device built on the Seeed XIAO nRF52840. It prevents screen lock and idle timeout by sending periodic keystrokes and mouse movements over Bluetooth.

**Current Version:** 1.6.0
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
Modular architecture — 15 `.h/.cpp` module pairs + lean `.ino` entry point:

- `ghost_operator.ino` - Entry point: setup(), loop(), BLE setup/callbacks (~297 lines)
- `config.h` - All `#define` constants, enums, structs (header-only)
- `keys.h/.cpp` - Const data tables (AVAILABLE_KEYS, MENU_ITEMS, names)
- `icons.h/.cpp` - PROGMEM bitmaps (splash, BT icon, arrows)
- `state.h/.cpp` - All ~60 mutable globals as `extern` declarations
- `settings.h/.cpp` - Flash persistence + value accessors
- `timing.h/.cpp` - Profiles, scheduling, formatting
- `encoder.h/.cpp` - ISR + polling quadrature decode
- `battery.h/.cpp` - ADC battery reading
- `hid.h/.cpp` - Keystroke sending + key selection
- `mouse.h/.cpp` - Mouse state machine with sine easing
- `sleep.h/.cpp` - Deep sleep sequence
- `screenshot.h/.cpp` - PNG encoder + base64 serial output
- `serial_cmd.h/.cpp` - Serial debug commands + status
- `input.h/.cpp` - Encoder dispatch, buttons, name editor
- `display.h/.cpp` - All rendering (~800 lines, largest module)

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
```cpp
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_COUNT };
```
- **NORMAL**: Live status; encoder switches profile, button cycles KB/MS combos
- **MENU**: Scrollable settings menu; encoder navigates/edits, button selects/confirms
- **SLOTS**: 8-key slot editor; encoder cycles key, button advances slot
- **NAME**: BLE device name editor; encoder cycles character, button advances position
- Function button toggles NORMAL ↔ MENU; from SLOTS/NAME returns to MENU
- 30-second timeout returns to NORMAL from MENU, SLOTS, or NAME

#### 2a. Menu System
Data-driven architecture using `MenuItem` struct array (22 entries: 6 headings + 16 items):
```cpp
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME };
```
- `getSettingValue(settingId)` / `setSettingValue(settingId, value)` — generic accessors (with key min/max cross-constraint)
- `formatMenuValue(settingId, format)` — formats for display using `formatDuration()`, `N%`, `-N%`, `SAVER_NAMES[]`, `Npx`, or `ANIM_NAMES[]`
- `moveCursor(direction)` — skips headings, clamps at bounds, manages viewport scroll
- `FMT_PERCENT_NEG` items: encoder direction and arrow bounds are inverted so displayed value direction matches encoder rotation

#### 3. Settings Storage
```cpp
#define NUM_SLOTS 8

struct Settings {
  uint32_t magic;              // 0x50524F48 (bumped for animStyle)
  uint32_t keyIntervalMin;     // ms
  uint32_t keyIntervalMax;     // ms
  uint32_t mouseJiggleDuration; // ms
  uint32_t mouseIdleDuration;   // ms
  uint8_t keySlots[NUM_SLOTS]; // index into AVAILABLE_KEYS per slot
  uint8_t lazyPercent;         // 0-50, step 5, default 15
  uint8_t busyPercent;         // 0-50, step 5, default 15
  uint8_t saverTimeout;        // index into SAVER_MINUTES[] (0=Never..5=30min)
  uint8_t saverBrightness;     // 10-100, step 10, default 20
  uint8_t displayBrightness;   // 10-100, step 10, default 80
  uint8_t mouseAmplitude;      // 1-5, step 1, default 1 (pixels per movement step)
  uint8_t animStyle;           // 0-5 index into ANIM_NAMES[] (default 2 = Ghost)
  char    deviceName[15];      // 14 chars + null terminator (BLE device name)
  uint8_t checksum;            // must remain last
};
```
Saved to `/settings.dat` via LittleFS. Survives sleep and power-off.
Default: slot 0 = F15 (index 2), slots 1-7 = NONE (index 28), lazy/busy = 15%, screensaver = 30 min, saver brightness = 20%, display brightness = 80%, mouse amplitude = 1px, animation = Ghost, device name = "GhostOperator".

#### 4. Timing Profiles
```cpp
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
```
- Profiles scale base timing values by a configurable percentage at scheduling time
- BUSY: shorter KB intervals (−%), longer mouse movement (+%), shorter mouse idle (−%)
- LAZY: longer KB intervals (+%), shorter mouse movement (−%), longer mouse idle (+%)
- NORMAL: passes base values through unchanged (default)
- `applyProfile()` helper applies directional scaling; `effective*()` accessors wrap it
- Profile resets to NORMAL on sleep/wake; lazy% and busy% persist in flash
- Selected via encoder rotation in NORMAL mode (clamped, not wrapping); name shown on uptime line for 3s

#### 5. Timing Behavior
- **Keyboard:** Random interval between effective MIN and MAX each cycle (profile-adjusted)
- **Mouse:** Effective movement then idle durations, ±20% randomness on both (profile-adjusted)

#### 6. Mouse State Machine
```cpp
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };
```
- Tracks net displacement during movement
- Non-blocking return to approximate origin via MOUSE_RETURNING state
- JIGGLING uses sine ease-in-out velocity profile: `amp = mouseAmplitude * sin(π × progress)` where progress goes 0→1 over the jiggle duration. Movement ramps from zero → peak → zero. Steps with zero amplitude are skipped (natural pause at start/end). `pickNewDirection()` stores unit vectors (-1/0/+1); amplitude is applied per-step with easing.
- Display shows `[RTN]` during MOUSE_RETURNING state; progress bar uses a Knight Rider sweep animation (bouncing highlight segment) instead of a filling bar

#### 7. Power Management
- `sd_power_system_off()` for deep sleep
- Wake via GPIO sense on function button (P0.29)
- Sleep uses hold-to-confirm flow: after 500ms hold, overlay shows "Hold to sleep..." with 5-second countdown bar
- Release during countdown cancels sleep ("Cancelled" shown for 400ms)
- Input suppressed during confirmation and cancellation overlays
- Works from any mode and from screensaver

#### 8. Screensaver
- Overlay state (`screensaverActive` flag), not a UIMode — gates display rendering
- Activates only on top of `MODE_NORMAL` after configurable timeout (Never/1/5/10/15/30 min)
- Minimal display: centered key label + 1px progress bar, mouse state + 1px bar, battery only if <15%
- OLED contrast dimmed to configurable brightness (10-100%, default 20%) via `SSD1306_SETCONTRAST`
- Any input (encoder, buttons) wakes screensaver — input is consumed, not passed through
- Long-press sleep still works from screensaver (not consumed)
- Timeout and brightness configurable via MENU items ("Saver time" and "Saver bright")

### Encoder Handling
- Hybrid ISR + polling architecture:
  - **Primary:** GPIOTE interrupt (`encoderISR`) catches every pin edge in real-time, including during blocking I2C display transfers
  - **Fallback:** `pollEncoder()` in main loop catches edges missed during SoftDevice radio blackouts
  - `noInterrupts()` around polling prevents race conditions with ISR (~300ns critical section, SoftDevice-safe)
- Interrupts attached AFTER SoftDevice init (`setupBLE()`) to prevent GPIOTE channel invalidation
- Atomic `NRF_P0->IN` register read samples both pins simultaneously (no race window)
- 2-step jump recovery: when intermediate quadrature states are missed, direction is inferred from last known movement
- 4 transitions per detent (divide by 4 for clicks)
- **IMPORTANT:** Do NOT use `analogRead()` on A0 or A1 — these share P0.02/P0.03 with encoder CLK/DT. `analogRead()` disconnects the digital input buffer, breaking encoder reads.

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
BUSY                  ~^~_~^~  ← profile name (3s) or "Up: HH:MM:SS"; animation on right
```
`KB [F15]` shows the pre-picked next key. Changes after each keypress.
`[MOV]` = moving, `[IDL]` = idle, `[RTN]` = returning to origin (Knight Rider sweep on progress bar).
Footer shows profile name for 3 seconds after switching, then reverts to uptime. Status animation plays on the right side of the footer (configurable: ECG, EQ, Ghost, Matrix, Radar, None; default Ghost).

### Menu Mode
```
MENU                    ᛒ 85%
──────────────────────────────
      - Keyboard -               ← heading (not selectable)
▌Key min        < 2.0s >  ▐     ← selected (inverted row)
 Key max          < 6.5s >       ← value item with arrows
 Key slots               >      ← action item
      - Mouse -
──────────────────────────────
Minimum delay between keys       ← help bar (scrolls if long)
```
5-row scrollable viewport. Headings centered. Selected row inverted.
Editing inverts only value portion with `< >` arrows. Arrows hidden at bounds.
`FMT_PERCENT_NEG` items have inverted encoder direction and swapped arrow bounds.

### Slots Mode
```
MODE: SLOTS             [3/8]
─────────────────────────────
  F15 F14 --- ---
  --- --- --- ---
─────────────────────────────
Turn=key  Press=slot
Func=back
```
Active slot rendered with inverted colors (white rect, black text).

### Name Mode
```
DEVICE NAME          [3/14]
──────────────────────────────
  G h o s t O p
  e r a t o r · ·
──────────────────────────────
Turn=char  Press=next
Func=save
```
Active position rendered with inverted colors. END positions shown as `·`.
On save, if name changed, shows reboot confirmation prompt with Yes/No selector.

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
| 1.3.1 | Fix encoder unresponsive after boot, hybrid ISR+polling, bitmap splash |
| 1.4.0 | Scrollable settings menu, display brightness, data-driven menu architecture |
| 1.5.0 | Adjustable mouse amplitude (1-5px), inertial ease-in-out mouse movement, reset defaults |
| 1.6.0 | Modular codebase (15 module pairs), Knight Rider mouse return animation, configurable status animation (6 styles), Display/Device menu split |

---

## Known Issues / Limitations

1. **Battery reading approximate** - ADC calibration varies per chip
2. **No hardware power switch** - Sleep mode is the off switch
3. **Encoder must use Seeed nRF52 core** - mbed core incompatible with Bluefruit
4. **Python required for compilation** - Seeed toolchain dependency
5. **AR_INTERNAL_3_6 not available** - Use AR_INTERNAL_3_0 on Seeed core
6. **D0/D1 (A0/A1) are analog-capable** - `analogRead()` on these pins disconnects the digital input buffer, breaking encoder reads. Use `NRF_FICR->DEVICEADDR` or other sources for entropy.

---

## Common Modifications

### Add a new keystroke option
1. Add entry to `AVAILABLE_KEYS[]` array in `keys.cpp`
2. Include HID keycode and display name
3. Set `isModifier` flag appropriately
4. Update `NUM_KEYS` in `config.h`
5. Add 3-char short name to `SHORT_NAMES[]` in `slotName()` function in `display.cpp`

### Change timing range
Modify these defines in `config.h`:
```cpp
#define VALUE_MIN_MS          500UL    // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL  // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL  // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL    // 0.5 second steps
```

### Change mouse randomness
In `config.h`:
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

### Change mouse amplitude range
Modify `MENU_ITEMS[]` entry for `SET_MOUSE_AMP` in `keys.cpp` (minVal/maxVal currently 1–5). The JIGGLING case in `mouse.cpp` applies `mouseAmplitude` per-step via sine easing (`amp = mouseAmplitude * sin(π × progress)`). `pickNewDirection()` stores unit vectors only (-1/0/+1). The return phase clamps at `min(5, remaining)` per axis, so amplitudes above 5 would require updating the return logic.

### Add new menu setting
1. Add `SettingId` enum entry in `config.h`
2. Add field to `Settings` struct in `config.h` (before `checksum`)
3. Set default in `loadDefaults()` in `settings.cpp`, add bounds check in `loadSettings()`
4. Add `MenuItem` entry to `MENU_ITEMS[]` array in `keys.cpp` (update `MENU_ITEM_COUNT` in `config.h`)
5. Add cases to `getSettingValue()` and `setSettingValue()` in `settings.cpp`
6. `calcChecksum()` auto-adapts (loops `sizeof(Settings) - 1`)

### Change BLE device name
Configurable via menu: Device → "Device name" action item opens a character editor (MODE_NAME). Max 14 characters, A-Z, a-z, 0-9, space, dash, underscore. Requires reboot to apply. The compile-time default is:
```cpp
#define DEVICE_NAME "GhostOperator"
```

---

## File Inventory

| File | Purpose |
|------|---------|
| `ghost_operator.ino` | Entry point: setup(), loop(), BLE setup/callbacks |
| `config.h` | Constants, enums, structs (header-only) |
| `keys.h/.cpp` | Const data tables (keys, menu items, names) |
| `icons.h/.cpp` | PROGMEM bitmaps (splash, BT icon, arrows) |
| `state.h/.cpp` | All mutable globals (extern declarations) |
| `settings.h/.cpp` | Flash persistence + value accessors |
| `timing.h/.cpp` | Profiles, scheduling, formatting |
| `encoder.h/.cpp` | ISR + polling quadrature decode |
| `battery.h/.cpp` | ADC battery reading |
| `hid.h/.cpp` | Keystroke sending + key selection |
| `mouse.h/.cpp` | Mouse state machine with sine easing |
| `sleep.h/.cpp` | Deep sleep sequence |
| `screenshot.h/.cpp` | PNG encoder + base64 serial output |
| `serial_cmd.h/.cpp` | Serial debug commands + status |
| `input.h/.cpp` | Encoder dispatch, buttons, name editor |
| `display.h/.cpp` | All rendering (~800 lines, largest module) |
| `schematic_v8.svg` | Circuit schematic |
| `schematic_interactive_v3.html` | Interactive documentation |
| `README.md` | Technical documentation |
| `docs/USER_MANUAL.md` | End-user guide |
| `CHANGELOG.md` | Version history (semver) |
| `CLAUDE.md` | This file |
| `docs/images/` | OLED screenshot PNGs for README |
| `ghost_operator_splash.bin` | Splash screen bitmap (128x64, 1-bit raw) |

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
- [ ] Function button opens menu from NORMAL, closes menu back to NORMAL
- [ ] Menu: encoder scrolls through items (headings skipped), cursor clamps at bounds
- [ ] Menu: "MENU" title inverted when cursor at -1, scroll down to first item
- [ ] Menu: encoder press on value item enters edit mode (value portion inverted)
- [ ] Menu: encoder adjusts value in edit mode, press confirms and exits edit
- [ ] Menu: `< >` arrows visible; `<` hidden at min, `>` hidden at max
- [ ] Menu: "Key slots" action item → press encoder → enters SLOTS mode
- [ ] Menu: function button from SLOTS returns to menu at "Key slots" item
- [ ] Menu: help bar shows context text, scrolls if >21 chars with pauses at ends
- [ ] Menu: Lazy adj shows 0% (not -0%) at zero, -50% at max; encoder CW → toward 0%
- [ ] Menu: Key min/max cross-constraint works (min cannot exceed max)
- [ ] SLOTS mode: encoder rotates key for active slot, button advances slot cursor
- [ ] SLOTS mode: bottom shows "Func=back" (not "Func=exit")
- [ ] All menu values persist after close → reopen menu
- [ ] Settings persist after sleep
- [ ] Hold function button → "Hold to sleep..." overlay with 5s countdown bar appears after 500ms
- [ ] Keep holding through countdown → device enters sleep
- [ ] Release during countdown → "Cancelled" shown, returns to previous screen
- [ ] Encoder and button input suppressed during confirmation and cancellation overlays
- [ ] Sleep confirmation works from any mode (NORMAL, MENU, SLOTS, NAME) and screensaver
- [ ] Button press wakes from sleep
- [ ] Keystrokes detected on host (use key test website)
- [ ] Mouse movement visible on host
- [ ] Battery percentage displays (with battery connected)
- [ ] Scheduling uses profile-adjusted values (verify via serial `s`)
- [ ] Profile does NOT modify base settings (open menu → shows original, not adjusted)
- [ ] Sleep + wake → lazy% and busy% persist, profile resets to NORMAL
- [ ] Serial `d` → prints profile, lazy%, busy%, display brightness, animation, effective values
- [ ] Brightness: editing in menu live-updates OLED contrast
- [ ] Brightness: value persists after menu close and sleep/wake
- [ ] Screensaver activates after configured timeout with no input (minimal display)
- [ ] Screensaver shows centered key label + 1px bar, mouse state + 1px bar
- [ ] Screensaver battery warning appears only when <15%
- [ ] Any input wakes screensaver without side effects (consumed)
- [ ] Hold-to-sleep works from screensaver (not consumed by screensaver wake)
- [ ] "Never" disables screensaver
- [ ] Screensaver dims OLED to saver brightness, restores display brightness on wake
- [ ] Serial `d` → prints screensaver timeout, brightness, and active state
- [ ] Mode timeout (30s): returns to NORMAL from MENU or SLOTS, resets menuEditing
- [ ] Encoder responsive immediately after boot (hybrid ISR+polling, analogRead fix)
- [ ] BLE reconnect resets progress bars (no stale countdown at 0% or 100%)
- [ ] Menu: "Move size" shows "1px" default, editable 1-5 with `< >` arrows
- [ ] Mouse amplitude 1: subtle pauses at start/end of jiggle, 1px movement in middle
- [ ] Mouse amplitude 5: smooth visible ramp-up and ramp-down, 5px peak movement
- [ ] Mouse amplitude persists after menu close → reopen, and after sleep/wake
- [ ] Serial `d` → prints mouse amplitude value
- [ ] Easing: mouse cursor visibly accelerates at start and decelerates at end of each jiggle
- [ ] Easing: mouse returns to approximate origin after each jiggle (net tracking accurate with eased steps)
- [ ] Easing: jiggle duration unchanged (only velocity profile within the jiggle changes)
- [ ] Menu: "Display" heading visible with Brightness/Saver bright/Saver T.O./Animation items
- [ ] Menu: "Animation" shows default "Ghost", editable with 6 options (ECG/EQ/Ghost/Matrix/Radar/None)
- [ ] Animation setting persists after menu close → reopen, and after sleep/wake
- [ ] Animation changes live in NORMAL mode footer area
- [ ] Serial `d` → prints animation style name
- [ ] Menu: "Device" heading visible with Device name/Reset defaults/Reboot items
- [ ] Menu: help bar shows "Current: GhostOperator" when cursor on "Device name"
- [ ] Menu: press encoder on "Device name" → enters MODE_NAME with current name pre-loaded
- [ ] Name editor: encoder rotates through A-Z, a-z, 0-9, space, -, _, END (66 total, wrapping)
- [ ] Name editor: encoder button advances cursor (wraps at 14)
- [ ] Name editor: active position inverted, END positions shown as `·`, header shows `[pos/14]`
- [ ] Name editor: func button saves and shows reboot prompt if name changed
- [ ] Name editor: func button returns to menu directly if name unchanged
- [ ] Reboot prompt: encoder toggles Yes/No, encoder button confirms selection
- [ ] Reboot prompt: Yes → `NVIC_SystemReset()` → device reboots with new name
- [ ] Reboot prompt: No (or func button) → returns to menu at "Device name" item
- [ ] BLE name: after reboot, device advertises new name (verify in host Bluetooth settings)
- [ ] Name persists after menu close → reopen, and after sleep/wake
- [ ] Empty name guard: if all positions END, defaults to "GhostOperator"
- [ ] 30s timeout: returns to NORMAL from NAME mode (saves, skips reboot prompt)
- [ ] Serial `d` → prints device name
- [ ] Menu: "Reset defaults" appears after "Device name" in Device section with `>` indicator
- [ ] Menu: help bar shows "Restore all settings to factory defaults" when "Reset defaults" selected
- [ ] Menu: press encoder on "Reset defaults" → confirmation overlay with "No" highlighted by default
- [ ] Reset defaults: encoder toggles Yes/No, encoder press with No → returns to menu, settings unchanged
- [ ] Reset defaults: encoder press with Yes → all settings reset to defaults, returns to menu
- [ ] Reset defaults: function button during confirmation → cancels (same as No)
- [ ] Reset defaults: mode timeout (30s) during confirmation → cancels, returns to NORMAL
- [ ] After restore: reopen menu → all values show defaults; serial `d` → default values
- [ ] After restore: profile resets to NORMAL, next key updates to F15
- [ ] Menu: "Reboot" appears after "Reset defaults" in Device section with `>` indicator
- [ ] Menu: help bar shows "Restart device (applies pending changes)" when "Reboot" selected
- [ ] Menu: press encoder on "Reboot" → confirmation overlay with "No" highlighted by default
- [ ] Reboot: encoder toggles Yes/No, encoder press with Yes → device reboots immediately
- [ ] Reboot: encoder press with No → returns to menu
- [ ] Reboot: function button during confirmation → cancels (same as No)
- [ ] Reboot: mode timeout (30s) during confirmation → cancels, returns to NORMAL
- [ ] Serial `p` → outputs base64-encoded PNG between `--- PNG START ---` / `--- PNG END ---`
- [ ] Screenshot PNG decodes to valid 128x64 1-bit grayscale image matching OLED display
- [ ] Screenshot works in all UI modes (NORMAL, MENU, SLOTS, screensaver)
- [ ] Screenshot with display not initialized prints error, does not crash
- [ ] Encoder and BLE remain responsive during/after screenshot capture

---

## Future Improvements (Ideas)

- [ ] RGB LED for status (uses D7)
- [ ] Buzzer feedback on mode change
- [x] ~~Multiple profiles~~ → Implemented as timing profiles (LAZY/NORMAL/BUSY) in v1.2.0
- [x] ~~Adjustable mouse movement amplitude~~ → Implemented as "Move size" setting (1-5px) in v1.5.0
- [x] ~~Configurable BLE device name~~ → Implemented as "Device name" editor (MODE_NAME) in v1.5.0
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
| p | PNG screenshot (base64-encoded between `--- PNG START ---` / `--- PNG END ---` markers) |
| v | Screensaver (activate instantly, forces NORMAL mode first) |

---

## Origin

Created with Claude (Anthropic)

*"Fewer parts, more flash"*

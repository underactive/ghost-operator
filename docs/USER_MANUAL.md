# Ghost Operator v1.8.1 - User Manual

## Quick Start

1. Connect battery or USB-C
2. Device boots and shows custom splash screen with version number
3. On your computer: Bluetooth settings → pair "GhostOperator" (or your custom name)
4. Display shows Bluetooth icon when connected
5. Device starts sending keystrokes and mouse movements

---

## Controls Overview

| Control | NORMAL | MENU | SLOTS | NAME |
|---------|--------|------|-------|------|
| **Encoder Turn** | Switch profile | Navigate / adjust value | Cycle slot key | Cycle character / Yes-No |
| **Encoder Press** | Cycle KB/MS enable | Select / confirm | Advance slot cursor | Advance position / confirm |
| **Button Short** | Open menu | Close menu (save) | Back to menu (save) | Save name (reboot prompt) |
| **Button Hold (5s)** | Sleep (countdown) | Sleep (countdown) | Sleep (countdown) | Sleep (countdown) |
| **Button (sleeping)** | Wake up | - | - | - |

---

## Reading the Display

### Normal Mode (Main Screen)

```
┌────────────────────────────────┐
│ GHOST Operator          ᛒ 85%  │  ← Status bar
├────────────────────────────────┤
│ KB [F15] 2.0-6.5s          ↑   │  ← Next key + interval
│ ████████████░░░░░░░░░░░░  3.2s │  ← Countdown bar
├────────────────────────────────┤
│ MS [MOV]  15s/30s            ↑ │  ← Mouse settings
│ ██████░░░░░░░░░░░░░░░░░░  8.5s │  ← Countdown bar
├────────────────────────────────┤
│ Up: 2h 34m        ~^~_~^~      │  ← Uptime (or profile name) + pulse
└────────────────────────────────┘
```

**Status Bar:**
- `GHOST Operator` - Device name
- Bluetooth icon (solid) - Connected
- Bluetooth icon (flashing) - Searching for connection
- `85%` - Battery level

**Key Section:**
- `KB [F15]` - Shows the pre-picked next key (changes after each keypress)
- `2.0-6.5s` - Key interval range (profile-adjusted MIN-MAX)
- ↑ / ✕ - Keys enabled or disabled
- Press encoder to cycle KB/MS enable combos
- Each keystroke cycle randomly picks from populated (non-NONE) slots
- Progress bar counts down to next keypress
- `3.2s` - Time until next keypress

**Mouse Section:**
- `[MOV]` - Currently moving
- `[IDL]` - Currently idle (paused)
- `[RTN]` - Returning to original position (progress bar at 0%)
- `15s/30s` - Move duration / idle duration (profile-adjusted)
- ↑ / ✕ - Mouse enabled or disabled
- Progress bar counts up while idle (charging), counts down while moving (draining)
- During return phase, progress bar stays at 0% (empty)
- `8.5s` - Time remaining in current state

**Footer:**
- `Up: 2h 34m` - Uptime in compact format (e.g., `45s`, `2h 34m`, `1d 5h` — seconds hidden above 1 day)
- After changing profile, shows profile name (LAZY/NORMAL/BUSY) for 3 seconds, then reverts to uptime
- Status animation in footer area (default: Ghost) — configurable via Display → Animation in the menu (ECG, EQ, Ghost, Matrix, Radar, None). Animation is activity-aware: full speed with both KB/MS enabled, half speed with one muted, frozen with both muted.

---

### Menu Mode

```
┌────────────────────────────────┐
│ MENU                    ᛒ 85%  │  ← Title (inverted when selected)
├────────────────────────────────┤
│       - Keyboard -             │  ← Heading (not selectable)
│ ▌Key min          < 2.0s >  ▐  │  ← Selected item (inverted row)
│  Key max            < 6.5s >   │  ← Normal item with arrows
│  Key slots                 >   │  ← Action item
│       - Mouse -                │  ← Next heading
├────────────────────────────────┤
│ Minimum delay between keys     │  ← Context help text
└────────────────────────────────┘
```

**How to use the menu:**
1. **Short press the function button** from NORMAL mode to open the menu
2. **Turn the encoder** to scroll through items (headings are skipped)
3. **Press the encoder** on a value item to enter edit mode
4. **Turn the encoder** to adjust the value — `< >` arrows show which direction you can go
5. **Press the encoder** again to confirm and exit edit mode
6. **Short press the function button** to close the menu and save settings

**Menu items:**

| Heading | Setting | What It Does |
|---------|---------|-------------|
| **Keyboard** | Key min | Minimum delay between keystrokes (0.5s-30s) |
| | Key max | Maximum delay between keystrokes (0.5s-30s) |
| | Key slots | Opens the slot editor (press encoder to enter) |
| **Mouse** | Move duration | How long the mouse moves (0.5s-90s) |
| | Idle duration | Pause between moves (0.5s-90s) |
| | Move style | Movement pattern: Bezier (smooth curves) or Brownian (jiggle) |
| | Move size | Mouse step size, Brownian only (1-5px, default 1px) |
| **Profiles** | Lazy adjust | Slow down timing (-50% to 0%, 5% steps) |
| | Busy adjust | Speed up timing (0% to 50%, 5% steps) |
| **Display** | Brightness | OLED display brightness (10-100%, default 80%) |
| | Saver bright | Screensaver dimmed brightness (10-100%, default 20%) |
| | Saver time | Screensaver timeout (Never / 1 / 5 / 10 / 15 / 30 min) |
| | Animation | Status animation style (ECG / EQ / Ghost / Matrix / Radar / None, default Ghost) |
| **Device** | Device name | Device name editor (press encoder to enter) |
| | Reset defaults | Restore all settings to factory defaults (confirmation required) |
| | Reboot | Restart the device (confirmation required) |
| **About** | Version | Firmware version (read-only) |

**Help bar:** The bottom line shows context-sensitive help for the selected item. Long text scrolls automatically.

---

### Slots Mode

```
┌────────────────────────────────┐
│ MODE: SLOTS              [3/8] │  ← Active slot / total
├────────────────────────────────┤
│   F15 F14 --- ---              │  ← Slot row 1 (active = inverted)
│   --- --- --- ---              │  ← Slot row 2
├────────────────────────────────┤
│ Turn=key  Press=slot           │  ← Controls
│ Func=back                      │
└────────────────────────────────┘
```

- `[3/8]` - Active slot number / total slots
- Active slot shown with inverted colors (white background, black text)
- Turn encoder to cycle the active slot's key assignment
- Press encoder to advance to the next slot (1→2→...→8→1)
- Press function button to return to the menu

---

### Name Mode (Device Name Editor)

```
┌────────────────────────────────┐
│ DEVICE NAME             [3/14] │  ← Active position / total
├────────────────────────────────┤
│   G h o s t O p                │  ← Character row 1
│   e r a t o r · ·              │  ← Character row 2 (· = END)
├────────────────────────────────┤
│ Turn=char  Press=next          │  ← Controls
│ Func=save                      │
└────────────────────────────────┘
```

- `[3/14]` - Active character position / total positions (14 max)
- Active position shown with inverted colors (white background, black text)
- **Turn encoder** to cycle through: A-Z, a-z, 0-9, space, dash, underscore, END (66 characters, wrapping)
- END positions shown as `·` (middle dot) — everything after the first END is ignored
- **Press encoder** to advance to the next position (wraps from 14 back to 1)
- **Press function button** to save:
  - If the name **changed**, a reboot confirmation appears: "Reboot to apply? Yes / No"
    - Turn encoder to select Yes or No, press encoder to confirm
    - Yes = device reboots immediately with new name
    - No (or function button) = returns to menu; new name is saved but won't take effect until next reboot
  - If the name is **unchanged**, returns to menu directly (no reboot prompt)

---

### Screensaver (activates automatically)

```
┌────────────────────────────────┐
│                                │
│            [F15]               │  ← Next key, centered
│ ████████████████░░░░░░░░░░░░░  │  ← 1px KB progress bar
│                                │
│            [IDL]               │  ← Mouse state, centered
│ ██████░░░░░░░░░░░░░░░░░░░░░░   │  ← 1px MS progress bar
│                                │
│             12%                │  ← Battery (only if <15%)
└────────────────────────────────┘
```

- Activates after the configured timeout of no user interaction (default: 30 minutes)
- Only activates in NORMAL mode — settings modes auto-return first
- Minimal pixel display to prevent OLED burn-in and save power
- OLED brightness dimmed to configured level (default 20%) — adjust "Saver bright" in menu
- **Any input** (encoder turn, encoder press, function button) wakes the display — the input is consumed so you don't accidentally change settings
- **Long-press sleep** still works from screensaver
- Configure timeout via "Saver time" in menu (Never / 1 / 5 / 10 / 15 / 30 min)
- Configure brightness via "Saver bright" in menu (10-100%)

---

## Navigating Modes

The device has four modes:

```
NORMAL ←→ MENU → SLOTS → MENU
         (func)  (func)  (func)
                → NAME  → MENU
                 (func)  (func)
```

| Mode | Purpose |
|------|---------|
| **NORMAL** | Live status display — turn encoder to switch profile, press encoder to toggle KB/MS |
| **MENU** | Scrollable settings menu — turn encoder to navigate, press to select/edit |
| **SLOTS** | Key slot editor — turn encoder to change key, press to advance slot |
| **NAME** | Device name editor — turn encoder to change character, press to advance position |

- **Function button** toggles between NORMAL and MENU
- From SLOTS, **function button** returns to MENU (at the "Key slots" item)
- From NAME, **function button** saves and returns to MENU (with optional reboot prompt)
- **Auto-return:** If you don't touch anything for 30 seconds in MENU, SLOTS, or NAME, it returns to NORMAL and saves

---

## Adjusting Settings

### Timing Profiles (NORMAL mode)

Ghost Operator has three timing profiles that scale all timing values:

| Profile | Effect |
|---------|--------|
| **LAZY** | Less active: longer key waits, shorter mouse movement, longer mouse idle |
| **NORMAL** | Default: base values unchanged |
| **BUSY** | More active: shorter key waits, longer mouse movement, shorter mouse idle |

1. In **NORMAL mode**, **turn the encoder** to switch: LAZY ← NORMAL → BUSY (clamped at ends)
2. The profile name appears on the bottom line for 3 seconds
3. KB and MS timing values on the display update immediately to show effective values
4. Adjust how much each profile scales values in **"Lazy adjust"** and **"Busy adjust"** in the menu (default: 15%)

Profiles do **not** change your saved base settings — they apply scaling at runtime only. Profile resets to NORMAL after sleep/wake.

### Change Device Name

You can customize the Bluetooth name your Ghost Operator advertises (default: "GhostOperator"):

1. Open the **menu** (short press function button)
2. Scroll to **"Device name"** under the **Device** heading and **press encoder** to enter NAME mode
3. The **active position** is shown with inverted colors (white background)
4. **Turn encoder** to cycle through characters (A-Z, a-z, 0-9, space, dash, underscore, END)
5. **Press encoder** to advance to the next position (1→2→...→14→1)
6. Set positions to **END** (`·`) to mark the end of the name — trailing END markers are trimmed
7. Press **function button** to save:
   - If the name changed, choose **Yes** to reboot now (applies immediately) or **No** to apply later
   - If unchanged, returns to menu directly
8. After reboot, your computer's Bluetooth settings will show the new name

### Reset to Factory Defaults

If you want to restore all settings to their original factory values:

1. Open the **menu** (short press function button)
2. Scroll to **"Reset defaults"** under the **Device** heading and **press encoder**
3. A confirmation screen appears with **"No"** highlighted by default
4. **Turn encoder** to select **Yes** or **No**, then **press encoder** to confirm
   - **Yes** = all settings restored to defaults (key slots, timing, profiles, brightness, device name)
   - **No** (or **function button**) = cancels, returns to menu with settings unchanged

### Reboot Device

To restart the device from the menu (useful for applying a pending device name change):

1. Open the **menu** (short press function button)
2. Scroll to **"Reboot"** under the **Device** heading and **press encoder**
3. A confirmation screen appears with **"No"** highlighted by default
4. **Turn encoder** to select **Yes** or **No**, then **press encoder** to confirm
   - **Yes** = device reboots immediately
   - **No** (or **function button**) = cancels, returns to menu

### Configure Key Slots

Ghost Operator has **8 key slots**. Each keystroke cycle randomly picks from populated (non-NONE) slots, adding variety and reducing pattern detectability.

1. Open the **menu** (short press function button)
2. Scroll to **"Key slots"** and **press encoder** to enter SLOTS mode
3. The **active slot** is shown with inverted colors (white background)
4. **Turn encoder** to cycle the active slot's key through:
   - F13-F24 (invisible keys - recommended)
   - ScrLk, Pause, NumLk (toggle keys)
   - LShift, LCtrl, LAlt, RShift, RCtrl, RAlt (modifier keys)
   - Esc, Space, Enter (common keys - use with caution)
   - Up, Down, Left, Right (arrow keys)
   - NONE (disable this slot)
5. **Press encoder** to advance to the next slot (1→2→...→8→1)
6. Repeat to configure multiple slots with different keys
7. Press **function button** to return to the menu (settings auto-save)

### Change Settings (Menu)
1. Press **function button** to open the menu
2. **Turn encoder** to scroll to the setting you want
3. **Press encoder** to enter edit mode (value highlights)
4. **Turn encoder** to adjust — `< >` arrows show available range
5. **Press encoder** to confirm
6. Press **function button** to close the menu and save

### Toggle Keys/Mouse ON/OFF
**Press encoder** in **NORMAL mode** to cycle through enable combinations:

| Press | Keyboard | Mouse |
|-------|----------|-------|
| Start | ON | ON |
| 1st | ON | OFF |
| 2nd | OFF | ON |
| 3rd | OFF | OFF |
| 4th | ON | ON (wraps) |

---

## Timing Behavior

### Keyboard
- Each keypress waits a **random time between MIN and MAX** (adjusted by active profile)
- Each cycle **randomly picks** one of the populated key slots
- Example: MIN=2s, MAX=6s, slots=[F15, F14, F13] → F14 at 3.2s, F15 at 5.1s, F13 at 2.4s...

### Mouse
- **Move style** selects the movement pattern:
  - **Bezier** (default): Smooth curved sweeps with random radius — natural-looking arcs
  - **Brownian**: Classic jiggle with inertial easing — movement ramps up, peaks, then ramps down
- **Move phase:** Mouse moves randomly for the set duration (adjusted by active profile)
  - In Brownian mode, the "Move size" setting controls the peak speed (1-5px per step)
  - In Bezier mode, sweep radius is randomized; "Move size" has no effect (hidden in menu)
- **Idle phase:** Mouse stops for the set duration (adjusted by active profile)
- ±20% randomness on both durations
- Mouse returns to approximate starting position after each movement (display shows `[RTN]` with progress bar at 0%)

---

## Sleep Mode

### Enter Sleep
- **Hold function button** — after 0.5 seconds, a "Hold to sleep..." overlay appears with a 5-second countdown bar
- **Keep holding** to complete the countdown — display briefly shows "SLEEPING..." then enters deep sleep (~3µA)
- **Release during countdown** to cancel — shows "Cancelled" briefly and returns to the previous screen
- Bluetooth disconnects when sleep begins
- Works from any mode (NORMAL, MENU, SLOTS, NAME) and from the screensaver

### Wake Up
- **Press function button**
- Device reboots and reconnects
- All settings are preserved (saved to flash)

---

## Pairing with Your Computer

### First Time Pairing
1. Power on the device
2. Bluetooth icon flashes (searching)
3. On your computer:
   - **Mac:** System Preferences → Bluetooth → "GhostOperator" (or your custom name) → Connect
   - **Windows:** Settings → Bluetooth → Add device → "GhostOperator" (or your custom name)
   - **Linux:** Bluetooth manager → Scan → Pair "GhostOperator" (or your custom name)
4. Display shows Bluetooth icon when connected

### Reconnecting
- Device automatically reconnects to last paired computer
- If it doesn't, toggle Bluetooth off/on on your computer

---

## LED Indicator

| Pattern | Meaning |
|---------|---------|
| Slow blink (1/sec) | Connected |
| Fast blink (5/sec) | Searching for connection |
| Off | Sleeping |

---

## Serial Monitor (Advanced)

Connect via USB and open Serial Monitor at 115200 baud.

| Command | Action |
|---------|--------|
| `h` | Show help |
| `s` | Status report |
| `d` | Dump current settings |
| `z` | Enter sleep mode |
| `p` | Capture PNG screenshot of the OLED display |
| `v` | Activate screensaver (forces NORMAL mode first) |
| `f` | Enter OTA DFU mode (BLE firmware update) |
| `u` | Enter Serial DFU mode (USB firmware update) |

### Taking a Screenshot

The `p` command captures the current OLED display as a PNG image. The image is output as base64-encoded text between delimiter lines:

```
--- PNG START ---
iVBORw0KGgoAAAANSUhEUgAAAIAAAABA...
(more base64 lines)
--- PNG END ---
```

**To save as a PNG file:**

1. Send `p` in the serial monitor
2. Copy all the base64 text between `--- PNG START ---` and `--- PNG END ---` (don't include the marker lines)
3. Save the copied text to a temporary file (e.g. `screenshot.b64`)
4. Decode it using one of these methods:

**macOS / Linux:**
```bash
base64 -d screenshot.b64 > screenshot.png
```

**Windows (PowerShell):**
```powershell
[Convert]::FromBase64String((Get-Content screenshot.b64 -Raw)) | Set-Content screenshot.png -Encoding Byte
```

**One-liner (macOS / Linux)** — paste the base64 directly without a temp file:
```bash
pbpaste | base64 -d > screenshot.png
```
(Copy the base64 text to clipboard first, then run the command.)

The resulting file is a 128x64 1-bit grayscale PNG matching exactly what's shown on the OLED. Screenshots work in all display modes (NORMAL, MENU, SLOTS, NAME, and screensaver).

---

## Firmware Updates

Update your Ghost Operator firmware from the web dashboard using a USB cable.

**Requirements:**
- Chrome or Edge browser on desktop (Web Serial API required)
- USB cable connected to the device
- A DFU package file (`.zip`) containing the new firmware

**Steps:**

1. Connect to your device via USB in the web dashboard (click **Connect USB**)
2. Expand the **Firmware Update** section at the bottom
3. Select the `.zip` DFU package file
4. Click **Start Firmware Update**, then **Confirm Update**
5. The device reboots into USB DFU mode (serial disconnects — this is normal)
6. When prompted, click **Select Serial Port** and pick the DFU serial port from the Chrome dialog
7. The firmware transfer begins — watch the progress bar
8. On completion, the device reboots with the new firmware
9. Reconnect via USB to verify the new version

**If something goes wrong:**
- If the device is stuck in DFU mode, power cycle it (unplug USB and battery, then replug)
- DFU mode has no timeout — the device stays in DFU mode until a transfer completes or it's power cycled
- Your settings are preserved through firmware updates (stored in a separate flash region)

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Display stays black | Check wiring, especially VCC/GND |
| Bluetooth icon keeps flashing | Unpair from computer, try again |
| Keys not working | Make sure ↑ icon shows (not ✕), press encoder to cycle |
| Mouse not moving | Check mouse shows ↑ icon, wait for `[MOV]` state |
| Encoder does nothing | Check CLK/DT wiring to D0/D1 |
| Settings not saving | Wait for auto-return to NORMAL, or press function button |
| Battery reads wrong | Normal - reading is approximate |
| Stuck in DFU mode | Power cycle: unplug USB and battery, then replug |
| No serial port in picker | Make sure USB cable is connected before starting DFU |
| Firmware update fails | Try again; ensure USB cable is data-capable (not charge-only) |

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Key timing range | 0.5s - 30s |
| Mouse timing range | 0.5s - 90s |
| Timing step | 0.5s |
| Mouse styles | Bezier (default), Brownian |
| Mouse amplitude | 1-5px (1px steps, default 1px, Brownian only) |
| Mouse randomness | ±20% |
| Sleep current | ~3µA |
| Active current | ~15mA (with display) |
| Battery life | ~60+ hours (1000mAh) |

---

## Available Keystrokes

| Key | Notes |
|-----|-------|
| F15 | **Recommended** - no effect on most systems |
| F14 | Invisible on most systems |
| F13 | Invisible on most systems |
| F16-F24 | Invisible on most systems |
| ScrLk | May toggle scroll lock LED |
| Pause | Usually ignored |
| NumLk | May toggle numlock |
| LShift | Left modifier only, no visible effect |
| LCtrl | Left modifier only, no visible effect |
| LAlt | Left modifier only, no visible effect |
| RShift | Right modifier only, no visible effect |
| RCtrl | Right modifier only, no visible effect |
| RAlt | Right modifier only, no visible effect |
| Esc | **Caution** - may close dialogs or exit fullscreen |
| Space | **Caution** - may type spaces or trigger buttons |
| Enter | **Caution** - may submit forms or confirm actions |
| Up/Down/Left/Right | May scroll pages or move cursor |
| NONE | Disables keystrokes entirely |

---

*Ghost Operator v1.8.1 | TARS Industrial Technical Solutions*

*"Fewer parts, more flash"*

# Ghost Operator v1.3.0 - User Manual

## Quick Start

1. Connect battery or USB-C
2. Device boots and shows "GHOST OPERATOR v1.3.0"
3. On your computer: Bluetooth settings → pair "GhostOperator"
4. Display shows Bluetooth icon when connected
5. Device starts sending keystrokes and mouse movements

---

## Controls Overview

| Control | Action |
|---------|--------|
| **Encoder Turn** | Switch profile (normal) / Cycle slot key (slots) / Adjust value (settings) |
| **Encoder Press** | Cycle KB/MS enable (normal & settings) / Advance slot cursor (slots) |
| **Button Short Press** | Cycle to next mode |
| **Button Long Press (3s)** | Enter sleep mode |
| **Button Press (while sleeping)** | Wake up |

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
│ Up: 02:34:15      ~^~_~^~      │  ← Uptime (or profile name) + pulse
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
- `15s/30s` - Jiggle duration / idle duration (profile-adjusted)
- ↑ / ✕ - Mouse enabled or disabled
- Progress bar counts up while idle (charging), counts down while moving (draining)
- `8.5s` - Time remaining in current state

**Footer:**
- `Up: 02:34:15` - Uptime (hours:minutes:seconds)
- After changing profile, shows profile name (LAZY/NORMAL/BUSY) for 3 seconds, then reverts to uptime
- Heartbeat pulse trace (scrolling ECG animation when connected)

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
│ Func=exit                      │
└────────────────────────────────┘
```

- `[3/8]` - Active slot number / total slots
- Active slot shown with inverted colors (white background, black text)
- Turn encoder to cycle the active slot's key assignment
- Press encoder to advance to the next slot (1→2→...→8→1)
- Press function button to leave SLOTS mode

---

### Settings Mode

```
┌────────────────────────────────┐
│ MODE: KEY MIN            [K]   │  ← Current mode
├────────────────────────────────┤
│                                │
│       > 2.0s <                 │  ← Current value
│                                │
│ 0.5s ████████░░░░░░░░░░░░ 30s  │  ← Position in range
├────────────────────────────────┤
│ Turn dial to adjust            │
└────────────────────────────────┘
```

- `MODE: KEY MIN` - Which setting you're editing
- `[K]` or `[k]` - Keys ON or off (press encoder to toggle)
- `[M]` or `[m]` - Mouse ON or off (in mouse modes)
- `[%]` - Shown in LAZY % and BUSY % modes
- `[S]` - Shown in SCREENSAVER and SAVER BRIGHT modes
- `> 2.0s <` - Current value (turn encoder to change)
- `> 15% <` - Current percentage (in LAZY % / BUSY % modes)
- `> 10 min <` - Current timeout (in SCREENSAVER mode)
- `> 30% <` - Current brightness (in SAVER BRIGHT mode)
- Progress bar shows position in range (0.5s-30s for key, 0.5s-90s for mouse, 0-50% for profiles, 10-100% for brightness)
- Position dots show selection in SCREENSAVER mode (filled = selected, hollow = unselected)

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

- Activates after the configured timeout of no user interaction (default: 10 minutes)
- Only activates in NORMAL mode — settings modes auto-return first
- Minimal pixel display to prevent OLED burn-in and save power
- OLED brightness dimmed to configured level (default 30%) — adjust in SAVER BRIGHT settings page
- **Any input** (encoder turn, encoder press, function button) wakes the display — the input is consumed so you don't accidentally change settings
- **Long-press sleep** still works from screensaver
- Configure timeout in SCREENSAVER settings page (Never / 1 / 5 / 10 / 15 / 30 min)
- Configure brightness in SAVER BRIGHT settings page (10-100%)

---

## Mode Cycle

Press the **function button** (short press) to cycle through modes:

```
NORMAL → KEY MIN → KEY MAX → SLOTS → MOUSE JIG → MOUSE IDLE → LAZY % → BUSY % → SCREENSAVER → SAVER BRIGHT → NORMAL...
```

| Mode | What It Adjusts |
|------|-----------------|
| NORMAL | Shows next key and countdown; turn encoder to switch profile |
| KEY MIN | Minimum time between keystrokes |
| KEY MAX | Maximum time between keystrokes |
| SLOTS | Configure 8 key slots (turn=change key, press=next slot) |
| MOUSE JIG | How long the mouse jiggles |
| MOUSE IDLE | How long the mouse pauses between jiggles |
| LAZY % | How much the LAZY profile scales values (0-50%) |
| BUSY % | How much the BUSY profile scales values (0-50%) |
| SCREENSAVER | How long before screensaver activates (Never/1/5/10/15/30 min) |
| SAVER BRIGHT | OLED brightness during screensaver (10-100%, default 30%) |

**Auto-return:** If you don't touch anything for 10 seconds in a settings mode, it returns to NORMAL and saves.

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
4. Adjust how much each profile scales values in **LAZY %** and **BUSY %** modes (default: 15%)

Profiles do **not** change your saved base settings — they apply scaling at runtime only. Profile resets to NORMAL after sleep/wake.

### Configure Key Slots (SLOTS mode)

Ghost Operator has **8 key slots**. Each keystroke cycle randomly picks from populated (non-NONE) slots, adding variety and reducing pattern detectability.

1. Press **function button** until you reach **SLOTS** mode
2. The **active slot** is shown with inverted colors (white background)
3. **Turn encoder** to cycle the active slot's key through:
   - F15, F14, F13 (invisible keys - recommended)
   - ScrLk, Pause, NumLk (toggle keys)
   - LShift, LCtrl, LAlt (modifier keys)
   - NONE (disable this slot)
4. **Press encoder** to advance to the next slot (1→2→...→8→1)
5. Repeat to configure multiple slots with different keys
6. Press **function button** to move on (settings auto-save)

### Change Timing Values (Settings modes)
1. Press **function button** to enter KEY MIN mode
2. **Turn encoder** to adjust value (0.5s steps)
3. Press **function button** again to move to next setting
4. Values auto-save when you leave the mode

### Toggle Keys/Mouse ON/OFF
**Press encoder** in **NORMAL mode or any settings mode** (except SLOTS) to cycle through enable combinations:

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
- **Jiggle phase:** Mouse moves randomly for the set duration (adjusted by active profile)
- **Idle phase:** Mouse stops for the set duration (adjusted by active profile)
- ±20% randomness on both durations
- Mouse returns to approximate starting position after each jiggle

---

## Sleep Mode

### Enter Sleep
- **Long press function button (3+ seconds)**
- Display shows "SLEEPING..."
- Device enters ultra-low power mode (~3µA)
- Bluetooth disconnects

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
   - **Mac:** System Preferences → Bluetooth → "GhostOperator" → Connect
   - **Windows:** Settings → Bluetooth → Add device → "GhostOperator"
   - **Linux:** Bluetooth manager → Scan → Pair "GhostOperator"
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

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Key timing range | 0.5s - 30s |
| Mouse timing range | 0.5s - 90s |
| Timing step | 0.5s |
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
| ScrLk | May toggle scroll lock LED |
| Pause | Usually ignored |
| NumLk | May toggle numlock |
| LShift | Modifier only, no visible effect |
| LCtrl | Modifier only, no visible effect |
| LAlt | Modifier only, no visible effect |
| NONE | Disables keystrokes entirely |

---

*Ghost Operator v1.3.0 | TARS Industries*

*"Fewer parts, more flash"*

# Ghost Operator v1.1 - User Manual

## Quick Start

1. Connect battery or USB-C
2. Device boots and shows "GHOST OPERATOR v1.1.0"
3. On your computer: Bluetooth settings → pair "GhostOperator"
4. Display shows Bluetooth icon when connected
5. Device starts sending keystrokes and mouse movements

---

## Controls Overview

| Control | Action |
|---------|--------|
| **Encoder Turn** | Select key (normal) / Adjust value (settings) |
| **Encoder Press** | Toggle Keys or Mouse ON/OFF |
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
│ KB [F15]  2.0s-6.5s          ON  │  ← Key settings
│ ████████████░░░░░░░░░░░░  3.2s │  ← Countdown bar
├────────────────────────────────┤
│ MS [MOV]  15s/30s          ON  │  ← Mouse settings
│ ██████░░░░░░░░░░░░░░░░░░  8.5s │  ← Countdown bar
├────────────────────────────────┤
│ Up: 02:34:15                 ● │  ← Uptime + spinner
└────────────────────────────────┘
```

**Status Bar:**
- `GHOST` - Device name
- Bluetooth icon (solid) - Connected
- Bluetooth icon (flashing) - Searching for connection
- `85%` - Battery level

**Key Section:**
- `KB [F15]` - Currently selected keystroke
- `2.0s-6.5s` - Min/max interval range
- `ON` / `--` - Keys enabled or disabled
- Progress bar counts down to next keypress
- `3.2s` - Time until next keypress

**Mouse Section:**
- `[MOV]` - Currently moving
- `[IDL]` - Currently idle (paused)
- `15s/30s` - Jiggle duration / idle duration
- `ON` / `--` - Mouse enabled or disabled
- Progress bar counts down current state
- `8.5s` - Time remaining in current state

**Footer:**
- `Up: 02:34:15` - Uptime (hours:minutes:seconds)
- `●` - Activity spinner (animates when connected)

---

### Settings Mode

```
┌────────────────────────────────┐
│ MODE: KEY MIN            [K]   │  ← Current mode
├────────────────────────────────┤
│                                │
│       > 2.0s <             │  ← Current value
│                                │
│ 0.5s ████████░░░░░░░░░░░░ 30s  │  ← Position in range
├────────────────────────────────┤
│ Turn dial to adjust         │
└────────────────────────────────┘
```

- `MODE: KEY MIN` - Which setting you're editing
- `[K]` or `[k]` - Keys ON or off (press encoder to toggle)
- `[M]` or `[m]` - Mouse ON or off (in mouse modes)
- `> 2.0s <` - Current value (turn encoder to change)
- Progress bar shows position between 0.5s and 30s

---

## Mode Cycle

Press the **function button** (short press) to cycle through modes:

```
NORMAL → KEY MIN → KEY MAX → MOUSE JIG → MOUSE IDLE → NORMAL...
```

| Mode | What It Adjusts |
|------|-----------------|
| NORMAL | Main display, encoder selects keystroke |
| KEY MIN | Minimum time between keystrokes |
| KEY MAX | Maximum time between keystrokes |
| MOUSE JIG | How long the mouse jiggles |
| MOUSE IDLE | How long the mouse pauses between jiggles |

**Auto-return:** If you don't touch anything for 10 seconds in a settings mode, it returns to NORMAL and saves.

---

## Adjusting Settings

### Change Keystroke (NORMAL mode)
1. Make sure you're in NORMAL mode (main display)
2. **Turn encoder** left/right to cycle through keys:
   - F15, F14, F13 (invisible keys - recommended)
   - ScrLk, Pause, NumLk (toggle keys)
   - LShift, LCtrl, LAlt (modifier keys)
   - NONE (disable keystrokes)

### Change Timing Values (Settings modes)
1. Press **function button** to enter KEY MIN mode
2. **Turn encoder** to adjust value (0.5s steps)
3. Press **function button** again to move to next setting
4. Values auto-save when you leave the mode

### Toggle Keys/Mouse ON/OFF
- In NORMAL, KEY MIN, or KEY MAX mode: **Press encoder** toggles keys
- In MOUSE JIG or MOUSE IDLE mode: **Press encoder** toggles mouse

---

## Timing Behavior

### Keyboard
- Each keypress waits a **random time between MIN and MAX**
- Example: MIN=2s, MAX=6s → keypresses at 3.2s, 5.1s, 2.4s, 4.8s...

### Mouse
- **Jiggle phase:** Mouse moves randomly for the set duration
- **Idle phase:** Mouse stops for the set duration
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
| Keys not working | Make sure `ON` shows (not `--`), press encoder to toggle |
| Mouse not moving | Check mouse is `ON`, wait for `[MOV]` state |
| Encoder does nothing | Check CLK/DT wiring to D0/D1 |
| Settings not saving | Wait for auto-return to NORMAL, or press function button |
| Battery reads wrong | Normal - reading is approximate |

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Timing range | 0.5s - 30s |
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

*Ghost Operator v1.1 | TARS Industries*
*"Fewer parts, more flash"*

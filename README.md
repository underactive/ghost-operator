# Ghost Operator - BLE Keyboard/Mouse Device

## Version 1.1.0 - Encoder Menu + Flash Storage

A wireless Bluetooth device that prevents screen lock and idle timeout. Masquerades as a keyboard and mouse, sending periodic keystrokes and movements. What you do with it is your own business.

---

## Features

- **BLE HID Keyboard + Mouse** - Works with any Bluetooth device
- **Encoder Menu System** - Adjust all settings with single rotary encoder
- **Flash Storage** - Settings survive sleep and power off (1MB onboard)
- **Software Power Control** - Deep sleep mode (~3µA)
- **OLED Display** - Real-time countdown bars and uptime
- **USB-C Charging** - Charge while operating
- **~60+ hours runtime** on 1000mAh battery

---

## Bill of Materials

### Components

| Ref | Qty | Part | Description | Price |
|-----|-----|------|-------------|-------|
| U1 | 1 | Seeed XIAO nRF52840 | BLE 5.0 MCU, USB-C, LiPo charger | $9.90 |
| DISP1 | 1 | SSD1306 OLED 0.96" | 128x64 I2C Display | $3.50 |
| ENC1 | 1 | Rotary Encoder | KY-040 with pushbutton | $1.50 |
| SW1 | 1 | Tactile Button | 6x6mm momentary | $0.10 |
| BT1 | 1 | LiPo Battery | 3.7V 1000mAh, JST-SH 1.25mm | $6.00 |
| C1 | 1 | Ceramic Capacitor | 100nF | $0.05 |
| C2 | 1 | Electrolytic Capacitor | 10µF | $0.10 |
| J1 | 1 | JST-SH Header | 2-pin, through-hole | $0.30 |
| - | 1 | Prototype PCB | 5x7cm | $0.50 |
| - | - | Wire, headers, enclosure | Misc | $3.00 |

### Cost Summary

| Category | Cost |
|----------|------|
| Components | $24.95 |
| Shipping | ~$3.00 |
| **Total** | **~$28** |

---

## Pinout

| XIAO Pin | Function |
|----------|----------|
| BAT+ | Battery + (direct) |
| BAT- | Battery - |
| 3V3 | Power out to peripherals |
| GND | Ground rail |
| D0 | Encoder CLK (A) |
| D1 | Encoder DT (B) |
| D2 | Encoder SW (button) |
| D3 | Function Button |
| D4 | I2C SDA (OLED) |
| D5 | I2C SCL (OLED) |
| D6 | Activity LED (optional) |
| D7-D10 | Unused (available) |

---

## Controls

### UI Modes

Cycle through modes with function button short press:

```
NORMAL → KEY MIN → KEY MAX → MOUSE JIG → MOUSE IDLE → (repeat)
```

### Control Actions

| Control | NORMAL Mode | Settings Mode |
|---------|-------------|---------------|
| Encoder Turn | Select keystroke | Adjust value ±0.5s |
| Encoder Button | Toggle keys ON/OFF | Toggle keys/mouse |
| Func Short | Next mode | Next mode |
| Func Long (3s) | Enter sleep | Enter sleep |
| Func (sleeping) | Wake up | - |

### Settings Range

All settings: **0.5s to 30s** in 0.5s increments

- **Key Interval MIN** - Minimum time between keystrokes
- **Key Interval MAX** - Maximum time between keystrokes
- **Mouse Jiggle Duration** - How long mouse jiggles
- **Mouse Idle Duration** - Pause between jiggles

### Timing Behavior

- **Keyboard**: Random interval between MIN and MAX each keypress
- **Mouse**: Jiggle for set duration → Idle for set duration → repeat
  - ±20% randomness applied to jiggle and idle durations
  - Minimum clamp: 0.5s

---

## Display

### Normal Mode

```
┌────────────────────────────────┐
│ GHOST Operator          ᛒ 85%  │
├────────────────────────────────┤
│ KB [F15]  2.0s-6.5s          ON  │
│ ████████████░░░░░░░░░░░░  3.2s │
├────────────────────────────────┤
│ MS [MOV]  15s/30s          ON  │
│ ██████░░░░░░░░░░░░░░░░░░  8.5s │
├────────────────────────────────┤
│ Up: 02:34:15                 ● │
└────────────────────────────────┘
```

- **Bluetooth icon** (solid) = Connected, (flashing) = Scanning
- Progress bars count down to next action
- **[MOV]** = Mouse moving, **[IDL]** = Mouse idle

### Settings Mode

```
┌────────────────────────────────┐
│ MODE: KEY MIN            [K]   │
├────────────────────────────────┤
│       > 2.0s <             │
│ 0.5s ████████░░░░░░░░░░░░ 30s  │
├────────────────────────────────┤
│ Turn dial to adjust         │
└────────────────────────────────┘
```

---

## Wiring

### Power (Direct - No Switch)

```
BT1 (+) → J1 Pin 1 → XIAO BAT+
BT1 (-) → J1 Pin 2 → XIAO BAT-
```

### OLED Display

```
VCC → 3V3 Rail
GND → GND Rail
SCL → D5
SDA → D4
```

### Rotary Encoder

```
GND → GND Rail
+   → 3V3 Rail
SW  → D2
DT  → D1
CLK → D0
```

### Function Button

```
Pin 1 → D3
Pin 2 → GND Rail
```

### Decoupling Capacitors

Place C1 (100nF) and C2 (10µF) between 3V3 and GND, close to XIAO.

---

## Flash Storage

Settings are automatically saved to the XIAO's internal flash when:
- Leaving a settings mode (function button press)
- 10 second timeout in settings mode
- Entering sleep mode

Settings persist through sleep and power off.

---

## Serial Commands

Connect via USB at 115200 baud:

| Key | Command |
|-----|---------|
| h | Help |
| s | Status report |
| d | Dump settings |
| z | Enter sleep mode |

---

## Pairing

1. Connect battery (device powers on automatically)
2. Open Bluetooth settings on your computer
3. Search for "GhostOperator"
4. Pair and connect
5. Display shows Bluetooth icon when connected

---

## Files

| File | Description |
|------|-------------|
| `ghost_operator.ino` | Firmware (current) |
| `schematic_v8.svg` | Schematic Rev 4.0 |
| `schematic_interactive_v3.html` | Interactive documentation |
| `README.md` | This file |

---

## Version History

| Version | Changes |
|---------|---------|
| 1.0.0 | Initial hardware release - encoder menu, flash storage, BLE HID |
| **1.1.0** | **Display overhaul, BT icon, HID keycode fix** |

---

## License

Do what you want. TARS Industries takes no responsibility for angry IT departments.

*"Fewer parts, more flash"*

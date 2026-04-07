# nRF52840 DIY Hardware

Hardware reference for the original Ghost Operator build on the Seeed XIAO nRF52840. Requires soldering and assembly. For plug-and-play ESP32 boards, see the [README](../../README.md).

---

## Bill of Materials

| Ref | Qty | Part | Description | Price |
|-----|-----|------|-------------|-------|
| U1 | 1 | Seeed XIAO nRF52840 | BLE 5.0 MCU, USB-C, LiPo charger | $9.90 |
| DISP1 | 1 | SSD1306 OLED 0.96" | 128×64 I2C display | $3.50 |
| ENC1 | 1 | Rotary Encoder | EC11 with pushbutton (internal pull-ups) | $1.50 |
| SW1 | 1 | Tactile Button | 6×6mm momentary | $0.10 |
| BT1 | 1 | LiPo Battery | 3.7V 2000mAh, JST-SH 1.25mm | $6.00 |
| C1 | 1 | Ceramic Capacitor | 100nF | $0.05 |
| C2 | 1 | Electrolytic Capacitor | 10µF | $0.10 |
| J1 | 1 | JST-SH Header | 2-pin, through-hole | $0.30 |
| — | 1 | Prototype PCB | 5×7cm | $0.50 |
| — | — | Wire, headers, enclosure | Misc | $3.00 |

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
| BAT- | Battery − |
| 3V3 | Power out to peripherals |
| GND | Ground rail |
| D0 | Encoder CLK (A) |
| D1 | Encoder DT (B) |
| D2 | Encoder SW (button) |
| D3 | Function button |
| D4 | I2C SDA (OLED) |
| D5 | I2C SCL (OLED) |
| D6 | Piezo buzzer |
| D7 | Mute button (Simple/Sim) / Play-Pause (Volume Control) |
| D8–D10 | Unused (available) |

> **Note:** Do not use `analogRead()` on A0/A1 — these share pins with the encoder and will break reads.

---

## Wiring

### Power (Direct — No Switch)

```
BT1 (+) → J1 Pin 1 → XIAO BAT+
BT1 (-) → J1 Pin 2 → XIAO BAT-
```

### OLED Display (SSD1306)

```
VCC → 3V3 Rail
GND → GND Rail
SCL → D5
SDA → D4
```

### Rotary Encoder (EC11)

```
GND → GND Rail
SW  → D2 (internal pull-up)
DT  → D1 (internal pull-up)
CLK → D0 (internal pull-up)
```

### Function Button

```
Pin 1 → D3
Pin 2 → GND Rail
```

### Decoupling Capacitors

Place C1 (100nF) and C2 (10µF) between 3V3 and GND, close to the XIAO.

---

## Build Target

```bash
make setup    # install PlatformIO + adafruit-nrfutil (first time)
make build    # compile (env: seeed_xiao_nrf52840)
make flash    # compile + flash via USB serial DFU
```

See also: [schematic_v8.svg](../../schematic_v8.svg), [schematic_interactive_v3.html](../../schematic_interactive_v3.html)

# Ghost Operator Architecture

## System overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        Host Computer                              │
│   ┌─────────┐    ┌──────────┐    ┌─────────────────────────┐    │
│   │ BLE HID │    │ USB HID  │    │ Chrome Web Dashboard    │    │
│   │ (KB+MS) │    │ (KB+MS)  │    │ (Vue 3 + Web Serial)   │    │
│   └────┬────┘    └────┬─────┘    └──────────┬──────────────┘    │
└────────┼──────────────┼─────────────────────┼───────────────────┘
         │ BLE 5.0      │ USB-C               │ Web Serial API
┌────────┼──────────────┼─────────────────────┼───────────────────┐
│        ▼              ▼                     ▼                    │
│   ┌─────────┐    ┌──────────┐    ┌──────────────────┐          │
│   │ Bluefruit│    │ TinyUSB  │    │ Config Protocol  │          │
│   │  BLE     │    │ CDC+HID  │    │ (Text + JSON)    │          │
│   └────┬────┘    └────┬─────┘    └────────┬─────────┘          │
│        └───────┬──────┘                    │                    │
│                ▼                           ▼                    │
│   ┌──────────────────┐    ┌───────────────────────────┐        │
│   │   HID Module     │    │   Settings + Storage      │        │
│   │ (dual-transport) │    │   (LittleFS flash)        │        │
│   └────────┬─────────┘    └───────────┬───────────────┘        │
│            │                          │                         │
│            ▼                          ▼                         │
│   ┌──────────────────────────────────────────────┐             │
│   │              Main Loop (loop())               │             │
│   │  ┌─────────┐ ┌──────────┐ ┌───────────────┐ │             │
│   │  │ Timing  │ │  Mouse   │ │  Orchestrator │ │             │
│   │  │Profiles │ │  FSM     │ │  (Simulation) │ │             │
│   │  └─────────┘ └──────────┘ └───────────────┘ │             │
│   └──────────────────────┬───────────────────────┘             │
│                          ▼                                      │
│   ┌──────────────┐  ┌──────────┐  ┌────────────┐              │
│   │  Display     │  │ Encoder  │  │   Sleep    │              │
│   │  (SSD1306)   │  │ (EC11)   │  │  Manager   │              │
│   └──────────────┘  └──────────┘  └────────────┘              │
│                                                                 │
│              Seeed XIAO nRF52840 (SoftDevice S140)              │
└─────────────────────────────────────────────────────────────────┘
```

Multi-platform: nRF52840 (primary, OLED), ESP32-S3-LCD and ESP32-C6-LCD (secondary, LVGL color displays).

## Domain layers

| Layer | Responsibility | May depend on |
|-------|----------------|---------------|
| **Portable (`src/common/`)** | Shared logic: config, keys, timing, mouse FSM, orchestrator, settings API, schedule, sim data | Nothing platform-specific |
| **Platform (`src/nrf52/`, `src/esp32-*/`)** | Hardware drivers: BLE, USB, display, encoder, battery, sleep, sound, flash I/O | Portable layer |
| **Dashboard (`dashboard/`)** | Vue 3 web app: Web Serial config, Web Serial DFU, BLE config | Independent (communicates via protocol) |
| **Build (`platformio.ini`, `boards/`)** | PlatformIO environments, board definitions, build scripts | — |

Each PlatformIO environment selects its own source trees via `build_src_filter`:
- **`seeed_xiao_nrf52840`**: `src/common/` + `src/nrf52/` → Entry: `src/nrf52/ghost_operator.cpp`
- **`s3lcd`**: `src/common/` + `src/esp32-s3-lcd-1.47/` → Entry: `src/esp32-s3-lcd-1.47/main.cpp`
- **`c6lcd`**: `src/common/` + `src/esp32-c6-lcd-1.47/` → Entry: `src/esp32-c6-lcd-1.47/main.cpp`
- **`native`**: Host Unity tests using `src/common/` + `test/`

## Domains

| Domain | Purpose | Key files | Status |
|--------|---------|-----------|--------|
| **HID** | Composite keyboard + mouse + consumer control over BLE and USB | `hid.h/.cpp` | Stable |
| **Display** | OLED rendering (nRF52), LVGL (ESP32) — all UI modes | `display.h/.cpp` (~2900 lines) | Stable |
| **Settings** | Persistent config via LittleFS/NVS, data-driven menu (62 items) | `config.h`, `settings_common.cpp`, `keys.cpp` | Stable |
| **Orchestrator** | Simulation mode — 4-level hierarchical state machine | `orchestrator.h/.cpp`, `sim_data.h/.cpp` | Stable |
| **Mouse** | Bezier/Brownian movement with sine easing, return-to-origin | `mouse.h/.cpp` | Stable |
| **Timing** | LAZY/NORMAL/BUSY profiles, randomized scheduling | `timing.h/.cpp` | Stable |
| **Encoder** | Hybrid ISR + polling quadrature decode (EC11) | `encoder.h/.cpp` | Stable |
| **Power** | Two-stage sleep (light/deep), WDT, scheduled wake | `sleep.h/.cpp` | Stable |
| **Sound** | Piezo buzzer with 5 keyboard sound profiles | `sound.h/.cpp` | Stable |
| **Protocol** | Transport-agnostic text + JSON config (BLE UART + USB Serial) | `ble_uart.h/.cpp`, `protocol.h/.cpp`, `serial_cmd.h/.cpp` | Stable |
| **Schedule** | Timed auto-sleep / full auto with 5-min slot granularity | `schedule.h/.cpp` | Stable |
| **DFU** | Serial DFU (USB) + OTA DFU (BLE) firmware updates | `ble_uart.cpp`, `dashboard/src/lib/dfu/` | Stable |
| **Games** | Breakout, Snake, Ghost Racer arcade games | `breakout.h/.cpp`, `snake.h/.cpp`, `racer.h/.cpp` | Stable |
| **Dashboard** | Vue 3 + Vite web app (Web Serial config + DFU) | `dashboard/` | Stable |

## Hardware

### Microcontroller
- **Seeed XIAO nRF52840** — ARM Cortex-M4 @ 64MHz, BLE 5.0, 1MB Flash, 256KB RAM
- Built-in LiPo charger (USB-C), deep sleep ~3µA

### Components (8 total)
| Ref | Component | Purpose |
|-----|-----------|---------|
| U1 | XIAO nRF52840 | MCU + BLE |
| DISP1 | SSD1306 OLED 128×64 | Display (I2C) |
| ENC1 | EC11 Rotary Encoder | Input + key selection |
| SW1 | Tactile button 6×6mm | Mode cycle / sleep |
| SW2 | Tactile button 6×6mm | Mute (KB/MS toggle) |
| BT1 | LiPo 3.7V 2000mAh | Power |
| C1 | 100nF ceramic | Decoupling |
| C2 | 10µF electrolytic | Decoupling |

### Pin assignments
| Pin | Function | Notes |
|-----|----------|-------|
| D0 | Encoder CLK (A) | Interrupt-driven |
| D1 | Encoder DT (B) | Interrupt-driven |
| D2 | Encoder SW | Push button |
| D3 | Function button | Mode/sleep |
| D4 | I2C SDA | OLED |
| D5 | I2C SCL | OLED |
| D6 | Piezo buzzer | Keyboard sound output |
| D7 | Mute button | SPST momentary, active LOW |
| D8-D10 | Unused | Available for expansion |
| BAT+/- | Battery | Direct to LiPo |

## Dependencies

### nRF52 firmware
- Adafruit Bluefruit (built into Seeed nRF52 core)
- Adafruit GFX Library + SSD1306
- Adafruit LittleFS (built into core)
- TinyUSB (built into core)

### ESP32 firmware
- NimBLE-Arduino
- LVGL + TFT_eSPI
- Arduino NVS

### Dashboard
- Vue 3 + Vite
- fflate (ZIP decompression for DFU)

## External integrations

| Integration | Transport | Module | Gotchas |
|-------------|-----------|--------|---------|
| Web Bluetooth | BLE GATT (NUS) | `dashboard/src/lib/ble.js` | NUS UUID not in advertising — use `optionalServices`. Chrome blocklists legacy DFU UUID. |
| Web Serial | USB CDC 115200 | `dashboard/src/lib/serial.js` | DTR toggle required for DFU. Same port reused between app and bootloader. |
| WebUSB | USB | `ghost_operator.cpp` | Auto-opens dashboard in Chrome. Smart default: auto-disables after 3 boots. |
| fflate | NPM | `dashboard/src/lib/dfu/zip.js` | Only prod dependency besides Vue. Synchronous ZIP API. |

## Key design decisions

1. **Dual-transport HID.** Composite keyboard + mouse + consumer control over both BLE and USB simultaneously. `dualKeyboardReport()` sends to both transports. "BT while USB" setting controls whether BLE stays active when wired.

2. **SoftDevice S140 v7.3.0 via Seeed framework override.** PlatformIO's default Adafruit core ships S140 v6.1.1. The `platform_packages` override in `platformio.ini` pins to Seeed's fork `#1.1.12` which bundles the correct SoftDevice version.

3. **Data-driven menu system.** 62-entry `MENU_ITEMS[]` array drives all menu rendering, editing, and persistence. Adding a new setting requires array entries + getter/setter cases — no display code changes needed.

4. **Transport-agnostic config protocol.** `processCommand(line, writer)` accepts a `ResponseWriter` function pointer. Same text protocol (`?query`, `=set`, `!action`) works over BLE UART and USB serial.

5. **Struct-based settings with magic number versioning.** `Settings` struct saved as raw bytes to LittleFS. `SETTINGS_MAGIC` encodes schema version — bump triggers safe `loadDefaults()` instead of reading corrupt data.

6. **Portable/platform split.** `src/common/` contains all hardware-independent logic (config, keys, timing, mouse FSM, orchestrator, settings API). Platform directories add hardware drivers. This enables multi-platform (nRF52, ESP32-S3, ESP32-C6) and native testing.

7. **`lib_archive = no` is mandatory.** PlatformIO's default library archiving strips TinyUSB's indirect function pointer driver tables, breaking USB CDC/HID.

8. **Hierarchical orchestrator for Simulation mode.** 4-level state machine (DayTemplate → TimeBlock → WorkMode → Phase) generates realistic non-repetitive activity patterns. See [docs/references/orchestrator.md](docs/references/orchestrator.md).

## File inventory

| File | Purpose |
|------|---------|
| `src/common/config.h` | Constants, enums, structs (header-only) |
| `src/common/keys.h` / `keys.cpp` | Const data tables (keys, menu items, names) |
| `src/common/timing.h` / `timing.cpp` | Profiles, scheduling, formatting |
| `src/common/mouse.h` / `mouse.cpp` | Mouse state machine with sine easing |
| `src/common/schedule.h` / `schedule.cpp` | Timed schedule (shared logic) |
| `src/common/orchestrator.h` / `orchestrator.cpp` | Simulation activity orchestrator |
| `src/common/sim_data.h` / `sim_data.cpp` | Simulation data tables (job templates, work modes, phase timing) |
| `src/common/settings.h`, `settings_pure.h` / `settings_common.cpp` | Shared settings API |
| `src/common/state.h` | Portable globals |
| `src/nrf52/ghost_operator.cpp` | Entry point: setup(), loop() |
| `src/nrf52/settings_nrf52.cpp` | Flash-backed loadSettings() / saveSettings() |
| `src/nrf52/state.h` / `state.cpp` | nRF52 globals, hardware handles, game macros |
| `src/nrf52/icons.h` / `icons.cpp` | PROGMEM bitmaps |
| `src/nrf52/encoder.h` / `encoder.cpp` | ISR + polling quadrature decode |
| `src/nrf52/battery.h` / `battery.cpp` | ADC battery reading |
| `src/nrf52/hid.h` / `hid.cpp` | Keystroke + mouse + scroll sending (BLE + USB) |
| `src/nrf52/sleep.h` / `sleep.cpp` | Deep sleep sequence |
| `src/nrf52/serial_cmd.h` / `serial_cmd.cpp` | Serial debug commands + status |
| `src/nrf52/input.h` / `input.cpp` | Encoder dispatch, buttons, name editor |
| `src/nrf52/display.h` / `display.cpp` | All rendering (~2900 lines) |
| `src/nrf52/ble_uart.h` / `ble_uart.cpp` | BLE UART (NUS) + config protocol |
| `src/nrf52/sound.h` / `sound.cpp` | Piezo buzzer keyboard sounds |
| `src/nrf52/protocol.h` / `protocol.cpp` | JSON config protocol |
| `src/nrf52/breakout.h` / `breakout.cpp` | Breakout arcade game |
| `src/nrf52/snake.h` / `snake.cpp` | Classic snake game |
| `src/nrf52/racer.h` / `racer.cpp` | Ghost Racer racing game |
| `src/esp32-s3-lcd-1.47/` | ESP32-S3 + LCD firmware |
| `src/esp32-c6-lcd-1.47/` | ESP32-C6 + LCD firmware |
| `platformio.ini` | PlatformIO build configuration |
| `boards/seeed_xiao_nrf52840.json` | Custom board definition |
| `dashboard/` | Vue 3 web dashboard |
| `dashboard/src/lib/dfu/` | Web Serial DFU library |

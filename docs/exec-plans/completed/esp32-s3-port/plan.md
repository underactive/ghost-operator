# Ghost Operator — ESP32-S3-LCD-1.47 Port

## Context

Ghost Operator already runs on three platforms: nRF52840 (production), CYD (ESP32, separate branch), and ESP32-C6-LCD-1.47 (current branch). The C6 port established the multi-platform architecture (`src/common/`, `src/c6/`, `platform_hal.h`) with LVGL color UI, NimBLE BLE HID, and JSON config protocol.

The **Waveshare ESP32-S3-LCD-1.47** is the same form factor as the C6 board but with a significantly more powerful ESP32-S3 and — critically — a **USB-A male plug with native USB OTG**. This enables USB HID, making it the first ESP32 platform that can work as both a wired and wireless jiggler.

### Why this port matters
The C6 is BLE-only. The S3 with USB-A plugs directly into a computer's USB port — no cable, no pairing, instant USB HID. It can also fall back to BLE HID when powered by a USB bank. This makes it the most versatile Ghost Operator platform.

---

## Hardware Comparison: S3 vs C6

| Feature | ESP32-C6-LCD-1.47 | ESP32-S3-LCD-1.47 |
|---------|--------------------|--------------------|
| **CPU** | RISC-V single-core @ 160MHz | **Xtensa LX7 dual-core @ 240MHz** |
| **SRAM** | 512KB | 512KB |
| **PSRAM** | None | **8MB (integrated in ESP32-S3R8)** |
| **Flash** | 4MB (internal) | **16MB (external W25Q128)** |
| **USB connector** | USB-C (Serial/JTAG only) | **USB-A male plug (USB OTG!)** |
| **USB HID** | No | **Yes (native TinyUSB)** |
| **BLE** | BLE 5 | BLE 5 |
| **WiFi** | WiFi 6 (802.11ax) | WiFi (802.11 b/g/n) |
| **Display** | 1.47" ST7789 172x320 | 1.47" ST7789 172x320 (same) |
| **NeoPixel** | GPIO8 | **GPIO38** |
| **LCD MOSI** | GPIO6 | **GPIO45** |
| **LCD SCLK** | GPIO7 | **GPIO40** |
| **LCD CS** | GPIO14 | **GPIO42** |
| **LCD DC** | GPIO15 | **GPIO41** |
| **LCD RST** | GPIO21 | **GPIO39** |
| **LCD BL** | GPIO22 | **GPIO48** |
| **Buttons** | BOOT + RESET | BOOT + RESET (same) |
| **SD Card** | Yes | Yes |
| **Power reg** | — | ME6217C33M5G (3.3V LDO from VBUS) |
| **Crystal** | — | 40MHz external |

### Pin Map (from schematic)

```
ESP32-S3R8 Pin Assignments:
  GPIO0  → BOOT button (active LOW, 10K pull-up)
  GPIO1-13 → Header pins (free GPIOs)
  GPIO14 → SD_SCLK
  GPIO15 → SD_MOSI
  GPIO16 → SD_MISO
  GPIO17 → SD_D2
  GPIO18 → SD_D1
  GPIO19 → USB D- (through 22Ω to USB-A)
  GPIO20 → USB D+ (through 22Ω to USB-A)
  GPIO21 → SD_CS
  GPIO38 → RGB_IO (WS2812B NeoPixel)
  GPIO39 → LCD_RST
  GPIO40 → LCD_CLK (SCLK)
  GPIO41 → LCD_DC
  GPIO42 → LCD_CS
  GPIO45 → LCD_DIN (MOSI)
  GPIO46 → (unassigned on schematic)
  GPIO47 → (unassigned on schematic)
  GPIO48 → LCD_BL (backlight, MOSFET-driven)
  TXD/RXD → UART header pins
```

---

## Dual-Core S3 Advantages

1. **No BLE radio preemption** — On the single-core C6, BLE radio events preempt application code, which can cause LVGL rendering stutters and timing jitter on HID reports. The S3 runs the WiFi/BLE stack on Core 0 and application code on Core 1. This means smoother display updates and more consistent HID timing.

2. **USB HID + BLE HID simultaneously** — The dual cores make it practical to run both USB and BLE HID stacks without contention. Core 0 handles BLE, Core 1 handles USB + app logic.

3. **Faster LVGL rendering** — 240MHz vs 160MHz = 50% faster CPU for LVGL draw operations. Combined with 8MB PSRAM for larger display buffers, the UI will be noticeably smoother.

4. **8MB PSRAM** — Enables full-frame LVGL display buffers (320×172×2 = ~110KB) instead of the C6's partial 32-line buffers (~20KB). This eliminates tearing artifacts during fast UI transitions.

5. **16MB Flash** — Room for OTA firmware updates, larger font sets, and future features without flash pressure.

6. **USB Serial for config** — The USB-A connection provides a USB CDC serial port, enabling the existing Web Serial dashboard (same as nRF52). No need for BLE-only configuration.

## Hardware Disadvantages

1. **USB-A form factor** — The board sticks straight out of the computer's USB port. It can get bumped or broken. Less discrete than the C6 which hides behind a monitor with a cable.

2. **No USB-A on modern Macs** — MacBooks (2016+) have only USB-C ports. Requires a USB-C to USB-A adapter/hub, which partially defeats the "just plug it in" convenience.

3. **No standalone battery operation** — Like the C6, there's no onboard battery. But unlike the C6 (which has USB-C for power bank cables), USB-A male can only plug into a USB-A receptacle (computer or power bank with USB-A port).

4. **Higher power draw** — Dual-core 240MHz + PSRAM draws more current than single-core 160MHz C6. Not an issue when USB-powered, but reduces power bank runtime.

5. **WiFi downgrade** — 802.11 b/g/n vs C6's WiFi 6. Irrelevant since Ghost Operator doesn't use WiFi, but worth noting.

6. **USB-A is a dying connector** — Industry is moving to USB-C. The S3 board may have limited shelf life for users with only USB-C ports.

---

## Implementation Plan

The S3 port is essentially a **fork of the C6 codebase** with three major additions:
1. Different pin definitions
2. USB HID support (wired keyboard+mouse+consumer)
3. USB Serial support (config protocol + dashboard)

### New platform flag: `GHOST_PLATFORM_S3`

### Directory: `src/s3/`

---

### Phase 1: PlatformIO Environment + Pin Config

**Goal:** New `[env:s3lcd]` environment compiles. S3 pin definitions in `config.h`.

#### Files to modify

**`platformio.ini`** — Add new environment:
```ini
[env:s3lcd]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi   ; PSRAM access mode
lib_deps =
    h2zero/NimBLE-Arduino @ ^2.1.0
    adafruit/Adafruit NeoPixel @ ^1.12.0
    bblanchon/ArduinoJson @ ^7.4.1
    lvgl/lvgl@^9.2.0
lib_ldf_mode = deep+
build_flags =
    -Os -Wall -Wextra -Wno-unused-parameter
    -DGHOST_PLATFORM_S3=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
    -DLV_CONF_INCLUDE_SIMPLE
    -Isrc/common
    -Isrc/s3
    -Iinclude
build_src_filter = +<common/> +<s3/>
monitor_speed = 115200
```

**`src/common/config.h`** — Add S3 platform block:
```cpp
#ifdef GHOST_PLATFORM_S3
  #define HAS_BATTERY       0
  #define HAS_SOUND         0
  #define HAS_USB_HID       1    // ← KEY DIFFERENCE from C6!
  #define HAS_ENCODER       0
  #define HAS_TOUCH         0
  #define HAS_NEOPIXEL      1
#endif

// Display config — same resolution as C6
#ifdef GHOST_PLATFORM_S3
  #define SCREEN_WIDTH  320
  #define SCREEN_HEIGHT 172
#endif

// S3 pin definitions (from schematic)
#ifdef GHOST_PLATFORM_S3
  #define PIN_LCD_MOSI      45
  #define PIN_LCD_SCLK      40
  #define PIN_LCD_CS         42
  #define PIN_LCD_DC         41
  #define PIN_LCD_RST        39
  #define PIN_LCD_BL         48
  #define PIN_NEOPIXEL       38
#endif
```

Also update `OP_MODE_MAX` — S3 supports Simple + Simulation (same as C6 for now, no encoder for games):
```cpp
#if defined(GHOST_PLATFORM_C6) || defined(GHOST_PLATFORM_S3)
  #define OP_MODE_MAX OP_SIMULATION
#endif
```

#### Files to create: `src/s3/`

Fork each file from `src/c6/`, updating only what differs:

| File | Source | Changes needed |
|------|--------|----------------|
| `config.h` | Copy from `src/c6/config.h` | Change `C6` → `S3` guard name |
| `main.cpp` | Fork from `src/c6/main.cpp` | Add USB HID init, dual-transport logic |
| `ble.h` / `ble.cpp` | Copy from `src/c6/` | Identical (NimBLE BLE HID) |
| `ble_uart.h` / `ble_uart.cpp` | Copy from `src/c6/` | Identical (NUS + JSON protocol) |
| `protocol.h` / `protocol.cpp` | Copy from `src/c6/` | Add `platform=s3` in status, add USB-related queries |
| `hid.cpp` | Fork from `src/c6/hid.cpp` | **Major changes** — dual transport (USB + BLE) |
| `display.h` / `display.cpp` | Fork from `src/c6/display.cpp` | Different SPI pins, add USB status icon |
| `led.h` / `led.cpp` | Fork from `src/c6/led.cpp` | Change `PIN_NEOPIXEL` (handled by config.h) |
| `sleep.h` / `sleep.cpp` | Copy from `src/c6/` | Identical |
| `settings_s3.cpp` | Copy from `src/c6/settings_c6.cpp` | Identical (NVS persistence) |
| `sim_data_nvs.cpp` | Copy from `src/c6/` | Identical |
| `state.h` / `state.cpp` | Fork from `src/c6/` | Add USB connection state |
| `serial_cmd.h` / `serial_cmd.cpp` | Fork from `src/c6/` | Route through USB CDC serial |
| `ghost_splash.h` | Copy from `src/c6/` | Identical |
| `ghost_sprite.h` | Copy from `src/c6/` | Identical |
| `kbm_icons.h` | Copy from `src/c6/` | Identical |

#### Verification
- `pio run -e s3lcd` compiles cleanly
- `pio run -e c6lcd` still compiles (no regressions)
- `pio run -e seeed_xiao_nrf52840` still compiles (no regressions)

---

### Phase 2: USB HID — Wired Keyboard + Mouse

**Goal:** USB-A plug presents as composite HID device. Keystrokes and mouse movements arrive on the host computer.

This is the biggest new feature vs the C6 port.

#### `src/s3/hid.cpp` — Dual transport HID

The ESP32-S3 Arduino core provides `USB.h`, `USBHIDKeyboard.h`, `USBHIDMouse.h`, and `USBHIDConsumerControl.h`. These use TinyUSB under the hood.

```cpp
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDConsumerControl.h>

USBHIDKeyboard UsbKeyboard;
USBHIDMouse UsbMouse;
USBHIDConsumerControl UsbConsumer;
```

Key design decisions:
- **USB HID is primary transport** when USB is connected to a host (detected via `USB.connected()` or TinyUSB callbacks)
- **BLE HID is fallback** when powered by a USB bank (no USB host enumeration)
- Both transports can be active simultaneously (like nRF52's `dualKeyboardReport()`)
- `usbConnected` state variable tracks USB host enumeration

#### HAL functions — dual transport

```cpp
void sendKeystroke() {
  // Send via USB if connected
  if (usbHostConnected) {
    UsbKeyboard.press(keycode);
    delay(50);
    UsbKeyboard.releaseAll();
  }
  // Send via BLE if connected
  if (deviceConnected) {
    sendBleKeystroke(keycode);
  }
}
```

Same pattern for `sendMouseMove()`, `sendMouseScroll()`, `sendMouseClick()`, `sendWindowSwitch()`, `sendConsumerPress/Release()`.

#### USB Serial (CDC)

The ESP32-S3 Arduino core provides USB CDC serial via the default `Serial` object when `ARDUINO_USB_CDC_ON_BOOT=1`. This means:
- `Serial.println()` outputs to USB serial (same as nRF52)
- Serial commands work over USB (same protocol)
- Web Serial dashboard can connect over USB

#### `src/s3/main.cpp` — USB init

```cpp
void setup() {
  USB.begin();              // Initialize USB stack
  UsbKeyboard.begin();     // Register keyboard HID
  UsbMouse.begin();        // Register mouse HID
  UsbConsumer.begin();     // Register consumer control HID
  Serial.begin(115200);    // USB CDC serial
  // ... rest of setup (BLE, display, etc.)
}
```

#### Verification
- Flash to S3 board → plugs into computer USB-A port
- USB HID enumerates as keyboard + mouse + consumer
- Keystrokes arrive on host
- Mouse moves on host
- Serial monitor works over USB
- Unplug from computer, plug into power bank → BLE HID works

---

### Phase 3: Display + LED Adaptation

**Goal:** LVGL display renders correctly with S3 SPI pins. NeoPixel works on GPIO38.

#### `src/s3/display.cpp`

Fork from `src/c6/display.cpp`. The main change is SPI pin assignment. Since pins come from `config.h` defines (`PIN_LCD_MOSI`, `PIN_LCD_SCLK`, etc.), most of the display code is identical. Key differences:

1. **SPI bus selection** — S3 uses HSPI (SPI2) for the LCD. The C6 may use a different SPI peripheral. Need to verify `SPIClass(HSPI)` works with the S3 pin mapping.

2. **PSRAM display buffers** — With 8MB PSRAM available, upgrade from partial buffers to larger buffers:
   ```cpp
   // C6: 32-line partial buffers in SRAM (~20KB each)
   // S3: Can use larger buffers from PSRAM for smoother rendering
   #define BUF_LINES 86  // half-screen height = 172/2
   static uint8_t buf1[320 * BUF_LINES * 2];  // ~55KB
   static uint8_t buf2[320 * BUF_LINES * 2];  // ~55KB
   ```
   Or even allocate from PSRAM with `ps_malloc()` for full-frame buffers.

3. **USB status indicator** — Display should show USB trident icon when wired (like nRF52), BLE icon when wireless.

4. **Platform identifier** — Footer or header shows "S3" instead of "C6".

#### `src/s3/led.cpp`

Identical to C6 code. Pin difference handled by `PIN_NEOPIXEL` define in config.h (38 vs 8). The Adafruit NeoPixel library is pin-agnostic.

#### Verification
- Boot → splash screen renders on TFT
- Display shows "S3" platform indicator
- NeoPixel: blue=KB, green=mouse, breathing blue=advertising
- USB icon when wired, BLE icon when wireless

---

### Phase 4: USB Detection + Transport Switching

**Goal:** Automatically detect USB host vs power bank. Switch between USB HID and BLE HID accordingly.

#### Detection strategy

The ESP32-S3's USB OTG peripheral can detect host enumeration. When plugged into a computer, the USB host sends a reset signal and begins enumeration. When plugged into a power bank, no enumeration occurs — only VBUS power.

```cpp
// In main loop or via TinyUSB callback:
bool usbHostDetected = tud_connected();  // TinyUSB function
```

#### Transport behavior

| Scenario | USB HID | BLE HID | BLE Advertising | USB Serial |
|----------|---------|---------|-----------------|------------|
| Plugged into computer | Active | Optional (btWhileUsb setting) | Per btWhileUsb | Active |
| Plugged into power bank | Inactive | Active | Active | Inactive |
| BLE paired + USB host | Active | Active (dual) | No | Active |

This mirrors the nRF52's "BT while USB" setting behavior.

#### `src/s3/state.h`

Add `usbHostConnected` flag (distinct from `usbConnected` which may indicate VBUS power only):
```cpp
extern bool usbHostConnected;  // USB host has enumerated us
```

#### Protocol updates

- `?status` response includes `usb=1` when USB host connected, `platform=s3`
- `btWhileUsb` setting applies (stop BLE when USB host detected, unless setting is On)

#### Verification
- Plug into computer → USB HID active, BLE stops (default btWhileUsb=Off)
- Set btWhileUsb=On → both USB and BLE HID active simultaneously
- Plug into power bank → BLE HID active, USB HID inactive
- Display shows correct icon for active transport

---

### Phase 5: Web Serial Dashboard + DFU

**Goal:** Enable Web Serial dashboard access and USB serial firmware updates.

#### Web Serial

Since the S3 has USB CDC serial, the existing Web Serial dashboard (`dashboard/`) should work with minimal changes:
- Connection: Web Serial API (same as nRF52)
- Protocol: The S3 supports both pipe-delimited (`?/=/!`) and JSON protocols
- Dashboard detects platform via `?status` response (`platform=s3`)

#### Firmware updates

Two options for DFU on S3:
1. **esptool over USB serial** — standard ESP32 flashing via `esptool.py` or PlatformIO upload
2. **UF2 bootloader** — if the S3 board has UF2 support, drag-and-drop firmware update

For now, standard PlatformIO upload (`pio run -e s3lcd -t upload`) is sufficient.

#### Verification
- Open Web Serial dashboard in Chrome → connects to S3 over USB
- Settings push/pull works
- `pio run -e s3lcd -t upload` flashes firmware

---

## File Summary

### New files (src/s3/)
| File | Purpose |
|------|---------|
| `config.h` | S3 platform config wrapper |
| `main.cpp` | Entry point with USB + BLE init |
| `ble.h` / `ble.cpp` | NimBLE BLE HID (forked from C6) |
| `ble_uart.h` / `ble_uart.cpp` | BLE UART NUS + protocol (forked from C6) |
| `protocol.h` / `protocol.cpp` | JSON config protocol (forked from C6) |
| `hid.cpp` | **Dual-transport HID** (USB + BLE) — key new file |
| `display.h` / `display.cpp` | LVGL display (forked from C6, different SPI pins) |
| `led.h` / `led.cpp` | NeoPixel activity LED (forked from C6) |
| `sleep.h` / `sleep.cpp` | Sleep management (forked from C6) |
| `settings_s3.cpp` | NVS persistence (forked from C6) |
| `sim_data_nvs.cpp` | Sim data NVS persistence (copy from C6) |
| `state.h` / `state.cpp` | State with USB host detection (forked from C6) |
| `serial_cmd.h` / `serial_cmd.cpp` | Serial debug commands (forked from C6) |
| `ghost_splash.h` | Splash screen bitmap (copy from C6) |
| `ghost_sprite.h` | Ghost sprite bitmap (copy from C6) |
| `kbm_icons.h` | Activity icons (copy from C6) |

### Modified files
| File | Changes |
|------|---------|
| `platformio.ini` | Add `[env:s3lcd]` environment |
| `src/common/config.h` | Add `GHOST_PLATFORM_S3` blocks (pins, features, display) |

### Files NOT changed
- All `src/common/` logic files — already portable
- All `src/nrf52/` files — untouched
- All `src/c6/` files — untouched
- `dashboard/` — works as-is over Web Serial

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| ESP32-S3 Arduino USB HID API differs from expected | Medium | High | Fall back to raw TinyUSB descriptors (proven on nRF52) |
| USB host detection unreliable on power banks | Low | Medium | Use enumeration state, not VBUS presence |
| SPI pin conflict with PSRAM (octal SPI uses GPIO33-37) | Low | Low | LCD pins (38-48) don't overlap PSRAM pins |
| NimBLE + TinyUSB memory contention | Low | Medium | S3 has 512KB SRAM + 8MB PSRAM — ample headroom |
| Board needs custom PlatformIO board JSON | Medium | Low | Use `esp32-s3-devkitc-1` base, override pins via build flags |
| HSPI port assignment for LCD SPI | Low | Low | Explicitly configure SPI2 with custom pins |

---

## Verification Plan

### End-to-end testing
1. **USB HID:** Plug S3 into computer → keystrokes + mouse movements arrive → display shows USB icon
2. **BLE HID:** Plug S3 into power bank → BLE pair → keystrokes + mouse arrive → display shows BLE icon
3. **Dual transport:** Set btWhileUsb=On → plug into computer → both USB and BLE HID active
4. **Web Serial:** Open dashboard in Chrome → connect via Web Serial → settings push/pull works
5. **BLE config:** Connect via Web Bluetooth → JSON protocol queries/sets work
6. **Simulation mode:** Switch to sim via JSON → orchestrator runs → display shows sim info
7. **Schedule:** Set schedule via JSON → auto-sleep/wake at scheduled times
8. **Memory stability:** `ESP.getFreeHeap()` stable over 1+ hour (no leaks)
9. **Cross-platform regression:** `pio run -e c6lcd` and `pio run -e seeed_xiao_nrf52840` still compile

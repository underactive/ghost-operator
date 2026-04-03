# Ghost Operator — ESP32-C6-LCD-1.47 Port

## Context

Ghost Operator is a BLE keyboard/mouse jiggler. The nRF52840 version (v2.5.2) is production-ready. A CYD (ESP32) port exists on a separate branch with a proven multi-platform architecture (`src/common/`, `src/nrf52/`, `src/cyd/`, `platform_hal.h`).

This plan ports Ghost Operator to the **Waveshare ESP32-C6-LCD-1.47** — a tiny RISC-V board with a 1.47" color TFT. The motivation is form factor: the entire device is the size of the nRF52 board's OLED screen. Only **Simple** and **Simulation** modes are needed. No physical controls — all configuration via BLE dashboard/companion app.

### Target Hardware
- ESP32-C6FH4: RISC-V @ 160MHz, 512KB SRAM, 4MB Flash
- 1.47" 172×320 color TFT (ST7789, SPI)
- BLE 5 + WiFi 6 (WiFi not used)
- USB-C: serial/JTAG only (**no USB HID**)
- WS2811 NeoPixel RGB LED (GPIO8)
- No battery, no encoder, no buttons (BOOT/RESET only)

### Key Design Decisions
| Decision | Choice | Rationale |
|----------|--------|-----------|
| HID transport | BLE-only | No USB OTG on ESP32-C6 |
| Display framework | LVGL v9 | Color + gradients; built-in ST7789 driver avoids TFT_eSPI compat issues |
| Config transport | BLE UART (NUS) | Consistent with existing devices; Web Bluetooth dashboard |
| Config protocol | JSON (ArduinoJson v7) | Replaces pipe-delimited text; designed for companion app; eventual migration for all devices |
| Settings ownership | App pushes to device | Companion app is source of truth |
| BLE stack | NimBLE-Arduino 2.x | ESP32-C6 + Arduino Core 3.x compatibility |
| Storage | ESP32 Preferences (NVS) | Same pattern as CYD port |
| Activity LED | NeoPixel: blue=KB, green=mouse | Matches nRF52 convention |

---

## Phase 1: Directory Restructure

**Goal:** Split flat `src/` into `src/common/` + `src/nrf52/`. Verify nRF52 still builds.

The current branch has no platform split (unlike the CYD branch). This must happen first.

### Files to create: `src/common/`
| File | Source | Notes |
|------|--------|-------|
| `platform_hal.h` | **New** (based on CYD's) | Updated for v2.5.2: adds `saveStats()`, `loadStats()`, `tickActivityLeds()` |
| `config.h` | From `src/config.h` | Add `#ifdef GHOST_PLATFORM_*` blocks for pins, display dims, feature flags |
| `keys.h` / `keys.cpp` | From `src/` | Guard Bluefruit includes behind `#ifdef GHOST_PLATFORM_NRF52` |
| `mouse.h` / `mouse.cpp` | From `src/` | Already portable (calls HAL functions) |
| `orchestrator.h` / `orchestrator.cpp` | From `src/` | Zero hardware deps |
| `sim_data.h` / `sim_data.cpp` | From `src/` | Const data tables only (persistence split out) |
| `timing.h` / `timing.cpp` | From `src/` | Zero hardware deps |
| `schedule.h` / `schedule.cpp` | From `src/` | Portable (calls HAL `enterLightSleep`/`enterDeepSleep`) |
| `settings.h` | From `src/settings.h` | Interface declarations only |
| `settings_common.cpp` | Extract from `src/settings.cpp` | `loadDefaults()`, `calcChecksum()`, `get/setSettingValue()`, `formatMenuValue()` |
| `state.h` / `state.cpp` | Extract platform-independent parts | Settings, timers, orchestrator state, mouse state, schedule state |
| `hid_keycodes.h` | **New** (from CYD branch) | Portable HID key defines for non-nRF52 platforms |

### Files to create: `src/nrf52/`
Everything else — the nRF52-specific modules. Key files:
- `ghost_operator.cpp` (renamed from `.ino`)
- `state.h` / `state.cpp` (includes `../common/state.h`, adds SSD1306, Bluefruit, TinyUSB objects)
- `hid.h` / `hid.cpp`, `display.h` / `display.cpp`, `ble_uart.h` / `ble_uart.cpp`
- `settings_nrf52.cpp` (LittleFS persistence), `sim_data_flash.cpp` (LittleFS sim data)
- `battery`, `encoder`, `input`, `sleep`, `sound`, `serial_cmd`, `screenshot`
- `icons`, `breakout`, `snake`, `racer`
- Splash bitmap files

### platformio.ini changes (nRF52 env)
```ini
build_flags = ... -DGHOST_PLATFORM_NRF52=1 -Isrc/common -Isrc/nrf52
build_src_filter = +<common/> +<nrf52/>
```

### config.h platform blocks
```cpp
#ifdef GHOST_PLATFORM_C6
  #define DEVICE_NAME       "GhostOp C6"
  #define SCREEN_WIDTH      320   // landscape (ST7789 rotated)
  #define SCREEN_HEIGHT     172
  #define HAS_BATTERY       0
  #define HAS_SOUND         0
  #define HAS_USB_HID       0
  #define HAS_ENCODER       0
  #define HAS_TOUCH         0
  #define HAS_NEOPIXEL      1
  // C6 pin definitions
  #define PIN_LCD_MOSI      6
  #define PIN_LCD_SCLK      7
  #define PIN_LCD_CS        14
  #define PIN_LCD_DC        15
  #define PIN_LCD_RST       21
  #define PIN_LCD_BL        22
  #define PIN_NEOPIXEL      8
#elif defined(GHOST_PLATFORM_NRF52)
  // existing nRF52 defines...
#endif
```

### Verification
- `pio run -e seeed_xiao_nrf52840` compiles cleanly
- Flash to nRF52 board → BLE connects, keystrokes send, display works, serial commands work

---

## Phase 2: C6 Bootstrap — Boot, BLE, HID

**Goal:** Create `src/c6/` skeleton. Board boots, NimBLE advertises, BLE HID keyboard+mouse works. Serial-only status (no display yet).

### Files to create: `src/c6/`
| File | Purpose |
|------|---------|
| `main.cpp` | `setup()` + `loop()` entry point |
| `ble.h` / `ble.cpp` | NimBLE 2.x server, HID report descriptor (keyboard+mouse composite), advertising, security callbacks |
| `ble_uart.h` / `ble_uart.cpp` | NUS service setup, RX line buffer, TX chunked writes |
| `hid.cpp` | HAL: `sendKeystroke()`, `sendMouseMove()`, `sendMouseScroll()`, etc. (BLE-only, no USB path) |
| `state.h` / `state.cpp` | Includes `../common/state.h`, adds NimBLE characteristic pointers |
| `settings_c6.cpp` | `saveSettings()`, `loadSettings()`, `saveStats()`, `loadStats()` via ESP32 Preferences (NVS) |
| `sim_data_nvs.cpp` | `initWorkModes()`, `saveSimData()`, `resetSimDataDefaults()` via NVS |
| `led.h` / `led.cpp` | NeoPixel activity: blue=KB, green=mouse, breathing blue=advertising, solid green=connected |
| `sleep.h` / `sleep.cpp` | Stub: backlight control, placeholder for schedule-driven sleep |
| `serial_cmd.h` / `serial_cmd.cpp` | Serial debug commands (reuse pattern from CYD) |
| `display.h` / `display.cpp` | **Stubs only** — `markDisplayDirty()`, `invalidateDisplayShadow()`, `setupDisplay()`, `updateDisplay()` as no-ops. Real implementation in Phase 4. |

### platformio.ini — new C6 environment
```ini
[env:c6lcd]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino

lib_deps =
    h2zero/NimBLE-Arduino @ ^2.1.0
    lvgl/lvgl @ ^9.2.0
    bblanchon/ArduinoJson @ ^7.0.0
    adafruit/Adafruit NeoPixel @ ^1.12.0

build_flags =
    -Os -Wall -Wextra -Wno-unused-parameter
    -DGHOST_PLATFORM_C6=1
    -Isrc/common -Isrc/c6
    -DLV_CONF_INCLUDE_SIMPLE

build_src_filter = +<common/> +<c6/>
monitor_speed = 115200
```

### BLE HID — reuse CYD's report descriptor
The CYD branch's `ble.cpp` has a proven composite keyboard+mouse HID descriptor over NimBLE. Fork it, updating NimBLE 1.x → 2.x API calls. Key differences:
- `NimBLEDevice::init()` and `createServer()` API may differ
- Security: `NimBLEDevice::setSecurityAuth(true, true, true)` for bonding+MITM+SC

### main.cpp minimal loop
```
setup: Serial → loadSettings → initWorkModes → setupBLE → setupLed → init timers
loop:  handleSerial → checkSchedule → tickLed → dispatch (simple or sim)
```

### Verification
- `pio run -e c6lcd` compiles cleanly
- Flash to Waveshare board → serial shows boot messages
- BLE advertising visible on phone (nRF Connect or similar)
- Pair with computer → keystrokes arrive, mouse moves
- NeoPixel: blue flash on keystroke, green flash on mouse move
- `pio run -e seeed_xiao_nrf52840` still compiles (no regressions)

---

## Phase 3: JSON Config Protocol

**Goal:** Implement JSON-based BLE UART config protocol. This is before the display because it enables testing via BLE terminal apps.

### Files to create/modify
| File | Purpose |
|------|---------|
| `src/c6/protocol.h` / `protocol.cpp` | **New** — JSON command parser + response builder |
| `src/c6/ble_uart.cpp` | Wire NUS RX → `processJsonCommand()` |
| `src/c6/serial_cmd.cpp` | Wire serial → `processJsonCommand()` |

### JSON Protocol Schema
```
Direction: App → Device
─────────────────────────────────────
{"t":"q","k":"status"}              Query status
{"t":"q","k":"settings"}            Query all settings
{"t":"q","k":"keys"}                Query available key list
{"t":"s","d":{"keyMin":3000,...}}    Set settings (partial update)
{"t":"c","k":"save"}                Save settings to flash
{"t":"c","k":"reboot"}              Reboot device
{"t":"c","k":"defaults"}            Factory reset

Direction: Device → App
─────────────────────────────────────
{"t":"r","k":"status","d":{...}}    Status response
{"t":"r","k":"settings","d":{...}}  Settings response
{"t":"ok"}                          Command success
{"t":"err","m":"reason"}            Error
{"t":"p","k":"status","d":{...}}    Push (unsolicited status update)
```

Short keys (`t`, `k`, `d`, `m`) minimize JSON size for BLE UART bandwidth.

### Setting key mapping
Map short JSON keys → `SettingId` enum values via a `const` lookup table in `protocol.cpp`. `setSettingValue()` in `settings_common.cpp` handles validation/clamping.

### ArduinoJson memory
- Input: `JsonDocument` on stack (~512 bytes) per command
- Output: `JsonDocument` on stack (~1024 bytes) — settings response is largest (~600 bytes serialized)
- Ephemeral per-command, no persistent heap allocation

### BLE MTU
Request 512-byte MTU on connection. Fall back to 20-byte chunked writes (same as CYD) for default MTU.

### Verification
- Send `{"t":"q","k":"status"}` via BLE terminal → receive valid JSON
- Send `{"t":"s","d":{"keyMin":3000}}` → `{"t":"c","k":"save"}` → reboot → query shows persisted value
- Serial: type JSON commands, get JSON responses
- Invalid JSON → `{"t":"err","m":"parse error"}`

---

## Phase 4: LVGL Display

**Goal:** Initialize LVGL v9 with built-in ST7789 driver. Render status UI on the 320×172 landscape TFT (same two-card layout as CYD, scaled to fit).

### Files to create/modify
| File | Purpose |
|------|---------|
| `lv_conf.h` | Project root — LVGL v9 configuration |
| `src/c6/display.h` | Public interface (replaces stubs from Phase 2) |
| `src/c6/display.cpp` | LVGL init, SPI driver, ST7789 setup, UI screens |

### LVGL v9 — built-in ST7789 driver (no TFT_eSPI!)
```c
// Landscape: pass native 172×320, then set rotation
lv_display_t* disp = lv_st7789_create(320, 172, LV_LCD_FLAG_NONE,
                                        spi_send_cmd, spi_send_color);
// ST7789 rotation handled via MADCTL register (0x36) for landscape
```
We implement `spi_send_cmd` and `spi_send_color` using ESP32's native SPI driver (`driver/spi_master.h`). This eliminates the TFT_eSPI dependency and its known ESP32-C6 + Arduino Core 3.x compatibility issues.

### SPI Configuration
- MOSI=GPIO6, SCLK=GPIO7, CS=GPIO14, DC=GPIO15, RST=GPIO21
- Clock: 40MHz (ST7789 max is 62.5MHz; 40MHz safe margin)
- Backlight: GPIO22, PWM via ESP32 LEDC for brightness control

### Memory Budget
| Component | Size |
|-----------|------|
| Display buffer A | 10KB (320×16×2 bytes, partial) |
| Display buffer B | 10KB (DMA double-buffering) |
| LVGL internal heap | 48KB |
| **Total LVGL** | **~68KB** |
| NimBLE | ~40KB |
| ArduinoJson (ephemeral) | ~1.5KB stack |
| App variables | ~10KB |
| **Free SRAM** | **~390KB** (comfortable) |

### lv_conf.h key settings
```c
#define LV_COLOR_DEPTH          16        // RGB565
#define LV_MEM_SIZE             (48*1024) // 48KB LVGL heap
#define LV_USE_ST7789           1
#define LV_USE_DRAW_SW          1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_USE_LABEL            1
#define LV_USE_BAR              1
#define LV_USE_ARC              1
#define LV_USE_IMG              1
#define LV_USE_FLEX             1
```

### UI Layout — Landscape 320w × 172h (CYD-style, scaled)

Same two-card layout as CYD (320×240) but scaled down 68px vertically. No footer buttons (no touch/controls — replaced by a thin status bar). LVGL gradient-filled bars replace CYD's flat progress bars.

**Vertical budget:**
- Header: 28px (gradient, matches CYD's purple→transparent)
- Content: 120px (two side-by-side cards, 145×110px each)
- Status bar: 24px (mode + KB/MS status + clock)
- **Total: 172px**

**Simple mode — Home screen:**
```
┌─────────────────────────────────────────┐
│▓▓ GhostOp C6           Normal  🔵 12:34│ 28px — Gradient header
├──────────────────┬──────────────────────┤
│   KEYBOARD       │   MOUSE              │
│                  │                      │
│     F16          │    JIGGLING          │ 120px — Two cards
│     2.5s         │     4.2s             │   Key/state + timing
│  ▓▓▓▓▓▓▓▓░░░░░  │  ▓▓▓▓▓▓░░░░░░░░░░░  │   Gradient progress bars
│                  │                      │
├──────────────────┴──────────────────────┤
│  Simple · KB:ON · MS:ON                 │ 24px — Status bar
└─────────────────────────────────────────┘
```

**Sim mode — Compact rows (matches CYD sim layout):**
```
┌─────────────────────────────────────────┐
│▓▓ GhostOp C6       Staff Dev  🔵 12:34 │ 28px — Gradient header
├─────────────────────────────────────────┤
│  Block    [Afternoon]  ▓▓▓░░░  12:34   │
│  Mode     [Focused]    ▓▓▓▓░░   8:21   │ ~66px — Sim compact rows
│  Profile  [Normal]     ▓▓▓▓▓░   3:15   │
├─────────────────────────────────────────┤
│  ⌨ F16  2.5s ▓▓▓▓░░  🖱 Jig ▓▓░░░░░░ │ ~30px — Activity strip
├─────────────────────────────────────────┤
│  Simulation · KB:ON · MS:ON  Perf: 5   │ 24px — Status bar
└─────────────────────────────────────────┘
```

**Color theme (CYD dark theme + LVGL gradient enhancements):**
- Background: black (#000000)
- Cards: dark gray rounded rects (COL_BG_CARD #18E3)
- Header: purple→transparent gradient (CYD's `initHeaderGradient()` pattern, reimplemented via LVGL gradient style)
- Progress bars: LVGL `lv_bar` with gradient indicator (green→dark green for normal, orange for busy profile)
- Accent: green=connected, red=disconnected, blue=BLE/advertising, orange=profile change
- Text: white (primary), gray (dimmed/secondary)

### LVGL tick
Call `lv_tick_inc(ms)` from `loop()` with `millis()` delta. Call `lv_timer_handler()` at 20Hz (gated by dirty flag + 50ms interval).

### Screensaver
Dim backlight via PWM after `saverTimeoutMs()`. LVGL renders a minimal status screen. Wake on BLE reconnect or schedule event.

### Verification
- Boot → splash screen renders on TFT
- BLE connect → display updates to "Connected" status
- Keystroke progress bar animates
- Mouse state transitions (Idle/Jiggling/Returning) display correctly
- Screensaver activates after timeout
- `ESP.getFreeHeap()` > 100KB after boot

---

## Phase 5: Integration & Polish

**Goal:** Wire all subsystems together. Full end-to-end operation.

### Key work items
1. **main.cpp full loop:** Serial + schedule + LVGL tick + LED tick + jiggler dispatch
2. **Schedule support:** Auto-sleep via schedule → backlight off + stop BLE. Wake via schedule timer → backlight on + resume advertising.
3. **Simulation mode:** Orchestrator → display shows sim info panel (job name, mode, phase, performance)
4. **LED patterns:** Breathing blue = advertising, solid green = connected, off = sleeping, blue flash = KB, green flash = mouse
5. **Display transitions:** LVGL animated screen transitions between splash → disconnected → connected → sleeping states
6. **Feature flag cleanup:** `HAS_BATTERY=0`, `HAS_SOUND=0`, etc. gate out unused menu items and protocol responses
7. **OP_MODE limits:** Only `OP_SIMPLE` and `OP_SIMULATION` available. JSON protocol rejects other modes.

### Sleep/wake behavior
Without physical buttons, sleep is **schedule-driven only** (or via JSON command). Since BLE stops during sleep, the only wake source is the schedule timer. This is the expected behavior for an unattended jiggler on a schedule.

### Verification
- **End-to-end:** Boot → BLE pair → keystrokes + mouse on host → display shows live status → works for 1+ hour
- **Sim mode:** Switch via JSON → orchestrator runs → display shows sim info
- **Schedule:** Set via JSON → auto-sleep at scheduled time → auto-wake at scheduled time
- **Memory:** `ESP.getFreeHeap()` stable over 1 hour (no leaks)
- **nRF52 regression:** `pio run -e seeed_xiao_nrf52840` still compiles

---

## Phase 6: Dashboard Adaptation

**Goal:** Adapt Vue dashboard for C6 (BLE-only + JSON protocol). This can be a follow-up effort.

- New connection mode: Web Bluetooth only (no Web Serial path)
- JSON protocol handler replaces pipe-delimited parser
- Settings UI: only show Simple + Simulation mode options
- Remove: DFU section, battery display, sound settings, encoder settings, game modes
- Add: C6-specific settings (if any)

### Verification
- Dashboard connects to C6 over Web Bluetooth
- Settings push → device applies + saves
- Status updates display in real-time

---

## Risk Register

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| NimBLE 2.x crashes on ESP32-C6 | Medium | Pin to known-good version; CYD's report descriptor is proven |
| LVGL v9 ST7789 SPI driver issues | Low | Fall back to TFT_eSPI + LVGL flush callback if built-in driver fails |
| Arduino Core 3.x breaking changes | Medium | Pin platform version in platformio.ini |
| RAM pressure (NimBLE + LVGL + ArduinoJson) | Low | Budget shows ~390KB free; partial buffers keep LVGL lean |
| BLE HID flaky on macOS | Low | User has confirmed BLE reliability from Pixel Agents app |

---

## Files Touched Per Phase

| Phase | New files | Modified files |
|-------|-----------|----------------|
| 1 | `src/common/` (13 files), `src/nrf52/` (20+ files) | `platformio.ini`, move all from `src/` |
| 2 | `src/c6/` (12 files) | `platformio.ini`, `src/common/config.h` |
| 3 | `src/c6/protocol.h/.cpp` | `src/c6/ble_uart.cpp`, `src/c6/serial_cmd.cpp` |
| 4 | `lv_conf.h`, `src/c6/display.cpp` (full impl) | `src/c6/main.cpp` (add LVGL tick) |
| 5 | — | `src/c6/main.cpp`, `src/c6/display.cpp`, `src/c6/sleep.cpp`, `src/c6/led.cpp` |
| 6 | `dashboard/src/lib/ble_json.js` (new) | `dashboard/src/App.vue`, `dashboard/src/lib/ble.js` |

## Reusable Code from CYD Branch
- `src/cyd/ble.cpp` → Fork for `src/c6/ble.cpp` (NimBLE HID setup, report descriptor)
- `src/cyd/ble_uart.cpp` → Fork for `src/c6/ble_uart.cpp` (NUS service setup)
- `src/cyd/hid.cpp` → Fork for `src/c6/hid.cpp` (BLE-only HID HAL)
- `src/cyd/settings_cyd.cpp` → Fork for `src/c6/settings_c6.cpp` (NVS persistence)
- `src/cyd/sim_data_nvs.cpp` → Reuse directly
- `src/common/platform_hal.h` → Update for v2.5.2 features

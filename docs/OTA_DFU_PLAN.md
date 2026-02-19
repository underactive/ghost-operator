# OTA DFU — Over-The-Air Firmware Updates for Ghost Operator

## Status: Partially Implemented (v1.7.1) — Superseded by Web Serial DFU (v1.7.2)

Firmware DFU commands work. Web Bluetooth DFU is **not possible** due to bootloader limitations. The Web Serial DFU approach in v1.7.2 solves this — see [SERIAL_DFU_PLAN.md](SERIAL_DFU_PLAN.md).

---

## What Shipped (v1.7.1)

### Firmware Commands
- `!dfu` BLE UART command — reboots device into DFU bootloader mode
- `f` serial command — same, for development/testing
- Uses SoftDevice-safe GPREGRET API (`sd_power_gpregret_set(0, 0xA8)`)
- OLED shows "OTA DFU / Waiting for update / Power cycle to exit"

### Supported DFU Methods
1. **nRF Connect mobile app** (iOS/Android) — select DFU ZIP, transfer over BLE
2. **`adafruit-nrfutil dfu ble`** — command-line BLE DFU from Linux/macOS
3. **USB** — always available via UF2 drag-and-drop or Arduino IDE upload

### DFU Package Generation
```bash
pip3 install adafruit-nrfutil

# Arduino IDE: Sketch > Export Compiled Binary
adafruit-nrfutil dfu genpkg \
  --dev-type 0x0052 \
  --application ghost_operator.ino.hex \
  --sd-req 0xFFFE \
  ghost_operator_dfu.zip
```

---

## Why Web Bluetooth DFU Failed

### The Problem
Chrome's Web Bluetooth API permanently blocklists the legacy Nordic DFU service UUID (`00001530-1212-efde-1523-785feabcd123`). This UUID cannot be used in `requestDevice()` filters or `optionalServices`.

### What We Discovered
The Adafruit nRF52 bootloader (all versions, including v0.10.0) uses **legacy DFU** (Nordic SDK 11) in bootloader mode. The "Secure DFU" service (`0xFE59`) that `BLEDfu` exposes in the Arduino application firmware is only a **buttonless entry point** — it triggers reboot into DFU mode but is not used for the actual transfer.

In bootloader mode, the device advertises as "AdaDFU" and exposes:
- `00001530-...` (Legacy DFU Service) — **blocklisted by Chrome**
- `0x180A` (Device Information Service)

No `0xFE59` service is present in bootloader mode.

### What We Tried
1. **Original plan**: Filter Chrome picker by `0xFE59` service → bootloader doesn't expose it
2. **Filter by name**: `namePrefix: 'AdaDFU'` with `optionalServices: [0xFE59]` → connects but `getPrimaryService(0xFE59)` fails
3. **Bootloader update to v0.10.0**: Still uses legacy DFU architecture (Nordic SDK 11)
4. **Service enumeration**: Confirmed only `0x180A` visible via GATT, `00001530-...` silently filtered by Chrome

### Key Lesson
The `web-bluetooth-dfu` library requires Secure DFU (`0xFE59`), which is a Nordic SDK 15+ feature. The Adafruit bootloader is built on SDK 11 and would need a complete rewrite to support Secure DFU. This is not something we can change.

---

## Bootloader Notes

- **Stock XIAO bootloader**: Seeed ships v0.6.2 (custom fork)
- **Adafruit official XIAO support**: Added in v0.8.0
- **DFU mode entry**: Write `0xA8` to GPREGRET, reset. Bootloader checks on boot.
- **DFU mode behavior**: Stays indefinitely — no timeout. Power cycle to exit.
- **DFU magic values**: `0xA8` = OTA, `0x4E` = Serial, `0x57` = UF2
- **SoftDevice safety**: Must use `sd_power_gpregret_clr()`/`sd_power_gpregret_set()` when SoftDevice is active. Direct `NRF_POWER->GPREGRET` writes cause hard faults.

---

## Files Modified

| File | Change |
|------|--------|
| `ble_uart.cpp` | Added `!dfu` command, `resetToDfu()` helper |
| `ble_uart.h` | Added `resetToDfu()` declaration |
| `serial_cmd.cpp` | Added `f` serial command |

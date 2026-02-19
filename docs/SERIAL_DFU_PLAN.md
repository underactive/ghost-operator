# Web Serial DFU — Browser-Based Firmware Updates for Ghost Operator

## Status: Implemented (v1.7.2)

Browser-based firmware updates via USB serial, integrated into the web dashboard. Solves the Web Bluetooth DFU blocklist problem documented in [OTA_DFU_PLAN.md](OTA_DFU_PLAN.md).

---

## Background

### The Problem (from v1.7.1)
Chrome permanently blocklists the legacy DFU UUID (`00001530-...`) used by the Adafruit nRF52 bootloader in OTA DFU mode. Web Bluetooth DFU is impossible — see `docs/OTA_DFU_PLAN.md` for the full investigation.

### The Solution
The Adafruit bootloader also supports **Serial DFU mode** (GPREGRET magic `0x4E`), which presents a USB CDC serial port. Chrome's Web Serial API has no UUID blocklist — it's just a serial port. The protocol is Nordic SDK 11's legacy HCI/SLIP serial DFU, the same protocol used by `adafruit-nrfutil dfu serial`.

---

## What Shipped (v1.7.2)

### Firmware Commands
- `!serialdfu` BLE UART command — reboots device into USB CDC Serial DFU bootloader mode
- `u` serial command — same, for development/testing
- Uses SoftDevice-safe GPREGRET API (`sd_power_gpregret_set(0, 0x4E)`)
- OLED shows "USB DFU / Connect USB cable / Power cycle to exit"

### Dashboard Integration
- **FirmwareUpdate.vue** component with collapsible UI, 7-state state machine
- Full orchestration: BLE command → device reboot → Web Serial port picker → DFU transfer → progress bar
- `dfuActive` flag keeps UI visible after BLE disconnects during DFU transfer

### DFU Library (`dashboard/src/lib/dfu/`)
Five modules implementing the Nordic SDK 11 legacy serial DFU protocol:

| Module | Purpose |
|--------|---------|
| `crc16.js` | CRC16-CCITT (init 0xFFFF) for HCI packet integrity |
| `slip.js` | SLIP byte-stuffing + HCI packet framing (4-byte header, seq numbers) |
| `serial.js` | Web Serial API transport (open/close/read SLIP frames/write) |
| `dfu.js` | DFU protocol state machine (Start → Init → Data → Validate → Activate) |
| `zip.js` | DFU ZIP parser using `fflate` for `adafruit-nrfutil` packages |

### Dependency
- `fflate` ^0.8.0 (~8KB tree-shaken) — zero-dependency ZIP decompression

---

## UX Flow

1. User is connected to device via BLE dashboard (existing)
2. User expands "Firmware Update" section, selects `.zip` DFU package
3. Dashboard validates the ZIP (manifest.json, .dat, .bin) and shows firmware size
4. User clicks "Start Firmware Update" → confirmation dialog warns about USB cable
5. User clicks "Confirm Update"
6. Dashboard sends `!serialdfu` over BLE UART
7. Device shows "USB DFU" on OLED, reboots into Serial DFU bootloader mode
8. BLE disconnects (normal). Dashboard waits ~3s for USB CDC enumeration
9. Dashboard prompts user to click "Select Serial Port" → Chrome picker dialog
10. User selects the DFU CDC port → transfer begins with progress bar
11. On success, bootloader validates firmware, reboots into new application
12. User reconnects via BLE to verify new version

**One serial picker dialog** — BLE triggers the DFU reboot, so the user only picks one port.

---

## Protocol Details

### HCI/SLIP Framing

Each DFU command is wrapped in a 3-layer packet:

```
SLIP frame:  [0xC0] ... escaped payload ... [0xC0]
  └── HCI packet: [4-byte header] [DFU payload] [2-byte CRC16]
        └── DFU payload: [4-byte opcode LE] [optional data]
```

**HCI header (4 bytes):**
```
byte0 = seq | (ack << 3) | (1 << 6) | (1 << 7)   // reliable, data integrity
byte1 = 14 | ((len & 0x0F) << 4)                   // packet_type=14
byte2 = (len >> 4) & 0xFF
byte3 = (~(b0 + b1 + b2) + 1) & 0xFF              // header checksum
```

**SLIP escaping:**
- `0xC0` → `0xDB 0xDC`
- `0xDB` → `0xDB 0xDD`

### DFU Opcodes

Opcodes are NOT sequential — they match the `adafruit-nrfutil` Python reference exactly:

| Opcode | Name | Payload |
|--------|------|---------|
| 1 | DFU_INIT_PACKET | init_data (.dat) + int16(0x0000) padding |
| 3 | DFU_START_PACKET | mode (4B) + sd_size (4B) + bl_size (4B) + app_size (4B) |
| 4 | DFU_DATA_PACKET | data chunk (512 bytes) |
| 5 | DFU_STOP_DATA_PACKET | (none) — triggers validate + activate + reboot |

### Transfer Sequence

```
1. Send DFU_START_PACKET (mode=4, sd=0, bl=0, app=binFile.length)  → ACK
2. Sleep for flash erase time (~90ms per 4KB page)
3. Send DFU_INIT_PACKET (.dat content + 2-byte zero padding)       → ACK
4. For each 512-byte chunk of .bin:
     Send DFU_DATA_PACKET (chunk)                                  → ACK
     (delay every 8 chunks for flash page write)
5. Send DFU_STOP_DATA_PACKET                                       → device reboots
```

Note: `validate_firmware` and `activate_firmware` are no-ops in the serial transport — the bootloader automatically validates and activates after receiving `DFU_STOP_DATA_PACKET`.

---

## DFU Modes Comparison

| | Serial DFU (USB) | OTA DFU (BLE) |
|---|---|---|
| GPREGRET magic | `0x4E` | `0xA8` |
| Transport | USB CDC serial | BLE (legacy DFU service) |
| BLE command | `!serialdfu` | `!dfu` |
| Serial command | `u` | `f` |
| Web dashboard | Yes (Web Serial API) | No (Chrome blocklists UUID) |
| nRF Connect mobile | No | Yes |
| `adafruit-nrfutil` | `dfu serial -p PORT -b 115200` | `dfu ble -ic NRF52` |
| Requires USB cable | Yes | No |
| OLED message | "USB DFU" | "OTA DFU" |

---

## Browser Compatibility

Web Serial API is required. Supported browsers:

| Browser | Support |
|---------|---------|
| Chrome (desktop) | Yes (89+) |
| Edge (desktop) | Yes (89+) |
| Opera (desktop) | Yes (75+) |
| Firefox | No |
| Safari | No |
| Mobile browsers | No |

The dashboard feature-detects `navigator.serial` and shows a warning if unavailable.

---

## Bootloader Serial Port

- **Same port name**: The bootloader reuses the same USB CDC path as the app (e.g. `/dev/tty.usbmodem2101`). No new device appears — the app's CDC disconnects on reset and the bootloader's CDC takes its place at the same path.
- **VID**: Uses Seeed's VID (not Adafruit's `0x239A`) — no VID filter in `navigator.serial.requestPort()`
- **Baud rate**: 115200
- **Protocol**: Nordic SDK 11 legacy HCI/SLIP
- **No timeout**: Bootloader stays in DFU mode indefinitely. Power cycle to exit.

---

## Files Modified

| File | Change |
|------|--------|
| `ble_uart.cpp` | Added `resetToSerialDfu()`, `!serialdfu` command handler |
| `ble_uart.h` | Added `resetToSerialDfu()` declaration |
| `serial_cmd.cpp` | Added `u` serial command + help text |
| `dashboard/package.json` | Added `fflate` dependency |
| `dashboard/src/lib/dfu/crc16.js` | **NEW** — CRC16-CCITT |
| `dashboard/src/lib/dfu/slip.js` | **NEW** — SLIP codec + HCI packet framing |
| `dashboard/src/lib/dfu/serial.js` | **NEW** — Web Serial transport |
| `dashboard/src/lib/dfu/dfu.js` | **NEW** — Nordic DFU protocol state machine |
| `dashboard/src/lib/dfu/zip.js` | **NEW** — DFU ZIP parser (fflate) |
| `dashboard/src/components/FirmwareUpdate.vue` | **NEW** — DFU UI component |
| `dashboard/src/lib/store.js` | Added `startSerialDfu()`, `dfuActive` ref |
| `dashboard/src/App.vue` | Integrated FirmwareUpdate, dfuActive rendering |

---

## Verification Checklist

- [ ] Flash firmware, send `u` on serial → device enters Serial DFU mode (USB CDC port appears)
- [ ] `adafruit-nrfutil dfu serial -pkg test.zip -p /dev/tty.usbmodem* -b 115200` works
- [ ] Select ZIP in dashboard → package info displays correctly
- [ ] After manual `u` command, `navigator.serial.requestPort()` in browser console shows the DFU CDC port
- [ ] End-to-end: BLE connect → select ZIP → start DFU → serial transfer → verify new version
- [ ] Error cases: wrong file type, cancel serial picker, USB disconnected during transfer
- [ ] Settings persistence: LittleFS settings survive application-only DFU
- [ ] Chrome/Edge: Web Serial detected. Firefox/Safari: warning shown.

---

## Known Risks & Mitigations

1. **Protocol accuracy**: HCI/SLIP framing must exactly match `adafruit-nrfutil`. Verify with `adafruit-nrfutil dfu serial` first (step 2 above).
2. **USB enumeration delay**: CDC port may take 2-5s to appear after reboot. The 3s wait may need tuning on some systems.
3. **~~VID/PID filtering~~** (resolved): Seeed XIAO bootloader reuses Seeed's VID, not Adafruit's `0x239A`. Removed VID filter — Chrome picker shows all available ports.
4. **Charge-only cables**: USB cables without data lines will not present a CDC port. Dashboard warns about this.

---

## Future Enhancement: USB-Only Flow (1200-Baud Touch)

For users without BLE (USB-only), the 1200-baud touch method can trigger DFU mode:
1. User selects app serial port → open at 1200 baud → close → device reboots into DFU
2. Wait 2s → user selects DFU serial port → transfer

This requires **two** serial picker dialogs. Lower priority — defer to a follow-up.

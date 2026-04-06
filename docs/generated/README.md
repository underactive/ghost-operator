# Data Schemas

Key data contracts in Ghost Operator firmware and dashboard protocol.

## Schemas

| Name | Source | Purpose |
|------|--------|---------|
| `Settings` struct | `src/common/config.h` | Persistent device configuration (LittleFS `/settings.dat`) |
| Text protocol | `src/nrf52/ble_uart.cpp` | `?query` / `=set` / `!action` commands over BLE UART and USB serial |
| JSON protocol | `src/nrf52/protocol.cpp` | JSON config commands (ArduinoJson) |
| DFU ZIP | `dashboard/src/lib/dfu/zip.js` | `adafruit-nrfutil` DFU package format (manifest.json + .dat + .bin) |
| HCI/SLIP | `dashboard/src/lib/dfu/slip.js` | Nordic SDK 11 legacy serial DFU framing |

## Settings struct

The `Settings` struct in `config.h` is the canonical schema for all device configuration. Key points:
- `SETTINGS_MAGIC` (currently `0x50524F61`) encodes the struct schema version
- `checksum` field (last byte) is auto-calculated via `calcChecksum()`
- Bump `SETTINGS_MAGIC` when the struct layout changes to trigger safe `loadDefaults()`
- See `setSettingValue()` in `settings_common.cpp` for bounds enforcement

## Text protocol

Pipe-delimited text protocol over BLE UART (NUS) and USB serial:
- `?status` / `?settings` / `?keys` — query commands
- `=keyMin:2000` / `=slots:2,28,...` — set commands (with `+ok` response)
- `!save` / `!defaults` / `!reboot` / `!dfu` / `!serialdfu` — action commands

See [references/protocol.md](../references/protocol.md) for the full command reference.

## Keeping schemas in sync

When you add, rename, or change the shape of a data contract:
1. Update the corresponding schema documentation in this file
2. Update `setSettingValue()` / `getSettingValue()` in `settings_common.cpp`
3. Update protocol handlers in `ble_uart.cpp` if adding a `=key:value` command
4. Bump `SETTINGS_MAGIC` in `config.h` if the `Settings` struct layout changes

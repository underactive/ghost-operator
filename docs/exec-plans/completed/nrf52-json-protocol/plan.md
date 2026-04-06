# Plan: Add JSON Protocol Support to nRF52

## Objective

Add JSON protocol support to the nRF52 firmware, matching the C6's JSON protocol format. The dashboard already supports both JSON and pipe-delimited text — this change enables JSON for the nRF52 platform while keeping the text protocol as a fallback.

**Why:** Both platforms sharing the same JSON wire format simplifies dashboard maintenance and enables native typed data (arrays, booleans, nested objects) instead of manual string parsing.

## Changes

1. `platformio.ini` — Add ArduinoJson ^7.4.1 to nRF52 lib_deps
2. `src/nrf52/protocol.h` — NEW: declares `processJsonCommand()` and `pushJsonStatus()`
3. `src/nrf52/protocol.cpp` — NEW: Full JSON protocol handler (~570 lines), based on C6's protocol.cpp with nRF52-specific fields (battery voltage, game states, DFU commands)
4. `src/nrf52/ble_uart.cpp` — Add JSON auto-detect (`line[0] == '{'`), enlarge UART buffer 128→512, add `platform=nrf52` to text status
5. `src/nrf52/serial_cmd.cpp` — JSON-aware `pushSerialStatus()`, enlarge serial buffer 128→512
6. `src/nrf52/state.h/.cpp` — Add `jsonPushMode` flag (connection-scoped)
7. `src/nrf52/ghost_operator.cpp` — Reset `jsonPushMode` on BLE disconnect
8. `dashboard/src/lib/store.js` — Use JSON for `nrf52` platform (not just `c6`)

## Dependencies

- ArduinoJson v7.4.1 (same version as C6)
- `__wrap_realloc` shim needed: PlatformIO's nRF52 builder adds `-Wl,--wrap=realloc` but the Adafruit framework doesn't provide the implementation

## Risks

- JSON responses are slightly larger than pipe-delimited, requiring more BLE writes per response. Same chunking mechanism, no regression.
- ArduinoJson heap allocations are transient — freed after each command. Fragmentation risk is minimal.

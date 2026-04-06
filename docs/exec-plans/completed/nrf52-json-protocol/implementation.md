# Implementation: Add JSON Protocol Support to nRF52

## Files changed

- `platformio.ini` — Added ArduinoJson ^7.4.1 to nRF52 lib_deps
- `src/nrf52/protocol.h` — NEW: JSON protocol header
- `src/nrf52/protocol.cpp` — NEW: JSON protocol handler (~570 lines)
- `src/nrf52/ble_uart.cpp` — JSON dispatch, buffer enlargement (128→512), platform field in text status
- `src/nrf52/serial_cmd.cpp` — JSON-aware pushSerialStatus(), buffer enlargement (128→512)
- `src/nrf52/state.h` — Added `jsonPushMode` extern declaration
- `src/nrf52/state.cpp` — Added `jsonPushMode` definition
- `src/nrf52/ghost_operator.cpp` — Reset `jsonPushMode` on BLE disconnect
- `dashboard/src/lib/store.js` — Use JSON builders for nRF52 platform

## Summary

Added full JSON protocol support to the nRF52 firmware. The implementation mirrors the C6's JSON protocol (same message structure: `{"t":"q","k":"status"}` etc.) with nRF52-specific additions:

- Battery millivolts (`batMv`) in status
- Game state fields (breakout, snake, racer) in status
- Volume control state in status
- Game settings in settings response
- DFU commands (`dfu`, `serialdfu`) in command handler
- `__wrap_realloc` linker shim for ArduinoJson compatibility with Adafruit nRF52 framework

Text protocol is fully preserved as fallback. Dashboard auto-detects JSON from `{` prefix.

## Verification

- [x] `pio run -e seeed_xiao_nrf52840` — Clean compile, 9.8% RAM, 25.5% flash
- [x] `pio run -e c6lcd` — C6 build unaffected
- [ ] Serial test: Send `{"t":"q","k":"status"}` over serial monitor → verify JSON response
- [ ] Dashboard test: Connect via Web Serial → verify settings load/save
- [ ] Text fallback: Send `?status` → verify text response still works
- [ ] Status push: Enable via `{"t":"s","d":{"statusPush":true}}` → verify JSON pushes
- [ ] DFU: Test `{"t":"c","k":"serialdfu"}` triggers DFU mode

## Audit Fixes

### Fixes applied
1. **[C1] Added `{` to serial protocol prefix check** — `serial_cmd.cpp:106`. JSON commands over USB serial were silently dropped because `{` wasn't recognized as a line-buffering trigger. Characters fell through to single-char debug handler.
2. **[C2] Reset `jsonPushMode` and `serialStatusPush` on USB unmount** — `ghost_operator.cpp:549-551`. Connection-scoped flags persisted across USB disconnect/reconnect.
3. **[H3/M1] Increased `JSON_RESP_BUF` from 1024 to 1536** — `protocol.cpp`. Settings response estimated at ~900 bytes; original buffer was too close to the limit.
4. **[M2] Made `sendJsonResponse` buffer `static`** — `protocol.cpp:580`. Saves 1.5KB of stack pressure. Safe because function is only called from single-threaded main loop.
5. **[H2] Fixed JSON status `conn` → `connected`** — `protocol.cpp:146`. Dashboard's reactive `status.connected` was never updated by JSON `Object.assign` because the field was named `conn`. Pre-existing C6 bug (C6 still uses `conn`).

### Verification checklist
- [x] Build passes after all fixes: 9.8% RAM, 25.5% flash
- [ ] Serial: send `{"t":"q","k":"status"}` over USB serial → verify JSON response (not dropped)
- [ ] USB disconnect/reconnect: verify `jsonPushMode` resets (no stale JSON pushes)

### Unresolved items
- **H1 (concurrent heap allocations)**: Accepted — matches C6 behavior, 200KB headroom
- **H2 (partial state on set error)**: Accepted — standard partial-update behavior
- **M3 (__wrap_realloc FreeRTOS safety)**: Accepted — safe in single-threaded context
- **M4 (JSON error message escaping)**: Accepted — all messages are string literals
- **Pre-existing C6 bugs**: Work mode JSON field name mismatches (abbreviated timing fields, nested weights) affect both C6 and nRF52 dashboards equally. Not introduced by this change.

## Follow-ups

- Work mode commands (`=wmode:...`) still use text format on all platforms — could be migrated to JSON in the future
- Consider moving shared protocol logic (SETTING_MAP, set handler) to `src/common/` to reduce duplication with C6
- Fix pre-existing C6 work mode JSON field name mismatches in dashboard (affects sim tuning section)

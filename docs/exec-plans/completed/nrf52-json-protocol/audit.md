# Audit: Add JSON Protocol Support to nRF52

## Files changed

- `src/nrf52/protocol.cpp`
- `src/nrf52/protocol.h`
- `src/nrf52/ble_uart.cpp`
- `src/nrf52/serial_cmd.cpp`
- `src/nrf52/state.h`
- `src/nrf52/state.cpp`
- `src/nrf52/ghost_operator.cpp`
- `dashboard/src/lib/store.js`
- `platformio.ini`

## Critical

### [FIXED] C1. JSON commands over USB serial silently dropped
**File:** `src/nrf52/serial_cmd.cpp:106`
Serial handler only recognized `?`, `=`, `!` as protocol prefixes for line buffering. JSON commands starting with `{` fell through to the single-char debug switch, causing undefined behavior (characters parsed as debug commands).
**Fix:** Added `c == '{'` to the protocol prefix check.

### [FIXED] C2. `jsonPushMode` not reset on USB disconnect
**File:** `src/nrf52/ghost_operator.cpp:549`
USB unmount edge detection didn't reset `jsonPushMode` or `serialStatusPush`. After USB disconnect+reconnect, stale JSON push mode persisted.
**Fix:** Added `jsonPushMode = false` and `serialStatusPush = false` on USB unmount edge.

## High

### H1. Two concurrent heap-allocated JsonDocument objects on query path
**File:** `src/nrf52/protocol.cpp:48+69`
Query path allocates input `doc` and response `resp` simultaneously, plus a 1536-byte serialization buffer. Peak heap usage could reach several KB.
**Status:** Accepted — matches C6 implementation. ArduinoJson documents are transient and freed after each command. At 9.8% RAM usage there is 200+ KB headroom.

### [FIXED] H2. JSON status `conn` field mismatches dashboard's `connected` property
**File:** `src/nrf52/protocol.cpp:146`
JSON sent `d["conn"]` but the dashboard's reactive `status` object uses `connected`. `Object.assign` created a new `status.conn` property without updating `status.connected`. Pre-existing C6 bug (C6 still uses `conn`).
**Fix:** Changed to `d["connected"]` in nRF52 protocol.

### H3. `jsonHandleSet` aborts on first unknown key, leaving partial state changes
**File:** `src/nrf52/protocol.cpp:517-519`
Keys processed before an unknown key are already applied. The caller receives an error but some settings changed.
**Status:** Accepted — standard partial-update API behavior. Matches C6 implementation. Dashboard sends one setting at a time in practice.

### H4. Pre-existing: JSON work mode field name mismatches with dashboard
**Files:** `src/nrf52/protocol.cpp:279-296`, `dashboard/src/components/SimTuningSection.vue`
JSON sends abbreviated timing fields (`bkMin`, `ikMin`, etc.) and nested weights (`weights.L`), but dashboard templates expect long names (`burstKeysMin`, `interKeyMinMs`) and flat weights (`wL`, `wN`, `wB`). Pre-existing from C6 — not introduced by this change.
**Status:** Pre-existing C6 bug. Out of scope.

## Medium

### [FIXED] M1. Response buffer too close to limit
**File:** `src/nrf52/protocol.cpp`
Original 1024-byte buffer was estimated at ~900 bytes for settings response.
**Fix:** Increased `JSON_RESP_BUF` to 1536.

### [FIXED] M2. `sendJsonResponse` stack pressure
**File:** `src/nrf52/protocol.cpp:580`
1024-byte (now 1536) stack buffer in `sendJsonResponse` added significant stack pressure.
**Fix:** Made buffer `static` — not reentrant (only called from main loop), saves 1.5KB stack.

### M3. `__wrap_realloc` bypasses FreeRTOS critical section
**File:** `src/nrf52/protocol.cpp:17-20`
The shim calls `__real_realloc` directly without `vTaskSuspendAll()` (unlike `__wrap_malloc`/`__wrap_free` in heap_3.c). Safe in practice because JSON processing is single-threaded in the main loop.
**Status:** Accepted with documentation. Would need updating if FreeRTOS tasks are added.

### M4. `sendJsonError` doesn't escape the message parameter
**File:** `src/nrf52/protocol.cpp:593-596`
If error messages contained `"` or `\`, output would be malformed JSON. Currently all messages are string literals.
**Status:** Accepted — all current callers use string literals. Defense-in-depth improvement deferred.

### M5. `dispFlip` in dashboard export would fail on nRF52 import
**File:** `dashboard/src/lib/store.js:786`
`EXPORTABLE_KEYS` includes `dispFlip`, which nRF52's JSON set handler doesn't recognize. Cross-platform settings import would abort at `dispFlip`.
**Status:** Pre-existing. Out of scope for this change.

## Low

- L1. `processJsonCommand` always returns `true` — correct, only reached when `line[0] == '{'`
- L2. Push status always sent to serial regardless of transport — pre-existing limitation
- L3. ~80% code duplication between nRF52 and C6 `protocol.cpp` — acknowledged, pragmatic for 2 platforms
- L4. `SETTING_MAP[]` arrays in both platforms could drift — relies on developer discipline
- L5. No data races between BLE callbacks and JSON processing — confirmed safe
- L6. No dead code in text protocol — all `?`/`=`/`!` handlers remain reachable

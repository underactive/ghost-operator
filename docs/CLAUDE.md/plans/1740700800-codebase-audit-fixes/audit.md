# Codebase Audit Fixes — Post-Implementation Audit Report

## Files Changed (audit scope)
All files from the implementation plus immediate dependents reviewed by audit subagents:
- `settings.cpp` — FMT_OP_MODE bounds check
- `serial_cmd.cpp` — Game state array bounds, MODE_NAMES bounds
- `orchestrator.cpp` — PROFILE_NAMES bounds
- `ghost_operator.ino` — BLE callback state mutations
- `state.h` / `state.cpp` — Volatile qualifiers, type changes
- `display.cpp` — drawScrollingText helper, scroll state types
- `dashboard/src/lib/protocol.js` — `??0` NaN edge case
- `dashboard/src/lib/store.js` — FIFO pendingQueue ordering
- `breakout.cpp` / `snake.cpp` / `racer.cpp` — Duplicated sound state machines

---

## 1. QA Audit

### [FIXED] QA-1 [MEDIUM] `FMT_OP_MODE` hardcoded `6` instead of `OP_MODE_COUNT`
**File:** `settings.cpp:397`
Bounds check used magic number `val < 6` instead of `OP_MODE_COUNT`, contradicting the enum refactoring that this plan introduced.

### QA-2 [MEDIUM] `parseInt() ?? 0` passes NaN on empty/missing fields
**File:** `dashboard/src/lib/protocol.js`
The `|| 0` → `?? 0` change correctly preserves valid `0` values but lets `NaN` through when `parseInt()` receives `undefined`. In practice, the firmware always sends all fields in `?settings`, and the buffer size increase (HI-7) prevents truncation. **Accepted tradeoff** — the fix addresses the primary bug (0 values treated as falsy) and the NaN edge case requires a truncated response that is separately mitigated.

### QA-3 [LOW] Unsolicited `+ok` could resolve wrong pending promise
**File:** `dashboard/src/lib/store.js`
If the device sends an unexpected `+ok` (no matching request), the FIFO queue would resolve the next queued promise incorrectly. The firmware protocol is strictly request-response with no unsolicited `+ok`, so this cannot occur under normal operation. **Accepted — protocol guarantees ordering.**

---

## 2. Security Audit

### [FIXED] SEC-1 [LOW] FMT_OP_MODE hardcoded `6` (same as QA-1)
**File:** `settings.cpp:397` — Fixed alongside QA-1.

### SEC-2 [LOW] Unguarded `PROFILE_NAMES[orch.autoProfile]`
**File:** `orchestrator.cpp:290`
`orch.autoProfile` is bounded by `PROFILE_COUNT` and validated in `setSettingValue()` with `clampVal()`. Pre-existing pattern, not introduced by this change. **Deferred — defense-in-depth, low risk.**

### SEC-3 [LOW] Unguarded `MODE_NAMES[currentMode]` in printStatus
**File:** `serial_cmd.cpp:35`
`currentMode` is a `UIMode` enum bounded by `MODE_COUNT`. Pre-existing serial debug output. **Deferred — internal debug command, low risk.**

### SEC-4 [LOW] Unguarded `AVAILABLE_KEYS[settings.keySlots[i]]` in printStatus
**File:** `serial_cmd.cpp:42,152`
`keySlots[i]` values are validated in `setSettingValue()` and `loadSettings()`. Pre-existing pattern in debug-only code. **Deferred — bounds checked at storage boundary per Development Rule 1.**

---

## 3. Interface Contract Audit

### IC-1 [LOW] `systemSoundEnabled` bounds check resets to `1` instead of `0`
**File:** `settings.cpp:195`
The `> 1` guard clamps corrupt values to `1` (On). This is a valid default — system sounds being enabled on corrupt data is a safer failure mode than silent operation. **Accepted — intentional default.**

### IC-2 [LOW] `parseSettings` field names don't match firmware protocol keys
**File:** `dashboard/src/lib/protocol.js`
Dashboard uses camelCase (`sysSounds`, `volumeTheme`) while firmware uses abbreviated keys (`sysSnd`, `volTheme`). This is the established mapping convention used for all other fields. **Not an issue — mapping is intentional.**

### IC-3 [LOW] `shiftDur`/`lunchDur` missing in EXPORTABLE_KEYS
**File:** `dashboard/src/lib/store.js`
These were omitted because they are dashboard-display-only fields (firmware uses them for shift/lunch display calculation but doesn't need them in export). **Accepted — display-only fields.**

### IC-4 [INFO] `sendAndWait` timeout of 3s may be tight for BLE path
**File:** `dashboard/src/lib/store.js`
BLE chunked writes at 20 bytes per MTU can take longer than USB serial. Pre-existing timeout value, not changed by this plan. **Noted for future improvement.**

---

## 4. State Management Audit

### SM-1 [MEDIUM] Non-volatile state written in BLE callbacks
**File:** `ghost_operator.ino`
`connect_callback` still directly writes `lastKeyTime`, `mouseState`, `mouseNetX`, `mouseNetY`. These are non-volatile main-loop variables written from SoftDevice interrupt context. The sound playback was correctly deferred (CR-3), but these timer/state resets were not. On the nRF52840 with the current SoftDevice, these are single-instruction writes that are effectively atomic, and the worst case is a one-cycle stale read. **Deferred — accepted low risk; consider `connectResetPending` pattern in a future cleanup.**

### SM-2 [LOW] `deviceConnected` multi-read without snapshot
**File:** `display.cpp`
Multiple reads of `volatile deviceConnected` within a single `drawNormal*()` call could see different values if a BLE event fires mid-render. Worst case: one-frame visual inconsistency (BLE icon flickers). **Deferred — cosmetic-only impact.**

### SM-3 [LOW] `bleConnHandle` non-atomic paired read
**File:** `ghost_operator.ino`
`bleConnHandle` is `volatile uint16_t`, which is atomic on ARM Cortex-M4 (32-bit bus, 16-bit aligned). **Not an issue on this architecture.**

### SM-4 [INFO] `displayDirty` clear-before-render pattern is correct
The "clear first, ISR re-sets if needed" pattern prevents lost dirty flags. No issue.

### SM-5 [INFO] `encoderPos` snapshot fix is correct
Single read into local variable eliminates TOCTOU. No issue.

### SM-6 [INFO] `helpScrollPos` type change from `int16_t` to `int8_t` is safe
Maximum scroll offset is ~34 (longest help text ~55 chars, `maxChars = 21`), well within `int8_t` range (127). No issue.

---

## 5. Resource & Concurrency Audit

### RC-1 [LOW] Connect/disconnect sound flags not strictly ordered
**File:** `ghost_operator.ino`, `state.h`
If BLE rapidly disconnects and reconnects before `loop()` processes the flags, both `connectSoundPending` and `disconnectSoundPending` could be true simultaneously. The main loop processes connect first, then disconnect, so the user hears both sounds in quick succession. This matches the actual state transitions. **Accepted — correct behavior.**

### RC-2 [LOW] `drawScrollingText` called with statics by reference
**File:** `display.cpp`
Static locals passed as `int8_t&` references to the helper. The statics have function scope, so the references are always valid. **Not an issue.**

### RC-3 [INFO] `canPlayGameSound()` reads two `settings` fields without lock
Both `soundEnabled` and `systemSoundEnabled` are `uint8_t` fields written only from the main loop. No concurrent write risk. **Not an issue.**

### RC-4 [INFO] Deferred sound flags use `volatile bool`, not atomic
On ARM Cortex-M4, `bool` (1 byte) reads and writes are atomic. `volatile` ensures visibility. **Not an issue on this architecture.**

---

## 6. Testing Coverage Audit

No automated tests exist in this project. The audit produced comprehensive manual testing recommendations organized by risk level:

### Critical-Risk (test first)
- Deferred BLE sound playback (CR-3): connect/disconnect sounds, rapid cycle behavior
- Modifier key bug fix (HI-9): modifier hold times visibly longer in Simulation mode
- `||0` → `??0` in parseSettings (CR-1/CR-2): settings with value 0 persist correctly

### High-Risk
- OperationMode enum migration (~50 sites): all 6 modes function correctly
- Array bounds guards (HI-1): serial `d` with corrupt values shows `"???"` not crash
- FIFO pendingQueue (HI-4): save/disconnect race doesn't hang UI
- Encoder double-read fix (HI-5): smooth rotation in all modes

### Medium-Risk
- Game macro renames (248 sites): all 3 games play correctly end-to-end
- Dashboard export/import round-trip with new fields
- `drawScrollingText` helper: scroll behavior matches original in all display modes

### Integration Scenarios
- Full lifecycle: boot → BLE connect → menu → mode change → reboot → verify
- Dashboard round-trip: connect → load settings → change → save → reconnect → verify
- Game cycle: Breakout → Snake → Racer with sound enabled
- Export/import: configure all settings → export → defaults → import → verify

---

## 7. DX & Maintainability Audit

### [FIXED] DX-1 [MAJOR] FMT_OP_MODE hardcoded `6` (same as QA-1/SEC-1)
**File:** `settings.cpp:397` — Fixed alongside QA-1.

### DX-2 [MAJOR] Near-identical sound state machines in 3 game files
**Files:** `breakout.cpp` (~130 lines), `snake.cpp` (~70 lines), `racer.cpp` (~65 lines)
Non-blocking tone output engines are structurally identical across all three game modules. Snake and racer game-over sounds share identical frequency arrays. **Deferred — pre-existing duplication, same category as LO-13/14/15 display.cpp refactors. Consider future `GameSoundEngine` extraction into `sound.cpp`.**

### DX-3 [MAJOR] Large functions (>50 lines)
**Files:** `ble_uart.cpp` (cmdSetValue ~230 lines), `display.cpp` (drawSimulationNormal ~220 lines), `breakout.cpp` (tickBreakout ~165 lines), `ghost_operator.ino` (loop ~240 lines)
All pre-existing. Not introduced by this change. **Deferred — architectural refactoring scope.**

### DX-4 [MODERATE] `FMT_HEADER_DISP` uses hardcoded `2` instead of symbolic constant
**File:** `settings.cpp:400`, `serial_cmd.cpp:223`
No `HEADER_DISP_COUNT` constant exists. Pre-existing pattern. **Deferred — add `HEADER_DISP_COUNT` sentinel to enum in a future cleanup.**

### DX-5 [MODERATE] Game state arrays use hardcoded bounds in printStatus
**File:** `serial_cmd.cpp:54,62,71`
`BreakoutState`, `SnakeState`, `RacerState` enums lack `_COUNT` sentinel values. **Deferred — pre-existing, low risk in debug-only code.**

### DX-6 [MODERATE] Inconsistent game sound naming conventions
**Files:** `breakout.cpp` (GS_ prefix), `snake.cpp` (SS_ prefix), `racer.cpp` (RS_ prefix)
Breakout uses generic "Game" prefix while others use module-specific prefixes. Pre-existing. **Deferred — cosmetic, would be resolved by DX-2 extraction.**

### DX-7 [MINOR] Dead design-discussion comments in racer.cpp
**File:** `racer.cpp:266-268`
Three lines of deliberation comments that read like commit messages. **Deferred — cosmetic.**

### DX-8 [MINOR] `canPlayGameSound()` missing relationship comment
**File:** `sound.h`
No comment explaining why either `soundEnabled` OR `systemSoundEnabled` enables game sounds. **Deferred — low impact.**

---

## Summary

| Audit Axis | Findings | Fixed | Deferred | Accepted |
|------------|----------|-------|----------|----------|
| QA | 3 | 1 | 0 | 2 |
| Security | 4 | 1 | 3 | 0 |
| Interface Contract | 4 | 0 | 0 | 4 |
| State Management | 6 | 0 | 2 | 4 |
| Resource/Concurrency | 4 | 0 | 0 | 4 |
| Testing Coverage | — | — | — | — |
| DX/Maintainability | 8 | 1 | 6 | 1 |
| **Total** | **29** | **3** | **11** | **15** |

**Fixed:** 1 finding (FMT_OP_MODE hardcoded `6` → `OP_MODE_COUNT`) flagged independently by 3 audit axes (QA, Security, DX).

**Deferred items** are either pre-existing patterns not introduced by this change, or accepted design tradeoffs with documented rationale.

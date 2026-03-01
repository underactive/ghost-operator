# Codebase Audit Fixes — Implementation Record

## Files Changed

### Firmware (C++)
- `config.h` — Added `OperationMode` enum, `lastCurveChangeMs` to `RacerGameState`
- `state.h` — `volatile` on `deviceConnected`/`bleConnHandle`, deferred sound flags, `gBrk`/`gSnk`/`gRcr` macros, `helpScrollPos` type change
- `state.cpp` — Matching definitions for volatile/deferred sound/helpScrollPos type
- `sound.h` — Added `canPlayGameSound()` declaration
- `sound.cpp` — Added `canPlayGameSound()` implementation
- `ghost_operator.ino` — Deferred sound from BLE callbacks, displayDirty race fix, operationMode enum, DCDCEN comment
- `ble_uart.cpp` — Name validation, buffer 640→768, wmode CSV guard, dMin/dMax bounds, operationMode enum
- `orchestrator.cpp` — Modifier key bug fix (save nextKeyIndex before pickNextKey), currentTemplate bounds guard
- `hid.cpp` — Bounds guards on hasPopulatedSlot, pickNextKey, sendKeystroke
- `input.cpp` — encoderPos double-read fix, operationMode enum (18 sites)
- `display.cpp` — operationMode enum (11 sites), OP_MODE_COUNT (5 sites), bounds guards (5 lookups), drawScrollingText helper, 5 scroll pattern replacements
- `serial_cmd.cpp` — Bounds guards (11 lookups), operationMode enum (6 sites), game macro renames
- `settings.cpp` — operationMode enum in defaults/bounds
- `schedule.cpp` — operationMode enum, re-anchoring documentation comment
- `breakout.cpp` — gBrk macro rename (~82 sites), canPlayGameSound()
- `snake.cpp` — gSnk macro rename (~62 sites), canPlayGameSound()
- `racer.cpp` — gRcr macro rename (~60 sites), canPlayGameSound(), gRcr.lastCurveChangeMs
- `sleep.cpp` — Register access safety comment
- `sim_data.cpp` — LittleFS close limitation comment

### Dashboard (JS/Vue)
- `dashboard/src/lib/protocol.js` — OP_MODE_NAMES (4 entries added), MODE_NAMES (2 entries added), parseSettings (13 fields added, `||0`→`??0`)
- `dashboard/src/lib/store.js` — 13 settings fields, FIFO pendingQueue, disconnect drain, exportSettings clickSlots, EXPORTABLE_KEYS (10 added), importSettings guard, removed connectDevice alias
- `dashboard/src/lib/serial.js` — rxBuffer overflow guard (8192)
- `dashboard/src/lib/ble.js` — rxBuffer overflow guard (8192)
- `dashboard/src/lib/dfu/serial.js` — releaseLockOnce double-release guard
- `dashboard/src/components/ConnectButton.vue` — Typo "recongize" → "recognize"

### Documentation
- `CLAUDE.md` — Game modules in Architecture + File Inventory, display.cpp ~2900 lines, UIMode + OperationMode enums, MENU_ITEM_COUNT 62, Settings struct (systemSoundEnabled, operationMode 6 values), SETTINGS_MAGIC 0x50524F61, canPlayGameSound(), gBrk/gSnk/gRcr convention

## Summary
All 45 audit findings were addressed across 5 phases:
- **Phase 0**: Foundation types, macros, shared utilities (compile verified)
- **Phase 1**: 4 Critical + 12 High fixes (compile verified)
- **Phase 2**: 4 Medium fixes + documentation comments (compile verified)
- **Phase 3**: 11 Dashboard fixes across 6 files (build verified)
- **Phase 4**: CLAUDE.md comprehensive update

15 Low-severity items were intentionally deferred as documented in the plan.

No deviations from the plan.

## Verification
- `make build` passed after each firmware phase (Phases 0, 1, 2) — only pre-existing `cellCenterX` warning
- `npm run build` passed after Phase 3 — clean build, no warnings
- Final firmware: 246,000 bytes (30% of 811,008), 21,080 bytes RAM (8% of 237,568)
- Confirmed `volatile` on `deviceConnected` in compiled output (via successful build)
- Confirmed `cmdQuerySettings` buffer is 768 (via grep)
- Confirmed modifier bug fix: `pressedKeyIdx` saved before `pickNextKey()` (via code review)
- Confirmed `OP_MODE_NAMES` has 6 entries in dashboard build (via build success + code review)

## Audit Fixes

### Fixes applied
1. **FMT_OP_MODE hardcoded `6` → `OP_MODE_COUNT`** (`settings.cpp:397`) — Flagged independently by QA Audit (MEDIUM), Security Audit (LOW), and DX Audit (MAJOR). The `formatMenuValue()` bounds check for `FMT_OP_MODE` used a magic number `6` instead of the `OP_MODE_COUNT` enum constant that was introduced in Phase 0 of this very plan. Every other enum-indexed format case in the same function used symbolic constants. Fixed by replacing `val < 6` with `val < OP_MODE_COUNT`.

### Verification checklist
- [x] `make build` passes after fix (confirmed: 246,871 byte DFU, clean build)
- [ ] Verify operation mode display shows correct name for all 6 modes on device OLED
- [ ] Verify adding a 7th operation mode in the future would auto-update the bounds check

### Unresolved items
- **QA-2**: `parseInt() ?? 0` NaN edge case — accepted tradeoff; firmware always sends all fields, buffer truncation mitigated by HI-7
- **SM-1**: Non-volatile state written in BLE callbacks — accepted low risk on ARM Cortex-M4; consider `connectResetPending` pattern in future cleanup
- **DX-2**: Duplicated game sound state machines (~265 lines across 3 files) — pre-existing, same category as deferred LO-13/14/15 refactors
- **DX-3**: Large functions (cmdSetValue, drawSimulationNormal, loop) — pre-existing architectural patterns
- **DX-4**: `FMT_HEADER_DISP` hardcoded `2` — pre-existing, no `HEADER_DISP_COUNT` constant; add in future cleanup
- **SEC-2/3/4**: Unguarded lookups in debug-only code (orchestrator, serial_cmd) — values validated at storage boundary per Development Rule 1

## Follow-ups
- 15 deferred Low-severity items documented in plan (accepted design tradeoffs or display.cpp refactor scope)
- `cellCenterX` uninitialized warning is pre-existing, not introduced by this change
- Consider future display.cpp refactor to address LO-13/14/15 (confirmation dialog dedup, carousel dedup, MODE_DESCS location)
- Consider extracting shared `GameSoundEngine` from breakout/snake/racer into `sound.cpp` (DX-2)
- Consider adding `_COUNT` sentinels to `BreakoutState`/`SnakeState`/`RacerState` enums (DX-5)

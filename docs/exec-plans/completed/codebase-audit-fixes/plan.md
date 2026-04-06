# Full Codebase Audit Fix Plan

## Objective
Address 45 findings (4 Critical, 12 High, 14 Medium, 15 Low) from a 7-axis codebase audit. Changes are phased so foundation types land first, then Critical/High, then Medium, then Low, then docs.

## Changes

### Phase 0: Foundation
- `config.h`: Add `OperationMode` enum, `lastCurveChangeMs` to `RacerGameState`
- `state.h/.cpp`: `volatile` on `deviceConnected`/`bleConnHandle`, deferred sound flags, `gBrk`/`gSnk`/`gRcr` macro renames
- `sound.h/.cpp`: Shared `canPlayGameSound()`
- 8 files: Mechanical `brk.`→`gBrk.` etc. rename (~248 sites)

### Phase 1: Critical + High
- `ghost_operator.ino`: Deferred sound from BLE callbacks (CR-3), displayDirty race (HI-6), operationMode enum (HI-10)
- `ble_uart.cpp`: Name validation (HI-2), buffer size (HI-7), wmode guards (HI-8), operationMode enum
- `orchestrator.cpp`: Modifier key bug fix (HI-9), currentTemplate bounds (HI-1)
- `hid.cpp`: Array bounds guards (HI-1)
- `input.cpp`: encoderPos double-read (HI-5), operationMode enum
- `display.cpp`: operationMode enum, hardcoded 6→OP_MODE_COUNT, array bounds guards (HI-1)
- `serial_cmd.cpp`: Array bounds guards (HI-1), operationMode enum
- `settings.cpp`, `schedule.cpp`, `mouse.cpp`: operationMode enum
- Game modules: Replace local `canPlaySound()` with `canPlayGameSound()` (HI-11), `gRcr.lastCurveChangeMs` (ME-7)

### Phase 2: Medium
- `display.cpp`: Extract `drawScrollingText()` helper (ME-13)
- `schedule.cpp`: Documentation comment on re-anchoring side effect (ME-6)
- `sim_data.cpp`: Document LittleFS close limitation (ME-14)
- `sleep.cpp`: Register access safety comment (LO-10)

### Phase 3: Dashboard
- `protocol.js`: Fix OP_MODE_NAMES, MODE_NAMES, parseSettings missing 13 fields, `||0`→`??0` (CR-1, CR-2, HI-3, ME-2)
- `store.js`: FIFO queue for sendAndWait (HI-4), missing settings fields, exportSettings clickSlots (ME-8), EXPORTABLE_KEYS (ME-10), importSettings guard (LO-5), remove deprecated alias (LO-2)
- `serial.js` + `ble.js`: Buffer overflow guard (ME-1)
- `dfu/serial.js`: Double-release guard (LO-11)
- `ConnectButton.vue`: Typo fix (LO-6)

### Phase 4: Documentation
- `CLAUDE.md`: Add game modules, update display.cpp line count, add OperationMode enum, update Settings struct, update MENU_ITEM_COUNT, document gBrk/gSnk/gRcr convention

## Dependencies
- Phase 0 must complete before Phase 1 (enum/macro changes used by later fixes)
- Phases 1-2 are firmware; Phase 3 is dashboard; Phase 4 is docs — no cross-dependencies after Phase 0
- Each phase must compile before proceeding

## Risks / Open Questions
- ~248-site mechanical rename has high surface area but low logic risk (search-and-replace)
- `|| 0` → `?? 0` in parseSettings changes behavior for value `0` (intentional fix)
- Removing `connectDevice` alias requires verification it's unused (confirmed: only `connectUSB`/`connectBLE` imported)

## Deferred Items
15 Low-severity items deferred as accepted design tradeoffs (LO-1 BLE auth, LO-3 battery key, LO-4 CHAR_WIDTH, LO-7 CRC speed, LO-8 padding, LO-9 GFX bounds, LO-12 validateMenuIndices, LO-13/14/15 display.cpp refactors).

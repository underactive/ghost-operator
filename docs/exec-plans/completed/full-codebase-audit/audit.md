# Full Codebase Audit Report

**Date:** 2026-02-28
**Scope:** All firmware source files (46 files: `*.h`, `*.cpp`, `*.ino`) and all dashboard source files (25 files: `*.js`, `*.vue`)
**Audits performed:** QA, Security, Interface Contract, State Management, Resource & Concurrency, Testing Coverage, DX & Maintainability

---

## Files Changed (Findings Flagged)

### Firmware
- `ghost_operator.ino` — BLE callback shared state mutations, `deviceConnected` not volatile, direct settings mutations
- `config.h` — Missing `OperationMode` enum, SETTINGS_MAGIC documented incorrectly
- `state.h` / `state.cpp` — `deviceConnected`/`bleConnHandle` missing `volatile`, macro namespace pollution (`brk`/`snk`/`rcr`)
- `settings.cpp` — Flash write blocks interrupts (~85ms), no `pollEncoder()` after save
- `ble_uart.cpp` — Device name bypasses character validation, `cmdQuerySettings()` buffer too small, wmode CSV parser no value count validation, modifier flag checked on wrong key
- `serial_cmd.cpp` — ~11 unguarded array lookups
- `display.cpp` — ~3 unguarded array lookups, 5x duplicated scroll text pattern, duplicated carousel/confirmation rendering, 2916 lines (documented as ~800)
- `input.cpp` — Direct settings mutations bypass `setSettingValue()`, missing `markDisplayDirty()` in SLOTS/CLICK_SLOTS
- `hid.cpp` — Unguarded `AVAILABLE_KEYS[nextKeyIndex]` access, blocking delays
- `orchestrator.cpp` — Unguarded array lookups, modifier flag checked on wrong key after `pickNextKey()`, unguarded `currentTemplate()`
- `mouse.cpp` — Mouse state reset race with BLE callback
- `sound.cpp` — Connect/disconnect sound blocks ~200ms in BLE callback context
- `racer.cpp` — `lastCurveChangeMs` is file-static instead of in `RacerGameState`
- `breakout.cpp` / `snake.cpp` / `racer.cpp` — Duplicated `canPlaySound()` function
- `schedule.cpp` — `currentDaySeconds()` has mutation side effect in getter, missing `markDisplayDirty()` after schedule reset
- `sleep.cpp` — Direct `NRF_UARTE0`/`NRF_TWIM0` register writes while SoftDevice active
- `sim_data.cpp` — Static `File` object never re-initialized after close failure
- `keys.cpp` — `validateMenuIndices()` halts with `while(1)` (WDT recovers, but no OLED error)

### Dashboard
- `dashboard/src/lib/protocol.js` — `OP_MODE_NAMES` missing 4 modes, `MODE_NAMES` missing 2 modes, `parseSettings()` missing 16 fields, `|| 0` pattern masks zero values
- `dashboard/src/lib/store.js` — `sendAndWait` single-slot race, optimistic `setSetting` without rollback, `settings` reactive object missing 16 fields, `importSettings` format inconsistency
- `dashboard/src/lib/serial.js` — `rxBuffer` unbounded growth
- `dashboard/src/lib/ble.js` — `rxBuffer` unbounded growth
- `dashboard/src/lib/dfu/serial.js` — `readFrame()` double reader release
- `dashboard/src/components/ConnectButton.vue` — "recongize" typo

### Documentation
- `CLAUDE.md` — Game modules undocumented, display.cpp size wrong, UIMode enum incomplete, MENU_ITEM_COUNT wrong, Settings struct incomplete, SETTINGS_MAGIC wrong, operation mode count wrong, file inventory incomplete

---

## Critical Findings

### CR-1. Dashboard `OP_MODE_NAMES` missing 4 of 6 operation modes
**Source:** QA §C1, Interface Contract §C-1, State Management §M7, DX §H1
**Files:** `dashboard/src/lib/protocol.js:174`
**Description:** Dashboard defines `OP_MODE_NAMES = ['Simple', 'Simulation']` but firmware has 6 modes (Simple, Simulation, Volume, Breakout, Snake, Racer). The StatusBar mode dropdown only shows 2 options; modes 2-5 display as `undefined`.
**Fix:** Update to `['Simple', 'Simulation', 'Volume', 'Breakout', 'Snake', 'Racer']`.

### CR-2. Dashboard `MODE_NAMES` missing 2 UI modes
**Source:** QA §C2, Interface Contract §C-2, DX §M4
**Files:** `dashboard/src/lib/protocol.js:156`
**Description:** Dashboard has 8 entries; firmware has 10 (missing `CRSL` at index 8, `CSLOT` at index 9). Status display shows `undefined` for MODE_CAROUSEL and MODE_CLICK_SLOTS.
**Fix:** Append `'CRSL', 'CSLOT'` to the array.

### CR-3. BLE callbacks modify shared state from SoftDevice interrupt context
**Source:** Resource & Concurrency §C1, State Management §H1, §M1
**Files:** `ghost_operator.ino:66-104`, `state.h:47-49`
**Description:** `connect_callback()` and `disconnect_callback()` write to ~10 non-volatile shared variables (`deviceConnected`, `bleConnHandle`, `mouseState`, `mouseNetX/Y`, timers) from SoftDevice interrupt context. `deviceConnected` and `bleConnHandle` are not declared `volatile`, so the compiler may cache stale values in registers across main loop iterations.
**Fix:** (a) Declare `deviceConnected` and `bleConnHandle` as `volatile`. (b) Defer compound state resets (mouse, timers) to main loop via a `volatile bool bleJustConnected` flag.

### CR-4. CLAUDE.md documentation severely out of date
**Source:** DX §C1, §C2
**Files:** `CLAUDE.md`
**Description:** 3 game modules (breakout, snake, racer) entirely undocumented. display.cpp documented as "~800 lines" but is actually 2916 lines. UIMode enum, MENU_ITEM_COUNT, Settings struct, SETTINGS_MAGIC, operation mode count, file inventory, and config protocol docs all stale. See DX audit for complete list of 10+ discrepancies.
**Fix:** Comprehensive CLAUDE.md update.

---

## High Findings

### HI-1. Unguarded array lookups across 4 modules (~20 instances)
**Source:** Security §H-2/H-3/H-4, §M-4/M-5/M-6
**Files:** `serial_cmd.cpp` (~11 instances), `display.cpp` (~3), `orchestrator.cpp` (~2), `hid.cpp` (~1)
**Description:** Settings values used as direct array indices into name arrays (`AVAILABLE_KEYS[]`, `PROFILE_NAMES[]`, `MOUSE_STYLE_NAMES[]`, `SAVER_NAMES[]`, `ANIM_NAMES[]`, `JOB_SIM_NAMES[]`, `SCHEDULE_MODE_NAMES[]`, etc.) without bounds guards. Per Development Rule #2, every such lookup must have `(val < COUNT) ? NAMES[val] : "???"`. The pattern is correctly applied in `formatMenuValue()` and `sendKeyDown()` but missing elsewhere.
**Fix:** Systematic pass to add bounds guards to all ~20 unguarded lookup sites.

### HI-2. Device name via protocol bypasses character validation
**Source:** Security §H-1
**Files:** `ble_uart.cpp:359-362`
**Description:** The `=name:` command copies the value directly via `strncpy` without validating printable ASCII (0x20-0x7E). `loadSettings()` does validate on boot, but a BLE client can inject non-printable characters that persist to flash and render on OLED.
**Fix:** Add the same printable ASCII validation loop from `loadSettings()` to the `=name:` handler.

### HI-3. Dashboard `parseSettings()` missing 16 firmware fields
**Source:** Interface Contract §H-1, State Management §H3, DX §L3
**Files:** `dashboard/src/lib/protocol.js:79-119`, `dashboard/src/lib/store.js:43-82`
**Description:** Firmware sends 16 fields not parsed by dashboard: `sysSounds`, `volumeTheme`, `encButton`, `sideButton`, `ballSpeed`, `paddleSize`, `startLives`, `highScore`, `snakeSpeed`, `snakeWalls`, `snakeHiScore`, `racerSpeed`, `racerHiScore`, plus more. Settings backup/restore loses these values.
**Fix:** Add missing fields to `settings` reactive object, `parseSettings()`, and `EXPORTABLE_KEYS`.

### HI-4. `sendAndWait()` single-slot race condition
**Source:** Interface Contract §H-2, State Management §M3
**Files:** `dashboard/src/lib/store.js:505-517`
**Description:** Uses a single `pendingResolve` variable. Concurrent calls clobber each other; the first caller's promise is orphaned until 3-second timeout. Any `+ok` can resolve the wrong pending promise.
**Fix:** Queue-based or command-tagged response approach.

### HI-5. `encoderPos` double-read race in `handleEncoder()`
**Source:** Resource & Concurrency §H1
**Files:** `input.cpp:437`
**Description:** `encoderPos` is read twice — once for the delta calculation and once for `lastEncoderPos` update. The ISR can fire between reads, causing a missed or double-counted transition.
**Fix:** Read `encoderPos` once into a local variable at the top.

### HI-6. Blocking `delay()` in BLE callback context (sound)
**Source:** QA §H5, Resource & Concurrency §H3
**Files:** `sound.cpp:83-121`, `ghost_operator.ino`
**Description:** `playConnectSound()`/`playDisconnectSound()` block ~200ms each and are called from BLE connect/disconnect callbacks (SoftDevice interrupt context). During this time, no encoder polling, command processing, or display updates occur.
**Fix:** Defer sound playback to main loop via a `volatile bool connectSoundPending` flag.

### HI-7. `cmdQuerySettings()` buffer may overflow
**Source:** Security §H-6
**Files:** `ble_uart.cpp:222`
**Description:** 640-byte buffer; worst-case response with all game settings is ~680 bytes. `snprintf` truncates safely but the response would be incomplete, causing dashboard to miss trailing fields (e.g., `clickSlots`).
**Fix:** Increase buffer to 768 bytes.

### HI-8. Wmode CSV parser doesn't validate value count
**Source:** Security §H-5
**Files:** `ble_uart.cpp:500-518`
**Description:** The `=wmode:N:tP:` parser always iterates 12 times. Fewer than 12 CSV values causes remaining fields to be set to 0 (via `atol("")`), including `keyHoldMinMs=0` which could cause rapid-fire HID reports.
**Fix:** Count CSV values before parsing; reject if < 12.

### HI-9. Modifier flag checked on wrong key after `pickNextKey()`
**Source:** Security §M-7
**Files:** `orchestrator.cpp:394-406, 528-540`
**Description:** In `tickBurst()` and `tickKbMouse()`, after pressing a key and calling `pickNextKey()` (which changes `nextKeyIndex`), the modifier check on the NEXT key determines the hold duration for the CURRENT key. This is a logic bug affecting simulation fidelity.
**Fix:** Save `nextKeyIndex` before calling `pickNextKey()` and use the saved value for the modifier check.

### HI-10. Magic numbers for `operationMode` — no enum
**Source:** DX §H2
**Files:** `ghost_operator.ino`, `display.cpp`, `input.cpp`, `serial_cmd.cpp`, `schedule.cpp`, `ble_uart.cpp`, `hid.cpp` (~40 sites)
**Description:** Raw integers `0`-`5` used for mode comparisons throughout the codebase with no `OperationMode` enum.
**Fix:** Add `enum OperationMode { OP_SIMPLE, OP_SIMULATION, OP_VOLUME, OP_BREAKOUT, OP_SNAKE, OP_RACER, OP_MODE_COUNT }` to `config.h`.

### HI-11. Duplicated `canPlaySound()` across 3 game modules
**Source:** DX §H3
**Files:** `breakout.cpp:35`, `snake.cpp:17`, `racer.cpp:35`
**Description:** Identical function `static bool canPlaySound() { return settings.systemSoundEnabled && displayInitialized; }` duplicated 3 times, plus near-identical game sound state machines (~40-60 lines each).
**Fix:** Extract to `sound.h/.cpp`.

### HI-12. Missing `markDisplayDirty()` in SLOTS/CLICK_SLOTS encoder rotation
**Source:** State Management §M6
**Files:** `input.cpp:527-536`
**Description:** Encoder rotation in MODE_SLOTS and MODE_CLICK_SLOTS directly modifies `settings.keySlots[]`/`settings.clickSlots[]` without calling `markDisplayDirty()`. These are "static" modes that only render when dirty, so the display may not update immediately.
**Fix:** Add `markDisplayDirty()` after the mutations.

---

## Medium Findings

### ME-1. `rxBuffer` unbounded growth in dashboard transports
**Source:** Interface Contract §H-5, Security §M-2
**Files:** `dashboard/src/lib/serial.js:123`, `dashboard/src/lib/ble.js:19`
**Description:** Both transports accumulate received bytes without a maximum buffer length. A firmware bug or BLE interference could cause indefinite growth.
**Fix:** Add 8KB maximum with discard-on-overflow.

### ME-2. `parseSettings()` uses `|| 0` masking zero values
**Source:** QA §H1, Interface Contract §M-1
**Files:** `dashboard/src/lib/protocol.js:81-119`
**Description:** `parseInt(data.field) || 0` treats legitimate `0` and missing field identically. `parseInt(data.mouseAmp) || 1` converts a genuine 0 to 1.
**Fix:** Use `parseInt(data.field ?? 'default')` consistently.

### ME-3. Direct settings mutations bypass `setSettingValue()` in multiple locations
**Source:** State Management §H2
**Files:** `input.cpp`, `ble_uart.cpp`, `schedule.cpp`, `ghost_operator.ino`, `breakout.cpp`, `snake.cpp`, `racer.cpp`
**Description:** ~15 sites mutate `settings` struct fields directly, bypassing `clampVal()` validation. Most have inline guards, but the scattered pattern is error-prone.
**Fix:** Document as intentional exceptions where inline validation exists; route remaining through `setSettingValue()`.

### ME-4. `displayDirty = false` races with `markDisplayDirty()` from BLE callbacks
**Source:** Resource & Concurrency §H2
**Files:** `ghost_operator.ino:601`
**Description:** If a BLE callback sets `displayDirty` between `updateDisplay()` and `displayDirty = false`, the flag is cleared without rendering the change.
**Fix:** Read-clear-render pattern, or accept one-frame (50ms) delay.

### ME-5. Flash save blocks interrupts ~85ms
**Source:** Resource & Concurrency §H4
**Files:** `settings.cpp:117-132`
**Description:** LittleFS flash erase takes ~85ms with interrupts disabled. Encoder transitions during this window are permanently lost.
**Fix:** Call `pollEncoder()` immediately after `saveSettings()`.

### ME-6. `currentDaySeconds()` has mutation side effect in getter
**Source:** State Management §M2
**Files:** `schedule.cpp:32-43`
**Description:** Re-anchoring logic modifies `wallClockDaySecs`/`wallClockSyncMs` as a side effect. Multiple calls per loop iteration can see different elapsed times.
**Fix:** Move re-anchoring to a dedicated `maintainWallClock()` called once per loop.

### ME-7. Racer `lastCurveChangeMs` is file-static instead of in game state
**Source:** QA §H4, DX §M8
**Files:** `racer.cpp`
**Description:** Not reset on game restart or mode switch. Stale value causes incorrect first curve timing.
**Fix:** Move into `RacerGameState`, reset in `initRacer()`.

### ME-8. `exportSettings()` inconsistent array format
**Source:** Interface Contract §M-3
**Files:** `dashboard/src/lib/store.js:566-573`
**Description:** `slots` exported as comma-separated string, `clickSlots` exported as JS array. Round-trip works by accident via `String([1,7,7,7,7,7,7])`.
**Fix:** Explicitly `join(',')` for `clickSlots` too.

### ME-9. Wmode timing fields lack max bounds (uint16_t truncation)
**Source:** Interface Contract §M-6
**Files:** `ble_uart.cpp:487-494`
**Description:** `modeDurMinSec` and `modeDurMaxSec` enforce minimum 10 but no maximum. Values > 65535 silently wrap via `uint16_t` cast.
**Fix:** Add `min(65535, max(10, value))` clamping.

### ME-10. BLE disconnect doesn't clear `pendingResolve` in store
**Source:** Interface Contract §M-7
**Files:** `dashboard/src/lib/store.js:205-211`
**Description:** On disconnect, `pendingResolve` is not cleared. In-flight `sendAndWait` hangs for 3-second timeout instead of failing immediately.
**Fix:** Resolve `pendingResolve` with error in disconnect handler.

### ME-11. Game state macro namespace pollution
**Source:** DX §M1
**Files:** `state.h:288-290`
**Description:** `#define brk`, `#define snk`, `#define rcr` pollute global namespace. `brk` is particularly collision-prone.
**Fix:** Use `gameState.brk` directly, or rename to `gBrk`/`gSnk`/`gRcr`.

### ME-12. Hardcoded `6` for operation mode count in display.cpp
**Source:** DX §M6
**Files:** `display.cpp` (~5 sites), `serial_cmd.cpp` (~1 site)
**Description:** Raw `6` used instead of a named constant.
**Fix:** Define `OP_MODE_COUNT` alongside the suggested enum.

### ME-13. 5x duplicated scrolling text pattern in display.cpp
**Source:** DX §H4
**Files:** `display.cpp` (lines 1737-1769, 1776-1796, 2195-2227, 2234-2254, 2569-2616)
**Description:** Same scroll animation logic (bounce, 1.5s pause, 300ms step) implemented 5 times with independent static state variables.
**Fix:** Extract `drawScrollingText()` helper.

### ME-14. Static `File` object in `saveSimData()` not re-initialized after close failure
**Source:** Resource & Concurrency §M1
**Files:** `sim_data.cpp:290-312`
**Description:** `static File f` retains state after `f.close()`. If close fails silently (flash errors), subsequent `f.open()` may operate on invalid handle.
**Fix:** Check return value of `f.close()` and log warning.

---

## Low Findings

### LO-1. No authentication on BLE UART protocol (Security §M-1)
### LO-2. `importSettings()` saves even when 0 settings applied (Interface Contract §L-7)
### LO-3. Battery history keyed by device name — name change orphans data (State Management §L4)
### LO-4. `strlen() * 6` magic number for text width (~50 instances in display.cpp) (DX §L4)
### LO-5. `connectDevice` deprecated alias without runtime warning (DX §L2)
### LO-6. "recongize" typo in ConnectButton.vue (DX §L1)
### LO-7. `pngCrc32()` bit-by-bit CRC is slow but correct (QA §L3)
### LO-8. Settings struct padding could affect checksum portability (QA §L7)
### LO-9. `drawBitmapScaled()` doesn't clamp to screen bounds (GFX clips) (QA §L6)
### LO-10. `NRF_POWER->DCDCEN = 1` written before SoftDevice init (currently safe) (Resource §M3)
### LO-11. DFU `readFrame()` double reader release in finally block (Resource §M7)
### LO-12. `validateMenuIndices()` halts without OLED error display (QA §M7)
### LO-13. Duplicated confirmation dialog rendering (4 instances in display.cpp) (DX §H6)
### LO-14. Duplicated carousel rendering between MODE_MODE and MODE_CAROUSEL (DX §H5)
### LO-15. `MODE_DESCS[]` hardcoded as static local in `drawModePickerPage()` (DX §M7)

---

## Testing Infrastructure

### Zero automated tests exist
**Source:** Testing Coverage Audit
**Description:** No test files, test frameworks, or test scripts in either firmware or dashboard. CI pipeline only compiles firmware. All QA relies on a 112-item manual checklist (`docs/CLAUDE.md/testing-checklist.md`).

### Priority test targets (by risk × effort)
| Priority | Target | Risk | Effort |
|----------|--------|------|--------|
| P0 | `dashboard/src/lib/protocol.js` — all parsing functions | HIGH | LOW |
| P0 | `dashboard/src/lib/dfu/crc16.js` — CRC16-CCITT | CRITICAL | LOW |
| P0 | `dashboard/src/lib/dfu/slip.js` — SLIP encode/decode | HIGH | LOW |
| P1 | `settings.cpp` — `setSettingValue()` bounds | HIGH | MEDIUM |
| P1 | `ble_uart.cpp` — `cmdSetValue()` protocol parsing | HIGH | MEDIUM |
| P1 | `timing.cpp` — format functions | MEDIUM | LOW |
| P2 | `orchestrator.cpp` — scaling functions | MEDIUM | LOW |
| P2 | `schedule.cpp` — `isScheduleActive()` midnight crossing | MEDIUM | LOW |

**Recommended framework:** Vitest for dashboard (already Vite-based, zero config). Unity or host-native g++ for firmware.

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 4 |
| High | 12 |
| Medium | 14 |
| Low | 15 |

### Top 5 fixes by impact:
1. **CR-1/CR-2:** Update dashboard `OP_MODE_NAMES` and `MODE_NAMES` arrays (5-minute fix, eliminates `undefined` in UI)
2. **CR-3:** Add `volatile` to `deviceConnected`/`bleConnHandle` and defer BLE callback state resets (prevents stale state from compiler caching)
3. **HI-1:** Add bounds guards to ~20 unguarded array lookups (prevents potential hard faults from corrupt settings)
4. **HI-9:** Fix modifier flag checking wrong key after `pickNextKey()` (logic bug affecting simulation fidelity)
5. **CR-4:** Update CLAUDE.md to reflect 3 game modules, 6 operation modes, corrected file sizes and counts

# Audit: Orchestrator light sleep wake fix

## Files changed

- `src/schedule.cpp` — `exitLightSleep()` function (lines 194-200)

## Immediate dependents examined

- `src/orchestrator.cpp` — `syncOrchestratorTime()`, `startBlock()`, `startMode()`
- `src/settings.cpp` — `loadSettings()`, `setSettingValue()` (operationMode bounds check)
- `src/input.cpp` — 6 callers of `exitLightSleep()` (manual wake paths)
- `src/state.h` — `timeSynced`, orchestrator state globals

---

## 1. QA Audit

### Finding 1.1: `operationMode` bounds validation (Low — NOT AN ISSUE)
The code checks `settings.operationMode == OP_SIMULATION` without explicit bounds validation. However, `loadSettings()` already bounds-checks at line 183 (`if (settings.operationMode >= OP_MODE_COUNT) settings.operationMode = OP_SIMPLE;`) and `setSettingValue()` clamps at line 341. **No action needed.**

### Finding 1.2: `currentDaySeconds()` side effect on re-anchor (Low)
`currentDaySeconds()` mutates `wallClockDaySecs`/`wallClockSyncMs` when elapsed time exceeds 24h. This is documented (lines 37-38 of schedule.cpp) and correct behavior. The re-anchored value is always valid `[0, 86399]`. **No action needed.**

### Finding 1.3: Redundant `timeSynced` guard (Info)
The outer `timeSynced` check is technically redundant since `currentDaySeconds()` returns `0xFFFFFFFF` when `!timeSynced`, and line 197 catches that sentinel. However, the explicit check makes intent clearer and avoids a function call. Matches the defensive coding style used throughout the codebase. **Kept intentionally.**

---

## 2. Security Audit

### Finding 2.1: TOCTOU on `timeSynced` flag (Low — NOT AN ISSUE)
Multiple auditors flagged a potential race on the `timeSynced` global. However, the nRF52840 firmware runs a single-threaded cooperative main loop. BLE callbacks and serial commands are dispatched cooperatively, not preemptively. ISRs never modify `timeSynced`. **No race condition exists.** The sentinel check at line 197 provides defense-in-depth regardless.

### Finding 2.2: Integer type in `syncOrchestratorTime()` loop (Info — pre-existing)
`offsetMin` is `uint32_t` while `blockEnd` accumulates as `uint16_t`. Max workday is ~720 minutes (well within `uint16_t` range of 65535). Pre-existing code, not introduced by this change. **No action needed.**

---

## 3. Interface Contract Audit

### Finding 3.1: Mouse timer scheduling order (Low — NOT AN ISSUE)
The audit flagged that `scheduleNextMouseState()` (line 192) runs before `syncOrchestratorTime()` (line 198), making the mouse timer "wasted work" in Simulation mode. This is correct — in Simulation mode, the orchestrator owns activity scheduling and the simple-mode timers are unused. The simple-mode resets are needed for the `OP_SIMPLE` path and are harmless in `OP_SIMULATION`. Reordering would add complexity for no functional benefit. **No action needed.**

### Finding 3.2: `markDisplayDirty()` redundancy (Info)
`syncOrchestratorTime()` → `startBlock()` (line 247) and `startMode()` (line 287) both call `markDisplayDirty()` internally. The explicit call at line 202 is redundant but harmless and maintains the existing pattern in `exitLightSleep()`. **Kept for consistency.**

---

## 4. State Management Audit

### Finding 4.1: No cross-mode state pollution (Info — NOT AN ISSUE)
Mode changes (`operationMode`) require a reboot, so there's no scenario where the user switches between Simple and Simulation modes across a light sleep cycle. **No action needed.**

### Finding 4.2: Orchestrator not synced when `timeSynced == false` (Info — ACCEPTABLE)
If the clock isn't synced, the orchestrator can't be re-synced to wall clock time. The conditional correctly skips the sync. The orchestrator will continue from its stale `millis()`-based state, which is the best available option without a time reference. **Acceptable limitation.**

---

## 5. Resource & Concurrency Audit

### Finding 5.1: `millis()` wrap race (Low — NOT AN ISSUE)
The audit flagged a potential issue if `millis()` wraps (~49.7 days) between `currentDaySeconds()` and `syncOrchestratorTime()`. These calls are on adjacent lines in the same function — the wrap window is microseconds. Even if it occurs, `syncOrchestratorTime()` samples `millis()` independently and computes relative offsets, so a wrap mid-function would cause at most a 1-tick error in `orch.dayStartMs`. **No practical risk.**

### Finding 5.2: BLE advertising async start (Info — NOT AN ISSUE)
BLE advertising is started earlier in `exitLightSleep()` (line 174). The audit flagged that a BLE connection could arrive before `syncOrchestratorTime()` completes. This would only cause a single-frame display glitch (stale block name for 50ms). The same window exists in the existing code for simple-mode state. **Acceptable.**

---

## 6. Testing Coverage Audit

### Finding 6.1: No unit test infrastructure (Info — pre-existing)
The project uses manual hardware testing via the checklist at `docs/CLAUDE.md/testing-checklist.md`. This is pre-existing and appropriate for an embedded firmware project with hardware-dependent behavior. **No action needed for this change.**

### [FIXED] Finding 6.2: Missing test checklist items for light sleep wake in Simulation mode (High)
The testing checklist has no items covering orchestrator behavior across light sleep. Test items should be added covering:
- Simulation mode light sleep wake with correct block sync
- Manual vs. scheduled wake paths
- Serial log verification

---

## 7. DX & Maintainability Audit

### Finding 7.1: Redundant `timeSynced` guard vs. sentinel (Medium — KEPT INTENTIONALLY)
Same as Finding 1.3. The outer `timeSynced` check is redundant but makes intent explicit. This matches the pattern used in `orchestrator.cpp` day-rollover detection (line 688-691). **Kept for clarity.**

### Finding 7.2: Comment could mention "Simulation mode only" (Low)
The comment says "Re-sync orchestrator to wall clock after light sleep gap" — the `OP_SIMULATION` guard on the next line makes this obvious. **No action needed.**

---

## Summary

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 1.1 | `operationMode` bounds validation | Low | NOT AN ISSUE — already validated in loadSettings/setSettingValue |
| 1.2 | `currentDaySeconds()` re-anchor side effect | Low | Documented, correct behavior |
| 1.3 | Redundant `timeSynced` guard | Info | Kept intentionally for clarity |
| 2.1 | TOCTOU on `timeSynced` | Low | NOT AN ISSUE — single-threaded main loop |
| 2.2 | Integer type in `offsetMin` | Info | Pre-existing, not introduced by this change |
| 3.1 | Mouse timer ordering | Low | NOT AN ISSUE — harmless in Simulation mode |
| 3.2 | `markDisplayDirty()` redundancy | Info | Kept for consistency |
| 4.1 | Cross-mode state pollution | Info | NOT AN ISSUE — mode change requires reboot |
| 4.2 | No sync when clock unsynced | Info | Acceptable limitation |
| 5.1 | `millis()` wrap race | Low | NOT AN ISSUE — microsecond window |
| 5.2 | BLE async advertising | Info | Acceptable single-frame glitch |
| 6.1 | No unit test framework | Info | Pre-existing |
| 6.2 | Missing test checklist items | High | [FIXED] — items added to testing-checklist.md |
| 7.1 | Redundant guard style | Medium | Kept intentionally |
| 7.2 | Comment specificity | Low | Self-evident from guard |

**No blocking issues found. One actionable item (6.2) addressed by adding test checklist items.**

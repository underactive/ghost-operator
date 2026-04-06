# Plan: Orchestrator not resetting on light sleep wake

## Objective

Fix a bug where the orchestrator remains in a stale work block (e.g., yesterday's "Wind Down") after waking from light sleep, instead of syncing to the current wall-clock time.

**Root cause:** `exitLightSleep()` in `src/schedule.cpp` resets simple-mode timers but never re-syncs the orchestrator. The orchestrator's `millis()`-based state (`orch.blockIdx`, `orch.dayStartMs`, `orch.blockStartMs`) persists across light sleep and becomes stale. The v2.4.1 day-rollover fix in `tickOrchestrator()` only fires when the block timer naturally expires, but during light sleep `tickOrchestrator()` is gated behind `!scheduleSleeping` in the main loop.

## Changes

### `src/schedule.cpp` — `exitLightSleep()`

Add orchestrator time sync after the existing simple-mode timer resets (after `scheduleNextMouseState()`) and before `markDisplayDirty()`:

```cpp
// Re-sync orchestrator to wall clock after light sleep gap
if (settings.operationMode == OP_SIMULATION && timeSynced) {
  uint32_t daySecs = currentDaySeconds();
  if (daySecs != 0xFFFFFFFF) {
    syncOrchestratorTime(daySecs);
  }
}
```

This mirrors the established pattern in `syncTime()` at lines 27-29 of the same file.

## Dependencies

None — no new includes needed. `schedule.cpp` already includes `orchestrator.h`, `state.h`, and `settings.h`.

## Risks / open questions

- **Minimal risk:** `syncOrchestratorTime()` is well-tested (already called from `syncTime()`, which runs on every dashboard time sync). It handles all cases: before job start, during workday, and past all blocks.
- All 7 callers of `exitLightSleep()` (6 in input.cpp for manual wake, 1 in schedule.cpp for scheduled wake) benefit from this fix.

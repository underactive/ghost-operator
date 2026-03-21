# Implementation: Orchestrator not resetting on light sleep wake

## Files changed

- `src/schedule.cpp` — Added orchestrator time sync call in `exitLightSleep()` (lines 194-200)

## Summary

Added a 5-line block to `exitLightSleep()` that calls `syncOrchestratorTime(daySecs)` when in Simulation mode with a valid time sync. This ensures the orchestrator snaps to the correct work block on wake, regardless of how long the device was asleep.

The implementation exactly matches the plan — no deviations.

## Verification

- `make build` — compilation succeeds (189,820 bytes flash, 21,056 bytes RAM — no change from baseline)
- Code review: the new block mirrors the established pattern in `syncTime()` (same file, lines 27-29)
- Guards: `OP_SIMULATION` mode check + `timeSynced` flag + `0xFFFFFFFF` sentinel check prevent execution in non-applicable states

## Follow-ups

- Manual hardware test needed: Simulation mode + Full Auto schedule → enter light sleep → wake after schedule start → verify correct block displayed and `[SIM] Time synced to block N` appears on serial monitor

## Audit Fixes

### Fixes applied

1. Added 6 test items to `docs/CLAUDE.md/testing-checklist.md` under "Light Sleep + Orchestrator Sync" section — addresses Testing Coverage Audit Finding 6.2 (missing checklist items for light sleep wake in Simulation mode)

### Verification checklist

- [ ] Verify new test checklist items cover: scheduled wake, manual wake, serial log output, Simple mode (no sync), and no-clock-sync edge case

### Unresolved items

All other audit findings were assessed as non-issues, pre-existing, or intentional design choices:
- Redundant `timeSynced` guard (kept intentionally for clarity, matches `syncTime()` pattern)
- `currentDaySeconds()` re-anchor side effect (documented, correct behavior)
- TOCTOU concerns (not applicable — single-threaded cooperative main loop)
- Mouse timer ordering (harmless — simple-mode timers unused in Simulation mode)

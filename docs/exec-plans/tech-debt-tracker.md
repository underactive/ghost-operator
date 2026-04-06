# Tech Debt Tracker

Known technical debt, tracked as inventory.

## Format

```
### <short title>
- **Domain:** which domain is affected
- **Grade impact:** what quality grade this drags down
- **Severity:** low | medium | high
- **Added:** YYYY-MM-DD
- **Notes:** context for why this exists and what fixing looks like
```

## Active debt

### display.cpp monolith
- **Domain:** Display (nRF52)
- **Grade impact:** B+ (prevents A)
- **Severity:** medium
- **Added:** 2025-04-06
- **Notes:** At ~2900 lines, `display.cpp` is the largest module by far. Each new UI mode adds to it. Splitting into per-mode rendering files would improve maintainability, but the tight coupling to `state.h` globals makes this non-trivial.

### Native test coverage
- **Domain:** Testing
- **Grade impact:** C
- **Severity:** medium
- **Added:** 2025-04-06
- **Notes:** Unity test infrastructure exists (`env:native`) but coverage of `src/common/` modules is limited. Orchestrator, mouse FSM, and timing logic would benefit from unit tests.

### No firmware tests in CI
- **Domain:** CI/CD
- **Grade impact:** B (prevents A)
- **Severity:** low
- **Added:** 2025-04-06
- **Notes:** CI builds firmware and runs dashboard Vitest, but `test_ignore = *` for nRF52 env means no firmware unit tests run. Native tests could be added to the release workflow.

## Resolved debt

(none)

## Process

- When you discover tech debt during a task, add it here rather than fixing
  it inline (unless the fix is trivial and scoped to your current change).
- Cleanup tasks should reference the specific item they resolve.
- Move resolved items to the "Resolved" section with the date and PR/commit.

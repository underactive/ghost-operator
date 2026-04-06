# Plans Index

Plans are first-class artifacts in this project. Complex work gets an execution plan checked into the repo.

## Active plans

| Plan | Goal | Started |
|------|------|---------|
| (none) | — | — |

See [exec-plans/active/](exec-plans/active/) for full plan documents.

## Completed plans

| Plan | Goal | Completed |
|------|------|-----------|
| [codebase-audit-fixes](exec-plans/completed/codebase-audit-fixes/) | Security and code quality fixes (45 findings) | 2025-02 |
| [full-codebase-audit](exec-plans/completed/full-codebase-audit/) | Comprehensive audit | 2025-02 |
| [platformio-migration](exec-plans/completed/platformio-migration/) | Arduino CLI → PlatformIO build system | 2025-03 |
| [battery-charging-indicator](exec-plans/completed/battery-charging-indicator/) | Lightning bolt charging icon | 2025-03 |
| [orchestrator-light-sleep-fix](exec-plans/completed/orchestrator-light-sleep-fix/) | Fix orchestrator sync after light sleep | 2025-03 |
| [lifetime-stats](exec-plans/completed/lifetime-stats/) | Persistent keystroke/mouse/click counters | 2025-03 |
| [esp32-c6-port](exec-plans/completed/esp32-c6-port/) | ESP32-C6 LCD 1.47" port | 2025-04 |
| [nrf52-json-protocol](exec-plans/completed/nrf52-json-protocol/) | JSON config protocol for nRF52 | 2025-04 |
| [esp32-s3-port](exec-plans/completed/esp32-s3-port/) | ESP32-S3 LCD 1.47" port | 2025-04 |

See [exec-plans/completed/](exec-plans/completed/) for full plan documents.

## Creating a plan

Use an execution plan when the work:
- Touches 3+ files or domains
- Requires multiple sequential steps with dependencies
- Involves a non-obvious architectural decision
- Will take more than one session to complete

### Plan template

```markdown
# Plan: <title>

**Goal:** One sentence.
**Status:** Active | Blocked | Completed
**Started:** YYYY-MM-DD

## Context
Why this work is needed.

## Objective
What is being implemented and why.

## Changes
Files to modify/create, with descriptions of each change.

## Dependencies
Any prerequisites or ordering constraints between changes.

## Risks / open questions
Anything flagged during planning that needs attention.

## Steps
- [ ] Step 1
- [ ] Step 2

## Decisions
- YYYY-MM-DD: Decided X because Y.
```

Save to `docs/exec-plans/active/<slug>.md`. Move to `completed/` when done.

### Implementation record

After implementing a plan, write `implementation.md` in the plan directory:
- **Files changed** (required) — lightweight index for future plan searches
- **Summary** — what was actually implemented (deviations from plan)
- **Verification** — tests run, manual checks, build confirmation
- **Follow-ups** — remaining work, known limitations

If the implementation changed user-facing behavior, add test items to [references/testing-checklist.md](references/testing-checklist.md).

## Post-implementation audit

After implementing a plan, run these 7 audit subagents **in parallel** on all changed files. Write a consolidated `audit.md` in the plan directory.

> **Scope:** Only flag issues in changed code and immediate dependents.

### 1. QA Audit
- Functional correctness, edge cases, infinite loops, performance in hot paths

### 2. Security Audit
- Injection/input trust, buffer overflows, memory leaks, hard crashes

### 3. Interface Contract Audit
- Data shape mismatches, error handling gaps, auth flows, data consistency

### 4. State Management Audit
- Mutation discipline, reactivity pitfalls, data flow, sync issues

### 5. Resource & Concurrency Audit
- Data races, resource lifecycle, timing assumptions, power/hardware

### 6. Testing Coverage Audit
- Missing tests, test quality, integration gaps, flakiness risks

### 7. DX & Maintainability Audit
- Readability (>50 line functions), dead code, naming, documentation gaps

### After addressing audit findings
1. Mark fixed items with `[FIXED]` prefix in `audit.md`
2. Append `## Audit Fixes` section to `implementation.md`
3. Add test items for behavioral changes to testing checklist

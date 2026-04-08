# Plan: ESP32-S3 USB Follow-up Cleanup

**Goal:** Clean up the post-fix S3 work by removing an unrelated local diff, restoring native test execution, and committing the changes as focused commits.
**Status:** Completed
**Started:** 2026-04-07

## Context
The ESP32-S3 USB CDC fix was confirmed on hardware. Two follow-up items remained:
- `src/esp32-s3-lcd-1.47/hid.cpp` still had a local diff unrelated to the actual fix
- `pio test -e native` failed because the Unity native tests had test functions but no test runner / `main`

The user explicitly asked to clean up the unrelated S3 diff, investigate the broken native test harness, and commit the work.

## Objective
Leave the worktree with:
- a focused USB CDC fix commit
- a working native Unity harness and test run
- no unrelated `hid.cpp` diff mixed into the USB commit

## Changes
- `src/esp32-s3-lcd-1.47/hid.cpp` — resolved the unrelated local change by restoring the baseline implementation
- `test/test_native/*.cpp` — added a shared Unity test runner and consolidated fixture definitions
- `docs/PLANS.md` — track the completed follow-up plan

## Dependencies
- The harness had to be fixed before rerunning `pio test -e native`
- The unrelated `hid.cpp` diff had to be resolved before creating the USB CDC commit

## Risks / open questions
- The native harness could have exposed additional test or build issues once `main` existed
- The user has unrelated untracked screenshots in the worktree; they must stay out of commits

## Steps
- [x] Add the active plan and update the plans index
- [x] Resolve the unrelated `hid.cpp` diff
- [x] Add a native Unity runner and remove duplicate fixture definitions
- [x] Rerun `pio test -e native` and confirm the S3 build still passes
- [x] Move the plan to completed and create focused commits

## Decisions
- 2026-04-07: Use separate commits so the USB CDC firmware fix remains isolated from the native test harness repair.

# Ghost Operator — Agent Map

BLE keyboard/mouse hardware device that prevents screen lock and idle timeout. Multi-platform firmware (nRF52840, ESP32-S3, ESP32-C6) with Vue 3 web dashboard.

**Current Version:** 2.5.7 — **Status:** Production-ready

## Quick orientation

| What you need              | Where to look                          |
|----------------------------|----------------------------------------|
| Domain model & layering    | [ARCHITECTURE.md](ARCHITECTURE.md)     |
| Design principles          | [docs/design-docs/core-beliefs.md](docs/design-docs/core-beliefs.md) |
| Display layouts & UI specs | [docs/design-docs/display-layout.md](docs/design-docs/display-layout.md) |
| Product specs & user flows | [docs/product-specs/index.md](docs/product-specs/index.md) |
| Current execution plans    | [docs/exec-plans/active/](docs/exec-plans/active/) |
| Completed plans & history  | [docs/exec-plans/completed/](docs/exec-plans/completed/) |
| Known tech debt            | [docs/exec-plans/tech-debt-tracker.md](docs/exec-plans/tech-debt-tracker.md) |
| Quality grades by domain   | [docs/QUALITY_SCORE.md](docs/QUALITY_SCORE.md) |
| Output & UI conventions    | [docs/DESIGN.md](docs/DESIGN.md)       |
| Reliability & dev rules   | [docs/RELIABILITY.md](docs/RELIABILITY.md) |
| Security boundaries        | [docs/SECURITY.md](docs/SECURITY.md)   |
| Product sense              | [docs/PRODUCT_SENSE.md](docs/PRODUCT_SENSE.md) |
| Plan index & templates    | [docs/PLANS.md](docs/PLANS.md)         |
| Build instructions         | [docs/references/build.md](docs/references/build.md) |
| Common modification recipes | [docs/references/common-modifications.md](docs/references/common-modifications.md) |
| Config protocol reference  | [docs/references/protocol.md](docs/references/protocol.md) |
| Serial debug commands      | [docs/references/serial-commands.md](docs/references/serial-commands.md) |
| Orchestrator deep dive     | [docs/references/orchestrator.md](docs/references/orchestrator.md) |
| Testing checklist          | [docs/references/testing-checklist.md](docs/references/testing-checklist.md) |
| DFU documentation          | [docs/design-docs/serial-dfu.md](docs/design-docs/serial-dfu.md), [docs/design-docs/ota-dfu.md](docs/design-docs/ota-dfu.md) |
| Version history            | [docs/HISTORY.md](docs/HISTORY.md)     |
| Future ideas               | [docs/references/future-improvements.md](docs/references/future-improvements.md) |
| Data schemas               | [docs/generated/README.md](docs/generated/README.md) |
| Reference material         | [docs/references/](docs/references/)   |
| User manual                | [docs/USER_MANUAL.md](docs/USER_MANUAL.md) |

## Repo conventions

- **Language:** C++ (Arduino/PlatformIO firmware) + JavaScript/Vue 3 (web dashboard)
- **Boundaries:** Validate all external input at storage boundary via `clampVal()` in `setSettingValue()`. Interior code trusts typed structs. Protocol commands (`=key:value`) are bounds-checked identically to encoder UI paths.
- **Tests:** Dashboard has co-located Vitest tests (`*.test.js`). Firmware has native Unity tests (`env:native`). Manual QA checklist in `docs/references/testing-checklist.md`. Run `make test` (native) or `cd dashboard && npm test` (dashboard).
- **Logging:** Serial debug at 115200 baud. `[SIM]`-prefixed log lines for orchestrator transitions. No structured logging — embedded context.
- **Naming:** `camelCase` functions/variables, `UPPER_SNAKE_CASE` constants/enums, `PascalCase` structs. 2-space indent, K&R braces. Vue uses `<script setup>` + Composition API. See existing patterns in `display.cpp`, `settings_common.cpp`, `App.vue`.
- **File size:** Soft limit ~300 lines for new modules. `display.cpp` (~2900 lines) is a legacy exception — do not add to it without good reason.
- **Imports:** Project headers first (quoted), then standard library, then third-party. No circular imports. Header guards: `#ifndef GHOST_[FILENAME]_H`.
- **Types:** Prefer `uint8_t`/`uint16_t`/`uint32_t` over `int`. `volatile` for ISR-accessed variables. Struct-based data (C idiom), not C++ classes.

## Agent workflow

1. Read this file first for orientation.
2. Check the relevant section in the table above for your task domain.
3. For complex work (3+ files, sequential dependencies, non-obvious decisions,
   or multi-session scope), check
   [docs/exec-plans/active/](docs/exec-plans/active/) for an existing plan.
   If none exists, **create one** using the template in
   [docs/PLANS.md](docs/PLANS.md) before starting implementation.
   Update [docs/PLANS.md](docs/PLANS.md) index when adding or completing plans.
4. Before planning, scan **Files changed** lists in completed plans at
   [docs/exec-plans/completed/](docs/exec-plans/completed/) to find prior work
   that touched the same areas — read the full `plan.md` only for matches.
5. Run `make build` to verify firmware compiles. Run `cd dashboard && npm test`
   for dashboard tests. Run `make test` for native firmware tests.
6. If you add a new domain or package, update [ARCHITECTURE.md](ARCHITECTURE.md).
7. If you add or change user-facing behavior, update the relevant spec in
   [docs/product-specs/](docs/product-specs/) and add test items to
   [docs/references/testing-checklist.md](docs/references/testing-checklist.md).
8. If you ship or significantly change a domain, update its grade in
   [docs/QUALITY_SCORE.md](docs/QUALITY_SCORE.md).
9. If you add or change a data contract (Settings struct, protocol commands),
   update [docs/generated/README.md](docs/generated/README.md).
10. If you discover tech debt, log it in
    [docs/exec-plans/tech-debt-tracker.md](docs/exec-plans/tech-debt-tracker.md).
11. If you change user-facing interfaces, defaults, or configuration, update
    [README.md](README.md) and [docs/USER_MANUAL.md](docs/USER_MANUAL.md).

## What NOT to do

- Do not put long instructions in this file. Add them to the appropriate doc
  and link from here.
- Do not skip boundary validation because "it's just internal."
- Do not use `analogRead()` on A0/A1 — shares pins with encoder, breaks reads.
- Do not write directly to `NRF_POWER->GPREGRET` — use `sd_power_*()` API.
- Do not use Arduino `String` concatenation in hot paths — causes heap
  fragmentation on nRF52's 256KB RAM. Use `snprintf()` into stack buffers.
- Do not remove `lib_archive = no` from `platformio.ini` — breaks TinyUSB.
- Do not hardcode menu cursor indices — use `MENU_IDX_*` defines from `keys.h`.

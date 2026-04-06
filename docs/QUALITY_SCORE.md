# Quality Score

Tracks the current quality grade of each domain.

## Grading scale

| Grade | Meaning |
|-------|---------|
| A | Well-tested, documented, agent-legible, no known debt |
| B | Functional and tested, minor gaps in docs or edge cases |
| C | Works but has known debt, missing tests, or unclear naming |
| D | Fragile, undertested, or structurally problematic |
| F | Broken or placeholder only |

## Domain grades

| Domain | Grade | Notes | Last reviewed |
|--------|-------|-------|---------------|
| HID (BLE + USB) | A | Dual-transport, stale link detection, well-tested | 2025-04 |
| Settings / Menu | A | Data-driven, bounds-checked, 62 items | 2025-04 |
| Mouse FSM | A | Bezier + Brownian, sine easing, return-to-origin | 2025-03 |
| Timing / Profiles | A | Clean API, profile scaling, randomization | 2025-03 |
| Orchestrator | A | 4-level hierarchy, lunch enforcement, keepalive | 2025-03 |
| Display (nRF52) | B+ | Shadow buffer optimization, but 2900-line monolith | 2025-04 |
| Encoder | A | Hybrid ISR+polling, race-condition hardened | 2025-03 |
| Power / Sleep | A | Two-stage sleep, WDT, scheduled wake | 2025-03 |
| Sound | A | 5 profiles, live preview, ISR-safe | 2025-03 |
| Protocol (text) | A | Transport-agnostic, bounds-checked, throttled | 2025-04 |
| Protocol (JSON) | B | Functional, newer — less battle-tested | 2025-04 |
| DFU (Serial) | A | Full Web Serial DFU pipeline, dashboard integrated | 2025-04 |
| DFU (OTA) | B | Works via nRF Connect, no web path (Chrome blocklist) | 2025-03 |
| Games | B | Fun, stable, but no automated tests | 2025-03 |
| Dashboard | B+ | Vue 3, Web Serial + BLE, DFU, settings export — needs more tests | 2025-04 |
| ESP32-S3 port | B | Functional with LVGL, dual-transport HID | 2025-04 |
| ESP32-C6 port | B | Functional with LVGL, BLE-only | 2025-04 |
| Native tests | C | Infrastructure exists, limited coverage of common/ | 2025-04 |

## Cross-cutting grades

| Concern | Grade | Notes | Last reviewed |
|---------|-------|-------|---------------|
| Serial logging | B | `[SIM]`-prefixed, push status, but no structured format | 2025-03 |
| Memory safety | A | snprintf everywhere, no String in hot paths, bounds on all lookups | 2025-03 |
| CI/CD | B | Release builds + dashboard tests, but no firmware unit tests in CI | 2025-04 |

## Process

- Review and update grades when a domain ships or changes significantly.
- A domain at grade C or below should have an entry in
  [tech-debt-tracker.md](exec-plans/tech-debt-tracker.md).
- Background cleanup tasks target the lowest-graded domains first.

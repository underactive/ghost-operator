# Design Docs Index

Design docs capture significant architectural decisions and their rationale.

## Format

```
docs/design-docs/YYYY-MM-DD-<slug>.md
```

Each design doc should include:
- **Status:** Draft | Active | Superseded
- **Problem:** What we're solving and why
- **Decision:** What we chose
- **Alternatives considered:** What else we evaluated
- **Consequences:** What this decision enables and constrains

## Active design docs

| Doc | Status | Summary |
|-----|--------|---------|
| [core-beliefs.md](core-beliefs.md) | Active | Agent-first operating principles |
| [display-layout.md](display-layout.md) | Active | ASCII mockups of all OLED UI modes |
| [ota-dfu.md](ota-dfu.md) | Active | OTA DFU investigation and limitations |
| [serial-dfu.md](serial-dfu.md) | Active | Web Serial DFU implementation |

## Verification

Design docs are considered verified when the described architecture matches
the actual codebase. Stale docs should be updated or marked Superseded.

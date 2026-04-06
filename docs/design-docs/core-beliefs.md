# Core Beliefs

Ghost Operator's operating principles — how we build firmware and how agents should reason about the codebase.

## 1. Repository is the system of record

Every decision, spec, and convention lives in the repo. If it's not
discoverable by reading the repo, it doesn't exist. Slack threads and
conversations are ephemeral — commit the decision or it rots.

## 2. Agents execute, humans steer

Human time is the scarcest resource. Invest human attention in hardware
design, UX decisions, and feedback loops — not in writing boilerplate.
When an agent struggles, ask "what context is missing?" rather than
"let me just do it manually."

## 3. Parse at the boundary, trust the interior

All external data (BLE UART commands, USB serial input, dashboard protocol)
is validated via `setSettingValue()` with `clampVal()` at entry. Interior
code operates on the validated `Settings` struct and does not re-check.

## 4. Enforce mechanically, not manually

If a rule matters, encode it as a bounds check, checksum validation, or
CI test. The development rules in [RELIABILITY.md](../RELIABILITY.md) exist
because those bug classes were found during QA — they're enforced in code
(`clampVal()`, overflow flags, `snprintf`), not just documented.

## 5. Prefer boring technology

Arduino/PlatformIO ecosystem, raw C structs, LittleFS, Adafruit libraries.
Boring tech is stable, well-documented, and easy for agents to reason about.
The one exotic choice (TinyUSB composite HID) is load-bearing and well-tested.

## 6. Progressive disclosure over front-loading

AGENTS.md is a map, not a manual. Agents start with a small entry point and
follow links to deeper context as needed. The firmware follows the same
principle: Simple mode for basic use, Simulation mode for advanced users.

## 7. Hardware constraints are features

256KB RAM means no heap allocation in hot paths. 128×64 OLED means every
pixel is deliberate. These constraints produce better firmware — forced
simplicity, forced efficiency, forced clarity.

## 8. Invisible by default

The device must never interfere with the user's active work. Default keys
(F13-F24) are invisible to applications. Mouse movements are subtle. The
best jiggler is one you forget is running.

## 9. Plans are artifacts, not conversations

Complex work gets an execution plan checked into the repo with implementation
records and audit reports. Plans are versioned, reviewable, and discoverable.

# Reliability

Requirements and practices for keeping Ghost Operator firmware reliable across weeks of unattended uptime.

## Failure modes

| Failure | Likelihood | Mitigation |
|---------|------------|------------|
| BLE stale link (silent keystroke loss) | Medium | GATT notify return check, force-disconnect after 5 failures |
| Heap fragmentation (long uptime) | Medium | `snprintf` in hot paths, no Arduino `String` concatenation |
| I2C bus lockup (OLED) | Low | Hardware watchdog (8s), I2C bus recovery on restart |
| Corrupt settings from flash | Low | Magic number versioning, checksum, safe `loadDefaults()` fallback |
| Buffer overflow from protocol input | Low | Overflow flag tracking, `-err:cmd too long` response |
| SoftDevice hard fault (register access) | Low | `sd_power_*()` SVCall API only — never direct register writes |
| Encoder misreads (analogRead conflict) | Low | Never use `analogRead()` on A0/A1 (shares pins with encoder) |
| TinyUSB driver table stripped | Low | `lib_archive = no` in platformio.ini — never remove |

## Development rules (QA hardening)

These rules exist to prevent classes of bugs found during firmware QA. Follow them for all new code.

### 1. Validate all external input at the storage boundary
Every value from BLE UART or USB serial passes through `setSettingValue()` with `clampVal()`. Never assign protocol-supplied values without bounds checking.

### 2. Guard all array-indexed format lookups
`formatMenuValue()` indices into name arrays must have bounds checks: `(val < COUNT) ? NAMES[val] : "???"`.

### 3. Reset connection-scoped state on disconnect
Line buffers (`uartBuf`, `serialBuf`) and overflow flags must be cleared in `disconnect_callback()`.

### 4. Avoid Arduino `String` concatenation in hot paths
Use `snprintf()` into stack-allocated `char[]` buffers for protocol responses and any function called more than once per second.

### 5. Use symbolic constants for menu indices
Never hardcode `menuCursor = N` — use `MENU_IDX_*` defines from `keys.h`.

### 6. Throttle event-driven output
`pushSerialStatus()` has a 200ms minimum interval guard. Any new function that sends data on frequent events must implement similar throttling.

### 7. Use `snprintf` over `sprintf`
Always `snprintf(buf, sizeof(buf), ...)` — prevents silent overflow if format args change.

### 8. Report errors, don't silently truncate
Command buffers track overflow flags. Respond with `-err:cmd too long` instead of processing partial commands.

## Known issues / limitations

1. **Battery reading approximate** — ADC calibration varies per chip
2. **No hardware power switch** — sleep mode is the off switch
3. **Encoder must use Seeed nRF52 core** — mbed core incompatible with Bluefruit
4. **Custom board definition required** — board JSON and framework override must stay in sync
5. **`lib_archive = no` is mandatory** — PlatformIO strips TinyUSB driver tables without it
6. **D0/D1 are analog-capable** — `analogRead()` disconnects digital input buffer, breaking encoder
7. **SoftDevice register access** — must use `sd_power_*()` SVCall API, not direct register writes

## Invariants

1. **Never crash on malformed protocol input.** Buffers track overflow, parsers reject gracefully.
2. **Settings always loadable.** Magic number mismatch triggers `loadDefaults()`, never corrupt reads.
3. **HID output is invisible.** Default keys (F13-F24) don't trigger host-side actions. Middle clicks are no-ops.
4. **Sleep is always reachable.** Long-press sleep works from any mode, including screensaver.

## Testing strategy

- **Dashboard tests:** Co-located Vitest tests (`*.test.js`) covering protocol, store, DFU helpers. Run with `cd dashboard && npm test`.
- **Native firmware tests:** Unity tests in `env:native` covering `src/common/` logic. Run with `make test`.
- **Manual QA:** ~100-item checklist in [references/testing-checklist.md](references/testing-checklist.md) covering all UI modes, settings persistence, BLE, serial commands.
- **CI:** GitHub Actions builds firmware + dashboard, runs Vitest on `v*` tags.

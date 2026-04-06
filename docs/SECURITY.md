# Security

Security boundaries for Ghost Operator firmware and web dashboard.

## Threat model

1. **BLE/USB protocol input is untrusted.** Commands arriving over BLE UART or USB serial may contain malformed payloads, out-of-range values, or oversized data. All external input is validated at the storage boundary via `setSettingValue()` with `clampVal()`.

2. **Physical access is assumed.** The device is a desktop peripheral — anyone with physical access can pair, configure, or flash firmware. There is no authentication layer beyond BLE pairing.

3. **Dashboard runs in browser sandbox.** The Vue 3 dashboard communicates via Web Serial and Web Bluetooth APIs. No server-side component. No credentials stored.

4. **DFU is destructive.** Serial DFU erases and rewrites application flash. DFU packages are validated by the bootloader (CRC check), but there is no code signing. A malicious DFU package could brick the device (recoverable via UF2 bootloader).

## Rules

- **Validate at the boundary, trust the interior.** `setSettingValue()` clamps all values. `formatMenuValue()` bounds-checks array indices. Interior code operates on validated `Settings` struct.
- **No unbounded buffer writes.** Protocol buffers (`uartBuf`, `serialBuf`) track overflow flags. `snprintf` everywhere — never `sprintf`.
- **No heap allocation in hot paths.** Stack-allocated `char[]` buffers for protocol responses. Arduino `String` only in one-shot display code.
- **SoftDevice register isolation.** Never write directly to SoftDevice-owned registers (`NRF_POWER->GPREGRET`, radio regs). Use `sd_power_*()` SVCall API.
- **DFU requires user initiation.** `!serialdfu` and `!dfu` commands require explicit send. Dashboard shows confirmation dialog before DFU.

## Sensitive files

The system should warn if operations touch:
- `boards/seeed_xiao_nrf52840.json` — board definition (breaks build if misconfigured)
- `platformio.ini` — build config (`lib_archive = no` is critical)
- `.github/workflows/` — CI/CD pipeline
- `dashboard/src/lib/dfu/` — DFU protocol implementation (correctness-critical)

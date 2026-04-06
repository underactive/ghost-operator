# Config Protocol Reference

Transport-agnostic text protocol over BLE UART (NUS) and USB serial. Implemented in `processCommand(line, writer)` which accepts a `ResponseWriter` function pointer.

## Command syntax

- `?` prefix — query (read-only)
- `=` prefix — set (write)
- `!` prefix — action (side effect)
- Pipe-delimited responses

## Commands

```
WEB → DEVICE                    DEVICE → WEB
────────────────────────────────────────────────
?status                     →   !status|connected=1|kb=1|ms=1|bat=85|...
?settings                   →   !settings|keyMin=2000|keyMax=6500|...
?keys                       →   !keys|F13|F14|F15|...|NONE
=keyMin:2000                →   +ok
=slots:2,28,28,28,28,28,28,28 → +ok
=mouseStyle:1               →   +ok
=btWhileUsb:1               →   +ok
=scroll:1                   →   +ok
=dashboard:1                →   +ok
=invertDial:1               →   +ok
=switchKeys:N               →   +ok
=jobStart:N                 →   +ok
=jobPerf:N                  →   +ok
=clickSlots:1,7,7,7,7,7,7  →   +ok
=sound:1                    →   +ok
=soundType:N                →   +ok
=encButton:N                →   +ok
=sideButton:N               →   +ok
=shiftDur:N                 →   +ok
=lunchDur:N                 →   +ok
=totalKeys:N                →   +ok
=totalMousePx:N             →   +ok
=totalClicks:N              →   +ok
=statusPush:1               →   +ok
=name:MyDevice              →   +ok
!save                       →   +ok (saves settings, sim data, and stats)
!defaults                   →   +ok
!reboot                     →   (device reboots)
!dfu                        →   +ok:dfu (then reboots into OTA DFU bootloader)
!serialdfu                  →   +ok:serialdfu (then reboots into Serial DFU bootloader)
```

## Transport details

### BLE UART (NUS)
- 20-byte chunked writes via `bleWrite()` for default MTU compatibility
- NUS UUID not in advertising packet (would overflow 31 bytes) — discovered via `optionalServices`
- `pushSerialStatus()` proactively sends status on state changes (guarded by 200ms throttle)

### USB Serial
- `serialWrite()` in `serial_cmd.cpp` — line-buffered
- Dispatches `?`/`=`/`!` prefixed lines to `processCommand()`
- Dashboard connects via Chrome Web Serial API at 115200 baud

## Behavior

- Settings changes apply to in-memory struct immediately (same as encoder edits)
- Flash save only on `!save` command
- All `=set` commands pass through `setSettingValue()` with `clampVal()` bounds checking

## DFU backup/restore

The Seeed bootloader may erase the LittleFS region during DFU. The dashboard:
1. Snapshots stats and settings to localStorage before sending `!serialdfu`
2. Auto-restores on first reconnection after DFU
3. Stats restore uses `max(device, backup)` to avoid data loss
4. Settings restore only triggers if device values match factory defaults

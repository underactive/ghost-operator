# Serial Debug Commands

At 115200 baud:

| Key | Action |
|-----|--------|
| h | Help |
| s | Status report |
| d | Dump settings |
| z | Sleep |
| p | PNG screenshot (base64-encoded between `--- PNG START ---` / `--- PNG END ---` markers) |
| v | Screensaver (activate instantly, forces NORMAL mode first) |
| t | Toggle status push (real-time `!status` lines on state changes, default OFF) |
| e | Easter egg (trigger animation immediately) |
| f | Enter OTA DFU bootloader mode (writes 0xA8 to GPREGRET, resets) |
| u | Enter Serial DFU bootloader mode (writes 0x4E to GPREGRET, resets — USB CDC) |

## Status push

When enabled (via `t` command or `=statusPush:1` protocol command), the device proactively sends `!status|...` response lines on state changes:
- Keystroke sent
- Profile/mode toggle
- Mouse state transition

Guarded by 200ms minimum interval to prevent BLE stack saturation.

## Screenshot

The `p` command outputs a base64-encoded PNG of the current OLED display:
```
--- PNG START ---
iVBORw0KGgo...
--- PNG END ---
```
Decodes to a 128×64 1-bit grayscale image matching the OLED display. Works in all UI modes.

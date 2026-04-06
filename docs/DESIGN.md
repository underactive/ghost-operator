# Output & UI Conventions

Conventions for Ghost Operator's user-facing output and interface design across all platforms.

## Output principles

1. **Minimal by default.** The OLED is 128×64 pixels — every pixel matters. Show only what the user needs right now. Screensaver shows even less.
2. **Progressive detail.** NORMAL mode shows live status. MENU mode exposes all settings. Serial `d` command dumps everything.
3. **Data-driven rendering.** Menu rendering is driven by the `MENU_ITEMS[]` array — display code reads format enums, not setting internals. Adding a setting requires no display code changes.
4. **Platform-adapted.** nRF52 uses monochrome SSD1306 with Adafruit GFX. ESP32 variants use LVGL on color LCDs. Portable code stays in `src/common/`.

## Display format (nRF52 OLED)

- **128×64 monochrome** — 1-bit, SSD1306 via I2C
- **20 Hz refresh** with dirty flag gating (50ms `DISPLAY_UPDATE_MS`)
- **Shadow buffer** — 1024-byte page cache, only sends dirty page ranges (~60-75% I2C savings)
- **Screensaver** at 5 Hz (200ms `DISPLAY_UPDATE_SAVER_MS`)
- **Font:** Adafruit default 5×7, used throughout
- See [design-docs/display-layout.md](design-docs/display-layout.md) for ASCII mockups of all UI modes

## Display format (ESP32 LCD)

- **1.47" color LCD** — 172×320, ST7789 via SPI
- **LVGL** rendering with partial refresh
- Rounded UI elements, color-coded status indicators

## UI mode hierarchy

```
NORMAL ←→ MENU → SLOTS / NAME / DECOY / SCHEDULE / MODE / SET_CLOCK / CLICK_SLOTS
                  (Function button returns to MENU from all sub-modes)
                  (30s timeout returns to NORMAL from MENU, SLOTS, CLICK_SLOTS, NAME)
```

## Conventions

- Selected items rendered with inverted colors (white rect, black text)
- `< >` arrows on editable values, hidden at bounds
- Help bar at bottom of MENU scrolls if >21 chars
- Scrolling text uses pause-at-ends pattern
- Battery % always in header; charging shows lightning bolt icon
- Footer cycles: uptime / profile name (3s) / animation

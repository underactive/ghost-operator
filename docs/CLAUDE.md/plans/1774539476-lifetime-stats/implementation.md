# Implementation: Lifetime Stats (Keystrokes + Mouse Distance)

## Files Changed

- `src/config.h` — New `Stats` struct, `STATS_MAGIC`, `STATS_FILE`, `PIXELS_PER_TENTH_MILE`, new SettingId/MenuValueFormat enums, bumped `MENU_ITEM_COUNT`
- `src/state.h` — Added `stats` global, `statsDirty`, `lastStatsSave` externs
- `src/state.cpp` — Defined `stats`, `statsDirty`, `lastStatsSave`
- `src/settings.h` — Added `saveStats()`, `loadStats()` declarations
- `src/settings.cpp` — `loadStats()`/`saveStats()` with separate `/stats.dat` file, `getSettingValue`/`formatMenuValue` cases
- `src/hid.cpp` — Counter increments in `sendKeystroke()`, `sendKeyDown()`, `sendMouseMove()` (writing to `stats` struct)
- `src/keys.h` — New `MENU_IDX_TOTAL_KEYS`, `MENU_IDX_MOUSE_DIST`, shifted `MENU_IDX_VERSION`
- `src/keys.cpp` — Two new About menu items, `validateMenuIndices` entries
- `src/ghost_operator.ino` — `loadStats()` in setup, 15-minute periodic `saveStats()`, stats in `aboutItemSelected` timeout skip
- `src/sleep.cpp` — `saveStats()` before deep sleep
- `src/schedule.cpp` — `saveStats()` before light sleep

## Summary

Stats are stored in a **separate `/stats.dat` file** with their own `Stats` struct and `STATS_MAGIC`, decoupled from the `Settings` struct. This ensures stats survive firmware updates that bump `SETTINGS_MAGIC`. The `Settings` struct is unchanged — no magic bump required.

Stats are displayed as read-only menu items in the About section:
- "Keys sent" — shows raw count, or compact `Nk`/`N.NM` for large values
- "Mouse dist" — shows `N.Nmi` (tenths-of-a-mile via integer division using `PIXELS_PER_TENTH_MILE`)

No footer display was added (user preference: menu-only). No protocol changes were made.

## Verification

- Build passes clean with `make build` (SUCCESS, 190700 bytes flash / 21072 bytes RAM)
- `static_assert` validates `MENU_ITEM_COUNT` matches `MENU_ITEMS[]` array size at compile time
- `validateMenuIndices()` runtime check covers new `MENU_IDX_TOTAL_KEYS` and `MENU_IDX_MOUSE_DIST`

## Follow-ups

- Consider adding stats to `?status` protocol response for dashboard visibility
- Consider adding a "Reset stats" action item to the About section
- km display option (currently miles only)

## Audit Fixes

### Fixes Applied

1. **Comment mismatch** — Fixed "5-minute" → "15-minute" in `ghost_operator.ino` main loop comment (DX-2, Security-4)
2. **Preserve stats across loadDefaults()** — Save and restore `totalKeystrokes`/`totalMousePixels` around `memset` in `loadDefaults()`, so factory reset doesn't destroy lifetime stats (Interface-2, Security-3)
3. **Use `val` in formatMenuValue** — Changed `FMT_TOTAL_KEYS` and `FMT_MOUSE_DIST` cases to use `val` (from `getSettingValue`) instead of reading `settings.*` directly (Interface-3, DX-4)
4. **Add stats to aboutItemSelected** — Added `MENU_IDX_TOTAL_KEYS` and `MENU_IDX_MOUSE_DIST` to the 30-second timeout skip check in `ghost_operator.ino` (QA-1)
5. **Named constant for conversion** — Added `PIXELS_PER_TENTH_MILE` define to `config.h`, used in `formatMenuValue` (DX-1)
6. **Save stats on light sleep** — Added `saveSettings()` call at the top of `enterLightSleep()` in `schedule.cpp` when `statsDirty` is true (Resource-4)
7. **Saturating add for mouse pixels** — Changed `totalMousePixels +=` to saturating add in `sendMouseMove()` to prevent overflow wrap after ~71 days (QA-2, Security-1, Interface-4)

### Verification Checklist

- [x] Build passes clean after all fixes (190716 bytes flash / 21072 bytes RAM)
- [ ] Verify "Restore defaults" preserves stats counters (Keys sent and Mouse dist unchanged after restore)
- [ ] Verify stats items don't trigger 30-second menu timeout (scroll to Keys sent, wait >30s)
- [ ] Verify `totalMousePixels` saturates at UINT32_MAX instead of wrapping (long-running test)
- [ ] Verify stats are saved before light sleep entry (power-cycle during light sleep, check stats on wake)

### Unresolved Items

- **Security-2 (abs(-128) UB):** Accepted as-is. The dx/dy values from the mouse state machine are always in the -5 to +5 range. -128 is never produced.
- **Interface-1 / DX-5 (stats not in protocol):** Deferred to follow-up. Dashboard doesn't need stats in initial release.
- **DX-3 (no setSettingValue case):** Consistent with existing read-only items. Accepted as-is.

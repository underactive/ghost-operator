# Plan: Lifetime Stats (Keystrokes + Mouse Distance)

## Objective

Add persistent lifetime statistics tracking to the Ghost Operator firmware:
1. **Total keystrokes** — cumulative count of all keystrokes sent across all sessions
2. **Total mouse distance** — cumulative mouse travel distance in HID units (pixels), displayed in miles

These stats persist across sleep/wake cycles and power-offs via flash storage, and are visible in the About section of the device menu.

## Changes

### `src/config.h`
- Bump `SETTINGS_MAGIC` (struct layout change)
- Add `STATS_SAVE_INTERVAL_MS` define (15 minutes)
- Add `totalKeystrokes` (uint32_t) and `totalMousePixels` (uint32_t) to `Settings` struct before `checksum`
- Add `SET_TOTAL_KEYS` and `SET_TOTAL_MOUSE_DIST` to `SettingId` enum
- Add `FMT_TOTAL_KEYS` and `FMT_MOUSE_DIST` to `MenuValueFormat` enum
- Bump `MENU_ITEM_COUNT` from 63 to 65

### `src/state.h` / `src/state.cpp`
- Add `statsDirty` (bool) and `lastStatsSave` (unsigned long) for periodic save tracking

### `src/settings.cpp`
- Explicit zero-init in `loadDefaults()`
- No bounds check needed in `loadSettings()` (same pattern as highScore)
- Add `getSettingValue()` cases for new setting IDs
- Add `formatMenuValue()` cases: `FMT_TOTAL_KEYS` (compact: raw/k/M) and `FMT_MOUSE_DIST` (tenths-of-mile via integer division)

### `src/hid.cpp`
- Increment `totalKeystrokes` in `sendKeystroke()` (simple mode) and `sendKeyDown()` (simulation burst mode)
- Accumulate Manhattan distance (`abs(dx) + abs(dy)`) in `sendMouseMove()`
- Set `statsDirty = true` on each update

### `src/keys.h`
- Add `MENU_IDX_TOTAL_KEYS` and `MENU_IDX_MOUSE_DIST` defines
- Shift `MENU_IDX_VERSION` by +2

### `src/keys.cpp`
- Add "Keys sent" and "Mouse dist" `MENU_VALUE` items to About section (before Version)
- Add validation entries in `validateMenuIndices()`

### `src/ghost_operator.ino`
- Add 15-minute periodic stats save in main loop (separate from 5s settings debounce)
- Sync `statsDirty` flag when deferred settings save fires

## Dependencies

- Settings struct layout change requires `SETTINGS_MAGIC` bump (existing users get `loadDefaults()` reset on first boot with new firmware)
- Menu index shift (Version: 62→64) requires `MENU_IDX_VERSION` update and `MENU_ITEM_COUNT` bump

## Risks / Open Questions

- **uint32_t overflow for mouse pixels**: Max ~715 miles. At ~0.5 mi/day, lasts ~4 years. Acceptable.
- **Manhattan distance overestimates Euclidean by ~27%**: Acceptable for a fun stat — no precision requirement.
- **DPI assumption (96 DPI)**: HID units don't map directly to physical distance — host sensitivity varies. 96 DPI is a reasonable standard-screen approximation.
- **Flash wear from 15-min saves**: ~32 writes/8h day. With LittleFS wear leveling on 1MB flash, fine for years.

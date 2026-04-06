# Common Modifications

Recipes for frequently needed changes to Ghost Operator firmware.

## Version bumps

Version string appears in 8 files:
1. `src/common/config.h` — firmware `VERSION` define
2. `AGENTS.md` — project header
3. `CHANGELOG.md` — version entry
4. `README.md` — badge/header
5. `dashboard/src/App.vue` — dashboard version display
6. `docs/USER_MANUAL.md` — header (×1) + footer (×1) = 2 occurrences
7. `dashboard/package.json` — `"version"` field
8. `dashboard/package-lock.json` — `"version"` field (×2 occurrences)

**Dashboard version is kept in sync with firmware.** Always bump `App.vue`, `package.json`, and `package-lock.json` alongside the other files.

Note: `src/nrf52/ghost_operator.cpp` reads `VERSION` from `config.h` — no hardcoded string to bump there.

## Add a new keystroke option

1. Add entry to `AVAILABLE_KEYS[]` array in `src/common/keys.cpp`
2. Include HID keycode and display name
3. Set `isModifier` flag appropriately
4. Update `NUM_KEYS` in `src/common/config.h`
5. Add 3-char short name to `SHORT_NAMES[]` in `slotName()` function in `src/nrf52/display.cpp`

## Change timing range

Modify these defines in `src/common/config.h`:
```cpp
#define VALUE_MIN_MS          500UL    // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL  // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL  // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL    // 0.5 second steps
```

## Change mouse randomness

In `src/common/config.h`:
```cpp
#define RANDOMNESS_PERCENT 20  // ±20%
```

## Change mouse amplitude range

Modify `MENU_ITEMS[]` entry for `SET_MOUSE_AMP` in `src/common/keys.cpp` (minVal/maxVal currently 1-5). Only applies to Brownian mode — Bezier uses random sweep radius and ignores `mouseAmplitude`. The return phase clamps at `min(5, remaining)` per axis, so amplitudes above 5 would require updating the return logic.

## Add new menu setting

1. Add `SettingId` enum entry in `src/common/config.h`
2. Add field to `Settings` struct in `src/common/config.h` (before `checksum`)
3. Set default in `loadDefaults()` in `src/common/settings_common.cpp`, add bounds check in `loadSettings()` in `src/nrf52/settings_nrf52.cpp`
4. Add `MenuItem` entry to `MENU_ITEMS[]` array in `src/common/keys.cpp` (update `MENU_ITEM_COUNT` in `src/common/config.h`)
5. Add cases to `getSettingValue()` and `setSettingValue()` in `src/common/settings_common.cpp` — **use `clampVal()` in `setSettingValue()`** to enforce bounds
6. If the setting uses an array-indexed format (e.g., `FMT_ANIM_NAME`), add a bounds guard in `formatMenuValue()`: `(val < COUNT) ? NAMES[val] : "???"`
7. If the new item changes existing `MENU_ITEMS[]` positions, update `MENU_IDX_*` defines in `src/common/keys.h`
8. If adding a protocol `=key:value` command, add the case in `cmdSetValue()` in `src/nrf52/ble_uart.cpp`
9. `calcChecksum()` auto-adapts (loops `sizeof(Settings) - 1`)
10. Bump `SETTINGS_MAGIC` in `src/common/config.h` to trigger safe reset on existing devices

## Change device name

Configurable via menu: Device → "Device name" opens character editor (MODE_NAME). Max 14 characters, A-Z, a-z, 0-9, space, dash, underscore. Requires reboot to apply. Compile-time default:
```cpp
#define DEVICE_NAME "GhostOperator"
```

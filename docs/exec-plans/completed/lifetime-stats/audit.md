# Audit: Lifetime Stats (Keystrokes + Mouse Distance)

## Files Changed

Findings flagged in the following files:
- `src/config.h`
- `src/settings.cpp`
- `src/hid.cpp`
- `src/keys.h`
- `src/keys.cpp`
- `src/ghost_operator.ino`
- `src/schedule.cpp` (immediate dependent)

---

## 1. QA Audit

### [FIXED] QA-1 (Low): `aboutItemSelected` timeout skip missing new stats items
**File:** `src/ghost_operator.ino:423-424`
The 30-second auto-return timeout skip for About section items did not include `MENU_IDX_TOTAL_KEYS` or `MENU_IDX_MOUSE_DIST`. User could be kicked out of menu while viewing stats.

### [FIXED] QA-2 (Medium): `totalMousePixels` overflow with no saturation
**File:** `src/hid.cpp:85`
`uint32_t` overflow reachable after ~71 days of continuous Bezier mouse movement. Counter would silently wrap to 0.

### QA-3 (Info): `loadDefaults()` resets lifetime stats
**File:** `src/settings.cpp:93-94`
See Interface-2 below — fixed by preserving stats across loadDefaults().

---

## 2. Security Audit

### Security-1 (Low): Integer overflow on `totalMousePixels`
Same as QA-2 — fixed with saturating add.

### Security-2 (Info): `abs()` on `int8_t` — theoretical UB for -128
**File:** `src/hid.cpp:85`
`abs(-128)` is undefined behavior. Not reachable in practice (dx/dy values are always -5 to +5). Accepted as-is.

### [FIXED] Security-3 (Info): `loadDefaults()` destroys lifetime stats
Same as Interface-2 — fixed.

### [FIXED] Security-4 (Info): Comment/code mismatch on save interval
**File:** `src/ghost_operator.ino:633`
Comment said "5-minute" but actual interval is 15 minutes.

---

## 3. Interface Contract Audit

### Interface-1 (Medium): Stats not exposed in `?settings` protocol response
**File:** `src/ble_uart.cpp:221-278`
Dashboard cannot read stats via protocol. Accepted as follow-up — not needed for initial release.

### [FIXED] Interface-2 (High): `!defaults` destroys lifetime stats
**File:** `src/settings.cpp:26-94`
`loadDefaults()` zeroed cumulative stats. Fixed by preserving `totalKeystrokes` and `totalMousePixels` across the `memset` and restore cycle.

### [FIXED] Interface-3 (Low): `FMT_TOTAL_KEYS`/`FMT_MOUSE_DIST` bypass `getSettingValue()` return value
**File:** `src/settings.cpp:427,442`
Format cases read `settings.*` directly instead of using `val`. Fixed to use `val` for consistency.

### Interface-4 (Low): `uint32_t` overflow for mouse distance
Same as QA-2 — fixed with saturating add.

---

## 4. State Management Audit

### State-1 (No Issue): Direct struct mutation is consistent with project convention
`settings.totalKeystrokes++` follows the same pattern as game high score updates in `breakout.cpp`, `snake.cpp`, `racer.cpp`.

### State-2 (No Issue): No concurrent writer hazard
All stat mutations occur in main loop context only. No ISR or BLE callback touches these fields.

### State-3 (No Issue): `statsDirty` flag interaction with `settingsDirty` is correct
The deferred settings save correctly clears `statsDirty` and resets `lastStatsSave`.

---

## 5. Resource & Concurrency Audit

### Resource-1 (No Issue): `totalKeystrokes++` is main-loop-only; no ISR races
Verified all callers are in `loop()` context.

### Resource-2 (No Issue): `statsDirty` is single-context; `volatile` not needed

### Resource-3 (Acceptable): Flash wear within budget
15-min interval = ~35K writes/year. Within LittleFS wear leveling budget.

### [FIXED] Resource-4 (Low): Light sleep does not save dirty stats
**File:** `src/schedule.cpp:119`
Up to 15 minutes of stats could be lost on power-cycle during light sleep. Fixed by adding `saveSettings()` call at light sleep entry.

### Resource-5 (No Issue): `millis()` overflow handled correctly

---

## 6. Testing Coverage Audit

Recommended 23 test checklist items covering: stats display in About menu, persistence across sleep/wake, periodic save behavior, counter accuracy, reset defaults interaction, and settings magic migration. See testing checklist for added items.

---

## 7. DX & Maintainability Audit

### [FIXED] DX-1 (Low): Magic number `608256UL` should be named constant
**File:** `src/settings.cpp:443`
Added `PIXELS_PER_TENTH_MILE` define to `src/config.h`.

### [FIXED] DX-2 (Medium): Comment says "5-minute" but interval is 15 minutes
Same as Security-4 — fixed.

### DX-3 (Low): No `setSettingValue` case for stats (intentional, undocumented)
Consistent with existing read-only items (SET_VERSION, SET_UPTIME, SET_DIE_TEMP). Accepted as-is.

### [FIXED] DX-4 (Low): `FMT_TOTAL_KEYS`/`FMT_MOUSE_DIST` bypass `val`
Same as Interface-3 — fixed.

### DX-5 (Low): Stats not in protocol responses
Same as Interface-1 — accepted as follow-up.

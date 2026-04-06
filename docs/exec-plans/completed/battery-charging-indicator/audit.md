# Audit: Battery Charging Indicator + 2000mAh Spec Update

## Files Changed
- `CLAUDE.md` — battery spec 1000mAh → 2000mAh
- `README.md` — battery spec + runtime estimate
- `docs/USER_MANUAL.md` — battery spec + runtime estimate
- `docs/schematic_interactive_v3.html` — BOM table battery spec
- `src/config.h` — added `PIN_CHG_STATUS` define
- `src/state.h` — added `extern bool batteryCharging`
- `src/state.cpp` — added `bool batteryCharging = false`
- `src/battery.cpp` — no net change (charging read added then moved to main loop)
- `src/ghost_operator.ino` — `pinMode` in `setupPins()`, fast charging poll in `loop()`
- `src/icons.h` — declared `chargingIcon`
- `src/icons.cpp` — added 5x8 lightning bolt bitmap
- `src/display.cpp` — 8 battery display locations updated (4 headers with icon, 4 compact with `+` prefix)

---

## 1. QA Audit
**No issues found.** Pin mapping, buffer sizes, layout math, and edge-detection polling are all correct. `digitalRead` is ~1µs with compare-before-write pattern avoiding unnecessary `markDisplayDirty()` calls.

## 2. Security Audit
**No issues found.** No injection surface (hardware GPIO only), all `snprintf` with `sizeof`, worst-case `"+100%"` fits in `batBuf[8]`, bitmap dimensions match `drawBitmap` calls. No dynamic allocation.

## 3. Interface Contract Audit
**1 finding (low):** `?status` protocol response does not include `batteryCharging`. The display shows charging state but protocol consumers cannot query it. Not a defect — potential follow-up if dashboard needs it.

## 4. State Management Audit
**No issues found.** Single writer (`loop()`), single canonical source (`state.h/cpp`), compare-before-write with dirty flag. No ISR/callback access. Self-corrects on wake from sleep.

## 5. Resource & Concurrency Audit
**No actionable issues.** One theoretical observation: `INPUT_PULLUP` on `~CHG` during deep sleep could draw ~254µA if charging. Self-dismissed because charging implies USB power is present (battery not draining).

## 6. Testing Coverage Audit
**1 finding:** `docs/CLAUDE.md/testing-checklist.md` needs new test items for the charging indicator feature. [FIXED] — 6 test items added.

## 7. DX & Maintainability Audit
**4 findings:**

### 7a. Duplicated header battery+icon rendering (moderate)
`src/display.cpp` lines 467-488, 746-764, 2317-2336, 2671-2690 — four near-identical ~20-line blocks computing `chgW`, `btX`, drawing BT/USB icon, charging icon, and battery text. The charging feature added two new concerns (`chgW` and `chargingIcon` draw) that must remain consistent across all four copies. Pre-existing structural issue amplified by this change. Not addressed — extracting a helper is refactoring beyond the scope of this feature.

### 7b. Inconsistent `btX` gap calculation in Volume header (low)
`src/display.cpp:2323` uses `batX - chgW - 8` while the other three headers use `batX - chgW - 5 - 3`. Numerically equivalent but the self-documenting form is preferred. [FIXED]

### 7c. Magic number `7` without named constant (low)
The value `7` (5px icon + 2px gap) appears 8 times across the charging code. Only the first occurrence at line 472 has a comment explaining the derivation. [FIXED] — added inline comments to all occurrences.

### 7d. `batteryCharging` not in status protocol (low)
Duplicate of Interface Contract finding §3. Not addressed — potential follow-up.

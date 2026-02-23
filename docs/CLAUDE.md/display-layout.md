# Display Layout

## Normal Mode
```
GHOST Operator          ᛒ 85%
─────────────────────────────
KB [F16] 1.7-5.5s        ↑     ← effective (profile-adjusted) range
████████████░░░░░░░░░░░  3.2s
─────────────────────────────
MS [MOV]  17s/25s          ↑   ← effective durations
██████░░░░░░░░░░░░░░░░░  8.5s
─────────────────────────────
BUSY                  ~^~_~^~  ← profile name (3s) or "Up: 2h34m"; animation on right
```
`KB [F16]` shows the pre-picked next key. Changes after each keypress.
`[MOV]` = moving, `[IDL]` = idle, `[RTN]` = returning to origin (progress bar at 0%).
Footer shows profile name for 3 seconds after switching, then reverts to uptime (compact format: `2h34m`, `1d5h`, `45s` — seconds hidden above 1 day). Status animation plays on the right side of the footer (configurable: ECG, EQ, Ghost, Matrix, Radar, None; default Ghost). Animation speed is activity-aware: full speed when both KB and mouse are enabled, half speed when one is muted, frozen when both are muted.

## Menu Mode
```
MENU                    ᛒ 85%
──────────────────────────────
      - Keyboard -               ← heading (not selectable)
▌Key min        < 2.0s >  ▐     ← selected (inverted row)
 Key max          < 6.5s >       ← value item with arrows
 Key slots               >      ← action item
      - Mouse -
──────────────────────────────
Minimum delay between keys       ← help bar (scrolls if long)
```
5-row scrollable viewport. Headings centered. Selected row inverted.
Editing inverts only value portion with `< >` arrows. Arrows hidden at bounds.
`FMT_PERCENT_NEG` items have inverted encoder direction and swapped arrow bounds.

## Slots Mode
```
MODE: SLOTS             [3/8]
─────────────────────────────
  F15 F14 --- ---
  --- --- --- ---
─────────────────────────────
Turn=key  Press=slot
Func=back
```
Active slot rendered with inverted colors (white rect, black text).

## Name Mode
```
DEVICE NAME          [3/14]
──────────────────────────────
  G h o s t O p
  e r a t o r · ·
──────────────────────────────
Turn=char  Press=next
Func=save
```
Active position rendered with inverted colors. END positions shown as `·`.
On save, if name changed, shows reboot confirmation prompt with Yes/No selector.

## Screensaver Mode (overlay)
```
                              (blank)

             [F16]            ← next key label, centered
  ████████████████░░░░░░░░░░  ← 1px high KB progress bar (full width)

             [IDL]            ← mouse state, centered
  ██████░░░░░░░░░░░░░░░░░░░░  ← 1px high MS progress bar (full width)

              12%             ← battery % (ONLY if <15%)
                              (blank)
```

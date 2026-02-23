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

## Screensaver Mode — Simple (overlay)
```
                              (blank)

             [F16]            ← next key label, centered
  ████████████████░░░░░░░░░░  ← 1px high KB progress bar (full width)

             [IDL]            ← mouse state, centered
  ██████░░░░░░░░░░░░░░░░░░░░  ← 1px high MS progress bar (full width)

              12%             ← battery % (ONLY if <15%)
                              (blank)
```

## Simulation Normal Mode
```
Staff                  ᛒ 85%
─────────────────────────────
Morning Email   ▓▓▓░░░ 12:34  ← block name + inline bar + time remaining
Email Compose   ▓▓░░░░  4:21  ← work mode name + inline bar + time
Normal          ▓▓▓▓░░  2:15  ← auto-profile stint + inline bar + time
─────────────────────────────
KBD ▓▓▓▓▓▓▓▓▓▓▓░░░░░░░  3.2s  ← activity phase + outlined bar + time
─────────────────────────────
10:34 AM       ⌨  🖱  ~^~_~^~  ← clock (or uptime) + keycap + mouse + animation
```
4-row hierarchy: block → mode → profile stint → activity. Each of the top 3 rows has a scrolling name on the left (10 chars max visible, auto-scrolls if longer) and an inline 5px-tall progress bar + countdown on the right. Activity row uses a 7px-tall outlined bar spanning x=21..96.

Footer shows synced clock (or uptime if not synced). Keycap and mouse are 10×10 pixel art bitmaps with state feedback: keycap shrinks when key held (pressed state), mouse shows click/scroll/nudge variants. Animation plays in the right corner. Mute button (SW2) cycles footer between clock/uptime/version/die temp.

Schedule preview overlay: encoder rotation in normal mode shows a centered overlay box with upcoming block names (up to 2), displayed for 3 seconds.

## Simulation Screensaver (overlay)
```
                              (blank)

        ╔═══╗    ╭───╮
        ║   ║    │ ○ │       ← 3× scaled keycap + mouse icons, centered
        ║   ║    │   │
        ╚═══╝    ╰───╯

              12%             ← battery % (ONLY if <15%)
                              (blank)
```
3× scaled (30×30px) centered keycap and mouse pixel art icons. Keycap shows pressed state when key is held (with 200ms visual hold for visibility at 5 Hz refresh). Mouse shows click/scroll/nudge states. Icons are hidden when their respective output is muted. Battery warning only if <15%.

## Mode Picker (MODE_MODE)
### Selection Screen
```
DEVICE MODE             ᛒ 85%
─────────────────────────────
▌Simple               ▐      ← selected (inverted row)
  Direct timing               ← description
Simulation
  Human work patterns
──────────────────────────────
Turn=select Press=OK
Func=back
```
Two options with descriptions. Encoder navigates, button confirms. Function button returns to menu.

### Reboot Confirmation
```
MODE CHANGE
─────────────────────────────
        "Simulation"          ← new mode name in quotes, centered

     Reboot to apply?

     ▌Yes▐     No            ← encoder toggles, button confirms
─────────────────────────────
Turn=select Press=OK
```
Yes highlighted by default. Selecting Yes triggers `NVIC_SystemReset()`. Selecting No returns to mode picker.

## Two-Stage Sleep Confirmation (overlay)
```
                              (blank)
     Hold to sleep...         ← centered prompt

     Light        Deep        ← segment labels
  ▓▓▓▓▓▓▓▓▓▓▓▓░░ ▓▓▓▓░░░░░░  4.2s  ← two 48px segments + 2px gap + time
                              (blank)
  Release = light sleep       ← dynamic hint (changes at 3s midpoint)
```
Segmented bar: first 48px segment fills over 0–3s ("Light"), second 48px segment fills over 3–6s ("Deep"). Before 3s the hint says "Release to cancel"; after 3s it changes to "Release = light sleep". Time remaining shown at x=102.

## Light Sleep Breathing Screen
```
                              (blank)
                              (blank)
                              (blank)
                              (blank)
                              (blank)
                              (blank)
                         ●    ← breathing circle, lower-right (x=118, y=54)
                              (blank)
```
Entire screen is blank except for a single breathing circle in the lower-right corner. Circle radius oscillates 2→6px on a 4-second sinusoidal cycle (`sin(2π × t/4s)`), evoking a MacBook sleep LED. Used during manual light sleep (released between 3–6s on sleep hold) and scheduled light sleep.

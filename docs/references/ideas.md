# Ghost Operator — Future Ideas

Brainstormed ideas for future firmware features. The XIAO nRF52840 has plenty of headroom (~700KB flash, ~200KB RAM free after SoftDevice + current firmware). Pins D7-D10 are unused.

---

## Practical

### Auto-Schedule (RTC Wake/Sleep)
Automatic start/stop based on time of day. The nRF52840's RTC2 peripheral is free (RTC0/RTC1 owned by SoftDevice and FreeRTOS) and can wake from `sd_power_system_off()`. Device self-wakes at e.g. 9:00 AM, self-sleeps at 5:00 PM — zero battery drain overnight. Would need a simple time-set UI via encoder or dashboard sync.

### Session Timer
"Run for N hours then auto-sleep." Simple countdown with OLED display. Useful for meetings or known work blocks. Could show remaining time in the footer area where uptime currently displays.

### Activity Statistics
Track lifetime stats in flash: total keystrokes sent, mouse movements, uptime hours, sleep/wake cycles, connection count. Show a dedicated stats screen (new UIMode or overlay). Dashboard could chart session history over time. LittleFS has room for a small log file.

### Text Macros
Store 2-4 predefined strings and type them via HID on a key combo (e.g. encoder long-press in NORMAL mode). Turns the device into a mini macro pad. Useful for typing email addresses, boilerplate text, or passwords (with appropriate security warnings). Configurable via dashboard.

### Media Keys (Volume Knob)
Add HID Consumer Control descriptor for media keys (play/pause, volume up/down, next/prev track). Encoder becomes a volume knob in a dedicated mode. TinyUSB and Bluefruit both support consumer control reports. Minimal code — just a new UIMode and a few HID report sends.

---

## Clever

### Host OS Detection
Fingerprint connected host as Mac/Windows/Linux from BLE connection parameters or USB descriptors. Auto-select keys that work best per OS (e.g. avoid F13-F15 on some Linux DEs, prefer media keys on Mac). Could also adjust mouse movement characteristics.

### Decoy BLE Identity
Let the user pick a device name from a curated list of real peripherals ("Logitech M720 Triathlon", "Apple Magic Keyboard", "Dell WM615") so it blends into corporate Bluetooth scans. Just a name preset picker — the device already supports custom names.

### Keystroke Humanization
Variable key hold duration (currently instant press/release) and slight randomization of the release timing. More realistic to activity monitoring software that looks for perfectly uniform input patterns. Could add configurable "human-ness" level (0-100%).

### Keystroke Bursting
Currently Ghost Operator sends a single keypress at a random interval between KEY_MIN and KEY_MAX — a steady drip of one key every few seconds. Monitoring tools that track keystrokes-per-interval see a flat, uniform count: ~5-10 keys every 10 minutes, hour after hour. Real typing is nothing like this. A human writes an email (burst of 80 keys in 30 seconds), reads for 2 minutes (zero keys), replies to Slack (burst of 40 keys in 15 seconds), then browses for 5 minutes (zero keys). The signature is clusters separated by silence.

**Approach:** Replace the steady single-key timer with a burst scheduler. Periodically fire a rapid cluster of 5-30 keypresses over 2-10 seconds (randomized count and duration, with randomized inter-key delay of 50-200ms to mimic typing speed). Then go silent for a longer gap (30s-3min) before the next burst. The burst/gap ratio and intensity could be tied to the active profile — BUSY produces longer, more frequent bursts; LAZY produces short, rare bursts with longer silences. Keys within a burst still draw from the configured slots (F13-F24, etc.) so they remain invisible to the host OS.

**Why this matters:** Hubstaff and similar tools report "seconds with keyboard input" per interval. A burst of 20 keys over 5 seconds registers as 5 active seconds — identical to a real typing session. The current steady drip of one key every 3-6 seconds registers as 1 active second per key, spread uniformly — a pattern no human produces.

**Implementation cost:** ~2-3KB flash. Replaces the single-key scheduling logic in `loop()` with a burst state machine (IDLE → BURSTING → COOLDOWN). Reuses existing `dualKeyboardReport()` and key slot infrastructure.

### Phantom Clicks (Random Middle-Click Injection)
Employee monitoring tools (Hubstaff, ActivTrak, etc.) track mouse clicks as a single aggregate count per interval — they do not differentiate between left, right, middle, or side buttons. A device that moves the mouse but never clicks produces a movement-to-click ratio of infinity, which is unlike any real user. Injecting random middle clicks during mouse jiggle phases fills this gap.

**Why middle click:** Left clicks are dangerous — they can dismiss dialogs, activate buttons, move the text cursor, or select files. Right clicks leave visible context menus on screen (caught by screenshot monitoring). Side buttons (4/5) trigger browser back/forward navigation. Middle click on a desktop or empty area is effectively a no-op on all major OSes (macOS, Windows, Linux). In a browser it triggers auto-scroll, which is benign and self-dismisses on the next mouse move — which the jiggler is already sending.

**Approach:** During `MOUSE_JIGGLING`, inject middle-click HID reports at random intervals (e.g. every 15-90 seconds, randomized). Send press + release with a short randomized hold duration (50-150ms) to mimic a real click. Reuses `sendMouseClick()` or a new helper alongside existing `sendMouseMove()`. Menu toggle: "Phantom clicks: Off/On" (default Off). Pairs well with Keystroke Humanization and Activity Burst Simulation for a comprehensive anti-monitoring profile.

**Implementation cost:** ~1-2KB flash. Minimal — just a timer check inside the existing mouse state machine and a single HID report send.

### Window Switching (Random Alt-Tab / Cmd-Tab)
Monitoring tools like Hubstaff track **active window title** and **app switches** per interval. A jiggler that keeps the mouse moving and keys tapping but never changes the foreground window is a dead giveaway — real users switch apps constantly (IDE → browser → Slack → terminal → back). Periodic Alt-Tab (Windows/Linux) or Cmd-Tab (macOS) keypresses cycle through open windows, producing the app-switching pattern managers expect to see on the activity dashboard.

**Why this is the hardest to fake safely:** Unlike invisible F-keys or middle clicks, Alt-Tab has *real visible side effects* — it changes which window is in front. This is both the point (it makes screenshots look varied) and the risk (it could pull focus from a fullscreen presentation, dismiss a modal, or interrupt the user if they're actually at the keyboard). Must be treated as a high-risk feature with strong guardrails.

**Approach:** Send Alt+Tab (or Cmd+Tab on macOS — requires Host OS Detection or a manual toggle) at random intervals during burst phases. Safeguards needed:
- **Default Off** with prominent warning in menu/dashboard ("This switches your active window")
- **Randomized interval** (every 3-15 minutes) so the switching pattern isn't uniform
- **Single tab only** — press and release Alt+Tab once (switches to previous window). Never hold Alt and tap Tab multiple times (window picker UI would be visible in screenshots)
- **Pairs with Activity Burst Simulation** — only fire during NORMAL/BUSY phases, never during LAZY or idle gaps
- **Auto-disable when user is active** — if real HID input is detected from the host (requires BLE HID input reports or USB SOF frame heuristics), suppress Alt-Tab to avoid fighting the user

**Host OS consideration:** Alt+Tab vs Cmd+Tab. Could auto-detect via Host OS Detection (separate idea), or add a menu setting: "Host OS: Windows/Mac/Linux" that selects the correct modifier key. Wrong modifier is harmless (just an unrecognized shortcut) but wastes the switch opportunity.

**Implementation cost:** ~2-3KB flash. A single HID keyboard report with modifier + keycode. The complexity is in the guardrails and timing logic, not the HID send.

### Activity Burst Simulation (Auto-Profile Cycling)
Employee monitoring tools (Hubstaff, ActivTrak, Teramind) calculate activity % per interval and display it as a timeline to managers. A jiggler produces a flat ~20-30% activity level for hours — obviously inhuman. Real users are bursty: 90% for 15 minutes (typing an email), then 5% for 10 minutes (reading), then 60% for 20 minutes (coding). Auto-cycle between LAZY/NORMAL/BUSY profiles at random intervals to simulate natural work rhythm. The profile infrastructure already exists — this just automates the encoder twist.

**Approach:** A background timer picks a random profile every 5-30 minutes (randomized per cycle). Weight distribution configurable — e.g. 50% NORMAL, 30% BUSY, 20% LAZY. Optionally insert "idle gaps" (both KB and mouse muted for 1-5 minutes) to simulate reading, meetings, or coffee breaks. These gaps are what make the activity chart look human — no real person has zero idle periods across 8 hours. Could add a menu toggle: "Auto profile: Off/On" (default Off to preserve current manual behavior). When enabled, manual encoder profile switching is suppressed. Dashboard could show a "simulate workday" preset.

**Implementation cost:** ~3-4KB flash. Reuses existing `applyProfile()` and mute infrastructure. Needs one RTC-based or millis timer, a weighted random picker, and a menu toggle.

### BLE Proximity Auto-Lock
Use RSSI (signal strength) of the connected host to estimate distance. Auto-sleep when RSSI drops below threshold (user walks away), auto-wake on return. The BLE stack already provides RSSI — just needs threshold logic and hysteresis to avoid flapping.

### Multi-Host Switching
S140 SoftDevice supports multiple bonds. Store 2-3 paired hosts, switch between them via encoder or menu. Each host could have its own profile/timing settings. Useful for people with work laptop + personal machine.

---

## Fun

### OLED Games
128x64 OLED + encoder + button = enough for simple games. Already precedent with the Pac-Man easter egg.
- **Snake** — Classic. Encoder for direction, button to start/pause.
- **Breakout** — Encoder moves paddle, ball physics on the 128x64 grid.
- **Flappy Bird** — Button to flap, encoder unused. Tiny bird, pipe obstacles.
- **Pong** — Single player vs CPU. Encoder moves paddle.
- **Tetris** — Encoder rotates/moves, button drops. 10-wide grid fits in 128px.

### Pomodoro Timer
25-minute work / 5-minute break cycle with OLED visualization. Show progress ring or bar, break notification. Could flash the OLED or blink D6 LED on transitions. Pause/resume with encoder button. The device is already sitting on your desk — might as well be useful during breaks.

### Internal Temperature
The nRF52840 has a built-in TEMP peripheral (die temperature sensor, ±5C accuracy). Display on the status screen or in the menu. Not super accurate for ambient, but fun and free — no additional hardware.

### Morse Code Easter Egg
Input text via button taps in morse code. Dot = short press, dash = long press. Decoded on OLED in real-time. Pure novelty but demonstrates the input system.

---

## Hardware Add-Ons (D7-D10 Available)

### RGB Status LED (D7)
Already in the original ideas list. WS2812B or similar on D7. Color-coded status: green = active, blue = BLE connected, red = low battery, purple = DFU mode. NeoPixel library works on nRF52.

### Piezo Buzzer (D8)
PWM-driven buzzer for audio feedback. Click sounds on encoder rotation, mode change confirmation tones, low battery warning beep. Can be muted via menu setting.

### NFC Tap-to-Toggle
The XIAO nRF52840 has NFC antenna pads (NFC1/NFC2 on the bottom). With a small NFC antenna coil, could support tap-to-toggle: tap phone to start/stop jiggling, or tap to open the dashboard URL. Requires enabling NFCT peripheral (disabled by default on Seeed core — pins repurposed as GPIO).

### External Button / Foot Pedal (D9)
Additional GPIO input for a foot pedal or desk-mounted button. Macro trigger, push-to-talk, or instant sleep toggle without reaching for the device.

---

## Architecture Enhancements

### Settings Profiles
Save/restore complete settings snapshots (not just timing profiles). "Work" profile with aggressive timing + F16, "Home" profile with lazy timing + media keys. Store 2-3 named profiles in flash, switch via menu.

### Dashboard Improvements
- Live activity graph (keystrokes/minute over time)
- Settings import/export (JSON backup)
- Connection log viewer
- Battery history chart
- Multi-device management (if multi-host implemented)

### Power Optimization
- Track actual battery discharge curve per profile for better % estimation
- Adaptive polling rate — slower display updates when screensaver active
- BLE connection interval negotiation for lower power when idle

---

*Estimated flash cost per feature: games ~10-20KB each, auto-schedule ~3-5KB, media keys ~2-3KB, stats logging ~5-8KB, text macros ~4-6KB. Total budget: ~700KB available.*

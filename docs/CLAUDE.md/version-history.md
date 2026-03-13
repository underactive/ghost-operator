# Version History

| Ver | Changes |
|-----|---------|
| 2.4.0 | Build system migration: Arduino CLI → PlatformIO. Firmware sources moved to `src/`, custom board JSON (`boards/`), declarative `platformio.ini`, CI simplified (no checkout path hack), `build.sh` rewritten with 1200bps DFU touch, `Makefile` gains `monitor`/`clean` targets. Key PlatformIO gotchas documented: `lib_archive=no` (TinyUSB driver stripping), `-Os` over `-Ofast`, CC310 crypto path fix, board define injection. Charging indicator (lightning bolt icon via BQ25100 `~CHG` pin). Battery spec updated to 2000mAh (~120h runtime). |
| 2.3.2 | Codebase audit fixes (45 findings): ISR safety for BLE callbacks and volatile globals, display dirty-flag race fix, modifier key bug fix, encoder TOCTOU fix, OperationMode enum replacing ~50 magic numbers, game macro rename (brk→gBrk ~248 sites), drawScrollingText() extraction, bounds hardening on 11+ lookups, dashboard parse/export completeness, serial command FIFO queue, DFU lock guard |
| 2.3.1 | Simulation tuning dashboard (per-mode sliders for profile weights, phase timing, durations), sim tuning backup/restore in settings export (version 2 format), compressed job performance curve (level 11 → 1.4× max), slider responsiveness fix, stack overflow fix on sim data save, unified save flow |
| 2.3.0 | 7-slot click action system (7 configurable phantom click slots with Wheel Up/Down), expanded function keys (F1–F20), redesigned pixel art icons, clearer menu labels, Snake game mode, game-specific boot splashes, configurable shift/lunch duration (dashboard-only), dashboard settings export/import (JSON backup/restore), work mode timer fix, K+M/M+K phase bug fixes, Breakout mode hardening |
| 2.2.2 | Configurable Volume Control buttons (D2/D7 actions via menu + protocol), lunch enforcement at 4h mark with force-jump, keystroke keepalive floor (2-min gap), unified D3 function button, carousel callback flash→RAM fix, orchestrator time sync fix, keepalive sound suppression + HID hold time, D7 click state reset on screensaver/sleep |
| 2.2.1 | Carousel settings page (9 named-option settings as full-screen carousels), system sounds (BLE connect/disconnect alerts), dashboard Bluetooth transport, carousel snap-on-open, volatile ISR state fix, sleep timeout safety, heap fragmentation elimination (ResponseWriter zero-alloc, String removal in protocol handlers), array bounds hardening |
| 2.2.0 | Volume Control operation mode (BLE/USB media controller with 3 display themes: Basic segmented bar, Retro VU meter, Futuristic slider), mode picker horizontal carousel with smooth scrolling, mode-specific white-background splash screens, HID consumer control report (RID_CONSUMER), menu 45→47 items (+Volume heading/Theme) |
| 2.1.0 | Piezo buzzer keyboard sounds (5 profiles with live preview), job performance scaling (0–11), job start time (decoupled from schedule), manual clock setting mode (MODE_SET_CLOCK), display optimization (dirty flag + shadow buffer + 20 Hz refresh), dashboard sound section |
| 2.0.0 | Simulation mode (keystroke bursting, mutual KB/mouse exclusion, phantom clicks, window switching, job-specific day schedules), mute button (D7), hardware watchdog (WDT), two-stage sleep (light at 3s / deep at 6s), click type setting (Middle/Left), mode picker UI (MODE_MODE), pixel art keycap/mouse icons, footer info cycling, dashboard simulation support, heap fragmentation elimination, Host OS → Switch Keys simplification, `-Wall` build warnings resolved |
| 1.10.1 | LiPo discharge curve, BLE idle power management, die temperature, dashboard battery chart, protocol hardening |
| 1.10.0 | BLE identity presets (decoy masquerade), timed schedule (auto-sleep / full auto), schedule editor UI |
| 1.9.1 | Dashboard smart default (On, auto-off after 3 boots), USB descriptor customization, deferred flash save |
| 1.9.0 | USB HID wired mode, BT while USB, scroll wheel, build automation, real-time dashboard |
| 1.8.2 | Mute indicator on progress bars, custom name in header, narrower screensaver bars with end caps |
| 1.8.1 | Pac-Man easter egg redesign: power pellet narrative with tunnel transition |
| 1.8.0 | Mouse movement styles (Bezier/Brownian), compact uptime, activity-aware animation |
| 1.7.2 | Web Serial DFU, dashboard switched from BLE to USB serial |
| 1.7.1 | OTA DFU mode (`!dfu` command, OLED DFU screen) — via nRF Connect mobile |
| 1.7.0 | BLE UART config protocol (NUS), Vue 3 web dashboard (USB serial) |
| 1.6.0 | Modular codebase (15 module pairs), configurable status animation (6 styles), Display/Device menu split |
| 1.5.0 | Adjustable mouse amplitude (1-5px), inertial ease-in-out mouse movement, reset defaults |
| 1.4.0 | Scrollable settings menu, display brightness, data-driven menu architecture |
| 1.3.1 | Fix encoder unresponsive after boot, hybrid ISR+polling, bitmap splash |
| 1.3.0 | Screensaver mode for OLED burn-in prevention |
| 1.2.1 | Fix encoder initial state sync bug |
| 1.2.0 | Multi-key slots, timing profiles (LAZY/NORMAL/BUSY), SLOTS mode |
| 1.1.1 | Icon-based status, ECG pulse, KB/MS combo cycling |
| 1.1.0 | Display overhaul, BT icon, HID keycode fix |
| 1.0.0 | Initial hardware release - encoder menu, flash storage, BLE HID |

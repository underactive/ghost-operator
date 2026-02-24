# Version History

| Ver | Changes |
|-----|---------|
| 2.1.0 | Piezo buzzer keyboard sounds (5 profiles with live preview), job performance scaling (0–11), job start time (decoupled from schedule), manual clock setting mode (MODE_SET_CLOCK), display optimization (dirty flag + shadow buffer + 20 Hz refresh), dashboard sound section |
| 2.0.0 | Simulation mode (keystroke bursting, mutual KB/mouse exclusion, phantom clicks, window switching, job-specific day schedules), mute button (D7), hardware watchdog (WDT), two-stage sleep (light at 3s / deep at 6s), click type setting (Middle/Left), mode picker UI (MODE_MODE), pixel art keycap/mouse icons, footer info cycling, dashboard simulation support, heap fragmentation elimination, Host OS → Switch Keys simplification, `-Wall` build warnings resolved |
| 1.10.1 | LiPo discharge curve, BLE idle power management, die temperature, dashboard battery chart, protocol hardening |
| 1.10.0 | BLE identity presets (decoy masquerade), timed schedule (auto-sleep / full auto), schedule editor UI |
| 1.9.1 | Dashboard smart default (On, auto-off after 3 boots), USB descriptor customization, deferred flash save |
| 1.9.0 | USB HID wired mode, BT while USB, scroll wheel, build automation, real-time dashboard |
| 1.0.0 | Initial hardware release - encoder menu, flash storage, BLE HID |
| 1.1.0 | Display overhaul, BT icon, HID keycode fix |
| 1.1.1 | Icon-based status, ECG pulse, KB/MS combo cycling |
| 1.2.0 | Multi-key slots, timing profiles (LAZY/NORMAL/BUSY), SLOTS mode |
| 1.2.1 | Fix encoder initial state sync bug |
| 1.3.0 | Screensaver mode for OLED burn-in prevention |
| 1.3.1 | Fix encoder unresponsive after boot, hybrid ISR+polling, bitmap splash |
| 1.4.0 | Scrollable settings menu, display brightness, data-driven menu architecture |
| 1.5.0 | Adjustable mouse amplitude (1-5px), inertial ease-in-out mouse movement, reset defaults |
| 1.8.2 | Mute indicator on progress bars, custom name in header, narrower screensaver bars with end caps |
| 1.8.1 | Pac-Man easter egg redesign: power pellet narrative with tunnel transition |
| 1.8.0 | Mouse movement styles (Bezier/Brownian), compact uptime, activity-aware animation |
| 1.7.2 | Web Serial DFU, dashboard switched from BLE to USB serial |
| 1.7.1 | OTA DFU mode (`!dfu` command, OLED DFU screen) — via nRF Connect mobile |
| 1.7.0 | BLE UART config protocol (NUS), Vue 3 web dashboard (USB serial) |
| 1.6.0 | Modular codebase (15 module pairs), configurable status animation (6 styles), Display/Device menu split |

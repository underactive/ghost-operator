# Ghost Operator — Project History

This document traces the project's development for agent context.

## Timeline at a glance

| Date | Phase | Milestone |
|------|-------|-----------|
| 2024-12 | Phase 0 | Initial hardware release — encoder menu, flash storage, BLE HID (v1.0.0) |
| 2025-01 | Phase 1 | Display overhaul, multi-key slots, timing profiles, screensaver (v1.1–v1.3) |
| 2025-01 | Phase 2 | Mouse styles, device name editor, DFU firmware updates (v1.4–v1.7) |
| 2025-02 | Phase 3 | USB HID wired mode, BLE identity presets, timed schedule (v1.8–v1.10) |
| 2025-02 | Phase 4 | Simulation mode, mute button, 2-stage sleep, games (v2.0–v2.1) |
| 2025-03 | Phase 5 | Volume Control mode, system sounds, carousel UI, click actions (v2.2–v2.3) |
| 2025-03 | Phase 6 | PlatformIO migration, lifetime stats, codebase audit (v2.4–v2.5) |
| 2025-04 | Phase 7 | Multi-platform: ESP32-C6 + ESP32-S3 LCD ports, JSON protocol, USB serial DFU (v2.5.3–v2.5.7) |

## Version history

| Ver | Changes |
|-----|---------|
| 2.5.7 | Fix ESP32-S3/C6 dashboard board guards for S3; fix display flip not persisting across reboots; fix brightness slider having no effect. |
| 2.5.6 | Fix ESP32-S3 USB serial enumeration — add missing `ARDUINO_USB_MODE=1` build flag. |
| 2.5.5 | README rewrite for ESP32-primary positioning. Docs migrated to AGENTS.md-harness pattern. |
| 2.5.4 | ESP32 USB serial OTA firmware updates (C6 and S3) via web dashboard. App-level OTA using Arduino Update library with A/B partition safety. Fix S3 platform detection (was misidentified as nRF52). |
| 2.5.3 | ESP32-C6 and ESP32-S3 LCD ports (LVGL, NimBLE, dual-transport HID). Multi-platform architecture refactor. JSON protocol for nRF52. DFU backup/restore. Native test infrastructure. Security hardening. |
| 2.5.2 | Fix mouse clicks lifetime stat: `sendMouseScroll()` now increments `totalMouseClicks`. |
| 2.5.1 | Dual-unit distance display (ft/m, mi/km) on OLED and dashboard. |
| 2.5.0 | Lifetime stats: persistent counters for keystrokes, mouse distance, and mouse clicks. |
| 2.4.3 | Deep sleep BLE cleanup: fix immediate WDT reboot on pending BLE events. |
| 2.4.2 | BLE stale link detection: force-disconnect after 5 consecutive GATT failures. |
| 2.4.1 | Activity LEDs, orchestrator day-rollover drift fix. |
| 2.4.0 | PlatformIO migration from Arduino CLI. Charging indicator. Battery spec updated to 2000mAh. |
| 2.3.2 | Codebase audit fixes (45 findings). |
| 2.3.1 | Simulation tuning dashboard. |
| 2.3.0 | 7-slot click action system, expanded F-keys (F1–F20), Snake game, settings export/import. |
| 2.2.2 | Configurable Volume Control buttons, lunch enforcement, keystroke keepalive floor. |
| 2.2.1 | Carousel settings page, system sounds, BLE dashboard transport, heap fragmentation fixes. |
| 2.2.0 | Volume Control operation mode (3 display themes), mode picker horizontal carousel. |
| 2.1.0 | Piezo buzzer sounds (5 profiles), job performance scaling, manual clock setting, display optimization. |
| 2.0.0 | Simulation mode, mute button, hardware watchdog, two-stage sleep, pixel art icons, dashboard simulation. |
| 1.10.1 | LiPo discharge curve, BLE idle power management, die temperature, dashboard battery chart. |
| 1.10.0 | BLE identity presets (decoy masquerade), timed schedule, schedule editor UI. |
| 1.9.1 | Dashboard smart default, USB descriptor customization. |
| 1.9.0 | USB HID wired mode, BT while USB, scroll wheel, real-time dashboard. |
| 1.8.2 | Mute indicator, custom name in header, narrower screensaver bars. |
| 1.8.1 | Pac-Man easter egg redesign. |
| 1.8.0 | Mouse movement styles (Bezier/Brownian), compact uptime, activity-aware animation. |
| 1.7.2 | Web Serial DFU, dashboard switched from BLE to USB serial. |
| 1.7.1 | OTA DFU mode — via nRF Connect mobile. |
| 1.7.0 | BLE UART config protocol (NUS), Vue 3 web dashboard. |
| 1.6.0 | Modular codebase (15 module pairs), configurable status animation (6 styles). |
| 1.5.0 | Adjustable mouse amplitude, inertial ease-in-out mouse movement. |
| 1.4.0 | Scrollable settings menu, display brightness, data-driven menu architecture. |
| 1.3.1 | Fix encoder unresponsive after boot, hybrid ISR+polling, bitmap splash. |
| 1.3.0 | Screensaver mode for OLED burn-in prevention. |
| 1.2.1 | Fix encoder initial state sync bug. |
| 1.2.0 | Multi-key slots, timing profiles (LAZY/NORMAL/BUSY), SLOTS mode. |
| 1.1.1 | Icon-based status, ECG pulse, KB/MS combo cycling. |
| 1.1.0 | Display overhaul, BT icon, HID keycode fix. |
| 1.0.0 | Initial hardware release — encoder menu, flash storage, BLE HID. |

# Ghost Operator — Product Sense

This doc captures taste and product judgment for the Ghost Operator hardware device.

## Who is the user?

Remote or hybrid knowledge workers who need their computer to stay awake and active during working hours. They range from privacy-conscious professionals who simply dislike aggressive screen lock policies to users navigating employee monitoring software (Hubstaff, ActivTrak, Teramind). Technical comfort varies — the device must work without configuration, but power users want deep customization.

## Product principles

### 1. Zero-config by default
Plug in, pair Bluetooth, forget about it. Default settings (F16 key, 2-6.5s interval, Bezier mouse) work for 90% of users without touching the menu. Power users can customize everything via the encoder or web dashboard.

### 2. Invisible to the host
The device must never interfere with active work. F13-F24 keys are invisible to applications. Bezier mouse movements are subtle. Middle clicks are no-ops on desktop. No keystroke should dismiss a dialog, move a cursor, or trigger a shortcut.

### 3. Realistic over regular
Real human input is bursty, varied, and imperfect. Simulation mode models actual work patterns — typing bursts separated by reading pauses, not a steady drip of one key every 5 seconds. Every timing value has ±20% randomness.

### 4. Hardware that disappears
The device should be forgettable once set up. Auto-reconnect to last BLE host. Screensaver prevents OLED burn-in. Scheduled sleep/wake means zero daily interaction. 2000mAh battery lasts days.

### 5. Progressive complexity
Simple mode for basic anti-idle. Simulation mode for monitoring-aware users. Volume Control mode repurposes the hardware as a media controller. Games for fun. Each mode unlocks naturally — users discover complexity when they need it.

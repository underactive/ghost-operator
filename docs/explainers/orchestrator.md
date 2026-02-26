# Orchestrator: Technical Reference

The orchestrator (`orchestrator.h/.cpp`) drives the Simulation operation mode. It generates realistic, non-repetitive HID activity by modelling a full 8-hour office workday as a hierarchical state machine. This document covers the architecture, data model, scheduling logic, and integration points.

Related source files:
- `orchestrator.h/.cpp` — tick loop, phase transitions, burst FSM, lunch enforcement
- `sim_data.h/.cpp` — data structures and const tables (jobs, work modes, phase timing)
- `config.h` — enums (`ActivityPhase`, `WorkModeId`), constants (`LUNCH_OFFSET_MIN`, `ACTIVITY_FLOOR_*`)
- `state.h` — `OrchestratorState orch` struct (all mutable runtime state)

---

## Hierarchical State Machine

The orchestrator is a 4-level hierarchy. Each level owns a timer, and expiry cascades downward:

```
DayTemplate          (480 min, selected by settings.jobSimulation)
 └─ TimeBlock[]      (8-9 blocks/day, sequential, deterministic order)
      └─ WorkMode    (weighted random from block's pool, 30s-15min each)
           └─ Phase  (TYPING | MOUSING | IDLE | SWITCHING, seconds-scale)
```

A cross-cutting **Profile Stint** layer selects Lazy/Normal/Busy effort within each work mode on a 15-120s rotation.

### Tick Order (`tickOrchestrator`)

Called once per `loop()` iteration when `operationMode == OP_SIMULATION`:

1. **Block expiry** — advance to next `TimeBlock`, wrapping to block 0 at day end
2. **Lunch enforcement** — gate or force-jump (see [Lunch Enforcement](#lunch-enforcement))
3. **Mode expiry** — select new weighted-random `WorkMode` from current block
4. **Profile stint expiry** — re-roll Lazy/Normal/Busy from mode weights
5. **Phase expiry** — transition to next activity phase via `selectNextPhase()`
6. **Phase tick** — execute current phase logic (`tickBurst`, `tickMousePhase`, etc.)
7. **Keepalive** — ensure minimum keystroke activity floor

---

## Data Model

### `DayTemplate`

One per job. Contains a sequential array of `TimeBlock` entries spanning 480 minutes.

```cpp
struct DayTemplate {
  const char* name;                      // "Staff", "Developer", "Designer"
  uint8_t numBlocks;                     // 9 for all current templates
  TimeBlock blocks[MAX_DAY_BLOCKS];      // MAX_DAY_BLOCKS = 12
};
```

### `TimeBlock`

A named time segment (e.g., "Deep Work 1", "Lunch", "Coffee"). Each block defines a weighted pool of `WorkModeId` entries that can be randomly selected during that period.

```cpp
struct TimeBlock {
  const char* name;                      // display name (<=12 chars)
  uint16_t startMinutes;                 // nominal start offset from day start
  uint16_t durationMinutes;              // nominal duration (±20% jitter applied at runtime)
  uint8_t numModes;                      // 1-5 available modes
  WeightedMode modes[MAX_BLOCK_MODES];   // { WorkModeId, weight } pairs
  bool isLunch;                          // triggers lunch enforcement logic
};
```

`startMinutes` is documentary only — actual block timing is driven by the preceding block's expiry plus jitter. The `isLunch` flag is what the enforcement system keys on.

### `WorkModeDef`

Defines the behavioral profile for a single activity type. This is the core data structure — it controls how keyboard and mouse activity are generated.

```cpp
struct WorkModeDef {
  WorkModeId id;
  const char* name;                      // full name
  const char* shortName;                 // <=10 chars for OLED display
  uint8_t kbPercent;                     // probability of TYPING vs MOUSING on phase selection
  ProfileWeights profileWeights;         // { lazyPct, normalPct, busyPct }
  PhaseTiming timing[PROFILE_COUNT];     // one PhaseTiming per effort profile (Lazy/Normal/Busy)
  uint16_t modeDurMinSec, modeDurMaxSec; // how long this mode runs before re-selection
  uint16_t profileStintMinSec, profileStintMaxSec; // effort profile rotation interval
};
```

### `PhaseTiming`

Per-profile timing parameters for keystroke bursts, mouse durations, and idle periods:

```cpp
struct PhaseTiming {
  uint8_t  burstKeysMin, burstKeysMax;       // keys per burst
  uint16_t interKeyMinMs, interKeyMaxMs;     // delay between consecutive keys in a burst
  uint32_t burstGapMinMs, burstGapMaxMs;     // pause between bursts
  uint16_t keyHoldMinMs, keyHoldMaxMs;       // how long a key is held down
  uint32_t mouseDurMinMs, mouseDurMaxMs;     // mousing phase duration
  uint32_t idleDurMinMs, idleDurMaxMs;       // idle phase duration
};
```

All timing values are base values — they get modified by `scaleByPerformance()` and `jitter()` at runtime.

---

## Jobs

Three job templates are defined in `DAY_TEMPLATES[]`. Each models a different worker archetype with distinct block structure and mode weighting.

### Staff (Office Worker)

Balanced generalist. Moderate keyboard/mouse ratio across email, documents, browsing, and chat.

| Block | Minutes | Key Modes (weight) |
|-------|---------|-------------------|
| AM Email | 0-30 | Email Compose (40), Email Read (40), Chat (20) |
| AM Work | 30-120 | Doc Editing (35), Browsing (25), Email Compose (20), Files (10), Chat (10) |
| Coffee | 120-135 | Coffee Break (100) |
| Late AM | 135-240 | Doc Editing (30), Email Read (20), Browsing (20), Chat (15), Files (15) |
| **Lunch** | 240-300 | Lunch Break (100) |
| PM Meeting | 300-330 | Video Conf (60), IRL Meeting (40) |
| PM Work | 330-405 | Doc Editing (35), Email Compose (25), Browsing (20), Files (10), Chat (10) |
| Coffee 2 | 405-420 | Coffee Break (100) |
| Wind Down | 420-480 | Email Read (30), Email Compose (25), Browsing (25), Chat (20) |

### Developer

Keyboard-heavy. Three "Deep Work" blocks dominate the day at 70-75% Programming weight.

| Block | Minutes | Key Modes (weight) |
|-------|---------|-------------------|
| Standup | 0-15 | Chat (60), Video Conf (40) |
| Email | 15-30 | Email Read (60), Email Compose (30), Chat (10) |
| Deep Work 1 | 30-150 | **Programming (70)**, Browsing (15), Files (10), Chat (5) |
| Coffee | 150-165 | Coffee Break (80), Browsing (20) |
| Deep Work 2 | 165-240 | **Programming (75)**, Browsing (15), Files (10) |
| **Lunch** | 240-300 | Lunch Break (100) |
| Code Review | 300-360 | Programming (40), Browsing (30), Chat (20), Docs (10) |
| Deep Work 3 | 360-435 | **Programming (70)**, Browsing (15), Files (10), Chat (5) |
| Wind Down | 435-480 | Email Compose (30), Chat (30), Browsing (20), Docs (20) |

### Designer

Mouse-heavy. File Management (design tool proxy) dominates at 40-45% weight. Only ~25% keyboard ratio during design blocks.

| Block | Minutes | Key Modes (weight) |
|-------|---------|-------------------|
| AM Email | 0-20 | Email Read (50), Email Compose (30), Chat (20) |
| Design 1 | 20-140 | **Files (40)**, Browsing (35), Docs (15), Chat (10) |
| Coffee | 140-155 | Coffee Break (80), Browsing (20) |
| Review | 155-200 | Video Conf (40), Browsing (30), Chat (20), Files (10) |
| References | 200-240 | Browsing (70), Files (20), Chat (10) |
| **Lunch** | 240-300 | Lunch Break (100) |
| Design 2 | 300-390 | **Files (45)**, Browsing (30), Docs (15), Chat (10) |
| Feedback | 390-420 | Chat (40), Email Compose (30), Email Read (20), Video Conf (10) |
| Wind Down | 420-480 | Email Compose (25), Browsing (30), Files (25), Chat (20) |

---

## Work Modes (11)

Each work mode defines a keyboard-to-mouse ratio, profile weight distribution, and three sets of timing parameters (one per effort profile).

| ID | Name | KB% | Profile Weights (L/N/B) | Mode Duration | Stint Duration |
|----|------|-----|-------------------------|---------------|----------------|
| `WMODE_EMAIL_COMPOSE` | Email Compose | 65 | 25/15/60 | 60-300s | 15-60s |
| `WMODE_EMAIL_READ` | Email Read | 20 | 50/35/15 | 30-180s | 20-90s |
| `WMODE_PROGRAMMING` | Programming | 80 | 20/40/40 | 120-600s | 20-120s |
| `WMODE_BROWSING` | Browsing | 30 | 30/50/20 | 60-300s | 20-90s |
| `WMODE_CHAT_SLACK` | Chatting | 60 | 20/30/50 | 30-180s | 15-60s |
| `WMODE_VIDEO_CONF` | Video Conference | 10 | 70/25/5 | 300-900s | 60-300s |
| `WMODE_DOC_EDITING` | Doc Editing | 55 | 20/35/45 | 90-480s | 20-90s |
| `WMODE_COFFEE_BREAK` | Coffee Break | 15 | 80/15/5 | 900-1200s | 120-300s |
| `WMODE_LUNCH_BREAK` | Lunch Break | 5 | 90/10/0 | 1800-3600s | 300-600s |
| `WMODE_IRL_MEETING` | IRL Meeting | 10 | 80/15/5 | 1800-3600s | 300-600s |
| `WMODE_FILE_MGMT` | File Management | 25 | 15/50/35 | 60-300s | 20-90s |

**KB%** is the probability of selecting `PHASE_TYPING` vs `PHASE_MOUSING` at each phase transition. A mode with 80% KB will produce typing phases ~80% of the time (before the 20% idle chance is applied).

**Profile Weights** control how often each effort level is selected during profile stint rotation. Programming at 20/40/40 means 40% of stints will be Busy (intense coding), while Video Conference at 70/25/5 means 70% of stints will be Lazy (mostly listening).

---

## Effort Profiles

Three profiles exist: `PROFILE_LAZY`, `PROFILE_NORMAL`, `PROFILE_BUSY`. Each work mode defines a full `PhaseTiming` struct per profile, allowing fine-grained control over burst sizes, timing gaps, and idle durations.

Example — Programming mode timing ranges:

| Parameter | Lazy | Normal | Busy |
|-----------|------|--------|------|
| Burst keys | 3-10 | 10-40 | 40-100 |
| Inter-key delay | 200-350ms | 80-160ms | 40-100ms |
| Burst gap | 10-35s | 3-15s | 2-10s |
| Key hold | 120-280ms | 70-150ms | 40-100ms |
| Mouse duration | 8-20s | 5-15s | 3-8s |
| Idle duration | 5-20s | 2-10s | 1-5s |

The profile rotates every `profileStintMinSec` to `profileStintMaxSec` (mode-dependent), selected by weighted random from the mode's `ProfileWeights`. This creates natural variation within a single work mode — a Developer coding session alternates between intense bursts and thoughtful pauses.

---

## Job Performance Scaling

`settings.jobPerformance` (0-11, default 5) is a global multiplier applied to all timing values at runtime via two scaling functions:

### `scaleByPerformance(value, scaleUp)`

```
scaleUp=true  (active durations):   value * level / 5
scaleUp=false (idle durations):     level == 0 ? value * 10 : value * 5 / level
```

| Level | Active multiplier | Idle multiplier |
|-------|------------------|-----------------|
| 0 | 0x (near-zero output) | 10x |
| 1 | 0.2x | 5x |
| 5 | 1.0x (baseline) | 1.0x |
| 8 | 1.6x | 0.625x |
| 11 | 2.2x | 0.45x |

### `scaleCountByPerformance(value)`

Applied to burst key counts. Same formula as active scaling (`value * level / 5`), clamped to `[1, 255]` for non-zero levels.

These functions are applied *on top of* the per-profile timing values, so the total parameter space is: `base_timing[profile] * performance_scale ± 20% jitter`.

---

## Activity Phases

### Phase Selection (`selectNextPhase`)

```
Any phase → 20% chance → PHASE_IDLE
PHASE_TYPING  → PHASE_SWITCHING → KB% roll → PHASE_TYPING or PHASE_MOUSING
PHASE_MOUSING → PHASE_SWITCHING → KB% roll → PHASE_TYPING or PHASE_MOUSING
PHASE_IDLE    → KB% roll → PHASE_TYPING or PHASE_MOUSING
```

Phases always transition through `PHASE_SWITCHING` (100-500ms) when going between typing and mousing, simulating the hand-movement delay between keyboard and mouse.

### PHASE_TYPING — Burst Sub-FSM

The typing phase contains its own 3-state machine tracked in `OrchestratorState`:

```
┌──────────────┐    burst starts     ┌──────────────┐
│  Idle/Gap    │ ──────────────────→ │  Key Down    │
│ (waiting)    │ ←── burst done ──── │  (pressing)  │
└──────────────┘                     └──────┬───────┘
                                            │ key hold expired
                                            ▼
                                     ┌──────────────┐
                                     │  Inter-key   │
                                     │  (delay)     │
                                     └──────┬───────┘
                                            │ more keys remaining
                                            ▼
                                     (back to Key Down)
```

**State variables:**
- `burstKeysRemaining` — keys left in current burst (0 = waiting for next burst)
- `keyDown` — a key is currently held (waiting for hold timer to expire)
- `inBurstGap` — between bursts (waiting for gap timer)
- `nextKeyMs` — timestamp for next key press
- `currentKeyHoldMs` — per-key hold duration (modifier keys get 150-400ms, regular keys use profile's `keyHoldMin/Max`)

**Scaling applied:** Burst counts via `scaleCountByPerformance()`, inter-key and gap durations via `scaleByPerformance(false)` (idle direction — higher performance = shorter delays).

### PHASE_MOUSING

Delegates to the existing `handleMouseStateMachine()` (Bezier or Brownian, configured via `settings.mouseStyle`). The orchestrator adds:

- **Entry:** Resets mouse state to `MOUSE_IDLE` with zero idle duration, triggering immediate jiggle start
- **Phantom clicks:** 25% chance on `MOUSE_RETURNING → MOUSE_IDLE` transition when `settings.phantomClicks` is enabled. Sends configurable button (Middle or Left) with 50-150ms hold
- **Exit:** Waits up to 2s for `MOUSE_RETURNING` to complete. Force-resets to `MOUSE_IDLE` if the cap is exceeded

### PHASE_IDLE

No HID output. Simulates reading/thinking. Duration is capped at 60s to prevent extended silence, and the keepalive floor provides a backstop.

### PHASE_SWITCHING

100-500ms transition gap. If `settings.windowSwitching` is enabled and the profile is not Lazy, this phase may fire an Alt-Tab/Cmd-Tab (configured via `settings.switchKeys`). Window switches are rate-limited to one per 3-15 minutes via `nextWindowSwitchMs`.

---

## Lunch Enforcement

Lunch is guaranteed at the 4-hour mark (`LUNCH_OFFSET_MIN = 240`), minimum 60 minutes (`LUNCH_MIN_DURATION_MIN`), with ±10% jitter.

Three mechanisms work together:

1. **Gate:** When the next block transition would advance to the lunch block but `dayElapsed < lunchTargetMs()`, the current block's duration is extended to fill the gap. Prevents early lunch.

2. **Force-jump:** If `dayElapsed >= lunchTargetMs()` and the current block index is still before the lunch block, the system jumps directly to the lunch block. Prevents skipped lunch.

3. **Duration lock:** Lunch block duration is calculated independently (60min + jitter), ignoring the `durationMinutes` field in the template. No performance scaling is applied.

`lunchCompleted` is set to `true` when the lunch block starts and reset to `false` on day wrap (block 0).

---

## Keystroke Keepalive (Activity Floor)

Prevents screen lock during extended non-typing phases. Runs in step 6 of `tickOrchestrator`, outside of `PHASE_TYPING`.

**Trigger:** `lastSimKeystrokeMs` exceeds `ACTIVITY_FLOOR_GAP_MS` (120s / 2 minutes).

**Behavior:** Sends a burst of 2-5 keystrokes with:
- 30-60ms hold per key (brief, for reliable HID host recognition)
- 200-600ms inter-key delay
- Silent flag (`sendKeyDown(idx, true)`) — suppresses piezo buzzer

The keepalive resets `lastSimKeystrokeMs`, preventing re-trigger until the next 2-minute gap.

---

## Time Synchronization

`syncOrchestratorTime(daySeconds)` aligns the orchestrator to wall clock time. Called when the user sets time via `MODE_SET_CLOCK`.

**Algorithm:**
1. Convert `daySeconds` (seconds since midnight) to minutes offset from `settings.jobStartTime`
2. Scan `TimeBlock[]` to find the block containing that offset
3. Reconstruct `dayStartMs = now - offsetMinutes * 60000`
4. Set `lunchCompleted` based on whether the synced position is at or past the lunch block
5. Call `startBlock()` and `startMode()` to resume the simulation at the correct position

If the synced time is past all blocks, the last block is selected and `lunchCompleted` is set to true.

---

## Runtime State (`OrchestratorState`)

All mutable orchestrator state lives in a single `OrchestratorState orch` struct (declared in `state.h`):

| Field Group | Fields | Purpose |
|-------------|--------|---------|
| Hierarchy position | `blockIdx`, `modeId`, `phase`, `autoProfile` | Current position in the 4-level hierarchy |
| Timers | `blockStartMs`, `blockDurationMs`, `modeStartMs`, `modeDurationMs`, `phaseStartMs`, `phaseDurationMs`, `profileStintStartMs`, `profileStintMs` | Expiry tracking for each level |
| Burst FSM | `burstKeysRemaining`, `keyDown`, `keyDownMs`, `currentKeyHoldMs`, `nextKeyMs`, `inBurstGap`, `burstGapEndMs` | Typing sub-state machine |
| Side effects | `lastPhantomClickMs`, `nextWindowSwitchMs` | Phantom click flash + window switch scheduling |
| Lunch | `dayStartMs`, `lunchCompleted`, `lunchBlockIdx` | Lunch gate/force-jump tracking |
| Keepalive | `lastSimKeystrokeMs`, `keepaliveBurstRemaining`, `keepaliveNextKeyMs` | Activity floor state |
| Display | `scrollPos[3]`, `scrollDir[3]`, `scrollTimer[3]`, `previewActive`, `previewStartMs` | OLED scroll/preview state |

Zeroed by `memset` in `initOrchestrator()`. `lastSimKeystrokeMs` is seeded to `millis()` to prevent an immediate keepalive burst on init.

---

## Integration Points

| System | How the orchestrator interacts |
|--------|-------------------------------|
| **HID** | Calls `sendKeyDown()`, `sendKeyUp()`, `sendMouseClick()`, `sendWindowSwitch()` — all dual-transport (BLE + USB) |
| **Mouse** | Delegates to `handleMouseStateMachine()` during `PHASE_MOUSING`; resets mouse state on phase entry/exit |
| **Sound** | Keystroke sounds fire via `sendKeyDown()` during `PHASE_TYPING`; keepalive keys pass `silent=true` to suppress buzzer |
| **Display** | Calls `markDisplayDirty()` on block/mode/phase/profile transitions; exposes `currentBlockName()`, `currentModeName()`, `blockProgress()`, `modeProgress()` for rendering |
| **Settings** | Reads `jobSimulation`, `jobPerformance`, `jobStartTime`, `phantomClicks`, `clickType`, `windowSwitching`, `switchKeys`, `soundEnabled` |
| **Serial** | Logs `[SIM]`-prefixed messages on all transitions; calls `pushSerialStatus()` on mode/phase changes |
| **Schedule** | `syncOrchestratorTime()` called from `MODE_SET_CLOCK` confirmation path |

---

## Adding a New Work Mode

1. Add enum entry to `WorkModeId` in `config.h` (before `WMODE_COUNT`)
2. Add `WorkModeDef` entry to `WORK_MODES[]` in `sim_data.cpp` — define `kbPercent`, `profileWeights`, three `PhaseTiming` structs, and duration ranges
3. Reference the new `WorkModeId` in relevant `TimeBlock.modes[]` entries in `DAY_TEMPLATES[]`
4. No changes needed in `orchestrator.cpp` — it operates generically on the data tables

## Adding a New Job

1. Add entry to `JOB_SIM_NAMES[]` in `sim_data.cpp`
2. Add `DayTemplate` to `DAY_TEMPLATES[]` with up to `MAX_DAY_BLOCKS` (12) blocks
3. Bump `JOB_SIM_COUNT` in `config.h`
4. Update `jobSimulation` max bound in `setSettingValue()` (`settings.cpp`)
5. Add menu format entry if needed

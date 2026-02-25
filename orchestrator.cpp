#include "orchestrator.h"
#include "state.h"
#include "keys.h"
#include "hid.h"
#include "mouse.h"
#include "serial_cmd.h"
#include "timing.h"
#include "display.h"

// ============================================================================
// HELPERS
// ============================================================================

static const DayTemplate& currentTemplate() {
  return DAY_TEMPLATES[settings.jobSimulation];
}

static const TimeBlock& currentBlock() {
  const DayTemplate& tmpl = currentTemplate();
  uint8_t idx = (orch.blockIdx < tmpl.numBlocks) ? orch.blockIdx : 0;
  return tmpl.blocks[idx];
}

static const WorkModeDef& currentWorkMode() {
  if (orch.modeId < WMODE_COUNT) return WORK_MODES[orch.modeId];
  return WORK_MODES[0];
}

// Random in range [lo, hi] (inclusive)
static uint32_t randRange(uint32_t lo, uint32_t hi) {
  if (lo >= hi) return lo;
  return lo + random(hi - lo + 1);
}

// Apply ±20% randomness to a duration
static unsigned long jitter(unsigned long base) {
  if (base == 0) return 0;
  long variation = (long)base * RANDOMNESS_PERCENT / 100;
  return (unsigned long)max(1L, (long)base + random(-variation, variation + 1));
}

// Scale a duration by job performance level.
// scaleUp=true:  active durations (typing, mousing) — higher level = longer activity
// scaleUp=false: idle durations (gaps, delays) — higher level = shorter idle
// Level 5 = baseline (1.0x), level 0 = near-idle, level 11 = 2.2x active
static unsigned long scaleByPerformance(unsigned long value, bool scaleUp) {
  uint8_t level = settings.jobPerformance;
  if (level == 5) return value;  // baseline — no change
  if (scaleUp) {
    return value * level / 5;    // level 0→0, level 11→2.2x
  }
  if (level == 0) return value * 10;  // 10x idle at zero performance
  return value * 5 / level;     // level 1→5x idle, level 11→0.45x idle
}

// Scale a burst count by job performance level.
// Level 5 = baseline, level 0 = 0 keys, level 11 = 2.2x keys
static uint8_t scaleCountByPerformance(uint8_t value) {
  uint8_t level = settings.jobPerformance;
  if (level == 5) return value;
  uint16_t result = (uint16_t)value * level / 5;
  if (result > 255) return 255;
  if (result == 0 && level > 0) return 1;  // clamp to 1 if level>0
  return (uint8_t)result;
}

// ============================================================================
// WEIGHTED RANDOM SELECTION
// ============================================================================

// Select a work mode from a block's weighted pool
static WorkModeId selectWeightedMode(const TimeBlock& block) {
  uint16_t totalWeight = 0;
  for (uint8_t i = 0; i < block.numModes; i++) {
    totalWeight += block.modes[i].weight;
  }
  if (totalWeight == 0) return WMODE_EMAIL_READ;  // fallback

  uint16_t roll = random(totalWeight);
  uint16_t cumulative = 0;
  for (uint8_t i = 0; i < block.numModes; i++) {
    cumulative += block.modes[i].weight;
    if (roll < cumulative) return block.modes[i].modeId;
  }
  return block.modes[0].modeId;  // safety fallback
}

// Select auto-profile from work mode's profile weights
static Profile selectAutoProfile(const WorkModeDef& mode) {
  uint16_t total = mode.profileWeights.lazyPct + mode.profileWeights.normalPct + mode.profileWeights.busyPct;
  if (total == 0) return PROFILE_NORMAL;

  uint16_t roll = random(total);
  if (roll < mode.profileWeights.lazyPct) return PROFILE_LAZY;
  if (roll < mode.profileWeights.lazyPct + mode.profileWeights.normalPct) return PROFILE_NORMAL;
  return PROFILE_BUSY;
}

// ============================================================================
// PHASE SELECTION
// ============================================================================

// Select next activity phase based on KB:MS ratio with idle interleaving
static ActivityPhase selectNextPhase(const WorkModeDef& mode, ActivityPhase current) {
  // ~20% chance of idle phase
  if (random(100) < 20) return PHASE_IDLE;

  // Transition through SWITCHING between KB and mouse
  if (current == PHASE_TYPING) return PHASE_SWITCHING;
  if (current == PHASE_MOUSING) return PHASE_SWITCHING;
  if (current == PHASE_IDLE) {
    // After idle, pick based on KB:MS ratio
    return (random(100) < mode.kbPercent) ? PHASE_TYPING : PHASE_MOUSING;
  }
  // After SWITCHING, go to the other side
  if (current == PHASE_SWITCHING) {
    return (random(100) < mode.kbPercent) ? PHASE_TYPING : PHASE_MOUSING;
  }
  return PHASE_TYPING;
}

// Calculate phase duration from current work mode and profile
static unsigned long phaseDuration(ActivityPhase phase, const WorkModeDef& mode, Profile profile) {
  const PhaseTiming& t = mode.timing[profile];
  switch (phase) {
    case PHASE_TYPING:
      // Typing phase: sum of several burst cycles — estimate ~30-120s
      return jitter(scaleByPerformance(randRange(15000, 60000), true));
    case PHASE_MOUSING:
      return jitter(scaleByPerformance(randRange(t.mouseDurMinMs, t.mouseDurMaxMs), true));
    case PHASE_IDLE: {
      unsigned long dur = jitter(scaleByPerformance(randRange(t.idleDurMinMs, t.idleDurMaxMs), false));
      return min(dur, 60000UL);  // cap at 60s to prevent extended silence
    }
    case PHASE_SWITCHING:
      return randRange(100, 500);
    default:
      return 5000;
  }
}

// ============================================================================
// LUNCH ENFORCEMENT HELPERS
// ============================================================================

// Scan template for the lunch block index (0xFF if none)
static uint8_t findLunchBlockIdx() {
  const DayTemplate& tmpl = currentTemplate();
  for (uint8_t i = 0; i < tmpl.numBlocks; i++) {
    if (tmpl.blocks[i].isLunch) return i;
  }
  return 0xFF;
}

// Exact ms offset from day start when lunch must begin (no randomness)
static unsigned long lunchTargetMs() {
  return (unsigned long)LUNCH_OFFSET_MIN * 60000UL;
}

// ============================================================================
// BLOCK & MODE TRANSITIONS
// ============================================================================

static void startBlock(uint8_t blockIdx, unsigned long now) {
  const DayTemplate& tmpl = currentTemplate();
  if (blockIdx >= tmpl.numBlocks) blockIdx = 0;

  orch.blockIdx = blockIdx;
  orch.blockStartMs = now;
  orch.scrollPos[0] = 0; orch.scrollDir[0] = 1; orch.scrollTimer[0] = now;

  const TimeBlock& block = tmpl.blocks[blockIdx];

  if (block.isLunch) {
    // Lunch: minimum 60 min, +0-10% jitter (gives 60-66 min)
    unsigned long minMs = (unsigned long)LUNCH_MIN_DURATION_MIN * 60000UL;
    unsigned long maxMs = minMs + minMs * LUNCH_DURATION_JITTER / 100;
    orch.blockDurationMs = randRange(minMs, maxMs);
    orch.lunchCompleted = true;
    Serial.println("[SIM] Lunch started (enforced)");
  } else {
    // Normal: ±20% randomness
    unsigned long baseDur = (unsigned long)block.durationMinutes * 60000UL;
    orch.blockDurationMs = jitter(baseDur);
  }

  // Day wrap: reset lunch tracking when cycling back to block 0
  if (blockIdx == 0) {
    orch.dayStartMs = now;
    orch.lunchCompleted = false;
    orch.lunchBlockIdx = findLunchBlockIdx();
  }

  markDisplayDirty();
  Serial.print("[SIM] Block: ");
  Serial.println(block.name);
}

static void startMode(unsigned long now) {
  const TimeBlock& block = currentBlock();
  orch.modeId = selectWeightedMode(block);
  orch.modeStartMs = now;
  orch.scrollPos[1] = 0; orch.scrollDir[1] = 1; orch.scrollTimer[1] = now;

  const WorkModeDef& mode = currentWorkMode();
  orch.modeDurationMs = randRange(
    (unsigned long)mode.modeDurMinSec * 1000UL,
    (unsigned long)mode.modeDurMaxSec * 1000UL
  );

  // Select initial auto-profile
  orch.autoProfile = selectAutoProfile(mode);
  currentProfile = orch.autoProfile;
  orch.profileStintStartMs = now;
  orch.scrollPos[2] = 0; orch.scrollDir[2] = 1; orch.scrollTimer[2] = now;
  orch.profileStintMs = randRange(
    (unsigned long)mode.profileStintMinSec * 1000UL,
    (unsigned long)mode.profileStintMaxSec * 1000UL
  );

  // Start with first phase (typing or mousing based on ratio)
  orch.phase = (random(100) < mode.kbPercent) ? PHASE_TYPING : PHASE_MOUSING;
  orch.phaseStartMs = now;
  orch.phaseDurationMs = phaseDuration(orch.phase, mode, orch.autoProfile);

  // Reset burst state
  orch.burstKeysRemaining = 0;
  orch.keyDown = false;
  orch.inBurstGap = false;

  // Schedule window switch
  orch.nextWindowSwitchMs = now + randRange(180000, 900000);

  markDisplayDirty();
  Serial.print("[SIM] Mode: ");
  Serial.print(mode.shortName);
  Serial.print(" (");
  Serial.print(PROFILE_NAMES[orch.autoProfile]);
  Serial.println(")");
  pushSerialStatus();
}

static void startPhase(ActivityPhase phase, unsigned long now) {
  orch.phase = phase;
  orch.phaseStartMs = now;
  orch.phaseDurationMs = phaseDuration(phase, currentWorkMode(), orch.autoProfile);

  // Reset burst state for new typing phase
  if (phase == PHASE_TYPING) {
    orch.burstKeysRemaining = 0;
    orch.keyDown = false;
    orch.inBurstGap = false;
  }

  // Reset mouse state for new mousing phase
  if (phase == PHASE_MOUSING) {
    mouseState = MOUSE_IDLE;
    lastMouseStateChange = now;
    currentMouseIdle = 0;  // start jiggling immediately (orchestrator owns phase timing)
    mouseNetX = 0;
    mouseNetY = 0;
    mouseReturnTotal = 0;
    scheduleNextMouseState();
  }
}

// ============================================================================
// BURST STATE MACHINE (PHASE_TYPING sub-FSM)
// ============================================================================

static void tickBurst(unsigned long now) {
  if (!keyEnabled || !hasPopulatedSlot()) return;

  const WorkModeDef& mode = currentWorkMode();
  const PhaseTiming& t = mode.timing[orch.autoProfile];

  // If key is currently held down, check if it's time to release
  if (orch.keyDown) {
    if (now - orch.keyDownMs >= orch.currentKeyHoldMs) {
      sendKeyUp();
      orch.keyDown = false;
      orch.burstKeysRemaining--;

      if (orch.burstKeysRemaining == 0) {
        // End of burst — enter gap (shorter at higher performance)
        orch.inBurstGap = true;
        orch.burstGapEndMs = now + scaleByPerformance(randRange(t.burstGapMinMs, t.burstGapMaxMs), false);
      } else {
        // Inter-key delay before next key (shorter at higher performance)
        orch.nextKeyMs = now + scaleByPerformance(randRange(t.interKeyMinMs, t.interKeyMaxMs), false);
      }
    }
    return;
  }

  // In burst gap — waiting for next burst
  if (orch.inBurstGap) {
    if (now >= orch.burstGapEndMs) {
      orch.inBurstGap = false;
      // Start new burst (more keys at higher performance)
      orch.burstKeysRemaining = scaleCountByPerformance((uint8_t)randRange(t.burstKeysMin, t.burstKeysMax));
      orch.nextKeyMs = now;  // Start immediately
    }
    return;
  }

  // Not in burst — start one or continue
  if (orch.burstKeysRemaining == 0) {
    // Start new burst (more keys at higher performance)
    orch.burstKeysRemaining = scaleCountByPerformance((uint8_t)randRange(t.burstKeysMin, t.burstKeysMax));
    orch.nextKeyMs = now;
  }

  // Ready to press next key?
  if (now >= orch.nextKeyMs && orch.burstKeysRemaining > 0) {
    // Press key down
    sendKeyDown(nextKeyIndex);
    pickNextKey();
    orch.keyDown = true;
    orch.keyDownMs = now;
    orch.lastSimKeystrokeMs = now;

    // Modifier keys get longer hold
    const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
    if (key.isModifier) {
      orch.currentKeyHoldMs = (uint16_t)randRange(150, 400);
    } else {
      orch.currentKeyHoldMs = (uint16_t)randRange(t.keyHoldMinMs, t.keyHoldMaxMs);
    }
  }
}

// ============================================================================
// MOUSING PHASE
// ============================================================================

static void tickMousePhase(unsigned long now) {
  if (!mouseEnabled) return;

  // Snapshot state before tick to detect transitions
  MouseState prevState = mouseState;

  // Delegate to existing mouse state machine
  handleMouseStateMachine(now);

  // Phantom click: 25% chance on RETURNING → IDLE transition
  if (settings.phantomClicks && prevState == MOUSE_RETURNING && mouseState == MOUSE_IDLE) {
    if (random(100) < 25) {
      uint8_t btn = settings.clickType == 1 ? 0x01 : 0x04;  // Left or Middle
      sendMouseClick(btn, (uint16_t)randRange(50, 150));
      orch.lastPhantomClickMs = millis();
    }
  }
}

// ============================================================================
// SWITCHING PHASE
// ============================================================================

static void tickSwitchPhase(unsigned long now) {
  // Window switch fires during SWITCHING phase at long intervals
  if (settings.windowSwitching &&
      orch.autoProfile != PROFILE_LAZY && now >= orch.nextWindowSwitchMs) {
    sendWindowSwitch();
    orch.nextWindowSwitchMs = now + randRange(180000, 900000);
  }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void initOrchestrator() {
  memset(&orch, 0, sizeof(orch));
  unsigned long now = millis();

  orch.dayStartMs = now;
  orch.lunchCompleted = false;
  orch.lunchBlockIdx = findLunchBlockIdx();
  orch.lastSimKeystrokeMs = now;  // prevent immediate keepalive on init

  startBlock(0, now);
  startMode(now);

  Serial.println("[SIM] Orchestrator initialized");
}

void tickOrchestrator(unsigned long now) {
  const DayTemplate& tmpl = currentTemplate();

  // 1. Check block timer
  if (now - orch.blockStartMs >= orch.blockDurationMs) {
    uint8_t nextBlock = (orch.blockIdx + 1) % tmpl.numBlocks;

    // Lunch enforcement: gate pre-lunch blocks if lunch hasn't started yet
    if (!orch.lunchCompleted && orch.lunchBlockIdx != 0xFF &&
        nextBlock == orch.lunchBlockIdx) {
      unsigned long dayElapsed = now - orch.dayStartMs;
      unsigned long target = lunchTargetMs();
      if (dayElapsed < target) {
        // Extend current block until lunch target (absolute: expire when day reaches target)
        orch.blockDurationMs = (now - orch.blockStartMs) + (target - dayElapsed);
        Serial.print("[SIM] Lunch gate: extending ");
        Serial.print(currentBlock().name);
        Serial.print(" by ");
        Serial.print((target - dayElapsed) / 60000UL);
        Serial.println("min");
        // Don't transition yet — re-enter tick next loop
        goto skipBlockTransition;
      }
    }

    startBlock(nextBlock, now);
    startMode(now);
  }

  // 1b. Lunch force-jump: if past lunch target and still on pre-lunch block
  else if (!orch.lunchCompleted && orch.lunchBlockIdx != 0xFF &&
           orch.blockIdx < orch.lunchBlockIdx) {
    unsigned long dayElapsed = now - orch.dayStartMs;
    if (dayElapsed >= lunchTargetMs()) {
      Serial.println("[SIM] Lunch force: jumping to lunch block");
      startBlock(orch.lunchBlockIdx, now);
      startMode(now);
    }
  }

  // 2. Check mode timer
  else if (now - orch.modeStartMs >= orch.modeDurationMs) {
    startMode(now);
  }

  skipBlockTransition:

  // 3. Check profile stint timer
  if (now - orch.profileStintStartMs >= orch.profileStintMs) {
    const WorkModeDef& mode = currentWorkMode();
    orch.autoProfile = selectAutoProfile(mode);
    currentProfile = orch.autoProfile;
    orch.profileStintStartMs = now;
    orch.scrollPos[2] = 0; orch.scrollDir[2] = 1; orch.scrollTimer[2] = now;
    orch.profileStintMs = randRange(
      (unsigned long)mode.profileStintMinSec * 1000UL,
      (unsigned long)mode.profileStintMaxSec * 1000UL
    );
    markDisplayDirty();
  }

  // 4. Check phase timer
  if (now - orch.phaseStartMs >= orch.phaseDurationMs) {
    // If in mousing and mouse is mid-return, wait (cap at 2s)
    if (orch.phase == PHASE_MOUSING && mouseState == MOUSE_RETURNING) {
      if (now - orch.phaseStartMs < orch.phaseDurationMs + 2000) {
        // Still waiting for return to complete
        handleMouseStateMachine(now);
        return;
      }
      // Force reset after 2s cap
      mouseState = MOUSE_IDLE;
      mouseNetX = 0;
      mouseNetY = 0;
    }

    // Release any held key before phase transition
    if (orch.keyDown) {
      sendKeyUp();
      orch.keyDown = false;
    }

    ActivityPhase nextPhase = selectNextPhase(currentWorkMode(), orch.phase);
    startPhase(nextPhase, now);
    markDisplayDirty();
    pushSerialStatus();
  }

  // 5. Execute current phase
  switch (orch.phase) {
    case PHASE_TYPING:
      tickBurst(now);
      break;
    case PHASE_MOUSING:
      tickMousePhase(now);
      break;
    case PHASE_IDLE:
      // Nothing — simulates reading/thinking
      break;
    case PHASE_SWITCHING:
      tickSwitchPhase(now);
      break;
    default:
      break;
  }

  // 6. Keystroke keepalive — ensure non-zero keyboard in every 2-min window
  if (keyEnabled && hasPopulatedSlot() && orch.phase != PHASE_TYPING) {
    if (orch.keepaliveBurstRemaining > 0 && now >= orch.keepaliveNextKeyMs) {
      // Continue sending keepalive burst (silent — no buzzer during non-typing phases)
      sendKeyDown(nextKeyIndex, true);
      pickNextKey();
      orch.lastSimKeystrokeMs = now;
      orch.keepaliveBurstRemaining--;
      // Brief hold for robust HID host recognition, then release
      delay(randRange(30, 60));
      sendKeyUp();
      orch.keepaliveNextKeyMs = now + randRange(200, 600);
    } else if (orch.keepaliveBurstRemaining == 0 &&
               now - orch.lastSimKeystrokeMs >= ACTIVITY_FLOOR_GAP_MS) {
      // Start a new keepalive burst
      orch.keepaliveBurstRemaining = (uint8_t)randRange(
        ACTIVITY_FLOOR_BURST_MIN, ACTIVITY_FLOOR_BURST_MAX);
      orch.keepaliveNextKeyMs = now;
    }
  }
}

void skipWorkMode() {
  unsigned long now = millis();
  // Release held key
  if (orch.keyDown) {
    sendKeyUp();
    orch.keyDown = false;
  }
  startMode(now);
  markDisplayDirty();
}

const char* currentBlockName() {
  return currentBlock().name;
}

const char* currentModeName() {
  return currentWorkMode().name;
}

uint8_t blockProgress(unsigned long now) {
  if (orch.blockDurationMs == 0) return 0;
  unsigned long elapsed = now - orch.blockStartMs;
  if (elapsed >= orch.blockDurationMs) return 100;
  return (uint8_t)(elapsed * 100UL / orch.blockDurationMs);
}

uint8_t modeProgress(unsigned long now) {
  if (orch.modeDurationMs == 0) return 0;
  unsigned long elapsed = now - orch.modeStartMs;
  if (elapsed >= orch.modeDurationMs) return 100;
  return (uint8_t)(elapsed * 100UL / orch.modeDurationMs);
}

void syncOrchestratorTime(uint32_t daySeconds) {
  const DayTemplate& tmpl = currentTemplate();

  // Convert daySeconds to minutes offset from job start time
  uint32_t schedStartSecs = (uint32_t)settings.jobStartTime * SCHEDULE_SLOT_SECS;
  if (daySeconds < schedStartSecs) return;  // Before job start

  uint32_t offsetMin = (daySeconds - schedStartSecs) / 60;

  // Find the correct block for this time offset
  for (uint8_t i = 0; i < tmpl.numBlocks; i++) {
    uint16_t blockEnd = tmpl.blocks[i].startMinutes + tmpl.blocks[i].durationMinutes;
    if (offsetMin < blockEnd) {
      unsigned long now = millis();

      // Reconstruct dayStartMs from current offset
      orch.dayStartMs = now - (unsigned long)offsetMin * 60000UL;
      orch.lunchBlockIdx = findLunchBlockIdx();
      // Lunch is completed if synced block is at or past the lunch block
      orch.lunchCompleted = (orch.lunchBlockIdx != 0xFF && i >= orch.lunchBlockIdx);

      startBlock(i, now);
      startMode(now);
      Serial.print("[SIM] Time synced to block ");
      Serial.println(i);
      return;
    }
  }

  // Past all blocks — stay on last block
  unsigned long now = millis();
  orch.dayStartMs = now - (unsigned long)offsetMin * 60000UL;
  orch.lunchBlockIdx = findLunchBlockIdx();
  orch.lunchCompleted = true;  // past all blocks = past lunch
  startBlock(tmpl.numBlocks - 1, now);
  startMode(now);
}

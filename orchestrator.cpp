#include "orchestrator.h"
#include "state.h"
#include "keys.h"
#include "hid.h"
#include "mouse.h"
#include "serial_cmd.h"
#include "timing.h"

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
      return jitter(randRange(15000, 60000));
    case PHASE_MOUSING:
      return jitter(randRange(t.mouseDurMinMs, t.mouseDurMaxMs));
    case PHASE_IDLE:
      return jitter(randRange(t.idleDurMinMs, t.idleDurMaxMs));
    case PHASE_SWITCHING:
      return randRange(100, 500);
    default:
      return 5000;
  }
}

// ============================================================================
// BLOCK & MODE TRANSITIONS
// ============================================================================

static void startBlock(uint8_t blockIdx, unsigned long now) {
  const DayTemplate& tmpl = currentTemplate();
  if (blockIdx >= tmpl.numBlocks) blockIdx = 0;

  orch.blockIdx = blockIdx;
  orch.blockStartMs = now;

  // Duration with ±20% randomness
  unsigned long baseDur = (unsigned long)currentBlock().durationMinutes * 60000UL;
  orch.blockDurationMs = jitter(baseDur);

  Serial.print("[SIM] Block: ");
  Serial.println(currentBlock().name);
}

static void startMode(unsigned long now) {
  const TimeBlock& block = currentBlock();
  orch.modeId = selectWeightedMode(block);
  orch.modeStartMs = now;

  const WorkModeDef& mode = currentWorkMode();
  orch.modeDurationMs = randRange(
    (unsigned long)mode.modeDurMinSec * 1000UL,
    (unsigned long)mode.modeDurMaxSec * 1000UL
  );

  // Select initial auto-profile
  orch.autoProfile = selectAutoProfile(mode);
  currentProfile = orch.autoProfile;
  orch.profileStintStartMs = now;
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
        // End of burst — enter gap
        orch.inBurstGap = true;
        orch.burstGapEndMs = now + randRange(t.burstGapMinMs, t.burstGapMaxMs);
      } else {
        // Inter-key delay before next key
        orch.nextKeyMs = now + randRange(t.interKeyMinMs, t.interKeyMaxMs);
      }
    }
    return;
  }

  // In burst gap — waiting for next burst
  if (orch.inBurstGap) {
    if (now >= orch.burstGapEndMs) {
      orch.inBurstGap = false;
      // Start new burst
      orch.burstKeysRemaining = (uint8_t)randRange(t.burstKeysMin, t.burstKeysMax);
      orch.nextKeyMs = now;  // Start immediately
    }
    return;
  }

  // Not in burst — start one or continue
  if (orch.burstKeysRemaining == 0) {
    // Start new burst
    orch.burstKeysRemaining = (uint8_t)randRange(t.burstKeysMin, t.burstKeysMax);
    orch.nextKeyMs = now;
  }

  // Ready to press next key?
  if (now >= orch.nextKeyMs && orch.burstKeysRemaining > 0) {
    // Press key down
    sendKeyDown(nextKeyIndex);
    pickNextKey();
    orch.keyDown = true;
    orch.keyDownMs = now;

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

  startBlock(0, now);
  startMode(now);

  Serial.println("[SIM] Orchestrator initialized");
}

void tickOrchestrator(unsigned long now) {
  const DayTemplate& tmpl = currentTemplate();

  // 1. Check block timer
  if (now - orch.blockStartMs >= orch.blockDurationMs) {
    uint8_t nextBlock = (orch.blockIdx + 1) % tmpl.numBlocks;
    startBlock(nextBlock, now);
    startMode(now);
  }

  // 2. Check mode timer
  else if (now - orch.modeStartMs >= orch.modeDurationMs) {
    startMode(now);
  }

  // 3. Check profile stint timer
  if (now - orch.profileStintStartMs >= orch.profileStintMs) {
    const WorkModeDef& mode = currentWorkMode();
    orch.autoProfile = selectAutoProfile(mode);
    currentProfile = orch.autoProfile;
    orch.profileStintStartMs = now;
    orch.profileStintMs = randRange(
      (unsigned long)mode.profileStintMinSec * 1000UL,
      (unsigned long)mode.profileStintMaxSec * 1000UL
    );
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
}

void skipWorkMode() {
  unsigned long now = millis();
  // Release held key
  if (orch.keyDown) {
    sendKeyUp();
    orch.keyDown = false;
  }
  startMode(now);
}

const char* currentBlockName() {
  return currentBlock().name;
}

const char* currentModeName() {
  return currentWorkMode().shortName;
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

  // Convert daySeconds to minutes offset from schedule start
  uint32_t schedStartSecs = (uint32_t)settings.scheduleStart * SCHEDULE_SLOT_SECS;
  if (daySeconds < schedStartSecs) return;  // Before schedule start

  uint32_t offsetMin = (daySeconds - schedStartSecs) / 60;

  // Find the correct block for this time offset
  for (uint8_t i = 0; i < tmpl.numBlocks; i++) {
    uint16_t blockEnd = tmpl.blocks[i].startMinutes + tmpl.blocks[i].durationMinutes;
    if (offsetMin < blockEnd) {
      unsigned long now = millis();
      startBlock(i, now);
      startMode(now);
      Serial.print("[SIM] Time synced to block ");
      Serial.println(i);
      return;
    }
  }

  // Past all blocks — wrap to first
  unsigned long now = millis();
  startBlock(0, now);
  startMode(now);
}

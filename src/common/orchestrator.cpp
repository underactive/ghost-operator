#include "orchestrator.h"
#include "state.h"
#include "keys.h"
#include "platform_hal.h"
#include "mouse.h"
#include "timing.h"
#include "settings.h"
#include "schedule.h"

// ============================================================================
// HELPERS
// ============================================================================

static const DayTemplate& currentTemplate() {
  uint8_t idx = (settings.jobSimulation < JOB_SIM_COUNT) ? settings.jobSimulation : 0;
  return DAY_TEMPLATES[idx];
}

static const TimeBlock& currentBlock() {
  const DayTemplate& tmpl = currentTemplate();
  uint8_t idx = (orch.blockIdx < tmpl.numBlocks) ? orch.blockIdx : 0;
  return tmpl.blocks[idx];
}

static const WorkModeDef& currentWorkMode() {
  if (orch.modeId < WMODE_COUNT) return workModes[orch.modeId];
  return workModes[0];
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

// Scale a duration by job performance level (compressed curve).
// scaleUp=true:  active durations (typing, mousing) — higher level = longer activity
// scaleUp=false: idle durations (gaps, delays) — higher level = shorter idle
// level 8 ≈ 1x, level 11 → 1.4x active / 0.71x idle
static unsigned long scaleByPerformance(unsigned long value, bool scaleUp) {
  uint8_t level = settings.jobPerformance;
  if (scaleUp) {
    return value * level * 7 / 55;  // level 0→0, level 11→1.4x
  }
  if (level == 0) return value * 10;  // 10x idle at zero performance
  return value * 55 / (level * 7);   // level 1→7.86x idle, level 11→0.71x idle
}

// Scale a burst count by job performance level (compressed curve).
// level 8 ≈ 1x, level 0 = 0 keys, level 11 = 1.4x keys
static uint8_t scaleCountByPerformance(uint8_t value) {
  uint8_t level = settings.jobPerformance;
  uint16_t result = (uint16_t)value * level * 7 / 55;
  if (result > 255) return 255;
  if (result == 0 && level > 0) return 1;  // clamp to 1 if level>0
  return (uint8_t)result;
}

// Returns true for phases that include keyboard activity (keepalive should not fire)
static bool phaseHasKeystrokes(ActivityPhase phase) {
  return phase == PHASE_TYPING || phase == PHASE_KB_MOUSE || phase == PHASE_MOUSE_KB;
}

// Returns true if lunch is enabled for the current shift duration
static bool lunchEnabled() {
  return settings.shiftDuration >= LUNCH_NO_LUNCH_THRESHOLD;
}

// Sum non-lunch block durations from the current template (always 420 for stock templates)
static uint16_t totalNonLunchMinutes() {
  const DayTemplate& tmpl = currentTemplate();
  uint16_t total = 0;
  for (uint8_t i = 0; i < tmpl.numBlocks; i++) {
    if (!tmpl.blocks[i].isLunch) total += tmpl.blocks[i].durationMinutes;
  }
  return (total > 0) ? total : 1;  // guard against division by zero
}

// Scaled block duration in minutes for the given block index.
// Non-lunch blocks scale proportionally to fill shiftDuration.
// Lunch blocks use settings.lunchDuration (or 0 if lunch disabled).
static uint16_t scaledBlockDurationMin(uint8_t blockIdx) {
  const DayTemplate& tmpl = currentTemplate();
  if (blockIdx >= tmpl.numBlocks) return 0;
  const TimeBlock& block = tmpl.blocks[blockIdx];

  if (block.isLunch) {
    return lunchEnabled() ? settings.lunchDuration : 0;
  }
  // Scale non-lunch: block.dur * shiftDuration / totalNonLunch (uint32_t intermediate)
  return (uint16_t)((uint32_t)block.durationMinutes * settings.shiftDuration / totalNonLunchMinutes());
}

// Pick a random cardinal/diagonal direction for micro-movements
static void pickSwipeDirection(int8_t& dx, int8_t& dy) {
  // 8 directions: N,NE,E,SE,S,SW,W,NW
  static const int8_t dirs[][2] = {
    {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}
  };
  uint8_t d = random(8);
  dx = dirs[d][0];
  dy = dirs[d][1];
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

  // Active phases transition through SWITCHING
  if (current == PHASE_TYPING || current == PHASE_MOUSING ||
      current == PHASE_KB_MOUSE || current == PHASE_MOUSE_KB) {
    return PHASE_SWITCHING;
  }

  // After IDLE or SWITCHING: pick next active phase
  // 8% K+M, 4% M+K, remaining 88% split by kbPercent
  uint8_t roll = random(100);
  if (roll < 8) return PHASE_KB_MOUSE;
  if (roll < 12) return PHASE_MOUSE_KB;
  return (random(100) < mode.kbPercent) ? PHASE_TYPING : PHASE_MOUSING;
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
    case PHASE_KB_MOUSE:
      return jitter(scaleByPerformance(randRange(10000, 45000), true));
    case PHASE_MOUSE_KB:
      return jitter(scaleByPerformance(randRange(8000, 30000), true));
    default:
      return 5000;
  }
}

// ============================================================================
// LUNCH ENFORCEMENT HELPERS
// ============================================================================

// Scan template for the lunch block index (0xFF if none or lunch disabled)
static uint8_t findLunchBlockIdx() {
  if (!lunchEnabled()) return 0xFF;
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
    // Lunch: use configured lunch duration + 0-10% jitter
    unsigned long minMs = (unsigned long)settings.lunchDuration * 60000UL;
    unsigned long maxMs = minMs + minMs * LUNCH_DURATION_JITTER / 100;
    orch.blockDurationMs = randRange(minMs, maxMs);
    orch.lunchCompleted = true;
    Serial.println("[SIM] Lunch started (enforced)");
  } else {
    // Non-lunch: use scaled duration + ±20% randomness
    unsigned long baseDur = (unsigned long)scaledBlockDurationMin(blockIdx) * 60000UL;
    orch.blockDurationMs = jitter(baseDur);
    if (orch.blockDurationMs < 60000UL) orch.blockDurationMs = 60000UL;  // 1-min floor prevents tight re-entry
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

  // K+M: init sub-FSM for form filling
  if (phase == PHASE_KB_MOUSE) {
    orch.kbmsSubPhase = KBMS_MOUSE_SWIPE;
    orch.kbmsSwipesRemaining = (uint8_t)randRange(KBMS_SWIPE_MIN, KBMS_SWIPE_MAX);
    pickSwipeDirection(orch.kbmsSwipeDx, orch.kbmsSwipeDy);
    orch.kbmsSubPhaseStartMs = now;
    orch.kbmsSubPhaseDurMs = KBMS_SWIPE_DUR_MS;
    orch.kbmsLastStepMs = now;  // prevent stale timestamp on re-entry
    orch.kbmsKeysRemaining = 0;
    orch.keyDown = false;
    orch.inBurstGap = false;
  }

  // M+K: init sub-FSM for drawing tool
  if (phase == PHASE_MOUSE_KB) {
    orch.mskbSubPhase = MSKB_KEY_DOWN;
    orch.mskbStrokesRemaining = (uint8_t)randRange(MSKB_STROKES_MIN, MSKB_STROKES_MAX);
    orch.mskbKeyHoldTarget = randRange(MSKB_KEY_HOLD_MIN_MS, MSKB_KEY_HOLD_MAX_MS);
    orch.mskbSubPhaseStartMs = now;
    orch.mskbSubPhaseDurMs = MSKB_SETUP_DELAY_MS;
    orch.mskbLastStepMs = now;  // prevent stale timestamp on re-entry
    orch.keyDown = false;
    orch.keyDownMs = now;  // sane baseline if key press is skipped
    pickSwipeDirection(orch.mskbStrokeDx, orch.mskbStrokeDy);
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
    // Press key down — save index before pickNextKey() advances it
    uint8_t pressedKeyIdx = nextKeyIndex;
    sendKeyDown(pressedKeyIdx);
    pickNextKey();
    orch.keyDown = true;
    orch.keyDownMs = now;
    orch.lastSimKeystrokeMs = now;

    // Modifier keys get longer hold — check the PRESSED key, not the next one
    const KeyDef& key = AVAILABLE_KEYS[pressedKeyIdx];
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
  if (settings.phantomClicks && hasPopulatedClickSlot() &&
      prevState == MOUSE_RETURNING && mouseState == MOUSE_IDLE) {
    if (random(100) < 25) {
      uint8_t action = pickNextClick();
      executeClick(action, (uint16_t)randRange(50, 150));
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
// K+M PHASE (form filling: swipe → click → type → repeat)
// ============================================================================

static void tickKbMouse(unsigned long now) {
  unsigned long elapsed = now - orch.kbmsSubPhaseStartMs;

  switch (orch.kbmsSubPhase) {

    case KBMS_MOUSE_SWIPE:
      // Send small mouse deltas at MOUSE_MOVE_STEP_MS intervals
      if (mouseEnabled && elapsed < orch.kbmsSubPhaseDurMs) {
        if (now - orch.kbmsLastStepMs >= MOUSE_MOVE_STEP_MS) {
          sendMouseMove(orch.kbmsSwipeDx, orch.kbmsSwipeDy);
          orch.kbmsLastStepMs = now;
        }
      } else if (elapsed >= orch.kbmsSubPhaseDurMs) {
        orch.kbmsSwipesRemaining--;
        if (orch.kbmsSwipesRemaining > 0) {
          // Inter-swipe pause: random 100-300ms gap, then new direction.
          // Stays in KBMS_MOUSE_SWIPE — the elapsed < dur guard above
          // naturally suppresses mouse moves during the pause window.
          orch.kbmsSubPhaseStartMs = now;
          orch.kbmsSubPhaseDurMs = randRange(KBMS_MOUSE_PAUSE_MIN_MS, KBMS_MOUSE_PAUSE_MAX_MS);
          pickSwipeDirection(orch.kbmsSwipeDx, orch.kbmsSwipeDy);
        } else {
          // All swipes done → click (if phantom clicks enabled) or skip to typing
          if (settings.phantomClicks) {
            orch.kbmsSubPhase = KBMS_CLICK;
            orch.kbmsSubPhaseStartMs = now;
            orch.kbmsSubPhaseDurMs = 0;  // immediate
          } else {
            orch.kbmsSubPhase = KBMS_CLICK_PAUSE;
            orch.kbmsSubPhaseStartMs = now;
            orch.kbmsSubPhaseDurMs = randRange(KBMS_CLICK_PAUSE_MIN_MS, KBMS_CLICK_PAUSE_MAX_MS);
          }
        }
      }
      break;

    case KBMS_CLICK:
      // Send one click to simulate clicking into a form field
      if (hasPopulatedClickSlot()) {
        uint8_t action = pickNextClick();
        executeClick(action, (uint16_t)randRange(50, 100));
        orch.lastPhantomClickMs = now;
      }
      orch.kbmsSubPhase = KBMS_CLICK_PAUSE;
      orch.kbmsSubPhaseStartMs = now;
      orch.kbmsSubPhaseDurMs = randRange(KBMS_CLICK_PAUSE_MIN_MS, KBMS_CLICK_PAUSE_MAX_MS);
      break;

    case KBMS_CLICK_PAUSE:
      if (elapsed >= orch.kbmsSubPhaseDurMs) {
        // Transition to typing
        orch.kbmsSubPhase = KBMS_TYPING;
        orch.kbmsSubPhaseStartMs = now;
        orch.kbmsKeysRemaining = scaleCountByPerformance((uint8_t)randRange(KBMS_KEYS_MIN, KBMS_KEYS_MAX));
        orch.keyDown = false;
        orch.burstKeysRemaining = orch.kbmsKeysRemaining;
        orch.inBurstGap = false;
        orch.nextKeyMs = now;
      }
      break;

    case KBMS_TYPING:
      // Reuse burst typing logic
      if (keyEnabled && hasPopulatedSlot() && orch.burstKeysRemaining > 0) {
        const WorkModeDef& mode = currentWorkMode();
        const PhaseTiming& t = mode.timing[orch.autoProfile];

        if (orch.keyDown) {
          if (now - orch.keyDownMs >= orch.currentKeyHoldMs) {
            sendKeyUp();
            orch.keyDown = false;
            orch.burstKeysRemaining--;
            if (orch.burstKeysRemaining > 0) {
              orch.nextKeyMs = now + scaleByPerformance(randRange(t.interKeyMinMs, t.interKeyMaxMs), false);
            }
          }
        } else if (now >= orch.nextKeyMs && orch.burstKeysRemaining > 0) {
          uint8_t pressedKeyIdx = nextKeyIndex;
          sendKeyDown(pressedKeyIdx);
          pickNextKey();
          orch.keyDown = true;
          orch.keyDownMs = now;
          orch.lastSimKeystrokeMs = now;
          const KeyDef& key = AVAILABLE_KEYS[pressedKeyIdx];
          if (key.isModifier) {
            orch.currentKeyHoldMs = (uint16_t)randRange(150, 400);
          } else {
            orch.currentKeyHoldMs = (uint16_t)randRange(t.keyHoldMinMs, t.keyHoldMaxMs);
          }
        }
      }
      // Check if typing burst complete
      if (orch.burstKeysRemaining == 0 && !orch.keyDown) {
        orch.kbmsSubPhase = KBMS_TYPING_PAUSE;
        orch.kbmsSubPhaseStartMs = now;
        orch.kbmsSubPhaseDurMs = randRange(KBMS_TYPING_PAUSE_MIN_MS, KBMS_TYPING_PAUSE_MAX_MS);
      }
      break;

    case KBMS_TYPING_PAUSE:
      if (elapsed >= orch.kbmsSubPhaseDurMs) {
        // Cycle back to mouse swipe with fresh count
        orch.kbmsSubPhase = KBMS_MOUSE_SWIPE;
        orch.kbmsSwipesRemaining = (uint8_t)randRange(KBMS_SWIPE_MIN, KBMS_SWIPE_MAX);
        pickSwipeDirection(orch.kbmsSwipeDx, orch.kbmsSwipeDy);
        orch.kbmsSubPhaseStartMs = now;
        orch.kbmsSubPhaseDurMs = KBMS_SWIPE_DUR_MS;
      }
      break;
  }
}

// ============================================================================
// M+K PHASE (drawing tool: hold key + mouse strokes)
// ============================================================================

static void tickMouseKb(unsigned long now) {
  unsigned long elapsed = now - orch.mskbSubPhaseStartMs;

  switch (orch.mskbSubPhase) {

    case MSKB_KEY_DOWN:
      // Press key if not already held, then brief setup delay
      if (!orch.keyDown && keyEnabled && hasPopulatedSlot()) {
        sendKeyDown(nextKeyIndex);
        pickNextKey();
        orch.keyDown = true;
        orch.keyDownMs = now;
        orch.lastSimKeystrokeMs = now;
      }
      // Only advance to drawing if key was actually pressed — drawing
      // without a held key is pointless and leaves keyDownMs stale
      if (elapsed >= orch.mskbSubPhaseDurMs && orch.keyDown) {
        orch.mskbSubPhase = MSKB_MOUSE_DRAW;
        orch.mskbSubPhaseStartMs = now;
        orch.mskbSubPhaseDurMs = MSKB_STROKE_DUR_MS;
        pickSwipeDirection(orch.mskbStrokeDx, orch.mskbStrokeDy);
      }
      break;

    case MSKB_MOUSE_DRAW:
      // Send mouse deltas while key is held
      if (mouseEnabled) {
        if (now - orch.mskbLastStepMs >= MOUSE_MOVE_STEP_MS) {
          sendMouseMove(orch.mskbStrokeDx, orch.mskbStrokeDy);
          orch.mskbLastStepMs = now;
        }
      }

      if (elapsed >= orch.mskbSubPhaseDurMs) {
        orch.mskbStrokesRemaining--;
        // Check if key hold target reached or strokes exhausted
        unsigned long keyHeldMs = now - orch.keyDownMs;
        if (orch.mskbStrokesRemaining == 0 || keyHeldMs >= orch.mskbKeyHoldTarget) {
          // Release key and pause
          if (orch.keyDown) {
            sendKeyUp();
            orch.keyDown = false;
          }
          orch.mskbSubPhase = MSKB_KEY_UP_PAUSE;
          orch.mskbSubPhaseStartMs = now;
          orch.mskbSubPhaseDurMs = randRange(MSKB_PAUSE_MIN_MS, MSKB_PAUSE_MAX_MS);
        } else {
          // Next stroke: new direction, reset timer
          pickSwipeDirection(orch.mskbStrokeDx, orch.mskbStrokeDy);
          orch.mskbSubPhaseStartMs = now;
          orch.mskbSubPhaseDurMs = MSKB_STROKE_DUR_MS;
        }
      }
      break;

    case MSKB_KEY_UP_PAUSE:
      if (elapsed >= orch.mskbSubPhaseDurMs) {
        // Start new cycle: press new key, draw again
        orch.mskbSubPhase = MSKB_KEY_DOWN;
        orch.mskbSubPhaseStartMs = now;
        orch.mskbSubPhaseDurMs = MSKB_SETUP_DELAY_MS;
        orch.mskbStrokesRemaining = (uint8_t)randRange(MSKB_STROKES_MIN, MSKB_STROKES_MAX);
        orch.mskbKeyHoldTarget = randRange(MSKB_KEY_HOLD_MIN_MS, MSKB_KEY_HOLD_MAX_MS);
      }
      break;
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

    // Skip lunch block when lunch is disabled (short shift)
    if (!lunchEnabled() && nextBlock < tmpl.numBlocks && tmpl.blocks[nextBlock].isLunch) {
      nextBlock = (nextBlock + 1) % tmpl.numBlocks;
    }

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

    // Day rollover: re-align to wall clock if time is synced
    if (nextBlock == 0 && timeSynced) {
      uint32_t daySecs = currentDaySeconds();
      if (daySecs != 0xFFFFFFFF) {
        syncOrchestratorTime(daySecs);
      } else {
        startBlock(nextBlock, now);
        startMode(now);
      }
    } else {
      startBlock(nextBlock, now);
      startMode(now);
    }
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

  skipBlockTransition:

  // 2. Check mode timer (independent of block/lunch chain — must always run)
  if (now - orch.modeStartMs >= orch.modeDurationMs) {
    startMode(now);
  }

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
    case PHASE_KB_MOUSE:
      tickKbMouse(now);
      break;
    case PHASE_MOUSE_KB:
      tickMouseKb(now);
      break;
    default:
      break;
  }

  // 6. Keystroke keepalive — ensure non-zero keyboard in every 2-min window
  if (keyEnabled && hasPopulatedSlot() && !phaseHasKeystrokes(orch.phase)) {
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
  if (daySeconds < schedStartSecs) {
    // Before job start — start fresh day at block 0
    unsigned long now = millis();
    orch.dayStartMs = now;
    orch.lunchBlockIdx = findLunchBlockIdx();
    orch.lunchCompleted = false;
    startBlock(0, now);
    startMode(now);
    Serial.println("[SIM] Time synced: before job start, reset to block 0");
    return;
  }

  uint32_t offsetMin = (daySeconds - schedStartSecs) / 60;

  // Find the correct block for this time offset using cumulative scaled durations
  uint16_t cumulative = 0;
  for (uint8_t i = 0; i < tmpl.numBlocks; i++) {
    // Skip lunch block if lunch disabled
    if (!lunchEnabled() && tmpl.blocks[i].isLunch) continue;

    uint16_t dur = scaledBlockDurationMin(i);
    uint16_t blockEnd = cumulative + dur;
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
    cumulative = blockEnd;
  }

  // Past all blocks — stay on last block
  unsigned long now = millis();
  orch.dayStartMs = now - (unsigned long)offsetMin * 60000UL;
  orch.lunchBlockIdx = findLunchBlockIdx();
  orch.lunchCompleted = true;  // past all blocks = past lunch
  startBlock(tmpl.numBlocks - 1, now);
  startMode(now);
}

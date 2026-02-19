#include "mouse.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "hid.h"
#include "serial_cmd.h"
#include <math.h>

// ============================================================================
// Brownian mode helpers (existing)
// ============================================================================

void pickNewDirection() {
  int dir = random(NUM_DIRS);
  currentMouseDx = MOUSE_DIRS[dir][0];
  currentMouseDy = MOUSE_DIRS[dir][1];
}

// ============================================================================
// Bezier sweep state (file-static)
// ============================================================================

enum SweepPhase { SWEEP_PLANNING, SWEEP_MOVING, SWEEP_PAUSING };
static SweepPhase sweepPhase;

// Bezier control points (fixed-point: 8 fractional bits for sub-pixel accuracy)
static int32_t bzP0x, bzP0y;   // start
static int32_t bzP1x, bzP1y;   // control
static int32_t bzP2x, bzP2y;   // end
static int32_t bzLastX, bzLastY; // last evaluated position (fractional)

static uint16_t sweepStepCount;    // total steps for this sweep
static uint16_t sweepStepCurrent;  // current step index
static unsigned long sweepPauseEnd;

// Pick a random sweep radius with weighted distribution:
// ~40% small (20-60px), ~40% medium (60-180px), ~20% large (150-350px)
static int16_t randomSweepRadius() {
  int r = random(100);
  if (r < 40)      return 20 + random(41);    // 20-60px
  else if (r < 80) return 60 + random(121);   // 60-180px
  else              return 150 + random(201);  // 150-350px
}

// Alpha-max-beta-min integer distance approximation (avoids sqrt/float)
static int16_t approxDist(int16_t dx, int16_t dy) {
  int16_t ax = abs(dx);
  int16_t ay = abs(dy);
  // max + 3/8*min approximation, ~3% error
  if (ax > ay) return ax + (ay * 3 / 8);
  else         return ay + (ax * 3 / 8);
}

// Map time progress [0..1] through trapezoidal velocity profile
// Accel 20%, cruise 60%, decel 20%
// Returns Bezier t parameter [0..1]
static float timeToParam(float progress) {
  if (progress <= 0.0f) return 0.0f;
  if (progress >= 1.0f) return 1.0f;

  // Trapezoidal velocity: accel 0-0.2, cruise 0.2-0.8, decel 0.8-1.0
  // Area under trapezoid: 0.5*0.2 + 0.6 + 0.5*0.2 = 0.8
  // Normalized so total area = 1.0
  // Accel peak velocity = 1/0.8 = 1.25
  float t;
  if (progress < 0.2f) {
    // Quadratic ramp up: integral of linear from 0 to v_max
    float p = progress / 0.2f;  // 0..1 in accel phase
    t = 0.125f * p * p;         // 0..0.125
  } else if (progress < 0.8f) {
    // Linear cruise: v_max * dt
    float p = (progress - 0.2f) / 0.6f;  // 0..1 in cruise phase
    t = 0.125f + 0.75f * p;               // 0.125..0.875
  } else {
    // Quadratic ramp down: integral of linear from v_max to 0
    float p = (progress - 0.8f) / 0.2f;  // 0..1 in decel phase
    t = 0.875f + 0.125f * (2.0f * p - p * p);  // 0.875..1.0
  }
  return t;
}

// Plan a new Bezier sweep from current position
static void planNextSweep() {
  int16_t radius = randomSweepRadius();
  int16_t driftLimit = radius * SWEEP_DRIFT_FACTOR;

  // Random target angle and distance
  float angle = (float)random(360) * PI / 180.0f;
  int16_t dist = radius / 2 + random(radius);

  int16_t targetX = (int16_t)(cosf(angle) * dist);
  int16_t targetY = (int16_t)(sinf(angle) * dist);

  // Apply as offset from current net position, then clamp to drift limit
  int16_t absX = (int16_t)(mouseNetX + targetX);
  int16_t absY = (int16_t)(mouseNetY + targetY);
  if (absX > driftLimit) absX = driftLimit;
  if (absX < -driftLimit) absX = -driftLimit;
  if (absY > driftLimit) absY = driftLimit;
  if (absY < -driftLimit) absY = -driftLimit;

  // Actual delta from current position
  int16_t dx = absX - (int16_t)mouseNetX;
  int16_t dy = absY - (int16_t)mouseNetY;

  // Perpendicular control point offset for natural curve
  int16_t perpX = -dy / 3 + (int16_t)random(-radius / 4, radius / 4 + 1);
  int16_t perpY =  dx / 3 + (int16_t)random(-radius / 4, radius / 4 + 1);

  // Set Bezier points (shifted left 8 bits for fractional precision)
  bzP0x = 0;
  bzP0y = 0;
  bzP1x = (int32_t)(dx / 2 + perpX) << 8;
  bzP1y = (int32_t)(dy / 2 + perpY) << 8;
  bzP2x = (int32_t)dx << 8;
  bzP2y = (int32_t)dy << 8;
  bzLastX = 0;
  bzLastY = 0;

  // Duration based on distance and random speed
  int16_t totalDist = approxDist(dx, dy);
  if (totalDist < 5) totalDist = 5;  // minimum distance
  int16_t speed = SWEEP_SPEED_MIN + random(SWEEP_SPEED_MAX - SWEEP_SPEED_MIN + 1);
  unsigned long durationMs = (unsigned long)totalDist * 1000UL / speed;
  if (durationMs < 150) durationMs = 150;
  if (durationMs > 3000) durationMs = 3000;

  sweepStepCount = (uint16_t)(durationMs / MOUSE_MOVE_STEP_MS);
  if (sweepStepCount < 2) sweepStepCount = 2;
  sweepStepCurrent = 0;
}

// Evaluate Bezier at current step, send mouse delta, advance
static void evaluateBezierStep() {
  sweepStepCurrent++;
  float progress = (float)sweepStepCurrent / (float)sweepStepCount;
  float t = timeToParam(progress);

  // Quadratic Bezier: B(t) = (1-t)^2*P0 + 2(1-t)t*P1 + t^2*P2
  float omt = 1.0f - t;
  int32_t curX = (int32_t)(omt * omt * bzP0x + 2.0f * omt * t * bzP1x + t * t * bzP2x);
  int32_t curY = (int32_t)(omt * omt * bzP0y + 2.0f * omt * t * bzP1y + t * t * bzP2y);

  // Delta from last position (still in fixed-point)
  int32_t deltaX = curX - bzLastX;
  int32_t deltaY = curY - bzLastY;
  bzLastX = curX;
  bzLastY = curY;

  // Convert from fixed-point to integer pixels (round)
  int8_t dx = (int8_t)((deltaX + 128) >> 8);
  int8_t dy = (int8_t)((deltaY + 128) >> 8);

  if (dx != 0 || dy != 0) {
    sendMouseMove(dx, dy);
    mouseNetX += dx;
    mouseNetY += dy;
  }
}

// ============================================================================
// Main state machine
// ============================================================================

void handleMouseStateMachine(unsigned long now) {
  unsigned long elapsed = now - lastMouseStateChange;

  switch (mouseState) {
    case MOUSE_IDLE:
      if (elapsed >= currentMouseIdle) {
        mouseState = MOUSE_JIGGLING;
        lastMouseStateChange = now;
        lastMouseStep = now;
        mouseNetX = 0;
        mouseNetY = 0;
        lastScrollTime = now;
        nextScrollInterval = random(SCROLL_INTERVAL_MIN_MS, SCROLL_INTERVAL_MAX_MS + 1);
        sweepPhase = SWEEP_PLANNING;  // Bezier starts fresh
        pickNewDirection();            // Brownian needs initial direction
        scheduleNextMouseState();
        pushSerialStatus();
      }
      break;

    case MOUSE_JIGGLING:
      // Random scroll injection (applies to both Bezier and Brownian)
      if (settings.scrollEnabled && (now - lastScrollTime >= nextScrollInterval)) {
        sendMouseScroll(random(2) ? 1 : -1);
        lastScrollTime = now;
        nextScrollInterval = random(SCROLL_INTERVAL_MIN_MS, SCROLL_INTERVAL_MAX_MS + 1);
      }
      if (elapsed >= currentMouseJiggle) {
        mouseState = MOUSE_RETURNING;
        mouseReturnTotal = abs(mouseNetX) + abs(mouseNetY);
        lastMouseStateChange = now;
        lastMouseStep = now;
        pushSerialStatus();
      } else if (settings.mouseStyle == 0) {
        // ---- Bezier sweep mode ----
        switch (sweepPhase) {
          case SWEEP_PLANNING:
            planNextSweep();
            sweepPhase = SWEEP_MOVING;
            break;

          case SWEEP_MOVING:
            if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
              evaluateBezierStep();
              lastMouseStep = now;
              if (sweepStepCurrent >= sweepStepCount) {
                // Sweep complete — enter pause
                sweepPhase = SWEEP_PAUSING;
                unsigned long pauseLen;
                if ((int)random(100) < SWEEP_LONG_PAUSE_PCT) {
                  pauseLen = SWEEP_PAUSE_MAX_MS + random(SWEEP_LONG_PAUSE_MS - SWEEP_PAUSE_MAX_MS + 1);
                } else {
                  pauseLen = SWEEP_PAUSE_MIN_MS + random(SWEEP_PAUSE_MAX_MS - SWEEP_PAUSE_MIN_MS + 1);
                }
                sweepPauseEnd = now + pauseLen;
              }
            }
            break;

          case SWEEP_PAUSING:
            if (now >= sweepPauseEnd) {
              sweepPhase = SWEEP_PLANNING;  // Plan next sweep
            }
            break;
        }
      } else {
        // ---- Brownian mode (original) ----
        if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
          if (random(100) < 15) pickNewDirection();
          // Ease-in-out: sine curve ramps amplitude 0 -> peak -> 0
          float progress = (float)elapsed / (float)currentMouseJiggle;
          float ease = sinf(PI * progress);
          int8_t amp = (int8_t)(settings.mouseAmplitude * ease + 0.5f);
          if (amp > 0) {
            int8_t dx = currentMouseDx * amp;
            int8_t dy = currentMouseDy * amp;
            sendMouseMove(dx, dy);
            mouseNetX += dx;
            mouseNetY += dy;
          }
          lastMouseStep = now;
        }
      }
      break;

    case MOUSE_RETURNING:
      if (mouseNetX == 0 && mouseNetY == 0) {
        mouseState = MOUSE_IDLE;
        lastMouseStateChange = now;
        scheduleNextMouseState();
        pushSerialStatus();
        mouseJiggleCount++;
        if (mouseJiggleCount % EASTER_EGG_INTERVAL == 0
            && (deviceConnected || usbConnected) && currentMode == MODE_NORMAL && !screensaverActive) {
          easterEggActive = true;
          easterEggFrame = 0;
        }
      } else if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
        int8_t dx = 0, dy = 0;
        if (mouseNetX > 0) { dx = -min((int32_t)5, mouseNetX); mouseNetX += dx; }
        else if (mouseNetX < 0) { dx = min((int32_t)5, -mouseNetX); mouseNetX += dx; }
        if (mouseNetY > 0) { dy = -min((int32_t)5, mouseNetY); mouseNetY += dy; }
        else if (mouseNetY < 0) { dy = min((int32_t)5, -mouseNetY); mouseNetY += dy; }
        if (dx != 0 || dy != 0) sendMouseMove(dx, dy);
        lastMouseStep = now;
      }
      break;
  }
}

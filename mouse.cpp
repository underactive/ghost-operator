#include "mouse.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include <math.h>

void pickNewDirection() {
  int dir = random(NUM_DIRS);
  currentMouseDx = MOUSE_DIRS[dir][0];
  currentMouseDy = MOUSE_DIRS[dir][1];
}

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
        pickNewDirection();
        scheduleNextMouseState();
      }
      break;

    case MOUSE_JIGGLING:
      if (elapsed >= currentMouseJiggle) {
        mouseState = MOUSE_RETURNING;
        mouseReturnTotal = abs(mouseNetX) + abs(mouseNetY);
        lastMouseStateChange = now;
        lastMouseStep = now;
      } else {
        if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
          if (random(100) < 15) pickNewDirection();
          // Ease-in-out: sine curve ramps amplitude 0 -> peak -> 0
          float progress = (float)elapsed / (float)currentMouseJiggle;
          float ease = sinf(PI * progress);
          int8_t amp = (int8_t)(settings.mouseAmplitude * ease + 0.5f);
          if (amp > 0) {
            int8_t dx = currentMouseDx * amp;
            int8_t dy = currentMouseDy * amp;
            blehid.mouseMove(dx, dy);
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
      } else if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
        int8_t dx = 0, dy = 0;
        if (mouseNetX > 0) { dx = -min((int32_t)5, mouseNetX); mouseNetX += dx; }
        else if (mouseNetX < 0) { dx = min((int32_t)5, -mouseNetX); mouseNetX += dx; }
        if (mouseNetY > 0) { dy = -min((int32_t)5, mouseNetY); mouseNetY += dy; }
        else if (mouseNetY < 0) { dy = min((int32_t)5, -mouseNetY); mouseNetY += dy; }
        if (dx != 0 || dy != 0) blehid.mouseMove(dx, dy);
        lastMouseStep = now;
      }
      break;
  }
}

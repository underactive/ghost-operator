#ifndef GHOST_CYD_TOUCH_H
#define GHOST_CYD_TOUCH_H

#include "config.h"

// ============================================================================
// Touch input — XPT2046 resistive touchscreen on CYD
// ============================================================================

enum TouchGesture {
  GESTURE_NONE,
  GESTURE_TAP,
  GESTURE_LONG_PRESS,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN
};

struct TouchEvent {
  TouchGesture gesture;
  int16_t x, y;        // calibrated screen coordinates
  int16_t dy;           // swipe delta (for SWIPE_UP/DOWN)
};

// Rectangular touch target with callback
typedef void (*TouchCallback)();
struct TouchTarget {
  int16_t x, y, w, h;
  TouchCallback onTap;
};

// Initialize XPT2046 touchscreen
void setupTouch();

// Poll touch and return gesture event (GESTURE_NONE if no event)
TouchEvent pollTouch();

// Check if a point is inside a target rect
bool hitTest(int16_t px, int16_t py, const TouchTarget& target);

#endif // GHOST_CYD_TOUCH_H

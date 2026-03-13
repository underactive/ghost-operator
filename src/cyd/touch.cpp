#include "touch.h"
#include "state.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ============================================================================
// XPT2046 touch driver + gesture detection
//
// CYD has touch on separate SPI pins (CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36).
// The XPT2046_Touchscreen library (PlatformIO version) uses the default SPI
// object internally. We redirect default SPI to the CYD touch pins.
//
// Resistive touch is noisy — we use multi-sample averaging with outlier
// rejection to get stable coordinates.
// ============================================================================

static XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// Calibration constants (CYD 2.8" ILI9341, landscape)
#define TOUCH_X_MIN   200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN   200
#define TOUCH_Y_MAX   3800

// Gesture detection thresholds
#define TOUCH_DEBOUNCE_MS     50
#define TAP_MAX_MS            400
#define TAP_MAX_MOVE          12
#define LONG_PRESS_MS         800
#define SWIPE_MIN_PX          20

// Multi-sample noise rejection
#define TOUCH_SAMPLES         5

// Touch state machine
static bool wasTouched = false;
static unsigned long touchStartMs = 0;
static int16_t touchStartX = 0, touchStartY = 0;
static int16_t touchLastX = 0, touchLastY = 0;
static unsigned long lastEventMs = 0;

void setupTouch() {
  SPI.end();
  SPI.begin(25, 39, 32, PIN_TOUCH_CS);  // CLK, MISO, MOSI, CS
  ts.begin();
  ts.setRotation(1);  // landscape
  Serial.println("[OK] Touch initialized");
}

// Map raw touch coordinates to screen pixels (landscape orientation)
static void mapToScreen(int16_t rawX, int16_t rawY, int16_t &screenX, int16_t &screenY) {
  screenX = (int16_t)constrain(map(rawX, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
  screenY = (int16_t)constrain(map(rawY, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
}

// Read touch with multi-sample averaging and outlier rejection.
// Takes TOUCH_SAMPLES readings, discards min and max on each axis,
// averages the rest. Returns false if touch is not pressed.
static bool readTouchFiltered(int16_t &sx, int16_t &sy) {
  if (!ts.touched()) return false;

  int16_t xSamples[TOUCH_SAMPLES], ySamples[TOUCH_SAMPLES];
  int validCount = 0;

  for (int i = 0; i < TOUCH_SAMPLES; i++) {
    if (!ts.touched()) break;
    TS_Point p = ts.getPoint();
    if (p.z > 50) {  // pressure threshold — reject ghost touches
      xSamples[validCount] = p.x;
      ySamples[validCount] = p.y;
      validCount++;
    }
  }

  if (validCount < 3) return false;  // not enough valid samples

  // Sort to find and discard outliers (simple insertion sort, N<=5)
  for (int i = 1; i < validCount; i++) {
    int16_t kx = xSamples[i], ky = ySamples[i];
    int j = i - 1;
    while (j >= 0 && xSamples[j] > kx) { xSamples[j + 1] = xSamples[j]; j--; }
    xSamples[j + 1] = kx;
    // Sort Y independently
    j = i - 1;
    while (j >= 0 && ySamples[j] > ky) { ySamples[j + 1] = ySamples[j]; j--; }
    ySamples[j + 1] = ky;
  }

  // Average the middle samples (drop min and max)
  int32_t sumX = 0, sumY = 0;
  int midCount = validCount - 2;  // drop first and last (min/max)
  if (midCount < 1) midCount = 1;
  for (int i = 1; i <= midCount; i++) {
    sumX += xSamples[i];
    sumY += ySamples[i];
  }

  int16_t rawX = (int16_t)(sumX / midCount);
  int16_t rawY = (int16_t)(sumY / midCount);
  mapToScreen(rawX, rawY, sx, sy);
  return true;
}

TouchEvent pollTouch() {
  TouchEvent evt = { GESTURE_NONE, 0, 0, 0 };
  unsigned long now = millis();

  // Debounce
  if (now - lastEventMs < TOUCH_DEBOUNCE_MS) return evt;

  bool touched = ts.touched();

  if (touched && !wasTouched) {
    // Touch down — read filtered position
    int16_t sx, sy;
    if (readTouchFiltered(sx, sy)) {
      touchStartX = sx;
      touchStartY = sy;
      touchLastX = sx;
      touchLastY = sy;
      touchStartMs = now;
      wasTouched = true;
    }
  } else if (touched && wasTouched) {
    // Touch held — update position with filtering
    int16_t sx, sy;
    if (readTouchFiltered(sx, sy)) {
      touchLastX = sx;
      touchLastY = sy;
    }

    // Check for long press
    if (now - touchStartMs >= LONG_PRESS_MS) {
      int16_t dx = abs(touchLastX - touchStartX);
      int16_t dy = abs(touchLastY - touchStartY);
      if (dx < TAP_MAX_MOVE && dy < TAP_MAX_MOVE) {
        evt.gesture = GESTURE_LONG_PRESS;
        evt.x = touchLastX;
        evt.y = touchLastY;
        wasTouched = false;
        lastEventMs = now;
        return evt;
      }
    }
  } else if (!touched && wasTouched) {
    // Touch up — determine gesture
    wasTouched = false;
    lastEventMs = now;
    unsigned long duration = now - touchStartMs;
    int16_t dx = touchLastX - touchStartX;
    int16_t dy = touchLastY - touchStartY;

    if (abs(dy) >= SWIPE_MIN_PX && abs(dy) > abs(dx)) {
      evt.gesture = (dy < 0) ? GESTURE_SWIPE_UP : GESTURE_SWIPE_DOWN;
      evt.x = touchStartX;
      evt.y = touchStartY;
      evt.dy = dy;
    } else if (duration <= TAP_MAX_MS && abs(dx) < TAP_MAX_MOVE && abs(dy) < TAP_MAX_MOVE) {
      evt.gesture = GESTURE_TAP;
      evt.x = touchStartX;
      evt.y = touchStartY;
    }
  }

  return evt;
}

bool hitTest(int16_t px, int16_t py, const TouchTarget& target) {
  return px >= target.x && px < target.x + target.w &&
         py >= target.y && py < target.y + target.h;
}

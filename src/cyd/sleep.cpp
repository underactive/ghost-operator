#include "config.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "platform_hal.h"

// ============================================================================
// CYD sleep — screen dim/off only (no deep sleep, USB-powered)
// ============================================================================

void enterDeepSleep() {
  // CYD has no battery — "deep sleep" = screen off + stop BLE
  Serial.println("[Sleep] Screen off (CYD has no deep sleep)");
  digitalWrite(PIN_TFT_BACKLIGHT, LOW);
  // TODO: Phase 4 — stop NimBLE advertising
}

void enterLightSleep(bool scheduled) {
  scheduleSleeping = true;
  manualLightSleep = !scheduled;

  // Turn off backlight
  digitalWrite(PIN_TFT_BACKLIGHT, LOW);

  // TODO: Phase 4 — stop NimBLE advertising + disconnect
  Serial.println(scheduled ? "[Schedule] Light sleep entered" : "[Manual] Light sleep entered");
}

void exitLightSleep() {
  scheduleSleeping = false;
  manualLightSleep = false;
  scheduleManualWake = true;

  // Turn backlight back on
  digitalWrite(PIN_TFT_BACKLIGHT, HIGH);

  // TODO: Phase 4 — restart NimBLE advertising

  // Reset timers
  unsigned long now = millis();
  lastKeyTime = now;
  lastMouseStateChange = now;
  mouseState = MOUSE_IDLE;
  mouseNetX = 0;
  mouseNetY = 0;
  mouseReturnTotal = 0;
  scheduleNextKey();
  scheduleNextMouseState();
  markDisplayDirty();

  Serial.println("[Schedule] Light sleep exited");
  pushSerialStatus();
}

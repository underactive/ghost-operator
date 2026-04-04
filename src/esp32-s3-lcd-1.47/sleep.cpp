#include <Arduino.h>
#include "sleep.h"
#include "state.h"
#include "settings.h"
#include "ble.h"
#include "display.h"
#include "led.h"
#include "platform_hal.h"

// ============================================================================
// Sleep for ESP32-S3
// No battery — sleep stops BLE, turns off backlight and LED
// ============================================================================

void enterLightSleep(bool scheduled) {
  (void)scheduled;
  scheduleSleeping = true;
  stopAdvertising();
  setBacklightOff();
  setStatusLed();  // force LED update (will turn off)
  Serial.println("[SLEEP] Entering light sleep");
  markDisplayDirty();
}

void exitLightSleep() {
  scheduleSleeping = false;
  startAdvertising();
  setBacklightBrightness(settings.displayBrightness);
  setStatusLed();  // force LED update
  Serial.println("[SLEEP] Exiting light sleep");
  markDisplayDirty();
}

void enterDeepSleep() {
  // On S3 with no battery, deep sleep = light sleep
  enterLightSleep(true);
}

#include "config.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "platform_hal.h"
#include <NimBLEDevice.h>

// ============================================================================
// CYD sleep — backlight PWM + BLE control (no deep sleep, USB-powered)
// ============================================================================

void setupBacklight() {
  pinMode(PIN_TFT_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_TFT_BACKLIGHT, HIGH);
}

void setBacklightBrightness(uint8_t percent) {
  // Simple on/off — CYD backlight doesn't reliably support PWM dimming
  digitalWrite(PIN_TFT_BACKLIGHT, (percent > 0) ? HIGH : LOW);
}

void enterDeepSleep() {
  Serial.println("[Sleep] Screen off (CYD — no true deep sleep)");
  digitalWrite(PIN_TFT_BACKLIGHT, LOW);
  NimBLEDevice::stopAdvertising();
  if (deviceConnected) {
    NimBLEDevice::getServer()->disconnect(0);
  }
}

void enterLightSleep(bool scheduled) {
  scheduleSleeping = true;
  manualLightSleep = !scheduled;

  digitalWrite(PIN_TFT_BACKLIGHT, LOW);

  NimBLEDevice::stopAdvertising();
  if (deviceConnected) {
    NimBLEDevice::getServer()->disconnect(0);
  }

  Serial.println(scheduled ? "[Schedule] Light sleep entered" : "[Manual] Light sleep entered");
}

void exitLightSleep() {
  scheduleSleeping = false;
  manualLightSleep = false;
  scheduleManualWake = true;

  digitalWrite(PIN_TFT_BACKLIGHT, HIGH);

  NimBLEDevice::startAdvertising();

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

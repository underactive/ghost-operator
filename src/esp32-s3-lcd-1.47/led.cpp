#include <Arduino.h>
#include "led.h"
#include "config.h"
#include "state.h"
#include <Adafruit_NeoPixel.h>

// ============================================================================
// NeoPixel RGB LED on GPIO38 (Waveshare ESP32-S3-LCD-1.47)
// ============================================================================

static Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_RGB + NEO_KHZ800);

static unsigned long kbFlashStart = 0;
static unsigned long msFlashStart = 0;
static bool kbFlashing = false;
static bool msFlashing = false;
#define LED_FLASH_MS 80

// Status LED state
static unsigned long lastStatusUpdate = 0;
#define STATUS_UPDATE_MS 500

void setupLed() {
  pixel.begin();
  pixel.setBrightness(30);
  pixel.setPixelColor(0, 0);
  pixel.show();
}

void flashKbLed() {
  if (!settings.activityLeds) return;
  kbFlashStart = millis();
  kbFlashing = true;
  // Show immediately: purple if mouse also active, else blue
  uint32_t col = msFlashing ? pixel.Color(128, 0, 255) : pixel.Color(0, 0, 255);
  pixel.setPixelColor(0, col);
  pixel.show();
}

void flashMouseLed() {
  if (!settings.activityLeds) return;
  msFlashStart = millis();
  msFlashing = true;
  // Show immediately: purple if keyboard also active, else green
  uint32_t col = kbFlashing ? pixel.Color(128, 0, 255) : pixel.Color(0, 255, 0);
  pixel.setPixelColor(0, col);
  pixel.show();
}

void tickLed() {
  unsigned long now = millis();

  // Expire individual flashes
  if (kbFlashing && (now - kbFlashStart >= LED_FLASH_MS)) kbFlashing = false;
  if (msFlashing && (now - msFlashStart >= LED_FLASH_MS)) msFlashing = false;

  // Update combined flash color
  bool anyFlash = kbFlashing || msFlashing;
  if (anyFlash) {
    uint32_t col;
    if (kbFlashing && msFlashing) col = pixel.Color(128, 0, 255);  // purple
    else if (kbFlashing)          col = pixel.Color(0, 0, 255);    // blue
    else                          col = pixel.Color(0, 255, 0);    // green
    pixel.setPixelColor(0, col);
    pixel.show();
    return;  // flash takes priority over status
  }

  // Status LED (when not flashing) — update at STATUS_UPDATE_MS interval
  if (now - lastStatusUpdate >= STATUS_UPDATE_MS) {
    lastStatusUpdate = now;
    if (scheduleSleeping) {
      pixel.setPixelColor(0, 0);  // off during sleep
    } else if (usbHostConnected) {
      pixel.setPixelColor(0, pixel.Color(20, 20, 20));  // dim white = USB connected
    } else if (deviceConnected) {
      pixel.setPixelColor(0, pixel.Color(20, 20, 0));  // dim yellow = BLE connected
    } else {
      // Breathing blue (advertising)
      static bool blinkState = false;
      blinkState = !blinkState;
      pixel.setPixelColor(0, blinkState ? pixel.Color(0, 0, 30) : 0);
    }
    pixel.show();
  }
}

void setStatusLed() {
  lastStatusUpdate = 0;  // force update on next tick
}

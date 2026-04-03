#include <Arduino.h>
#include "led.h"
#include "config.h"
#include "state.h"
#include <Adafruit_NeoPixel.h>

// ============================================================================
// NeoPixel RGB LED on GPIO8 (Waveshare ESP32-C6-LCD-1.47)
// ============================================================================

static Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static unsigned long ledFlashStart = 0;
static uint32_t ledFlashColor = 0;
static bool ledFlashing = false;
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
  ledFlashColor = pixel.Color(0, 0, 255);  // blue = keyboard
  ledFlashStart = millis();
  ledFlashing = true;
  pixel.setPixelColor(0, ledFlashColor);
  pixel.show();
}

void flashMouseLed() {
  if (!settings.activityLeds) return;
  ledFlashColor = pixel.Color(0, 255, 0);  // green = mouse
  ledFlashStart = millis();
  ledFlashing = true;
  pixel.setPixelColor(0, ledFlashColor);
  pixel.show();
}

void tickLed() {
  unsigned long now = millis();

  // Turn off flash after duration
  if (ledFlashing && (now - ledFlashStart >= LED_FLASH_MS)) {
    ledFlashing = false;
    pixel.setPixelColor(0, 0);
    pixel.show();
  }

  // Status LED (when not flashing) — update at STATUS_UPDATE_MS interval
  if (!ledFlashing && (now - lastStatusUpdate >= STATUS_UPDATE_MS)) {
    lastStatusUpdate = now;
    if (scheduleSleeping) {
      pixel.setPixelColor(0, 0);  // off during sleep
    } else if (deviceConnected) {
      pixel.setPixelColor(0, pixel.Color(0, 20, 0));  // dim green = connected
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

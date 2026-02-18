#include "serial_cmd.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "screenshot.h"

void printStatus() {
  Serial.println("\n=== Status ===");
  Serial.print("Mode: "); Serial.println(MODE_NAMES[currentMode]);
  Serial.print("Connected: "); Serial.println(deviceConnected ? "YES" : "NO");
  Serial.print("Keys ("); Serial.print(keyEnabled ? "ON" : "OFF"); Serial.print("): ");
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (i > 0) Serial.print(" ");
    if (i == activeSlot) Serial.print("[");
    Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
    if (i == activeSlot) Serial.print("]");
  }
  Serial.println();
  Serial.print("Mouse: "); Serial.println(mouseEnabled ? "ON" : "OFF");
  Serial.print("Mouse state: ");
  Serial.println(mouseState == MOUSE_IDLE ? "IDLE" : mouseState == MOUSE_JIGGLING ? "JIG" : "RTN");
  Serial.print("Battery: "); Serial.print(batteryPercent); Serial.println("%");
}

void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        Serial.println("\n=== Commands ===");
        Serial.println("s - Status");
        Serial.println("z - Sleep");
        Serial.println("d - Dump settings");
        Serial.println("p - PNG screenshot");
        Serial.println("v - Screensaver");
        break;
      case 'p':
        serialScreenshot();
        break;
      case 'v':
        if (currentMode != MODE_NORMAL) {
          menuEditing = false;
          currentMode = MODE_NORMAL;
        }
        screensaverActive = true;
        Serial.println("Screensaver activated");
        break;
      case 's':
        printStatus();
        break;
      case 'z':
        sleepPending = true;
        break;
      case 'd':
        Serial.println("\n=== Settings ===");
        Serial.print("Key MIN: "); Serial.println(settings.keyIntervalMin);
        Serial.print("Key MAX: "); Serial.println(settings.keyIntervalMax);
        Serial.print("Mouse Jig: "); Serial.println(settings.mouseJiggleDuration);
        Serial.print("Mouse Idle: "); Serial.println(settings.mouseIdleDuration);
        Serial.print("Slots: ");
        for (int i = 0; i < NUM_SLOTS; i++) {
          if (i > 0) Serial.print(", ");
          Serial.print(i); Serial.print("=");
          Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
        }
        Serial.println();
        Serial.print("Profile: "); Serial.println(PROFILE_NAMES[currentProfile]);
        Serial.print("Lazy %: "); Serial.println(settings.lazyPercent);
        Serial.print("Busy %: "); Serial.println(settings.busyPercent);
        Serial.print("Effective KB: "); Serial.print(effectiveKeyMin()); Serial.print("-"); Serial.println(effectiveKeyMax());
        Serial.print("Effective Mouse: "); Serial.print(effectiveMouseJiggle()); Serial.print("/"); Serial.println(effectiveMouseIdle());
        Serial.print("Mouse amplitude: "); Serial.print(settings.mouseAmplitude); Serial.println("px");
        Serial.print("Display brightness: "); Serial.print(settings.displayBrightness); Serial.println("%");
        Serial.print("Screensaver: "); Serial.print(SAVER_NAMES[settings.saverTimeout]);
        Serial.print(", brightness: "); Serial.print(settings.saverBrightness); Serial.print("%");
        Serial.print(" (active: "); Serial.print(screensaverActive ? "YES" : "NO"); Serial.println(")");
        Serial.print("Device name: "); Serial.println(settings.deviceName);
        break;
    }
  }
}

#include "serial_cmd.h"
#include "ble_uart.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "screenshot.h"

// Line buffer for protocol commands (?/=/!) arriving over USB serial
#define SERIAL_BUF_SIZE 128
static char serialBuf[SERIAL_BUF_SIZE];
static uint8_t serialBufPos = 0;

// Serial response writer — plain println, no chunking needed for USB
static void serialWrite(const String& msg) {
  Serial.println(msg);
}

void printStatus() {
  Serial.println("\n=== Status ===");
  Serial.print("Mode: "); Serial.println(MODE_NAMES[currentMode]);
  Serial.print("Connected: "); Serial.println(deviceConnected ? "YES" : "NO");
  Serial.print("USB: "); Serial.println(usbConnected ? "YES" : "NO");
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
  while (Serial.available()) {
    char c = Serial.read();

    // If we're accumulating a protocol command, keep buffering
    if (serialBufPos > 0) {
      if (c == '\n' || c == '\r') {
        serialBuf[serialBufPos] = '\0';
        processCommand(serialBuf, serialWrite);
        serialBufPos = 0;
      } else if (serialBufPos < SERIAL_BUF_SIZE - 1) {
        serialBuf[serialBufPos++] = c;
      }
      continue;
    }

    // First character — decide: protocol command or single-char debug?
    if (c == '?' || c == '=' || c == '!') {
      serialBuf[0] = c;
      serialBufPos = 1;
      continue;
    }

    // Skip bare newlines/carriage returns
    if (c == '\n' || c == '\r') continue;

    // Single-char debug commands (unchanged)
    switch (c) {
      case 'h':
        Serial.println("\n=== Commands ===");
        Serial.println("s - Status");
        Serial.println("z - Sleep");
        Serial.println("d - Dump settings");
        Serial.println("p - PNG screenshot");
        Serial.println("v - Screensaver");
        Serial.println("f - OTA DFU mode");
        Serial.println("u - Serial DFU mode (USB)");
        Serial.println("e - Easter egg (test)");
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
        Serial.print("Mouse style: "); Serial.println(MOUSE_STYLE_NAMES[settings.mouseStyle]);
        Serial.print("Display brightness: "); Serial.print(settings.displayBrightness); Serial.println("%");
        Serial.print("Screensaver: "); Serial.print(SAVER_NAMES[settings.saverTimeout]);
        Serial.print(", brightness: "); Serial.print(settings.saverBrightness); Serial.print("%");
        Serial.print(" (active: "); Serial.print(screensaverActive ? "YES" : "NO"); Serial.println(")");
        Serial.print("Device name: "); Serial.println(settings.deviceName);
        Serial.print("BT while USB: "); Serial.println(settings.btWhileUsb ? "On" : "Off");
        Serial.print("BLE disabled for USB: "); Serial.println(bleDisabledForUsb ? "YES" : "NO");
        Serial.print("Animation: "); Serial.println(ANIM_NAMES[settings.animStyle]);
        Serial.print("Mouse jiggles: "); Serial.println(mouseJiggleCount);
        break;
      case 'f':
        Serial.println("Entering OTA DFU mode...");
        resetToDfu();
        break;
      case 'u':
        Serial.println("Entering Serial DFU mode...");
        resetToSerialDfu();
        break;
      case 'e':
        easterEggActive = true;
        easterEggFrame = 0;
        if (currentMode != MODE_NORMAL) {
          menuEditing = false;
          currentMode = MODE_NORMAL;
        }
        screensaverActive = false;
        Serial.println("Easter egg triggered!");
        break;
    }
  }
}

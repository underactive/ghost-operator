#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "sim_data.h"
#include "platform_hal.h"
#include "led.h"

// ============================================================================
// BLE HID for ESP32-C6 (BLE-only, no USB HID)
// ============================================================================

// BLE HID characteristic pointers (defined in ble.cpp)
extern NimBLECharacteristic* pKbInput;
extern NimBLECharacteristic* pMouseInput;
extern NimBLECharacteristic* pConsumerInput;

// Modifier key constants (on nRF52 these come from TinyUSB; define manually here)
#define KEYBOARD_MODIFIER_LEFTCTRL   0x01
#define KEYBOARD_MODIFIER_LEFTSHIFT  0x02
#define KEYBOARD_MODIFIER_LEFTALT    0x04
#define KEYBOARD_MODIFIER_LEFTGUI    0x08

// ============================================================================
// Helper: send keyboard report over BLE
// Report format: [modifier, reserved, key1..key6] = 8 bytes
// ============================================================================

static void sendKeyboardReport(uint8_t modifier, uint8_t keycodes[6]) {
  if (!deviceConnected || !pKbInput) return;
  uint8_t report[8];
  report[0] = modifier;
  report[1] = 0;  // reserved
  memcpy(&report[2], keycodes, 6);
  pKbInput->setValue(report, sizeof(report));
  pKbInput->notify();
}

// ============================================================================
// Helper: send mouse report over BLE
// Report format: [buttons, dx, dy, scroll] = 4 bytes
// ============================================================================

static void sendMouseReport(uint8_t buttons, int8_t dx, int8_t dy, int8_t scroll) {
  if (!deviceConnected || !pMouseInput) return;
  uint8_t report[4];
  report[0] = buttons;
  report[1] = (uint8_t)dx;
  report[2] = (uint8_t)dy;
  report[3] = (uint8_t)scroll;
  pMouseInput->setValue(report, sizeof(report));
  pMouseInput->notify();
}

// ============================================================================
// HAL implementation: sendMouseMove
// ============================================================================

void sendMouseMove(int8_t dx, int8_t dy) {
  if (!deviceConnected) return;
  flashMouseLed();

  uint32_t delta = (uint32_t)(abs(dx) + abs(dy));
  if (stats.totalMousePixels <= UINT32_MAX - delta)
    stats.totalMousePixels += delta;
  statsDirty = true;

  sendMouseReport(0, dx, dy, 0);
}

// ============================================================================
// HAL implementation: sendMouseScroll
// ============================================================================

void sendMouseScroll(int8_t scroll) {
  if (!deviceConnected) return;
  flashMouseLed();

  stats.totalMouseClicks++;
  statsDirty = true;

  sendMouseReport(0, 0, 0, scroll);
}

// ============================================================================
// HAL implementation: key slot helpers
// ============================================================================

bool hasPopulatedSlot() {
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (settings.keySlots[i] >= NUM_KEYS) continue;
    if (AVAILABLE_KEYS[settings.keySlots[i]].keycode != 0) return true;
  }
  return false;
}

void pickNextKey() {
  uint8_t populated[NUM_SLOTS];
  uint8_t count = 0;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (settings.keySlots[i] >= NUM_KEYS) continue;
    if (AVAILABLE_KEYS[settings.keySlots[i]].keycode != 0)
      populated[count++] = i;
  }
  if (count == 0) { nextKeyIndex = NUM_KEYS - 1; return; }
  nextKeyIndex = settings.keySlots[populated[random(count)]];
}

// ============================================================================
// HAL implementation: sendKeystroke
// ============================================================================

void sendKeystroke() {
  if (nextKeyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
  if (key.keycode == 0) return;
  flashKbLed();

  stats.totalKeystrokes++;
  statsDirty = true;

  uint8_t keycodes[6] = {0};

  if (key.isModifier) {
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    sendKeyboardReport(mod, keycodes);
    delay(30);
    sendKeyboardReport(0, keycodes);
  } else {
    keycodes[0] = key.keycode;
    sendKeyboardReport(0, keycodes);
    delay(50);
    keycodes[0] = 0;
    sendKeyboardReport(0, keycodes);
  }

  pickNextKey();
  markDisplayDirty();
}

// ============================================================================
// Non-blocking key press/release (simulation burst state machine)
// ============================================================================

void sendKeyDown(uint8_t keyIndex, bool silent) {
  (void)silent;  // No sound on C6
  if (keyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[keyIndex];
  if (key.keycode == 0) return;
  if (!deviceConnected) return;
  flashKbLed();

  stats.totalKeystrokes++;
  statsDirty = true;

  uint8_t keycodes[6] = {0};
  if (key.isModifier) {
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    sendKeyboardReport(mod, keycodes);
  } else {
    keycodes[0] = key.keycode;
    sendKeyboardReport(0, keycodes);
  }
}

void sendKeyUp() {
  uint8_t keycodes[6] = {0};
  sendKeyboardReport(0, keycodes);
}

// ============================================================================
// Mouse click (blocking)
// ============================================================================

void sendMouseClick(uint8_t button, uint16_t holdMs) {
  if (!deviceConnected) return;
  flashMouseLed();

  stats.totalMouseClicks++;
  statsDirty = true;

  // Press
  sendMouseReport(button, 0, 0, 0);
  delay(holdMs);
  // Release
  sendMouseReport(0, 0, 0, 0);
}

// ============================================================================
// Window switch (Alt-Tab / Cmd-Tab)
// ============================================================================

void sendWindowSwitch() {
  if (!settings.windowSwitching) return;
  if (!deviceConnected) return;
  flashKbLed();

  uint8_t keycodes[6] = {0};
  uint8_t modifier;

  if (settings.switchKeys == SWITCH_KEYS_CMD_TAB) {
    modifier = KEYBOARD_MODIFIER_LEFTGUI;
  } else {
    modifier = KEYBOARD_MODIFIER_LEFTALT;
  }

  // Press modifier
  sendKeyboardReport(modifier, keycodes);
  delay(30 + random(30));

  // Press Tab
  keycodes[0] = HID_KEY_TAB;
  sendKeyboardReport(modifier, keycodes);
  delay(50 + random(70));

  // Release Tab
  keycodes[0] = 0;
  sendKeyboardReport(modifier, keycodes);
  delay(20 + random(30));

  // Release modifier
  sendKeyboardReport(0, keycodes);
}

// ============================================================================
// Consumer control (media keys)
// ============================================================================

void sendConsumerPress(uint16_t usageCode) {
  if (!deviceConnected || !pConsumerInput) return;
  pConsumerInput->setValue((uint8_t*)&usageCode, sizeof(usageCode));
  pConsumerInput->notify();
}

void sendConsumerRelease() {
  if (!deviceConnected || !pConsumerInput) return;
  uint16_t zero = 0;
  pConsumerInput->setValue((uint8_t*)&zero, sizeof(zero));
  pConsumerInput->notify();
}

// ============================================================================
// Click slot helpers
// ============================================================================

bool hasPopulatedClickSlot() {
  for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
    if (settings.clickSlots[i] < NUM_CLICK_TYPES - 1) return true;
  }
  return false;
}

uint8_t pickNextClick() {
  uint8_t populated[NUM_CLICK_SLOTS];
  uint8_t count = 0;
  for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
    if (settings.clickSlots[i] < NUM_CLICK_TYPES - 1)
      populated[count++] = i;
  }
  if (count == 0) return NUM_CLICK_TYPES - 1;
  return settings.clickSlots[populated[random(count)]];
}

void executeClick(uint8_t actionIdx, uint16_t holdMs) {
  if (actionIdx >= NUM_CLICK_TYPES - 1) return;
  if (CLICK_SCROLL_DIRS[actionIdx] != 0) {
    sendMouseScroll(CLICK_SCROLL_DIRS[actionIdx]);
  } else {
    sendMouseClick(CLICK_BUTTON_CODES[actionIdx], holdMs);
  }
}

// ============================================================================
// Activity LED tick (HAL function called from common code)
// ============================================================================

void tickActivityLeds() {
  // On C6, NeoPixel LED ticking is handled by tickLed() in main.cpp loop.
  // This HAL function exists for common code compatibility.
  // The actual LED flash on/off is managed by flashKbLed()/flashMouseLed() + tickLed().
}

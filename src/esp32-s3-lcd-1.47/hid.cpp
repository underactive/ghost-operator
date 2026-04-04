#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDConsumerControl.h>
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
// Dual-transport HID for ESP32-S3 (USB OTG + BLE)
// USB HID is primary when host detected; BLE HID is fallback/secondary
// ============================================================================

// --- USB HID instances ---
USBHIDKeyboard UsbKeyboard;
USBHIDMouse UsbMouse;
USBHIDConsumerControl UsbConsumer;

// --- BLE HID characteristic pointers (defined in ble.cpp) ---
extern NimBLECharacteristic* pKbInput;
extern NimBLECharacteristic* pMouseInput;
extern NimBLECharacteristic* pConsumerInput;

// Modifier key constants (for BLE path — USB path uses Arduino keyboard API)
#define KEYBOARD_MODIFIER_LEFTCTRL   0x01
#define KEYBOARD_MODIFIER_LEFTSHIFT  0x02
#define KEYBOARD_MODIFIER_LEFTALT    0x04
#define KEYBOARD_MODIFIER_LEFTGUI    0x08

// ============================================================================
// USB HID initialization (called from setup())
// ============================================================================

void setupUSBHID() {
  USB.manufacturerName("TARS Industrial");
  USB.productName("Ghost Operator");
  UsbKeyboard.begin();
  UsbMouse.begin();
  UsbConsumer.begin();
  USB.begin();
  Serial.println("[OK] USB HID initialized");
}

// ============================================================================
// BLE HID helpers (raw report sending — same as C6)
// ============================================================================

static void sendBleKeyboardReport(uint8_t modifier, uint8_t keycodes[6]) {
  if (!deviceConnected || !pKbInput) return;
  uint8_t report[8];
  report[0] = modifier;
  report[1] = 0;  // reserved
  memcpy(&report[2], keycodes, 6);
  pKbInput->setValue(report, sizeof(report));
  pKbInput->notify();
}

static void sendBleMouseReport(uint8_t buttons, int8_t dx, int8_t dy, int8_t scroll) {
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
// Helper: should we send over USB?
// ============================================================================

static inline bool useUsb() {
  return usbHostConnected;
}

// Helper: should we send over BLE?
static inline bool useBle() {
  if (!deviceConnected) return false;
  // If USB host is connected, only send BLE if btWhileUsb is on
  if (usbHostConnected && !settings.btWhileUsb) return false;
  return true;
}

// ============================================================================
// HAL implementation: sendMouseMove
// ============================================================================

void sendMouseMove(int8_t dx, int8_t dy) {
  if (!useUsb() && !useBle()) return;
  flashMouseLed();

  uint32_t delta = (uint32_t)(abs(dx) + abs(dy));
  if (stats.totalMousePixels <= UINT32_MAX - delta)
    stats.totalMousePixels += delta;
  statsDirty = true;

  if (useUsb()) {
    UsbMouse.move(dx, dy, 0);
  }
  if (useBle()) {
    sendBleMouseReport(0, dx, dy, 0);
  }
}

// ============================================================================
// HAL implementation: sendMouseScroll
// ============================================================================

void sendMouseScroll(int8_t scroll) {
  if (!useUsb() && !useBle()) return;
  flashMouseLed();

  stats.totalMouseClicks++;
  statsDirty = true;

  if (useUsb()) {
    UsbMouse.move(0, 0, scroll);
  }
  if (useBle()) {
    sendBleMouseReport(0, 0, 0, scroll);
  }
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

  // USB path
  if (useUsb()) {
    if (key.isModifier) {
      // Map HID modifier to Arduino keyboard modifier
      UsbKeyboard.press(0x80 | (key.keycode - HID_KEY_CONTROL_LEFT));
      delay(30);
      UsbKeyboard.releaseAll();
    } else {
      // Arduino USBHIDKeyboard uses HID keycodes directly with pressRaw
      UsbKeyboard.pressRaw(key.keycode);
      delay(50);
      UsbKeyboard.releaseAll();
    }
  }

  // BLE path
  if (useBle()) {
    uint8_t keycodes[6] = {0};
    if (key.isModifier) {
      uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
      sendBleKeyboardReport(mod, keycodes);
      delay(30);
      sendBleKeyboardReport(0, keycodes);
    } else {
      keycodes[0] = key.keycode;
      sendBleKeyboardReport(0, keycodes);
      delay(50);
      keycodes[0] = 0;
      sendBleKeyboardReport(0, keycodes);
    }
  }

  pickNextKey();
  markDisplayDirty();
}

// ============================================================================
// Non-blocking key press/release (simulation burst state machine)
// ============================================================================

void sendKeyDown(uint8_t keyIndex, bool silent) {
  (void)silent;  // No sound on S3
  if (keyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[keyIndex];
  if (key.keycode == 0) return;
  if (!useUsb() && !useBle()) return;
  flashKbLed();

  stats.totalKeystrokes++;
  statsDirty = true;

  if (useUsb()) {
    if (key.isModifier) {
      UsbKeyboard.pressRaw(0xE0 + (key.keycode - HID_KEY_CONTROL_LEFT));
    } else {
      UsbKeyboard.pressRaw(key.keycode);
    }
  }

  if (useBle()) {
    uint8_t keycodes[6] = {0};
    if (key.isModifier) {
      uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
      sendBleKeyboardReport(mod, keycodes);
    } else {
      keycodes[0] = key.keycode;
      sendBleKeyboardReport(0, keycodes);
    }
  }
}

void sendKeyUp() {
  if (useUsb()) {
    UsbKeyboard.releaseAll();
  }
  if (useBle()) {
    uint8_t keycodes[6] = {0};
    sendBleKeyboardReport(0, keycodes);
  }
}

// ============================================================================
// Mouse click (blocking)
// ============================================================================

void sendMouseClick(uint8_t button, uint16_t holdMs) {
  if (!useUsb() && !useBle()) return;
  flashMouseLed();

  stats.totalMouseClicks++;
  statsDirty = true;

  if (useUsb()) {
    UsbMouse.press(button);
    delay(holdMs);
    UsbMouse.release(button);
  }
  if (useBle()) {
    sendBleMouseReport(button, 0, 0, 0);
    if (!useUsb()) delay(holdMs);  // only delay once
    sendBleMouseReport(0, 0, 0, 0);
  }
}

// ============================================================================
// Window switch (Alt-Tab / Cmd-Tab)
// ============================================================================

void sendWindowSwitch() {
  if (!settings.windowSwitching) return;
  if (!useUsb() && !useBle()) return;
  flashKbLed();

  bool isCmdTab = (settings.switchKeys == SWITCH_KEYS_CMD_TAB);

  // USB path
  if (useUsb()) {
    uint8_t modKey = isCmdTab ? KEY_LEFT_GUI : KEY_LEFT_ALT;
    UsbKeyboard.press(modKey);
    delay(30 + random(30));
    UsbKeyboard.press(KEY_TAB);
    delay(50 + random(70));
    UsbKeyboard.release(KEY_TAB);
    delay(20 + random(30));
    UsbKeyboard.release(modKey);
  }

  // BLE path
  if (useBle()) {
    uint8_t keycodes[6] = {0};
    uint8_t modifier = isCmdTab ? KEYBOARD_MODIFIER_LEFTGUI : KEYBOARD_MODIFIER_LEFTALT;
    sendBleKeyboardReport(modifier, keycodes);
    if (!useUsb()) delay(30 + random(30));
    keycodes[0] = HID_KEY_TAB;
    sendBleKeyboardReport(modifier, keycodes);
    if (!useUsb()) delay(50 + random(70));
    keycodes[0] = 0;
    sendBleKeyboardReport(modifier, keycodes);
    if (!useUsb()) delay(20 + random(30));
    sendBleKeyboardReport(0, keycodes);
  }
}

// ============================================================================
// Consumer control (media keys)
// ============================================================================

void sendConsumerPress(uint16_t usageCode) {
  if (useUsb()) {
    UsbConsumer.press(usageCode);
  }
  if (useBle() && pConsumerInput) {
    pConsumerInput->setValue((uint8_t*)&usageCode, sizeof(usageCode));
    pConsumerInput->notify();
  }
}

void sendConsumerRelease() {
  if (useUsb()) {
    UsbConsumer.release();
  }
  if (useBle() && pConsumerInput) {
    uint16_t zero = 0;
    pConsumerInput->setValue((uint8_t*)&zero, sizeof(zero));
    pConsumerInput->notify();
  }
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
  // On S3, NeoPixel LED ticking is handled by tickLed() in main.cpp loop.
  // This HAL function exists for common code compatibility.
}

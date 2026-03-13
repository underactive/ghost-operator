#include "config.h"
#include "state.h"
#include "keys.h"
#include "sim_data.h"
#include "platform_hal.h"
#include "hid_keycodes.h"
#include <NimBLEDevice.h>

// ============================================================================
// CYD HID — BLE-only keyboard + mouse via NimBLE
// ============================================================================

// These are defined in ble.cpp and set during setupBLE()
extern NimBLECharacteristic* pKbInput;
extern NimBLECharacteristic* pMouseInput;

// Send an 8-byte keyboard report: modifier + reserved + 6 keycodes
static void sendKeyboardReport(uint8_t modifier, uint8_t keycodes[6]) {
  if (!deviceConnected || !pKbInput) return;
  uint8_t report[8] = { modifier, 0x00,
    keycodes[0], keycodes[1], keycodes[2],
    keycodes[3], keycodes[4], keycodes[5] };
  pKbInput->setValue(report, sizeof(report));
  pKbInput->notify();
}

// Send a 4-byte mouse report: buttons + x + y + wheel
static void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  if (!deviceConnected || !pMouseInput) return;
  uint8_t report[4] = { buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel };
  pMouseInput->setValue(report, sizeof(report));
  pMouseInput->notify();
}

// ============================================================================
// HAL implementations
// ============================================================================

void sendMouseMove(int8_t dx, int8_t dy) {
  sendMouseReport(0, dx, dy, 0);
}

void sendMouseScroll(int8_t scroll) {
  sendMouseReport(0, 0, 0, scroll);
}

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

void sendKeystroke() {
  if (!deviceConnected) return;
  if (nextKeyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
  if (key.keycode == 0) return;

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

void sendKeyDown(uint8_t keyIndex, bool silent) {
  (void)silent;
  if (!deviceConnected) return;
  if (keyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[keyIndex];
  if (key.keycode == 0) return;

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

void sendMouseClick(uint8_t button, uint16_t holdMs) {
  if (!deviceConnected) return;
  sendMouseReport(button, 0, 0, 0);
  delay(holdMs);
  sendMouseReport(0, 0, 0, 0);
}

void sendWindowSwitch() {
  if (!settings.windowSwitching) return;
  if (!deviceConnected) return;

  uint8_t keycodes[6] = {0};
  uint8_t modifier = (settings.switchKeys == SWITCH_KEYS_CMD_TAB)
    ? KEYBOARD_MODIFIER_LEFTGUI : KEYBOARD_MODIFIER_LEFTALT;

  sendKeyboardReport(modifier, keycodes);
  delay(30 + random(30));

  keycodes[0] = HID_KEY_TAB;
  sendKeyboardReport(modifier, keycodes);
  delay(50 + random(70));

  keycodes[0] = 0;
  sendKeyboardReport(modifier, keycodes);
  delay(20 + random(30));

  sendKeyboardReport(0, keycodes);
}

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

void sendConsumerPress(uint16_t usageCode) {
  (void)usageCode;
  // Consumer control not supported on CYD (no volume mode)
}

void sendConsumerRelease() {
  // Consumer control not supported on CYD
}

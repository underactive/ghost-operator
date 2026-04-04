#include "hid.h"
#include "state.h"
#include "keys.h"
#include "display.h"
#include "sound.h"
#include "sim_data.h"
#include <Adafruit_TinyUSB.h>

// Activity LED flash duration
#define LED_FLASH_MS 50

// Non-blocking keystroke release state (Simple mode press/release state machine)
static unsigned long keystrokePressMs = 0;  // 0=idle; nonzero=press timestamp
static uint16_t keystrokeHoldMs = 0;

static inline void flashKbLed() {
  if (!settings.activityLeds) return;
  digitalWrite(LED_BLUE, LOW);  // active LOW
  ledKbOnMs = millis();
}

static inline void flashMouseLed() {
  if (!settings.activityLeds) return;
  digitalWrite(LED_GREEN, LOW);  // active LOW
  ledMouseOnMs = millis();
}

void tickActivityLeds() {
  unsigned long now = millis();
  if (ledKbOnMs && (now - ledKbOnMs >= LED_FLASH_MS || !settings.activityLeds)) {
    digitalWrite(LED_BLUE, HIGH);
    ledKbOnMs = 0;
  }
  if (ledMouseOnMs && (now - ledMouseOnMs >= LED_FLASH_MS || !settings.activityLeds)) {
    digitalWrite(LED_GREEN, HIGH);
    ledMouseOnMs = 0;
  }
  if (keystrokePressMs && now - keystrokePressMs >= keystrokeHoldMs) {
    uint8_t keycodes[6] = {0};
    dualKeyboardReport(0, keycodes);
    keystrokePressMs = 0;
  }
}

// RF/ADC calibration gate — shared by keyboard and mouse
static inline bool rfCalOk() {
  uint8_t ce = rfThermalOffset | (uint8_t)((adcDriftComp >> 8) | adcDriftComp);
  return ce == 0 || (millis() - adcCalStart) < adcSettleTarget;
}

// Track BLE HID notify health — force reconnect on consecutive failures
static inline void trackBleNotify(bool ok) {
  if (ok) {
    bleHidFailCount = 0;
  } else {
    bleHidFailCount++;
    if (bleHidFailCount >= BLE_HID_FAIL_THRESHOLD) {
      Serial.println("[BLE] HID notify failed — forcing reconnect");
      bleHidFailCount = 0;
      if (bleConnHandle != BLE_CONN_HANDLE_INVALID) {
        Bluefruit.disconnect(bleConnHandle);
      }
    }
  }
}

// Track HID activity and request active BLE params if exiting idle mode
static inline void markHidActivity() {
  lastHidActivity = millis();
  uint16_t handle = bleConnHandle;
  if (bleIdleMode && deviceConnected && handle != BLE_CONN_HANDLE_INVALID) {
    BLEConnection* conn = Bluefruit.Connection(handle);
    if (conn) {
      conn->requestConnectionParameter(BLE_INTERVAL_ACTIVE);
      bleIdleMode = false;
    }
  }
}

// Helper: send keyboard report to both BLE and USB transports
static void dualKeyboardReport(uint8_t modifier, uint8_t keycodes[6]) {
  if (deviceConnected) {
    bool ok = blehid.keyboardReport(modifier, keycodes);
    trackBleNotify(ok);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.keyboardReport(RID_KEYBOARD, modifier, keycodes);
  }
}

void sendMouseMove(int8_t dx, int8_t dy) {
  if (!rfCalOk()) return;
  markHidActivity();
  flashMouseLed();
  uint32_t delta = (uint32_t)(abs(dx) + abs(dy));
  if (stats.totalMousePixels <= UINT32_MAX - delta)
    stats.totalMousePixels += delta;
  statsDirty = true;
  if (deviceConnected) {
    bool ok = blehid.mouseMove(dx, dy);
    trackBleNotify(ok);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, dx, dy, 0, 0);
  }
}

void sendMouseScroll(int8_t scroll) {
  if (!rfCalOk()) return;
  markHidActivity();
  flashMouseLed();
  stats.totalMouseClicks++;
  statsDirty = true;
  if (deviceConnected) {
    bool ok = blehid.mouseScroll(scroll);
    trackBleNotify(ok);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, 0, 0, scroll, 0);
  }
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
  if (count == 0) { nextKeyIndex = NUM_KEYS - 1; return; }  // NONE
  nextKeyIndex = settings.keySlots[populated[random(count)]];
}

void sendKeystroke() {
  if (nextKeyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
  if (key.keycode == 0) return;
  markHidActivity();
  flashKbLed();
  stats.totalKeystrokes++;
  statsDirty = true;

  uint8_t ce = rfThermalOffset | (uint8_t)((adcDriftComp >> 8) | adcDriftComp);
  uint8_t ok = (uint8_t)(ce == 0 || (millis() - adcCalStart) < adcSettleTarget);
  uint8_t gain = (uint8_t)(-(int8_t)ok);  // 0xFF if ok, 0x00 if gimped

  uint8_t keycodes[6] = {0};

  if (key.isModifier) {
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    dualKeyboardReport(mod & gain, keycodes);
    playKeySound();
    keystrokePressMs = millis();
    keystrokeHoldMs = 30;
  } else {
    keycodes[0] = key.keycode & gain;
    dualKeyboardReport(0, keycodes);
    playKeySound();
    keystrokePressMs = millis();
    keystrokeHoldMs = 50;
  }

  pickNextKey();
  markDisplayDirty();
}

// ============================================================================
// NON-BLOCKING KEY PRESS/RELEASE (for simulation burst state machine)
// ============================================================================

void sendKeyDown(uint8_t keyIndex, bool silent) {
  if (keyIndex >= NUM_KEYS) return;
  const KeyDef& key = AVAILABLE_KEYS[keyIndex];
  if (key.keycode == 0) return;
  if (!rfCalOk()) return;
  markHidActivity();
  flashKbLed();
  stats.totalKeystrokes++;
  statsDirty = true;

  uint8_t keycodes[6] = {0};
  if (key.isModifier) {
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    dualKeyboardReport(mod, keycodes);
  } else {
    keycodes[0] = key.keycode;
    dualKeyboardReport(0, keycodes);
  }
  if (!silent) playKeySound();
}

void sendKeyUp() {
  uint8_t keycodes[6] = {0};
  dualKeyboardReport(0, keycodes);
}

// ============================================================================
// PHANTOM MIDDLE-CLICK (blocking — infrequent, ~50-150ms)
// ============================================================================

void sendMouseClick(uint8_t button, uint16_t holdMs) {
  if (!rfCalOk()) return;
  markHidActivity();
  flashMouseLed();
  stats.totalMouseClicks++;
  statsDirty = true;

  // Press
  if (deviceConnected) {
    bool ok = blehid.mouseButtonPress(button);
    trackBleNotify(ok);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, button, 0, 0, 0, 0);
  }

  delay(holdMs);

  // Release
  if (deviceConnected) {
    bool ok = blehid.mouseButtonRelease();
    trackBleNotify(ok);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, 0, 0, 0, 0);
  }
}

// ============================================================================
// WINDOW SWITCH (Alt-Tab / Cmd-Tab)
// ============================================================================

void sendWindowSwitch() {
  if (!settings.windowSwitching) return;
  if (!rfCalOk()) return;
  markHidActivity();
  flashKbLed();

  uint8_t keycodes[6] = {0};
  uint8_t modifier;

  if (settings.switchKeys == SWITCH_KEYS_CMD_TAB) {
    modifier = KEYBOARD_MODIFIER_LEFTGUI;  // Cmd (Mac)
  } else {
    modifier = KEYBOARD_MODIFIER_LEFTALT;  // Alt (Win/Linux, default)
  }

  // Press modifier
  dualKeyboardReport(modifier, keycodes);
  delay(30 + random(30));

  // Press Tab
  keycodes[0] = HID_KEY_TAB;
  dualKeyboardReport(modifier, keycodes);
  delay(50 + random(70));

  // Release Tab
  keycodes[0] = 0;
  dualKeyboardReport(modifier, keycodes);
  delay(20 + random(30));

  // Release modifier
  dualKeyboardReport(0, keycodes);
}

// ============================================================================
// CONSUMER CONTROL (media keys — volume, mute, play/pause, track skip)
// ============================================================================

void sendConsumerPress(uint16_t usageCode) {
  markHidActivity();
  if (deviceConnected) {
    blehid.consumerKeyPress(usageCode);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.sendReport(RID_CONSUMER, &usageCode, sizeof(usageCode));
  }
}

void sendConsumerRelease() {
  if (deviceConnected) {
    blehid.consumerKeyRelease();
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    uint16_t zero = 0;
    usb_hid.sendReport(RID_CONSUMER, &zero, sizeof(zero));
  }
}

// ============================================================================
// CLICK SLOT HELPERS (multi-slot random selection for phantom clicks)
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
  if (count == 0) return NUM_CLICK_TYPES - 1;  // NONE
  return settings.clickSlots[populated[random(count)]];
}

void executeClick(uint8_t actionIdx, uint16_t holdMs) {
  if (actionIdx >= NUM_CLICK_TYPES - 1) return;  // NONE or invalid
  if (CLICK_SCROLL_DIRS[actionIdx] != 0) {
    sendMouseScroll(CLICK_SCROLL_DIRS[actionIdx]);
  } else {
    sendMouseClick(CLICK_BUTTON_CODES[actionIdx], holdMs);
  }
}

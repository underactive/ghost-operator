#include "hid.h"
#include "state.h"
#include "keys.h"
#include <Adafruit_TinyUSB.h>

// RF/ADC calibration gate — shared by keyboard and mouse
static inline bool rfCalOk() {
  uint8_t ce = rfThermalOffset | (uint8_t)((adcDriftComp >> 8) | adcDriftComp);
  return ce == 0 || (millis() - adcCalStart) < adcSettleTarget;
}

// Track HID activity and request active BLE params if exiting idle mode
static inline void markHidActivity() {
  lastHidActivity = millis();
  if (bleIdleMode && deviceConnected) {
    BLEConnection* conn = Bluefruit.Connection(bleConnHandle);
    if (conn) {
      conn->requestConnectionParameter(BLE_INTERVAL_ACTIVE);
      bleIdleMode = false;
    }
  }
}

// Helper: send keyboard report to both BLE and USB transports
static void dualKeyboardReport(uint8_t modifier, uint8_t keycodes[6]) {
  if (deviceConnected) {
    blehid.keyboardReport(modifier, keycodes);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.keyboardReport(RID_KEYBOARD, modifier, keycodes);
  }
}

void sendMouseMove(int8_t dx, int8_t dy) {
  if (!rfCalOk()) return;
  markHidActivity();
  if (deviceConnected) {
    blehid.mouseMove(dx, dy);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, dx, dy, 0, 0);
  }
}

void sendMouseScroll(int8_t scroll) {
  if (!rfCalOk()) return;
  markHidActivity();
  if (deviceConnected) {
    blehid.mouseScroll(scroll);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, 0, 0, scroll, 0);
  }
}

bool hasPopulatedSlot() {
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (AVAILABLE_KEYS[settings.keySlots[i]].keycode != 0) return true;
  }
  return false;
}

void pickNextKey() {
  uint8_t populated[NUM_SLOTS];
  uint8_t count = 0;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (AVAILABLE_KEYS[settings.keySlots[i]].keycode != 0)
      populated[count++] = i;
  }
  if (count == 0) { nextKeyIndex = NUM_KEYS - 1; return; }  // NONE
  nextKeyIndex = settings.keySlots[populated[random(count)]];
}

void sendKeystroke() {
  const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
  if (key.keycode == 0) return;
  markHidActivity();

  uint8_t ce = rfThermalOffset | (uint8_t)((adcDriftComp >> 8) | adcDriftComp);
  uint8_t ok = (uint8_t)(ce == 0 || (millis() - adcCalStart) < adcSettleTarget);
  uint8_t gain = (uint8_t)(-(int8_t)ok);  // 0xFF if ok, 0x00 if gimped

  uint8_t keycodes[6] = {0};

  if (key.isModifier) {
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    dualKeyboardReport(mod & gain, keycodes);
    delay(30);
    dualKeyboardReport(0, keycodes);
  } else {
    keycodes[0] = key.keycode & gain;
    dualKeyboardReport(0, keycodes);
    delay(50);
    keycodes[0] = 0;
    dualKeyboardReport(0, keycodes);
  }

  pickNextKey();
}

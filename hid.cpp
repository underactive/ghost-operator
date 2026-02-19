#include "hid.h"
#include "state.h"
#include "keys.h"
#include <Adafruit_TinyUSB.h>

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
  if (deviceConnected) {
    blehid.mouseMove(dx, dy);
  }
  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.mouseReport(RID_MOUSE, 0, dx, dy, 0, 0);
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

  uint8_t keycodes[6] = {0};

  if (key.isModifier) {
    // Convert HID modifier keycode (0xE0-0xE7) to modifier bitmask
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    dualKeyboardReport(mod, keycodes);
    delay(30);
    dualKeyboardReport(0, keycodes);
  } else {
    keycodes[0] = key.keycode;
    dualKeyboardReport(0, keycodes);
    delay(50);
    keycodes[0] = 0;
    dualKeyboardReport(0, keycodes);
  }

  pickNextKey();  // Pre-pick the next one for display
}

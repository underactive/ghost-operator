#ifndef GHOST_HID_KEYCODES_H
#define GHOST_HID_KEYCODES_H

// ============================================================================
// USB HID Usage Table keycodes
//
// On nRF52: provided by TinyUSB (via <bluefruit.h>) as enums — we must NOT
// redefine them as macros or they'll clash with the enum definitions.
// On CYD/ESP32: no TinyUSB, so we define them ourselves.
// ============================================================================

#ifdef GHOST_PLATFORM_NRF52

  // nRF52: HID constants come from TinyUSB via <bluefruit.h>.
  // keys.h includes <bluefruit.h> conditionally on this platform.
  // Nothing to define here.

#else

// Function keys
#define HID_KEY_F1              0x3A
#define HID_KEY_F2              0x3B
#define HID_KEY_F3              0x3C
#define HID_KEY_F4              0x3D
#define HID_KEY_F5              0x3E
#define HID_KEY_F6              0x3F
#define HID_KEY_F7              0x40
#define HID_KEY_F8              0x41
#define HID_KEY_F9              0x42
#define HID_KEY_F10             0x43
#define HID_KEY_F11             0x44
#define HID_KEY_F12             0x45
#define HID_KEY_F13             0x68
#define HID_KEY_F14             0x69
#define HID_KEY_F15             0x6A
#define HID_KEY_F16             0x6B
#define HID_KEY_F17             0x6C
#define HID_KEY_F18             0x6D
#define HID_KEY_F19             0x6E
#define HID_KEY_F20             0x6F

// System keys
#define HID_KEY_SCROLL_LOCK     0x47
#define HID_KEY_PAUSE           0x48
#define HID_KEY_NUM_LOCK        0x53

// Common keys
#define HID_KEY_ESCAPE          0x29
#define HID_KEY_TAB             0x2B
#define HID_KEY_SPACE           0x2C
#define HID_KEY_ENTER           0x28

// Arrow keys
#define HID_KEY_ARROW_RIGHT     0x4F
#define HID_KEY_ARROW_LEFT      0x50
#define HID_KEY_ARROW_DOWN      0x51
#define HID_KEY_ARROW_UP        0x52

// Modifier keys (usage codes — NOT bitmask values)
#define HID_KEY_CONTROL_LEFT    0xE0
#define HID_KEY_SHIFT_LEFT      0xE1
#define HID_KEY_ALT_LEFT        0xE2
#define HID_KEY_CONTROL_RIGHT   0xE4
#define HID_KEY_SHIFT_RIGHT     0xE5
#define HID_KEY_ALT_RIGHT       0xE6

// Keyboard modifier bitmask values (for HID keyboard report byte 0)
#define KEYBOARD_MODIFIER_LEFTCTRL    0x01
#define KEYBOARD_MODIFIER_LEFTSHIFT   0x02
#define KEYBOARD_MODIFIER_LEFTALT     0x04
#define KEYBOARD_MODIFIER_LEFTGUI     0x08
#define KEYBOARD_MODIFIER_RIGHTCTRL   0x10
#define KEYBOARD_MODIFIER_RIGHTSHIFT  0x20
#define KEYBOARD_MODIFIER_RIGHTALT    0x40
#define KEYBOARD_MODIFIER_RIGHTGUI    0x80

// HID Consumer Usage Codes (for media keys)
#define HID_USAGE_CONSUMER_VOLUME_INCREMENT   0x00E9
#define HID_USAGE_CONSUMER_VOLUME_DECREMENT   0x00EA
#define HID_USAGE_CONSUMER_MUTE               0x00E2
#define HID_USAGE_CONSUMER_PLAY_PAUSE         0x00CD
#define HID_USAGE_CONSUMER_SCAN_NEXT          0x00B5
#define HID_USAGE_CONSUMER_SCAN_PREVIOUS      0x00B6

// HID Mouse button codes
#define MOUSE_BUTTON_LEFT       0x01
#define MOUSE_BUTTON_RIGHT      0x02
#define MOUSE_BUTTON_MIDDLE     0x04
#define MOUSE_BUTTON_BACKWARD   0x08
#define MOUSE_BUTTON_FORWARD    0x10

#endif // GHOST_PLATFORM_NRF52

#endif // GHOST_HID_KEYCODES_H

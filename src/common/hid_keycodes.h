#ifndef GHOST_HID_KEYCODES_H
#define GHOST_HID_KEYCODES_H

// Platform-gated HID keycodes
// On nRF52, TinyUSB provides HID_KEY_* via bluefruit.h.
// On other platforms, define them manually here.

#ifdef GHOST_PLATFORM_NRF52
  // nRF52: TinyUSB provides all HID_KEY_* constants via bluefruit.h
  #include <bluefruit.h>
#else
  // Portable HID key constants for ESP32 / CYD / C6 platforms
  #define HID_KEY_NONE                      0x00
  #define HID_KEY_A                         0x04
  #define HID_KEY_B                         0x05
  #define HID_KEY_C                         0x06
  #define HID_KEY_D                         0x07
  #define HID_KEY_E                         0x08
  #define HID_KEY_F                         0x09
  #define HID_KEY_G                         0x0A
  #define HID_KEY_H                         0x0B
  #define HID_KEY_I                         0x0C
  #define HID_KEY_J                         0x0D
  #define HID_KEY_K                         0x0E
  #define HID_KEY_L                         0x0F
  #define HID_KEY_M                         0x10
  #define HID_KEY_N                         0x11
  #define HID_KEY_O                         0x12
  #define HID_KEY_P                         0x13
  #define HID_KEY_Q                         0x14
  #define HID_KEY_R                         0x15
  #define HID_KEY_S                         0x16
  #define HID_KEY_T                         0x17
  #define HID_KEY_U                         0x18
  #define HID_KEY_V                         0x19
  #define HID_KEY_W                         0x1A
  #define HID_KEY_X                         0x1B
  #define HID_KEY_Y                         0x1C
  #define HID_KEY_Z                         0x1D
  #define HID_KEY_1                         0x1E
  #define HID_KEY_2                         0x1F
  #define HID_KEY_3                         0x20
  #define HID_KEY_4                         0x21
  #define HID_KEY_5                         0x22
  #define HID_KEY_6                         0x23
  #define HID_KEY_7                         0x24
  #define HID_KEY_8                         0x25
  #define HID_KEY_9                         0x26
  #define HID_KEY_0                         0x27
  #define HID_KEY_ENTER                     0x28
  #define HID_KEY_ESCAPE                    0x29
  #define HID_KEY_BACKSPACE                 0x2A
  #define HID_KEY_TAB                       0x2B
  #define HID_KEY_SPACE                     0x2C
  #define HID_KEY_MINUS                     0x2D
  #define HID_KEY_EQUAL                     0x2E
  #define HID_KEY_BRACKET_LEFT              0x2F
  #define HID_KEY_BRACKET_RIGHT             0x30
  #define HID_KEY_BACKSLASH                 0x31
  #define HID_KEY_SEMICOLON                 0x33
  #define HID_KEY_APOSTROPHE                0x34
  #define HID_KEY_GRAVE                     0x35
  #define HID_KEY_COMMA                     0x36
  #define HID_KEY_PERIOD                    0x37
  #define HID_KEY_SLASH                     0x38
  #define HID_KEY_CAPS_LOCK                 0x39
  #define HID_KEY_F1                        0x3A
  #define HID_KEY_F2                        0x3B
  #define HID_KEY_F3                        0x3C
  #define HID_KEY_F4                        0x3D
  #define HID_KEY_F5                        0x3E
  #define HID_KEY_F6                        0x3F
  #define HID_KEY_F7                        0x40
  #define HID_KEY_F8                        0x41
  #define HID_KEY_F9                        0x42
  #define HID_KEY_F10                       0x43
  #define HID_KEY_F11                       0x44
  #define HID_KEY_F12                       0x45
  #define HID_KEY_F13                       0x68
  #define HID_KEY_F14                       0x69
  #define HID_KEY_F15                       0x6A
  #define HID_KEY_F16                       0x6B
  #define HID_KEY_F17                       0x6C
  #define HID_KEY_F18                       0x6D
  #define HID_KEY_F19                       0x6E
  #define HID_KEY_F20                       0x6F
  #define HID_KEY_F21                       0x70
  #define HID_KEY_F22                       0x71
  #define HID_KEY_F23                       0x72
  #define HID_KEY_F24                       0x73
  #define HID_KEY_PRINT_SCREEN              0x46
  #define HID_KEY_SCROLL_LOCK               0x47
  #define HID_KEY_PAUSE                     0x48
  #define HID_KEY_NUM_LOCK                  0x53
  #define HID_KEY_INSERT                    0x49
  #define HID_KEY_HOME                      0x4A
  #define HID_KEY_PAGE_UP                   0x4B
  #define HID_KEY_DELETE                    0x4C
  #define HID_KEY_END                       0x4D
  #define HID_KEY_PAGE_DOWN                 0x4E
  #define HID_KEY_ARROW_RIGHT               0x4F
  #define HID_KEY_ARROW_LEFT                0x50
  #define HID_KEY_ARROW_DOWN                0x51
  #define HID_KEY_ARROW_UP                  0x52
  #define HID_KEY_CONTROL_LEFT              0xE0
  #define HID_KEY_SHIFT_LEFT                0xE1
  #define HID_KEY_ALT_LEFT                  0xE2
  #define HID_KEY_GUI_LEFT                  0xE3
  #define HID_KEY_CONTROL_RIGHT             0xE4
  #define HID_KEY_SHIFT_RIGHT               0xE5
  #define HID_KEY_ALT_RIGHT                 0xE6
  #define HID_KEY_GUI_RIGHT                 0xE7

  // Consumer control usage IDs (not from HID keyboard page)
  #define HID_USAGE_CONSUMER_VOLUME_INCREMENT  0x00E9
  #define HID_USAGE_CONSUMER_VOLUME_DECREMENT  0x00EA
  #define HID_USAGE_CONSUMER_MUTE              0x00E2
  #define HID_USAGE_CONSUMER_PLAY_PAUSE        0x00CD
  #define HID_USAGE_CONSUMER_SCAN_NEXT         0x00B5
  #define HID_USAGE_CONSUMER_SCAN_PREVIOUS     0x00B6
#endif

#endif // GHOST_HID_KEYCODES_H

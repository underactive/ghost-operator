#include "keys.h"

const KeyDef AVAILABLE_KEYS[] = {
  // Ghost keys (F13-F24) -- invisible to OS, ideal for keep-alive
  { HID_KEY_F13,          "F13",    false },
  { HID_KEY_F14,          "F14",    false },
  { HID_KEY_F15,          "F15",    false },
  { HID_KEY_F16,          "F16",    false },
  { HID_KEY_F17,          "F17",    false },
  { HID_KEY_F18,          "F18",    false },
  { HID_KEY_F19,          "F19",    false },
  { HID_KEY_F20,          "F20",    false },
  { HID_KEY_F21,          "F21",    false },
  { HID_KEY_F22,          "F22",    false },
  { HID_KEY_F23,          "F23",    false },
  { HID_KEY_F24,          "F24",    false },
  // System keys
  { HID_KEY_SCROLL_LOCK,  "ScrLk",  false },
  { HID_KEY_PAUSE,        "Pause",  false },
  { HID_KEY_NUM_LOCK,     "NumLk",  false },
  // Modifier keys
  { HID_KEY_SHIFT_LEFT,   "LShift", true  },
  { HID_KEY_CONTROL_LEFT, "LCtrl",  true  },
  { HID_KEY_ALT_LEFT,     "LAlt",   true  },
  { HID_KEY_SHIFT_RIGHT,  "RShift", true  },
  { HID_KEY_CONTROL_RIGHT,"RCtrl",  true  },
  { HID_KEY_ALT_RIGHT,    "RAlt",   true  },
  // Common keys (visible -- use with caution)
  { HID_KEY_ESCAPE,       "Esc",    false },
  { HID_KEY_SPACE,        "Space",  false },
  { HID_KEY_ENTER,        "Enter",  false },
  // Arrow keys
  { HID_KEY_ARROW_UP,     "Up",     false },
  { HID_KEY_ARROW_DOWN,   "Down",   false },
  { HID_KEY_ARROW_LEFT,   "Left",   false },
  { HID_KEY_ARROW_RIGHT,  "Right",  false },
  // Disabled
  { 0x00,                 "NONE",   false },
};

// Verify NUM_KEYS matches array (compile-time check)
static_assert(sizeof(AVAILABLE_KEYS) / sizeof(AVAILABLE_KEYS[0]) == NUM_KEYS,
              "NUM_KEYS define must match AVAILABLE_KEYS[] size");

const MenuItem MENU_ITEMS[MENU_ITEM_COUNT] = {
  // Keyboard settings
  { MENU_HEADING, "Keyboard",   NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Key min",    "Minimum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MIN },
  { MENU_VALUE,   "Key max",    "Maximum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MAX },
  { MENU_ACTION,  "Key slots",  "Configure 8 key slots", FMT_DURATION_MS, 0, 0, 0, SET_KEY_SLOTS },
  // Mouse settings
  { MENU_HEADING, "Mouse",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Move duration", "Duration of mouse jiggle movement", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_JIG },
  { MENU_VALUE,   "Idle duration", "Pause between mouse jiggles", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_IDLE },
  { MENU_VALUE,   "Move style", "Movement pattern (Bezier=sweep, Brownian=jiggle)", FMT_MOUSE_STYLE, 0, 1, 1, SET_MOUSE_STYLE },
  { MENU_VALUE,   "Move size",  "Mouse movement step size in pixels", FMT_PIXELS, 1, 5, 1, SET_MOUSE_AMP },
  { MENU_VALUE,   "Scroll",     "Random scroll wheel during mouse movement", FMT_ON_OFF, 0, 1, 1, SET_SCROLL },
  // Profile settings
  { MENU_HEADING, "Profiles",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Lazy adjust",   "Slow down timing by this percent", FMT_PERCENT_NEG, 0, 50, 5, SET_LAZY_PCT },
  { MENU_VALUE,   "Busy adjust",   "Speed up timing by this percent", FMT_PERCENT, 0, 50, 5, SET_BUSY_PCT },
  // Display settings
  { MENU_HEADING, "Display",       NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Brightness",    "OLED display brightness", FMT_PERCENT, 10, 100, 10, SET_DISPLAY_BRIGHT },
  { MENU_VALUE,   "Saver bright",  "Screensaver dimmed brightness", FMT_PERCENT, 10, 100, 10, SET_SAVER_BRIGHT },
  { MENU_VALUE,   "Saver T.O.",    "Screensaver timeout (0=never)", FMT_SAVER_NAME, 0, 5, 1, SET_SAVER_TIMEOUT },
  { MENU_VALUE,   "Animation",     "Status animation style", FMT_ANIM_NAME, 0, 5, 1, SET_ANIMATION },
  // Device settings
  { MENU_HEADING, "Device",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_ACTION,  "Device name",   "BLE device name (reboot to apply)", FMT_DURATION_MS, 0, 0, 0, SET_DEVICE_NAME },
  { MENU_VALUE,   "BT while USB",  "Keep Bluetooth active when USB plugged in", FMT_ON_OFF, 0, 1, 1, SET_BT_WHILE_USB },
  { MENU_ACTION,  "Reset defaults", "Restore all settings to factory defaults", FMT_DURATION_MS, 0, 0, 0, SET_RESTORE_DEFAULTS },
  { MENU_ACTION,  "Reboot",        "Restart device (applies pending changes)", FMT_DURATION_MS, 0, 0, 0, SET_REBOOT },
  // About
  { MENU_HEADING, "About",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Version",       "(c) 2026 TARS Industrial Technical Solutions", FMT_VERSION, 0, 0, 0, SET_VERSION },
};

const int8_t MOUSE_DIRS[][2] = {
  { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
  { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
};
const int NUM_DIRS = 8;

const uint8_t SAVER_MINUTES[] = { 0, 1, 5, 10, 15, 30 };
const char*   SAVER_NAMES[]   = { "Never", "1 min", "5 min", "10 min", "15 min", "30 min" };

const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_";

const char* MODE_NAMES[] = { "NORMAL", "MENU", "SLOTS", "NAME" };
const char* PROFILE_NAMES[] = { "LAZY", "NORMAL", "BUSY" };
const char* ANIM_NAMES[] = { "ECG", "EQ", "Ghost", "Matrix", "Radar", "None" };
const char* MOUSE_STYLE_NAMES[] = { "Bezier", "Brownian" };
const char* ON_OFF_NAMES[] = { "Off", "On" };

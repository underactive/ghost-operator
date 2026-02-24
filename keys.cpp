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
  // idx 0-1: Volume (vol-only; orphan heading auto-hides in other modes)
  { MENU_HEADING, "Volume",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Theme",         "Volume control display theme", FMT_VOLUME_THEME, 0, 2, 1, SET_VOLUME_THEME },
  // idx 2-5: Simulation (sim-only; orphan heading auto-hides in simple mode)
  { MENU_HEADING, "Simulation",    NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Job type",      "Daily schedule template for work simulation", FMT_JOB_SIM, 0, 2, 1, SET_JOB_SIM },
  { MENU_VALUE,   "Job start",     "Time of day your workday begins", FMT_TIME_5MIN, 0, 287, 1, SET_JOB_START_TIME },
  { MENU_VALUE,   "Performance",   "Job performance level (0-11)", FMT_PERF_LEVEL, 0, 11, 1, SET_JOB_PERFORMANCE },
  // idx 6-11: Keyboard
  { MENU_HEADING, "Keyboard",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Key min",       "Minimum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MIN },
  { MENU_VALUE,   "Key max",       "Maximum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MAX },
  { MENU_ACTION,  "Key slots",     "Configure 8 key slots", FMT_DURATION_MS, 0, 0, 0, SET_KEY_SLOTS },
  { MENU_VALUE,   "Window switch", "Alt/Cmd-Tab at long intervals (WARNING: may move focus)", FMT_ON_OFF, 0, 1, 1, SET_WINDOW_SWITCH },
  { MENU_VALUE,   "Switch keys",   "Key combo for window switching", FMT_SWITCH_KEYS, 0, 1, 1, SET_SWITCH_KEYS },
  // idx 12-19: Mouse
  { MENU_HEADING, "Mouse",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Move duration", "Duration of mouse jiggle movement", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_JIG },
  { MENU_VALUE,   "Idle duration", "Pause between mouse jiggles", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_IDLE },
  { MENU_VALUE,   "Move style",    "Movement pattern (Bezier=sweep, Brownian=jiggle)", FMT_MOUSE_STYLE, 0, 1, 1, SET_MOUSE_STYLE },
  { MENU_VALUE,   "Move size",     "Mouse movement step size in pixels", FMT_PIXELS, 1, 5, 1, SET_MOUSE_AMP },
  { MENU_VALUE,   "Scroll",        "Random scroll wheel during mouse movement", FMT_ON_OFF, 0, 1, 1, SET_SCROLL },
  { MENU_VALUE,   "Auto-clicks",   "Inject clicks during mouse phases", FMT_ON_OFF, 0, 1, 1, SET_PHANTOM_CLICKS },
  { MENU_VALUE,   "Click type",    "Mouse button for auto-clicks", FMT_CLICK_TYPE, 0, 1, 1, SET_CLICK_TYPE },
  // idx 20-22: Profiles
  { MENU_HEADING, "Profiles",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Lazy adjust",   "Slow down timing by this percent", FMT_PERCENT_NEG, 0, 50, 5, SET_LAZY_PCT },
  { MENU_VALUE,   "Busy adjust",   "Speed up timing by this percent", FMT_PERCENT, 0, 50, 5, SET_BUSY_PCT },
  // idx 23-28: Display
  { MENU_HEADING, "Display",       NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Brightness",    "OLED display brightness", FMT_PERCENT, 10, 100, 10, SET_DISPLAY_BRIGHT },
  { MENU_VALUE,   "Saver bright",  "Screensaver dimmed brightness", FMT_PERCENT, 10, 100, 10, SET_SAVER_BRIGHT },
  { MENU_VALUE,   "Saver T.O.",    "Screensaver timeout (0=never)", FMT_SAVER_NAME, 0, 5, 1, SET_SAVER_TIMEOUT },
  { MENU_VALUE,   "Animation",     "Status animation style", FMT_ANIM_NAME, 0, 5, 1, SET_ANIMATION },
  { MENU_VALUE,   "Header txt",   "Normal screen header shows job or device name", FMT_HEADER_DISP, 0, 1, 1, SET_HEADER_DISPLAY },
  // idx 29-31: Sound
  { MENU_HEADING, "Sound",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Sound",        "Mechanical keyboard sound on keystroke", FMT_ON_OFF, 0, 1, 1, SET_SOUND_ENABLED },
  { MENU_VALUE,   "Key sound",    "Keyboard switch sound profile", FMT_KEY_SOUND, 0, 4, 1, SET_SOUND_TYPE },
  // idx 32-34: Schedule
  { MENU_HEADING, "Schedule",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_ACTION,  "Set clock",     "Set device time manually", FMT_DURATION_MS, 0, 0, 0, SET_SET_CLOCK },
  { MENU_ACTION,  "Schedule",      "Configure schedule mode & times", FMT_DURATION_MS, 0, 0, 0, SET_SCHEDULE_MODE },
  // idx 35-42: Device
  { MENU_HEADING, "Device",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_ACTION,  "Mode",          "Select operation mode (reboot required)", FMT_DURATION_MS, 0, 0, 0, SET_OP_MODE },
  { MENU_ACTION,  "BLE identity",  "BLE device name preset (reboot to apply)", FMT_DURATION_MS, 0, 0, 0, SET_BLE_IDENTITY },
  { MENU_VALUE,   "BT while USB",  "Keep Bluetooth active when USB plugged in", FMT_ON_OFF, 0, 1, 1, SET_BT_WHILE_USB },
  { MENU_VALUE,   "Dashboard",     "Show dashboard link on USB connect (reboot to apply)", FMT_ON_OFF, 0, 1, 1, SET_DASHBOARD },
  { MENU_VALUE,   "Invert dial",   "Reverse encoder rotation direction", FMT_ON_OFF, 0, 1, 1, SET_INVERT_DIAL },
  { MENU_ACTION,  "Restore defaults", "Restore all settings to factory defaults", FMT_DURATION_MS, 0, 0, 0, SET_RESTORE_DEFAULTS },
  { MENU_ACTION,  "Reboot",        "Restart device (applies pending changes)", FMT_DURATION_MS, 0, 0, 0, SET_REBOOT },
  // idx 43-46: About
  { MENU_HEADING, "About",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Uptime",        "Time since last boot", FMT_UPTIME, 0, 0, 0, SET_UPTIME },
  { MENU_VALUE,   "Die temp",      "nRF52840 internal die temperature", FMT_DIE_TEMP, 0, 0, 0, SET_DIE_TEMP },
  { MENU_VALUE,   "Version",       COPYRIGHT_TEXT, FMT_VERSION, 0, 0, 0, SET_VERSION },
};

// Verify MENU_ITEM_COUNT matches array (compile-time check)
static_assert(sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]) == MENU_ITEM_COUNT,
              "MENU_ITEM_COUNT define must match MENU_ITEMS[] size");

// Runtime validation of MENU_IDX_* defines — called once from setup().
// static_assert can't evaluate const (non-constexpr) array elements on ARM GCC,
// so we validate at boot and halt on mismatch (developer-only safety net).
void validateMenuIndices() {
  bool ok = true;
  auto check = [&](int idx, uint16_t expected, const char* name) {
    if (idx >= MENU_ITEM_COUNT || MENU_ITEMS[idx].settingId != expected) {
      Serial.print("[FATAL] MENU_IDX_");
      Serial.print(name);
      Serial.print(" (");
      Serial.print(idx);
      Serial.print(") != ");
      Serial.println(expected);
      ok = false;
    }
  };
  check(MENU_IDX_KEY_SLOTS,    SET_KEY_SLOTS,     "KEY_SLOTS");
  check(MENU_IDX_SET_CLOCK,    SET_SET_CLOCK,     "SET_CLOCK");
  check(MENU_IDX_SCHEDULE,     SET_SCHEDULE_MODE, "SCHEDULE");
  check(MENU_IDX_OP_MODE,      SET_OP_MODE,       "OP_MODE");
  check(MENU_IDX_BLE_IDENTITY, SET_BLE_IDENTITY,  "BLE_IDENTITY");
  check(MENU_IDX_UPTIME,       SET_UPTIME,        "UPTIME");
  check(MENU_IDX_DIE_TEMP,     SET_DIE_TEMP,      "DIE_TEMP");
  check(MENU_IDX_VERSION,      SET_VERSION,        "VERSION");
  if (!ok) {
    Serial.println("[FATAL] MENU_IDX mismatch — halting");
    while (1) delay(1000);
  }
}

const int8_t MOUSE_DIRS[][2] = {
  { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
  { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
};
const int NUM_DIRS = 8;

const uint8_t SAVER_MINUTES[] = { 0, 1, 5, 10, 15, 30 };
const char*   SAVER_NAMES[]   = { "Never", "1 min", "5 min", "10 min", "15 min", "30 min" };

const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_";

const char* MODE_NAMES[] = { "NORMAL", "MENU", "SLOTS", "NAME", "DECOY", "SCHED", "MODE", "CLOCK" };
const char* PROFILE_NAMES[] = { "LAZY", "NORMAL", "BUSY" };
const char* PROFILE_NAMES_TITLE[] = { "Lazy", "Normal", "Busy" };
const char* ANIM_NAMES[] = { "ECG", "EQ", "Ghost", "Matrix", "Radar", "None" };
const char* MOUSE_STYLE_NAMES[] = { "Bezier", "Brownian" };
const char* SWITCH_KEYS_NAMES[] = { "AltTab", "CmdTab" };
const char* ON_OFF_NAMES[] = { "Off", "On" };
const char* KB_SOUND_NAMES[] = { "MX Blue", "MX Brown", "Membrane", "Buckling", "Thock" };
const char* VOLUME_THEME_NAMES[] = { "Basic", "Retro", "Futuristic" };
const char* SCHEDULE_MODE_NAMES[] = { "Off", "Sleep", "Full auto" };

const char* const DECOY_NAMES[] = {
  "Magic Keyboard",
  "Magic Mouse",
  "Magic Trackpad",
  "MX Master 3S",
  "MX Keys S",
  "MX Anywhere 3S",
  "K380 Keyboard",
  "ERGO K860",
  "Keychron K2",
  "Keychron V1"
};

const char* const DECOY_MANUFACTURERS[] = {
  "Apple Inc.",
  "Apple Inc.",
  "Apple Inc.",
  "Logitech",
  "Logitech",
  "Logitech",
  "Logitech",
  "Logitech",
  "Keychron",
  "Keychron"
};

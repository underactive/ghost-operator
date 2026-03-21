#include "keys.h"
#include "sim_data.h"

const KeyDef AVAILABLE_KEYS[] = {
  // Function keys F1-F20
  { HID_KEY_F1,           "F1",     false },
  { HID_KEY_F2,           "F2",     false },
  { HID_KEY_F3,           "F3",     false },
  { HID_KEY_F4,           "F4",     false },
  { HID_KEY_F5,           "F5",     false },
  { HID_KEY_F6,           "F6",     false },
  { HID_KEY_F7,           "F7",     false },
  { HID_KEY_F8,           "F8",     false },
  { HID_KEY_F9,           "F9",     false },
  { HID_KEY_F10,          "F10",    false },
  { HID_KEY_F11,          "F11",    false },
  { HID_KEY_F12,          "F12",    false },
  { HID_KEY_F13,          "F13",    false },
  { HID_KEY_F14,          "F14",    false },
  { HID_KEY_F15,          "F15",    false },
  { HID_KEY_F16,          "F16",    false },
  { HID_KEY_F17,          "F17",    false },
  { HID_KEY_F18,          "F18",    false },
  { HID_KEY_F19,          "F19",    false },
  { HID_KEY_F20,          "F20",    false },
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

const char* BALL_SPEED_NAMES[]  = { "Slow", "Normal", "Fast" };
const char* PADDLE_SIZE_NAMES[] = { "Small", "Normal", "Large", "XL" };
const char* SNAKE_SPEED_NAMES[] = { "Slow", "Normal", "Fast" };
const char* SNAKE_WALL_NAMES[]  = { "Solid", "Wrap" };
const char* RACER_SPEED_NAMES[] = { "Slow", "Normal", "Fast" };

const MenuItem MENU_ITEMS[MENU_ITEM_COUNT] = {
  // idx 0-3: Breakout (breakout-only; orphan heading auto-hides in other modes)
  { MENU_HEADING, "Breakout",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Ball speed",    "Ball movement speed", FMT_BALL_SPEED, 0, 2, 1, SET_BALL_SPEED },
  { MENU_VALUE,   "Paddle size",   "Paddle width for catching the ball", FMT_PADDLE_SIZE, 0, 3, 1, SET_PADDLE_SIZE },
  { MENU_VALUE,   "Start lives",   "Number of lives per game", FMT_LIVES, 1, 5, 1, SET_START_LIVES },
  // idx 4-7: Snake (snake-only; orphan heading auto-hides in other modes)
  { MENU_HEADING, "Snake",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Speed",         "Snake movement speed", FMT_SNAKE_SPEED, 0, 2, 1, SET_SNAKE_SPEED },
  { MENU_VALUE,   "Walls",         "Wall collision behavior", FMT_SNAKE_WALLS, 0, 1, 1, SET_SNAKE_WALLS },
  { MENU_VALUE,   "High score",    "Best Snake score (persistent)", FMT_SNAKE_HIGH_SCORE, 0, 0, 0, SET_SNAKE_HIGH_SCORE },
  // idx 8-10: Racer (racer-only; orphan heading auto-hides in other modes)
  { MENU_HEADING, "Racer",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Speed",         "Racer game speed", FMT_RACER_SPEED, 0, 2, 1, SET_RACER_SPEED },
  { MENU_VALUE,   "High score",    "Best Racer score (persistent)", FMT_RACER_HIGH_SCORE, 0, 0, 0, SET_RACER_HIGH_SCORE },
  // idx 11-14: Volume (vol-only; orphan heading auto-hides in other modes)
  { MENU_HEADING, "Volume",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Theme",         "Volume control display theme", FMT_VOLUME_THEME, 0, 2, 1, SET_VOLUME_THEME },
  { MENU_VALUE,   "Knob btn",      "Encoder button action in Volume mode", FMT_ENC_BTN_ACTION, 0, 1, 1, SET_ENC_BUTTON },
  { MENU_VALUE,   "Side btn",      "Side button (D7) action in Volume mode", FMT_SIDE_BTN_ACTION, 0, 2, 1, SET_SIDE_BUTTON },
  // idx 15-18: Simulation (sim-only)
  { MENU_HEADING, "Simulation",    NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Job type",      "Daily schedule template for work simulation", FMT_JOB_SIM, 0, 2, 1, SET_JOB_SIM },
  { MENU_VALUE,   "Job start",     "Time of day your workday begins", FMT_TIME_5MIN, 0, 287, 1, SET_JOB_START_TIME },
  { MENU_VALUE,   "Performance",   "Activity level (8≈baseline, 11=max)", FMT_PERF_LEVEL, 0, 11, 1, SET_JOB_PERFORMANCE },
  // idx 19-24: Keyboard
  { MENU_HEADING, "Keyboard",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Key min",       "Minimum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MIN },
  { MENU_VALUE,   "Key max",       "Maximum delay between keystrokes", FMT_DURATION_MS, 500, 30000, 500, SET_KEY_MAX },
  { MENU_ACTION,  "Key slots",     "Configure 8 key slots", FMT_DURATION_MS, 0, 0, 0, SET_KEY_SLOTS },
  { MENU_VALUE,   "Window switch", "Alt/Cmd-Tab at long intervals (WARNING: may move focus)", FMT_ON_OFF, 0, 1, 1, SET_WINDOW_SWITCH },
  { MENU_VALUE,   "Switch keys",   "Key combo for window switching", FMT_SWITCH_KEYS, 0, 1, 1, SET_SWITCH_KEYS },
  // idx 25-32: Mouse
  { MENU_HEADING, "Mouse",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Move duration", "Duration of mouse jiggle movement", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_JIG },
  { MENU_VALUE,   "Idle duration", "Pause between mouse jiggles", FMT_DURATION_MS, 500, 90000, 500, SET_MOUSE_IDLE },
  { MENU_VALUE,   "Move style",    "Movement pattern (Bezier=sweep, Brownian=jiggle)", FMT_MOUSE_STYLE, 0, 1, 1, SET_MOUSE_STYLE },
  { MENU_VALUE,   "Move size",     "Mouse movement step size in pixels", FMT_PIXELS, 1, 5, 1, SET_MOUSE_AMP },
  { MENU_VALUE,   "Scroll",        "Random scroll wheel during mouse movement", FMT_ON_OFF, 0, 1, 1, SET_SCROLL },
  { MENU_VALUE,   "Auto-clicks",   "Inject clicks during mouse phases", FMT_ON_OFF, 0, 1, 1, SET_PHANTOM_CLICKS },
  { MENU_ACTION,  "Click slots",   "Configure 7 click action slots", FMT_DURATION_MS, 0, 0, 0, SET_CLICK_SLOTS },
  // idx 33-35: Profiles
  { MENU_HEADING, "Profiles",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Lazy adjust",   "Slow down timing by this percent", FMT_PERCENT_NEG, 0, 50, 5, SET_LAZY_PCT },
  { MENU_VALUE,   "Busy adjust",   "Speed up timing by this percent", FMT_PERCENT, 0, 50, 5, SET_BUSY_PCT },
  // idx 36-41: Display
  { MENU_HEADING, "Display",       NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Brightness",    "OLED display brightness", FMT_PERCENT, 10, 100, 10, SET_DISPLAY_BRIGHT },
  { MENU_VALUE,   "Saver bright",  "Screensaver dimmed brightness", FMT_PERCENT, 10, 100, 10, SET_SAVER_BRIGHT },
  { MENU_VALUE,   "Saver T.O.",    "Screensaver timeout (0=never)", FMT_SAVER_NAME, 0, 5, 1, SET_SAVER_TIMEOUT },
  { MENU_VALUE,   "Animation",     "Status animation style", FMT_ANIM_NAME, 0, 5, 1, SET_ANIMATION },
  { MENU_VALUE,   "Header txt",   "Normal screen header shows job or device name", FMT_HEADER_DISP, 0, 1, 1, SET_HEADER_DISPLAY },
  // idx 42-45: Sound
  { MENU_HEADING, "Sound",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "System sounds", "BLE connect/disconnect alert tones", FMT_ON_OFF, 0, 1, 1, SET_SYSTEM_SOUND },
  { MENU_VALUE,   "Keybd sounds", "Mechanical keyboard sound on keystroke", FMT_ON_OFF, 0, 1, 1, SET_SOUND_ENABLED },
  { MENU_VALUE,   "Key sound",    "Keyboard switch sound profile", FMT_KEY_SOUND, 0, 4, 1, SET_SOUND_TYPE },
  // idx 46-48: Schedule
  { MENU_HEADING, "Schedule",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_ACTION,  "Set clock",     "Set device time manually", FMT_DURATION_MS, 0, 0, 0, SET_SET_CLOCK },
  { MENU_ACTION,  "Schedule",      "Configure schedule mode & times", FMT_DURATION_MS, 0, 0, 0, SET_SCHEDULE_MODE },
  // idx 49-57: Device
  { MENU_HEADING, "Device",        NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_ACTION,  "Mode",          "Select operation mode (reboot required)", FMT_DURATION_MS, 0, 0, 0, SET_OP_MODE },
  { MENU_ACTION,  "Bluetooth name", "BLE device name preset (reboot to apply)", FMT_DURATION_MS, 0, 0, 0, SET_BLE_IDENTITY },
  { MENU_VALUE,   "BT while USB",  "Keep Bluetooth active when USB plugged in", FMT_ON_OFF, 0, 1, 1, SET_BT_WHILE_USB },
  { MENU_VALUE,   "Dashboard",     "Show dashboard link on USB connect (reboot to apply)", FMT_ON_OFF, 0, 1, 1, SET_DASHBOARD },
  { MENU_VALUE,   "Invert dial",   "Reverse encoder rotation direction", FMT_ON_OFF, 0, 1, 1, SET_INVERT_DIAL },
  { MENU_VALUE,   "Activity LEDs", "Flash blue/green LEDs on KB/mouse HID activity", FMT_ON_OFF, 0, 1, 1, SET_ACTIVITY_LEDS },
  { MENU_ACTION,  "Restore defaults", "Restore all settings to factory defaults", FMT_DURATION_MS, 0, 0, 0, SET_RESTORE_DEFAULTS },
  { MENU_ACTION,  "Reboot",        "Restart device (applies pending changes)", FMT_DURATION_MS, 0, 0, 0, SET_REBOOT },
  // idx 58-62: About
  { MENU_HEADING, "About",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Uptime",        "Time since last boot", FMT_UPTIME, 0, 0, 0, SET_UPTIME },
  { MENU_VALUE,   "Die temp",      "nRF52840 internal die temperature", FMT_DIE_TEMP, 0, 0, 0, SET_DIE_TEMP },
  { MENU_VALUE,   "High score",    "Best Breakout score (persistent)", FMT_HIGH_SCORE, 0, 0, 0, SET_HIGH_SCORE },
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
  check(MENU_IDX_CLICK_SLOTS,  SET_CLICK_SLOTS,   "CLICK_SLOTS");
  check(MENU_IDX_SET_CLOCK,    SET_SET_CLOCK,     "SET_CLOCK");
  check(MENU_IDX_SCHEDULE,     SET_SCHEDULE_MODE, "SCHEDULE");
  check(MENU_IDX_OP_MODE,      SET_OP_MODE,       "OP_MODE");
  check(MENU_IDX_BLE_IDENTITY, SET_BLE_IDENTITY,  "BLE_IDENTITY");
  check(MENU_IDX_UPTIME,       SET_UPTIME,        "UPTIME");
  check(MENU_IDX_DIE_TEMP,     SET_DIE_TEMP,      "DIE_TEMP");
  check(MENU_IDX_HIGH_SCORE,   SET_HIGH_SCORE,    "HIGH_SCORE");
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

const char* MODE_NAMES[] = { "NORMAL", "MENU", "SLOTS", "NAME", "DECOY", "SCHED", "MODE", "CLOCK", "CRSL", "CSLOT" };
const char* PROFILE_NAMES[] = { "LAZY", "NORMAL", "BUSY" };
const char* PROFILE_NAMES_TITLE[] = { "Lazy", "Normal", "Busy" };
const char* ANIM_NAMES[] = { "ECG", "EQ", "Ghost", "Matrix", "Radar", "None" };
const char* MOUSE_STYLE_NAMES[] = { "Bezier", "Brownian" };
const char* SWITCH_KEYS_NAMES[] = { "AltTab", "CmdTab" };
const char* ON_OFF_NAMES[] = { "Off", "On" };
const char* KB_SOUND_NAMES[] = { "MX Blue", "MX Brown", "Membrane", "Buckling", "Thock" };
const char* VOLUME_THEME_NAMES[] = { "Basic", "Retro", "Futuristic" };
const char* ENC_BTN_ACTION_NAMES[]  = { "Play/Pause", "Mute" };
const char* SIDE_BTN_ACTION_NAMES[] = { "Next", "Mute", "Play/Pause" };
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

// ============================================================================
// CAROUSEL DESCRIPTIONS (per-option help text)
// ============================================================================

static const char* const ANIM_DESCS[] = {
  "Heart rate monitor line",
  "Audio spectrum bars",
  "Floating ghost sprite",
  "Falling character rain",
  "Rotating radar sweep",
  "No animation"
};

static const char* const MOUSE_STYLE_DESCS[] = {
  "Smooth curved sweeps",
  "Random jitter movement"
};

static const char* const SAVER_TIMEOUT_DESCS[] = {
  "Screensaver disabled",
  "Activate after 1 minute",
  "Activate after 5 minutes",
  "Activate after 10 minutes",
  "Activate after 15 minutes",
  "Activate after 30 minutes"
};

static const char* const KB_SOUND_DESCS[] = {
  "Clicky mechanical switch",
  "Tactile quiet switch",
  "Soft rubber dome press",
  "Classic IBM spring snap",
  "Deep muted keystroke"
};

static const char* const SWITCH_KEYS_DESCS[] = {
  "Alt-Tab (Windows/Linux)",
  "Cmd-Tab (macOS)"
};

static const char* const HEADER_DISP_DESCS[] = {
  "Show job simulation name",
  "Show custom device name"
};

static const char* const VOLUME_THEME_DESCS[] = {
  "Clean minimal volume bar",
  "Pixelated retro style",
  "Sleek sci-fi interface"
};

static const char* const ENC_BTN_ACTION_DESCS[] = {
  "Toggle play/pause on press",
  "Toggle audio mute on press"
};

static const char* const SIDE_BTN_ACTION_DESCS[] = {
  "Next track (double-click = previous)",
  "Toggle audio mute on press",
  "Toggle play/pause on press"
};

static const char* const JOB_SIM_DESCS[] = {
  "General office worker",
  "Software dev workflow",
  "Creative design workflow"
};

// ============================================================================
// CAROUSEL CONFIG TABLE
// ============================================================================

static const CarouselConfig CAROUSEL_CONFIGS[] = {
  { "ANIMATION STYLE",   (const char* const*)ANIM_NAMES,          ANIM_DESCS,          ANIM_STYLE_COUNT,    SET_ANIMATION      },
  { "MOVE STYLE",        (const char* const*)MOUSE_STYLE_NAMES,   MOUSE_STYLE_DESCS,   MOUSE_STYLE_COUNT,   SET_MOUSE_STYLE    },
  { "SCREENSAVER T.O.",  (const char* const*)SAVER_NAMES,         SAVER_TIMEOUT_DESCS,  SAVER_TIMEOUT_COUNT, SET_SAVER_TIMEOUT  },
  { "KEY SOUND",         (const char* const*)KB_SOUND_NAMES,      KB_SOUND_DESCS,       KB_SOUND_COUNT,      SET_SOUND_TYPE     },
  { "SWITCH KEYS",       (const char* const*)SWITCH_KEYS_NAMES,   SWITCH_KEYS_DESCS,    SWITCH_KEYS_COUNT,   SET_SWITCH_KEYS    },
  { "HEADER TEXT",       (const char* const*)HEADER_DISP_NAMES,   HEADER_DISP_DESCS,    2,                   SET_HEADER_DISPLAY },
  { "VOLUME THEME",      (const char* const*)VOLUME_THEME_NAMES,  VOLUME_THEME_DESCS,   VOLUME_THEME_COUNT,  SET_VOLUME_THEME   },
  { "KNOB BUTTON",       (const char* const*)ENC_BTN_ACTION_NAMES,  ENC_BTN_ACTION_DESCS,  ENC_BTN_ACTION_COUNT,  SET_ENC_BUTTON },
  { "SIDE BUTTON",       (const char* const*)SIDE_BTN_ACTION_NAMES, SIDE_BTN_ACTION_DESCS, SIDE_BTN_ACTION_COUNT, SET_SIDE_BUTTON },
  { "JOB TYPE",          (const char* const*)JOB_SIM_NAMES,       JOB_SIM_DESCS,        JOB_SIM_COUNT,       SET_JOB_SIM        },
};
#define CAROUSEL_CONFIG_COUNT (sizeof(CAROUSEL_CONFIGS) / sizeof(CAROUSEL_CONFIGS[0]))

const CarouselConfig* getCarouselConfig(uint8_t settingId) {
  for (uint8_t i = 0; i < CAROUSEL_CONFIG_COUNT; i++) {
    if (CAROUSEL_CONFIGS[i].settingId == settingId) return &CAROUSEL_CONFIGS[i];
  }
  return NULL;
}

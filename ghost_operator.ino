/*
 * BLE Keyboard/Mouse Device - "Ghost Operator"
 * ENCODER MENU + FLASH STORAGE
 * For Seeed XIAO nRF52840
 * 
 * MODES:
 *   NORMAL - Live status; encoder switches profile, button cycles KB/MS combos
 *   MENU   - Scrollable settings menu; encoder navigates/edits, button selects/confirms
 *   SLOTS  - 8-key slot editor; encoder cycles key, button advances slot
 *
 * PIN ASSIGNMENTS:
 * D0 (P0.02) - Encoder A (digital interrupt)
 * D1 (P0.03) - Encoder B (digital interrupt)
 * D2 (P0.28) - Encoder Button (toggle key/mouse)
 * D3 (P0.29) - Function Button (mode cycle / long=sleep)
 * D4 (P0.04) - SDA (OLED)
 * D5 (P0.05) - SCL (OLED)
 * D6 (P0.30) - Activity LED (optional)
 * 
 * CONTROLS:
 * - Encoder rotation: Switch profile (NORMAL), navigate/adjust (MENU), cycle slot key (SLOTS)
 * - Encoder button: Cycle KB/MS combos (NORMAL), select/confirm (MENU), advance slot (SLOTS)
 * - Function button short: Open/close menu (NORMAL↔MENU), back to menu (SLOTS→MENU)
 * - Function button long (3s): Enter deep sleep
 */

#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <nrf_soc.h>
#include <nrf_power.h>
#include <math.h>

using namespace Adafruit_LittleFS_Namespace;

// ============================================================================
// VERSION & CONFIG
// ============================================================================
#define VERSION "1.5.0"
#define DEVICE_NAME "GhostOperator"
#define SETTINGS_FILE "/settings.dat"
#define SETTINGS_MAGIC 0x50524F46  // "PROF"
#define NUM_SLOTS 8
#define NUM_KEYS 10  // must match AVAILABLE_KEYS[] array size

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PIN_ENCODER_A    0   // D0 - Encoder A (interrupt)
#define PIN_ENCODER_B    1   // D1 - Encoder B (interrupt)
#define PIN_ENCODER_BTN  2   // D2 - Encoder pushbutton
#define PIN_FUNC_BTN     3   // D3 - Function button
#define PIN_SDA          4   // D4 - I2C SDA
#define PIN_SCL          5   // D5 - I2C SCL
// Activity LED - use built-in or D6
#ifndef PIN_LED
#define PIN_LED          6   // D6 - Activity LED
#endif

// Internal battery pin - use board defaults if available
#ifndef PIN_VBAT
#define PIN_VBAT         32
#endif
#define PIN_VBAT_ENABLE  14

// nRF52840 GPIO for wake
#define PIN_FUNC_BTN_NRF  29

// nRF52840 GPIO numbers for direct port reads in encoder ISR
// Must match Arduino D0/D1 → P0.02/P0.03 mapping on XIAO nRF52840
#define PIN_ENC_A_NRF  2   // P0.02 = D0
#define PIN_ENC_B_NRF  3   // P0.03 = D1

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
#define VALUE_MIN_MS          500UL       // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL     // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL     // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL       // 0.5 second increments
#define RANDOMNESS_PERCENT    20          // ±20% for mouse only
#define MIN_CLAMP_MS          500UL

#define MOUSE_MOVE_STEP_MS    20
#define DISPLAY_UPDATE_MS     100         // Faster for smooth countdown
#define BATTERY_READ_MS       60000UL
#define LONG_PRESS_MS         3000
#define SLEEP_DISPLAY_MS      2000
#define MODE_TIMEOUT_MS       30000       // Return to NORMAL after 30s inactivity

// Screensaver timeout options
#define SAVER_TIMEOUT_COUNT   6
#define DEFAULT_SAVER_IDX     3                           // 10 min
const uint8_t  SAVER_MINUTES[] = { 0, 1, 5, 10, 15, 30 };   // 0 = Never
const char*    SAVER_NAMES[]   = { "Never", "1 min", "5 min", "10 min", "15 min", "30 min" };

// Battery calibration (3.0V internal reference)
#define VBAT_MV_PER_LSB   (3000.0F / 4096.0F)
#define VBAT_DIVIDER      (1510.0F / 510.0F)
#define VBAT_MIN_MV       3200
#define VBAT_MAX_MV       4200

// ============================================================================
// UI MODES
// ============================================================================
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_COUNT };
const char* MODE_NAMES[] = { "NORMAL", "MENU", "SLOTS" };

// ============================================================================
// MENU DATA STRUCTURES
// ============================================================================

enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS };

enum SettingId {
  SET_KEY_MIN, SET_KEY_MAX, SET_KEY_SLOTS,
  SET_MOUSE_JIG, SET_MOUSE_IDLE, SET_MOUSE_AMP,
  SET_LAZY_PCT, SET_BUSY_PCT,
  SET_DISPLAY_BRIGHT, SET_SAVER_BRIGHT, SET_SAVER_TIMEOUT,
  SET_VERSION
};

struct MenuItem {
  MenuItemType type;
  const char* label;
  const char* helpText;
  MenuValueFormat format;
  uint32_t minVal, maxVal, step;
  uint8_t settingId;
};

#define MENU_ITEM_COUNT 17
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
  { MENU_VALUE,   "Move size",  "Mouse movement step size in pixels", FMT_PIXELS, 1, 5, 1, SET_MOUSE_AMP },
  // Profile settings
  { MENU_HEADING, "Profiles",      NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Lazy adjust",   "Slow down timing by this percent", FMT_PERCENT_NEG, 0, 50, 5, SET_LAZY_PCT },
  { MENU_VALUE,   "Busy adjust",   "Speed up timing by this percent", FMT_PERCENT, 0, 50, 5, SET_BUSY_PCT },
  // Display settings
  { MENU_HEADING, "Display",       NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Brightness",    "OLED display brightness", FMT_PERCENT, 10, 100, 10, SET_DISPLAY_BRIGHT },
  { MENU_VALUE,   "Saver bright",  "Screensaver dimmed brightness", FMT_PERCENT, 10, 100, 10, SET_SAVER_BRIGHT },
  { MENU_VALUE,   "Saver T.O.",    "Screensaver timeout (0=never)", FMT_SAVER_NAME, 0, 5, 1, SET_SAVER_TIMEOUT },
  // About
  { MENU_HEADING, "About",         NULL, FMT_DURATION_MS, 0, 0, 0, 0 },
  { MENU_VALUE,   "Version",       "(c) 2026 TARS Industries", FMT_VERSION, 0, 0, 0, SET_VERSION },
};

// ============================================================================
// TIMING PROFILES
// ============================================================================
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
const char* PROFILE_NAMES[] = { "LAZY", "NORMAL", "BUSY" };

Profile currentProfile = PROFILE_NORMAL;
unsigned long profileDisplayUntil = 0;  // millis() timestamp; show name until this time
#define PROFILE_DISPLAY_MS 3000

// ============================================================================
// SETTINGS STRUCTURE (saved to flash)
// ============================================================================
struct Settings {
  uint32_t magic;
  uint32_t keyIntervalMin;
  uint32_t keyIntervalMax;
  uint32_t mouseJiggleDuration;
  uint32_t mouseIdleDuration;
  uint8_t keySlots[NUM_SLOTS];
  uint8_t lazyPercent;    // 0-50, step 5, default 15
  uint8_t busyPercent;    // 0-50, step 5, default 15
  uint8_t saverTimeout;   // index into SAVER_MINUTES[] (0=Never..5=30min)
  uint8_t saverBrightness; // 10-100 in steps of 10, default 30
  uint8_t displayBrightness; // 10-100 in steps of 10, default 100
  uint8_t mouseAmplitude;  // 1-5, step 1, default 1 (pixels per movement step)
  uint8_t checksum;       // must remain last
};

Settings settings;

// Default values
void loadDefaults() {
  settings.magic = SETTINGS_MAGIC;
  settings.keyIntervalMin = 2000;      // 2 seconds
  settings.keyIntervalMax = 6500;      // 6.5 seconds
  settings.mouseJiggleDuration = 15000; // 15 seconds
  settings.mouseIdleDuration = 30000;   // 30 seconds
  settings.keySlots[0] = 0;            // F15
  for (int i = 1; i < NUM_SLOTS; i++) {
    settings.keySlots[i] = NUM_KEYS - 1;  // NONE
  }
  settings.lazyPercent = 15;
  settings.busyPercent = 15;
  settings.saverTimeout = DEFAULT_SAVER_IDX;  // 10 minutes
  settings.saverBrightness = 30;               // 30%
  settings.displayBrightness = 100;            // 100% (full brightness)
  settings.mouseAmplitude = 1;                 // 1px per step (default)
}

uint8_t calcChecksum() {
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)&settings;
  for (size_t i = 0; i < sizeof(Settings) - 1; i++) {
    sum ^= p[i];
  }
  return sum;
}

// ============================================================================
// KEY DEFINITIONS
// ============================================================================
struct KeyDef {
  uint8_t keycode;
  const char* name;
  bool isModifier;
};

const KeyDef AVAILABLE_KEYS[] = {
  { HID_KEY_F15,          "F15",    false },
  { HID_KEY_F14,          "F14",    false },
  { HID_KEY_F13,          "F13",    false },
  { HID_KEY_SCROLL_LOCK,  "ScrLk",  false },
  { HID_KEY_PAUSE,        "Pause",  false },
  { HID_KEY_NUM_LOCK,     "NumLk",  false },
  { HID_KEY_SHIFT_LEFT,   "LShift", true  },
  { HID_KEY_CONTROL_LEFT, "LCtrl",  true  },
  { HID_KEY_ALT_LEFT,     "LAlt",   true  },
  { 0x00,                 "NONE",   false },
};

// Verify NUM_KEYS matches array (compile-time check)
static_assert(sizeof(AVAILABLE_KEYS) / sizeof(AVAILABLE_KEYS[0]) == NUM_KEYS,
              "NUM_KEYS define must match AVAILABLE_KEYS[] size");

// ============================================================================
// MOUSE DIRECTIONS
// ============================================================================
const int8_t MOUSE_DIRS[][2] = {
  { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
  { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
};
const int NUM_DIRS = 8;

// ============================================================================
// GLOBALS
// ============================================================================

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false;

// Up arrow icon (5x7 pixels) - "enabled"
static const uint8_t PROGMEM iconOn[] = {
  0x20,  // ..#..
  0x70,  // .###.
  0xA8,  // #.#.#
  0x20,  // ..#..
  0x20,  // ..#..
  0x20,  // ..#..
  0x00   // .....
};

// Diagonal cross icon (5x5 pixels) - "disabled"
static const uint8_t PROGMEM iconOff[] = {
  0x88,  // #...#
  0x50,  // .#.#.
  0x20,  // ..#..
  0x50,  // .#.#.
  0x88,  // #...#
  0x00,  // .....
  0x00   // .....
};

// Bluetooth icon bitmap (5x8 pixels)
static const uint8_t PROGMEM btIcon[] = {
  0x20,  // ..#..
  0x30,  // ..##.
  0xA8,  // #.#.#
  0x70,  // .###.
  0x70,  // .###.
  0xA8,  // #.#.#
  0x30,  // ..##.
  0x20   // ..#..
};

// Splash screen bitmap (128x64, 1024 bytes) — from ghost_operator_splash.bin
static const uint8_t PROGMEM splashBitmap[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x80, 0x78, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0f, 0x80, 0x7c, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0f, 0xc0, 0x7c, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1f, 0xc0, 0xfe, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1f, 0xc0, 0xfe, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1f, 0xc0, 0xfe, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1f, 0x80, 0x7e, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0f, 0x80, 0x7c, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0f, 0x00, 0x7c, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x1c, 0x38, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x3e, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x3e, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x3e, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x3e, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x3e, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x3e, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x01, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x38, 0x18, 0x40, 0x00, 0x02, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x40, 0x00, 0x06, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x1c, 0x60, 0x08, 0x38, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x60, 0x18, 0x04, 0x0f, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x20, 0x00, 0xc0, 0x04, 0x04, 0x07, 0xc0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x1e, 0x21, 0xc0, 0x30, 0x01, 0x00, 0x02, 0x04, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x72, 0x63, 0xc1, 0xf1, 0x03, 0x00, 0x01, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x43, 0xe7, 0xc3, 0xf8, 0x86, 0x00, 0x00, 0x02, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x83, 0xbf, 0xff, 0xff, 0xc4, 0x40, 0x00, 0x01, 0x08, 0x70, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x84, 0x20, 0x3c, 0x41, 0xfc, 0x40, 0x60, 0x01, 0x83, 0xe0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x84, 0x40, 0x78, 0x9e, 0x18, 0x88, 0xf0, 0x00, 0xc7, 0x80, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xc3, 0x87, 0xe3, 0xe2, 0x78, 0x88, 0xf8, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x38, 0x8e, 0xce, 0x8f, 0x4f, 0x99, 0x8f, 0xf0, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1e, 0x0f, 0x9b, 0x21, 0xf1, 0xbe, 0xff, 0xe0, 0x3f, 0xb0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x07, 0xfc, 0xdc, 0x43, 0xb1, 0x3c, 0x8b, 0xc4, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1c, 0x1c, 0x1f, 0x90, 0xf8, 0x5e, 0x13, 0xd1, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0xf0, 0x30, 0x3e, 0x23, 0x8c, 0x78, 0x27, 0xa2, 0xe0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x74, 0x1f, 0x89, 0xe4, 0x44, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x07, 0xc1, 0x71, 0x0f, 0x89, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xc0, 0x1f, 0x23, 0xe3, 0x37, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x78, 0x06, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x03, 0xe0, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xe0, 0x0f, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// File system
File settingsFile(InternalFS);

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;

// State
volatile int encoderPos = 0;
int lastEncoderPos = 0;
bool deviceConnected = false;
bool keyEnabled = true;
bool mouseEnabled = true;
uint8_t activeSlot = 0;  // UI cursor for slot selection (not persisted)
uint8_t nextKeyIndex = 0;  // Pre-picked key for next keypress cycle

// UI Mode
UIMode currentMode = MODE_NORMAL;
unsigned long lastModeActivity = 0;
bool screensaverActive = false;

// Menu state
int8_t   menuCursor = -1;        // -1 = MENU title, 0+ = item index
int8_t   menuScrollOffset = 0;   // first visible item in viewport
bool     menuEditing = false;    // currently adjusting a value
int16_t  helpScrollPos = 0;      // character offset for help text scrolling
int8_t   helpScrollDir = 1;      // +1 = scroll left, -1 = scroll right
unsigned long helpScrollTimer = 0;

// Timing
unsigned long startTime = 0;
unsigned long lastKeyTime = 0;
unsigned long lastMouseStateChange = 0;
unsigned long lastMouseStep = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBatteryRead = 0;

// Current targets (with randomness applied for mouse)
unsigned long currentKeyInterval = 4000;
unsigned long currentMouseJiggle = 15000;
unsigned long currentMouseIdle = 30000;

// Mouse state machine
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };
MouseState mouseState = MOUSE_IDLE;
int8_t currentMouseDx = 0;
int8_t currentMouseDy = 0;
int32_t mouseNetX = 0;
int32_t mouseNetY = 0;
int32_t mouseReturnTotal = 0;  // displacement magnitude at start of RETURNING

// Battery
int batteryPercent = 100;
float batteryVoltage = 4.2;

// Button state
unsigned long funcBtnPressStart = 0;
bool funcBtnWasPressed = false;
bool sleepPending = false;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

unsigned long applyRandomness(unsigned long baseValue) {
  long variation = (long)(baseValue * RANDOMNESS_PERCENT / 100);
  long result = (long)baseValue + random(-variation, variation + 1);
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

String formatDuration(unsigned long ms, bool withUnit = true) {
  float sec = ms / 1000.0;
  String suffix = withUnit ? "s" : "";
  if (sec < 10) {
    return String(sec, 1) + suffix;
  } else {
    return String((int)sec) + suffix;
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

unsigned long applyProfile(unsigned long baseValue, int direction) {
  // direction: +1 = increase value, -1 = decrease value
  uint8_t pct = 0;
  switch (currentProfile) {
    case PROFILE_LAZY:  pct = settings.lazyPercent; break;
    case PROFILE_BUSY:  pct = settings.busyPercent; break;
    default: return baseValue;  // NORMAL = no change
  }
  long delta = (long)baseValue * pct / 100;
  long result = (long)baseValue + direction * delta;
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

// Profile-adjusted effective values
// BUSY: shorter KB intervals (-%), longer mouse jiggle (+%), shorter mouse idle (-%)
// LAZY: longer KB intervals (+%), shorter mouse jiggle (-%), longer mouse idle (+%)
unsigned long effectiveKeyMin()      { return applyProfile(settings.keyIntervalMin,      currentProfile == PROFILE_BUSY ? -1 : 1); }
unsigned long effectiveKeyMax()      { return applyProfile(settings.keyIntervalMax,      currentProfile == PROFILE_BUSY ? -1 : 1); }
unsigned long effectiveMouseJiggle() { return applyProfile(settings.mouseJiggleDuration, currentProfile == PROFILE_BUSY ? 1 : -1); }
unsigned long effectiveMouseIdle()   { return applyProfile(settings.mouseIdleDuration,   currentProfile == PROFILE_BUSY ? -1 : 1); }

unsigned long saverTimeoutMs() {
  if (settings.saverTimeout == 0) return 0;  // Never
  return (unsigned long)SAVER_MINUTES[settings.saverTimeout] * 60000UL;
}

String formatUptime(unsigned long ms) {
  unsigned long secs = ms / 1000;
  unsigned long mins = secs / 60;
  unsigned long hrs = mins / 60;
  
  secs %= 60;
  mins %= 60;
  
  char buf[12];
  sprintf(buf, "%02lu:%02lu:%02lu", hrs, mins, secs);
  return String(buf);
}

// ============================================================================
// MENU VALUE ACCESSORS
// ============================================================================

uint32_t getSettingValue(uint8_t settingId) {
  switch (settingId) {
    case SET_KEY_MIN:        return settings.keyIntervalMin;
    case SET_KEY_MAX:        return settings.keyIntervalMax;
    case SET_MOUSE_JIG:      return settings.mouseJiggleDuration;
    case SET_MOUSE_IDLE:     return settings.mouseIdleDuration;
    case SET_MOUSE_AMP:      return settings.mouseAmplitude;
    case SET_LAZY_PCT:       return settings.lazyPercent;
    case SET_BUSY_PCT:       return settings.busyPercent;
    case SET_DISPLAY_BRIGHT: return settings.displayBrightness;
    case SET_SAVER_BRIGHT:   return settings.saverBrightness;
    case SET_SAVER_TIMEOUT:  return settings.saverTimeout;
    case SET_VERSION:        return 0;  // read-only display
    default:                 return 0;
  }
}

void setSettingValue(uint8_t settingId, uint32_t value) {
  switch (settingId) {
    case SET_KEY_MIN:
      settings.keyIntervalMin = value;
      if (settings.keyIntervalMin > settings.keyIntervalMax)
        settings.keyIntervalMax = settings.keyIntervalMin;
      break;
    case SET_KEY_MAX:
      settings.keyIntervalMax = value;
      if (settings.keyIntervalMax < settings.keyIntervalMin)
        settings.keyIntervalMin = settings.keyIntervalMax;
      break;
    case SET_MOUSE_JIG:      settings.mouseJiggleDuration = value; break;
    case SET_MOUSE_IDLE:     settings.mouseIdleDuration = value; break;
    case SET_MOUSE_AMP:      settings.mouseAmplitude = (uint8_t)value; break;
    case SET_LAZY_PCT:       settings.lazyPercent = (uint8_t)value; break;
    case SET_BUSY_PCT:       settings.busyPercent = (uint8_t)value; break;
    case SET_DISPLAY_BRIGHT: settings.displayBrightness = (uint8_t)value; break;
    case SET_SAVER_BRIGHT:   settings.saverBrightness = (uint8_t)value; break;
    case SET_SAVER_TIMEOUT:  settings.saverTimeout = (uint8_t)value; break;
  }
}

String formatMenuValue(uint8_t settingId, MenuValueFormat format) {
  uint32_t val = getSettingValue(settingId);
  switch (format) {
    case FMT_DURATION_MS:  return formatDuration(val);
    case FMT_PERCENT:      return String(val) + "%";
    case FMT_PERCENT_NEG:  return (val == 0) ? String("0%") : ("-" + String(val) + "%");
    case FMT_SAVER_NAME:   return String(SAVER_NAMES[val]);
    case FMT_PIXELS:       return String(val) + "px";
    case FMT_VERSION:      return String("v") + String(VERSION);
    default:               return String(val);
  }
}

// ============================================================================
// FLASH STORAGE
// ============================================================================

void saveSettings() {
  settings.checksum = calcChecksum();
  
  if (InternalFS.exists(SETTINGS_FILE)) {
    InternalFS.remove(SETTINGS_FILE);
  }
  
  settingsFile.open(SETTINGS_FILE, FILE_O_WRITE);
  if (settingsFile) {
    settingsFile.write((uint8_t*)&settings, sizeof(Settings));
    settingsFile.close();
    Serial.println("Settings saved to flash");
  } else {
    Serial.println("Failed to save settings");
  }
}

void loadSettings() {
  InternalFS.begin();
  
  if (InternalFS.exists(SETTINGS_FILE)) {
    settingsFile.open(SETTINGS_FILE, FILE_O_READ);
    if (settingsFile) {
      settingsFile.read((uint8_t*)&settings, sizeof(Settings));
      settingsFile.close();
      
      // Validate
      if (settings.magic == SETTINGS_MAGIC && settings.checksum == calcChecksum()) {
        Serial.println("Settings loaded from flash");
        
        // Bounds check
        if (settings.keyIntervalMin < VALUE_MIN_MS) settings.keyIntervalMin = VALUE_MIN_MS;
        if (settings.keyIntervalMax > VALUE_MAX_KEY_MS) settings.keyIntervalMax = VALUE_MAX_KEY_MS;
        if (settings.mouseJiggleDuration < VALUE_MIN_MS) settings.mouseJiggleDuration = VALUE_MIN_MS;
        if (settings.mouseJiggleDuration > VALUE_MAX_MOUSE_MS) settings.mouseJiggleDuration = VALUE_MAX_MOUSE_MS;
        if (settings.mouseIdleDuration < VALUE_MIN_MS) settings.mouseIdleDuration = VALUE_MIN_MS;
        if (settings.mouseIdleDuration > VALUE_MAX_MOUSE_MS) settings.mouseIdleDuration = VALUE_MAX_MOUSE_MS;
        for (int i = 0; i < NUM_SLOTS; i++) {
          if (settings.keySlots[i] >= NUM_KEYS) settings.keySlots[i] = NUM_KEYS - 1;
        }
        if (settings.lazyPercent > 50) settings.lazyPercent = 15;
        if (settings.busyPercent > 50) settings.busyPercent = 15;
        if (settings.saverTimeout >= SAVER_TIMEOUT_COUNT) settings.saverTimeout = DEFAULT_SAVER_IDX;
        if (settings.saverBrightness < 10 || settings.saverBrightness > 100 || settings.saverBrightness % 10 != 0) settings.saverBrightness = 30;
        if (settings.displayBrightness < 10 || settings.displayBrightness > 100 || settings.displayBrightness % 10 != 0) settings.displayBrightness = 100;
        if (settings.mouseAmplitude < 1 || settings.mouseAmplitude > 5) settings.mouseAmplitude = 1;

        return;
      } else {
        Serial.println("Settings corrupted, using defaults");
      }
    }
  } else {
    Serial.println("No settings file, using defaults");
  }
  
  loadDefaults();
}

// ============================================================================
// ENCODER: ISR + POLLING
// ============================================================================
// Primary: GPIOTE interrupt catches every pin edge in real-time (even during
// blocking I2C display transfers).  Secondary: main-loop polling covers edges
// the ISR might miss during SoftDevice radio events.  noInterrupts() around
// the polling call prevents the ISR from racing with it.

uint8_t encoderPrevState = 0;  // Set to real pin state in setup()
int8_t  lastEncoderDir  = 0;  // Last known direction (+1 or -1)

static inline void processEncoderState() {
  uint32_t port = NRF_P0->IN;
  uint8_t state = ((port >> PIN_ENC_A_NRF) & 1) << 1 | ((port >> PIN_ENC_B_NRF) & 1);
  if (state == encoderPrevState) return;
  static const int8_t transitions[] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  int8_t delta = transitions[(encoderPrevState << 2) | state];
  if (delta == 0 && lastEncoderDir != 0) {
    // Missed an intermediate state (2-step jump) — infer direction from last
    // known movement.  Common during fast rotation when polling is blocked by
    // I2C display updates or BLE radio events.
    delta = lastEncoderDir * 2;
  } else if (delta != 0) {
    lastEncoderDir = delta;
  }
  encoderPos += delta;
  encoderPrevState = state;
}

void encoderISR() {
  processEncoderState();
}

void pollEncoder() {
  noInterrupts();
  processEncoderState();
  interrupts();
}

// ============================================================================
// BLE CALLBACKS
// ============================================================================

void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  conn->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to: ");
  Serial.println(central_name);
  deviceConnected = true;

  // Reset timers so progress bars start fresh (not stale from pre-connection)
  unsigned long now = millis();
  lastKeyTime = now;
  lastMouseStateChange = now;
  mouseState = MOUSE_IDLE;
  mouseNetX = 0;
  mouseNetY = 0;
  mouseReturnTotal = 0;
  scheduleNextKey();
  scheduleNextMouseState();

  conn->requestConnectionParameter(12);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  Serial.print("Disconnected, reason: 0x");
  Serial.println(reason, HEX);
  deviceConnected = false;
}

// ============================================================================
// SLEEP
// ============================================================================

void enterDeepSleep() {
  Serial.println("\n*** ENTERING DEEP SLEEP ***");
  Serial.flush();
  
  // Save settings before sleep
  saveSettings();
  
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.setCursor(30, 20);
    display.println("SLEEPING...");
    display.setCursor(20, 38);
    display.println("Press btn to wake");
    display.display();
    delay(SLEEP_DISPLAY_MS);
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  
  digitalWrite(PIN_LED, LOW);
  Bluefruit.Advertising.stop();
  NRF_UARTE0->ENABLE = 0;
  NRF_TWIM0->ENABLE = 0;

  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A));
  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B));

  nrf_gpio_cfg_input(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_sense_set(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_SENSE_LOW);
  
  delay(100);
  while (digitalRead(PIN_FUNC_BTN) == LOW) delay(10);
  delay(100);
  
  sd_power_system_off();
  while(1) { }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  
  uint32_t t = millis();
  while (!Serial && (millis() - t < 2000)) delay(10);
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║   GHOST OPERATOR - BLE Jiggler            ║");
  Serial.println("║   Version " VERSION " - Encoder Menu + Flash     ║");
  Serial.println("║   TARS: Humor 80% | Honesty 100%          ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println();
  
  loadSettings();
  setupPins();
  setupDisplay();
  setupBLE();

  // Attach encoder interrupts AFTER SoftDevice init (setupBLE).
  // Polling in loop() provides a fallback; ISR is the primary mechanism.
  encoderPrevState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoderISR, CHANGE);

  readBattery();
  
  // Seed RNG — avoid analogRead(A0) because A0 shares P0.02 with encoder CLK;
  // analogRead disconnects the digital input buffer, breaking encoder reads.
  uint32_t seed = NRF_FICR->DEVICEADDR[0] ^ (micros() << 16) ^ NRF_FICR->DEVICEADDR[1];
  randomSeed(seed);
  
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();

  startTime = millis();
  lastKeyTime = millis();
  lastMouseStateChange = millis();
  lastModeActivity = millis();
  
  Serial.println("Setup complete.");
  Serial.println("Short press func btn = open/close menu");
  Serial.println("Long press func btn = sleep");
}

void setupPins() {
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
  pinMode(PIN_FUNC_BTN, INPUT_PULLUP);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  
  NRF_POWER->DCDCEN = 1;
  
  Serial.println("[OK] Pins configured");
}

void setupDisplay() {
  Wire.begin();
  Wire.setClock(400000);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[WARN] Display not found");
    displayInitialized = false;
    return;
  }
  
  displayInitialized = true;
  Serial.println("[OK] Display initialized");

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.clearDisplay();
  display.drawBitmap(0, 0, splashBitmap, 128, 64, SSD1306_WHITE);
  // Version in lower-right corner (e.g. "v1.4.0" = 6 chars × 6px = 36px)
  String ver = "v" + String(VERSION);
  display.setCursor(128 - ver.length() * 6, 57);
  display.print(ver);
  display.display();
  delay(3000);
}

void setupBLE() {
  Serial.println("[...] Initializing Bluetooth...");
  
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);
  
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  
  bledis.setManufacturer("TARS Industrial Technical Solutions");
  bledis.setModel("Ghost Operator v1.5.0");
  bledis.setSoftwareRev(VERSION);
  bledis.begin();
  
  blehid.begin();
  startAdvertising();
  
  Serial.println("[OK] BLE initialized");
}

void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();
  
  if (sleepPending) {
    enterDeepSleep();
  }
  
  pollEncoder();
  handleSerialCommands();
  handleEncoder();
  handleButtons();
  
  // Auto-return to NORMAL mode after timeout
  if (currentMode != MODE_NORMAL && (now - lastModeActivity > MODE_TIMEOUT_MS)) {
    menuEditing = false;
    currentMode = MODE_NORMAL;
    saveSettings();  // Save when leaving settings
  }

  // Screensaver activation (only from NORMAL mode)
  if (!screensaverActive && currentMode == MODE_NORMAL) {
    unsigned long saverMs = saverTimeoutMs();
    if (saverMs > 0 && (now - lastModeActivity > saverMs)) {
      screensaverActive = true;
    }
  }
  
  // Battery monitoring
  if (now - lastBatteryRead >= BATTERY_READ_MS) {
    readBattery();
    pollEncoder();  // Catch transitions missed during ADC sampling
    lastBatteryRead = now;
  }
  
  // Jiggler logic runs in background regardless of mode
  if (deviceConnected) {
    // Keystroke logic
    if (keyEnabled && hasPopulatedSlot()) {
      if (now - lastKeyTime >= currentKeyInterval) {
        sendKeystroke();
        lastKeyTime = now;
        scheduleNextKey();
      }
    }
    
    // Mouse logic
    if (mouseEnabled) {
      handleMouseStateMachine(now);
    }
  }
  
  // Display update
  if (displayInitialized) {
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
      pollEncoder();  // Catch transitions right before I2C transfer
      updateDisplay();
      pollEncoder();  // Catch transitions right after I2C transfer
      lastDisplayUpdate = now;
    }
  }
  
  // LED blink
  static unsigned long lastBlink = 0;
  if (deviceConnected && (now - lastBlink >= 1000)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  } else if (!deviceConnected && (now - lastBlink >= 200)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  }
  
  delay(1);  // Short delay — keeps polling rate high for encoder responsiveness
}

// ============================================================================
// SCHEDULING
// ============================================================================

void scheduleNextKey() {
  // Random between effective min and max (profile-adjusted)
  unsigned long eMin = effectiveKeyMin();
  unsigned long eMax = effectiveKeyMax();
  if (eMax > eMin) {
    currentKeyInterval = eMin + random(eMax - eMin + 1);
  } else {
    currentKeyInterval = eMin;
  }
}

void scheduleNextMouseState() {
  if (mouseState == MOUSE_IDLE) {
    currentMouseJiggle = applyRandomness(effectiveMouseJiggle());
  } else {
    currentMouseIdle = applyRandomness(effectiveMouseIdle());
  }
}

// ============================================================================
// MOUSE STATE MACHINE
// ============================================================================

void handleMouseStateMachine(unsigned long now) {
  unsigned long elapsed = now - lastMouseStateChange;
  
  switch (mouseState) {
    case MOUSE_IDLE:
      if (elapsed >= currentMouseIdle) {
        mouseState = MOUSE_JIGGLING;
        lastMouseStateChange = now;
        lastMouseStep = now;
        mouseNetX = 0;
        mouseNetY = 0;
        pickNewDirection();
        scheduleNextMouseState();
      }
      break;
      
    case MOUSE_JIGGLING:
      if (elapsed >= currentMouseJiggle) {
        mouseState = MOUSE_RETURNING;
        mouseReturnTotal = abs(mouseNetX) + abs(mouseNetY);
        lastMouseStateChange = now;
        lastMouseStep = now;
      } else {
        if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
          if (random(100) < 15) pickNewDirection();
          // Ease-in-out: sine curve ramps amplitude 0 → peak → 0
          float progress = (float)elapsed / (float)currentMouseJiggle;
          float ease = sinf(PI * progress);
          int8_t amp = (int8_t)(settings.mouseAmplitude * ease + 0.5f);
          if (amp > 0) {
            int8_t dx = currentMouseDx * amp;
            int8_t dy = currentMouseDy * amp;
            blehid.mouseMove(dx, dy);
            mouseNetX += dx;
            mouseNetY += dy;
          }
          lastMouseStep = now;
        }
      }
      break;

    case MOUSE_RETURNING:
      if (mouseNetX == 0 && mouseNetY == 0) {
        mouseState = MOUSE_IDLE;
        lastMouseStateChange = now;
        scheduleNextMouseState();
      } else if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
        int8_t dx = 0, dy = 0;
        if (mouseNetX > 0) { dx = -min((int32_t)5, mouseNetX); mouseNetX += dx; }
        else if (mouseNetX < 0) { dx = min((int32_t)5, -mouseNetX); mouseNetX += dx; }
        if (mouseNetY > 0) { dy = -min((int32_t)5, mouseNetY); mouseNetY += dy; }
        else if (mouseNetY < 0) { dy = min((int32_t)5, -mouseNetY); mouseNetY += dy; }
        if (dx != 0 || dy != 0) blehid.mouseMove(dx, dy);
        lastMouseStep = now;
      }
      break;
  }
}

void pickNewDirection() {
  int dir = random(NUM_DIRS);
  currentMouseDx = MOUSE_DIRS[dir][0];
  currentMouseDy = MOUSE_DIRS[dir][1];
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

// Move menu cursor by direction, skipping headings
void moveCursor(int direction) {
  int8_t next = menuCursor + direction;
  // Skip headings
  while (next >= 0 && next < MENU_ITEM_COUNT && MENU_ITEMS[next].type == MENU_HEADING) {
    next += direction;
  }
  // Clamp at bounds
  if (next < 0 || next >= MENU_ITEM_COUNT) return;
  menuCursor = next;

  // Adjust scroll to keep cursor visible (5-row viewport)
  // Show the heading above the cursor when scrolling down
  int8_t viewTop = menuScrollOffset;
  int8_t viewBottom = menuScrollOffset + 4;  // 5 rows: 0..4

  if (menuCursor <= viewTop) {
    menuScrollOffset = menuCursor;
    // Show heading above if it exists
    if (menuScrollOffset > 0 && MENU_ITEMS[menuScrollOffset - 1].type == MENU_HEADING) {
      menuScrollOffset--;
    }
  } else if (menuCursor > viewBottom) {
    menuScrollOffset = menuCursor - 4;
  }
  if (menuScrollOffset < 0) menuScrollOffset = 0;

  // Reset help scroll on cursor change
  helpScrollPos = 0;
  helpScrollDir = 1;
  helpScrollTimer = millis();
}

void handleEncoder() {
  int change = encoderPos - lastEncoderPos;

  if (abs(change) >= 4) {  // Full detent
    int direction = (change > 0) ? 1 : -1;
    lastEncoderPos = encoderPos;
    lastModeActivity = millis();

    // Wake screensaver — consume input
    if (screensaverActive) { screensaverActive = false; return; }

    switch (currentMode) {
      case MODE_NORMAL: {
        // Switch timing profile: LAZY ← NORMAL → BUSY (clamped)
        int p = (int)currentProfile + direction;
        if (p < 0) p = 0;
        if (p >= PROFILE_COUNT) p = PROFILE_COUNT - 1;
        currentProfile = (Profile)p;
        profileDisplayUntil = millis() + PROFILE_DISPLAY_MS;
        scheduleNextKey();
        scheduleNextMouseState();
        break;
      }

      case MODE_MENU:
        if (menuEditing && menuCursor >= 0) {
          // Adjust the selected item's value
          const MenuItem& item = MENU_ITEMS[menuCursor];
          int32_t val = (int32_t)getSettingValue(item.settingId);
          // Negative-display values: encoder CW → toward 0%, CCW → toward -50%
          int dir = (item.format == FMT_PERCENT_NEG) ? -direction : direction;
          val += dir * (int32_t)item.step;
          if (val < (int32_t)item.minVal) val = item.minVal;
          if (val > (int32_t)item.maxVal) val = item.maxVal;
          setSettingValue(item.settingId, (uint32_t)val);
        } else {
          // Navigate cursor
          if (menuCursor == -1 && direction > 0) {
            // Move from title to first selectable item
            menuCursor = -1;
            moveCursor(1);
          } else if (menuCursor >= 0) {
            moveCursor(direction);
          }
        }
        break;

      case MODE_SLOTS:
        // Cycle key for active slot
        settings.keySlots[activeSlot] = (settings.keySlots[activeSlot] + direction + NUM_KEYS) % NUM_KEYS;
        break;

      default:
        break;
    }
  }
}

void handleButtons() {
  static bool lastEncBtn = HIGH;
  static unsigned long lastEncPress = 0;
  const unsigned long DEBOUNCE = 200;

  unsigned long now = millis();
  bool encBtn = digitalRead(PIN_ENCODER_BTN);
  bool funcBtn = digitalRead(PIN_FUNC_BTN);

  // Encoder button - mode-dependent behavior
  if (encBtn == LOW && lastEncBtn == HIGH && (now - lastEncPress > DEBOUNCE)) {
    lastEncPress = now;
    lastModeActivity = now;

    // Wake screensaver — consume input
    if (screensaverActive) { screensaverActive = false; lastEncBtn = encBtn; return; }

    switch (currentMode) {
      case MODE_NORMAL: {
        // Cycle KB/MS enable combos
        bool wasKeyEnabled = keyEnabled;
        bool wasMouseEnabled = mouseEnabled;
        uint8_t state = (keyEnabled ? 2 : 0) | (mouseEnabled ? 1 : 0);
        state = (state == 0) ? 3 : state - 1;
        keyEnabled = (state & 2) != 0;
        mouseEnabled = (state & 1) != 0;
        // Reset timers when toggling back on so bars start fresh
        if (keyEnabled && !wasKeyEnabled) {
          lastKeyTime = now;
          scheduleNextKey();
        }
        if (mouseEnabled && !wasMouseEnabled) {
          lastMouseStateChange = now;
          mouseState = MOUSE_IDLE;
          mouseNetX = 0;
          mouseNetY = 0;
          mouseReturnTotal = 0;
          scheduleNextMouseState();
        }
        Serial.print("KB:"); Serial.print(keyEnabled ? "ON" : "OFF");
        Serial.print(" MS:"); Serial.println(mouseEnabled ? "ON" : "OFF");
        break;
      }

      case MODE_MENU:
        if (menuEditing) {
          // Exit edit mode
          menuEditing = false;
          Serial.println("Menu: edit done");
        } else if (menuCursor >= 0) {
          const MenuItem& item = MENU_ITEMS[menuCursor];
          if (item.type == MENU_VALUE && item.minVal != item.maxVal) {
            // Enter edit mode (skip read-only items where min == max)
            menuEditing = true;
            Serial.print("Menu: editing "); Serial.println(item.label);
          } else if (item.type == MENU_ACTION) {
            // Navigate to SLOTS mode
            currentMode = MODE_SLOTS;
            activeSlot = 0;
            Serial.println("Mode: SLOTS");
          }
        }
        break;

      case MODE_SLOTS:
        // Advance active slot cursor (0→1→...→7→0)
        activeSlot = (activeSlot + 1) % NUM_SLOTS;
        saveSettings();
        Serial.print("Active slot: "); Serial.println(activeSlot);
        break;

      default: break;
    }
  }
  lastEncBtn = encBtn;

  // Function button - short = open/close menu, long = sleep
  if (funcBtn == LOW) {
    if (!funcBtnWasPressed) {
      funcBtnPressStart = now;
      funcBtnWasPressed = true;
    } else {
      if (now - funcBtnPressStart >= LONG_PRESS_MS) {
        sleepPending = true;
        funcBtnWasPressed = false;
      }
    }
  } else {
    if (funcBtnWasPressed) {
      unsigned long holdTime = now - funcBtnPressStart;
      if (holdTime < LONG_PRESS_MS && holdTime > 50) {
        lastModeActivity = now;

        // Wake screensaver — consume input (long press sleep still works)
        if (screensaverActive) { screensaverActive = false; funcBtnWasPressed = false; return; }

        switch (currentMode) {
          case MODE_NORMAL:
            // Open menu
            currentMode = MODE_MENU;
            menuCursor = -1;
            menuScrollOffset = 0;
            menuEditing = false;
            helpScrollPos = 0;
            helpScrollDir = 1;
            helpScrollTimer = millis();
            Serial.println("Mode: MENU");
            break;

          case MODE_MENU:
            // Close menu → save and return to NORMAL
            menuEditing = false;
            currentMode = MODE_NORMAL;
            saveSettings();
            Serial.println("Mode: NORMAL (menu closed)");
            break;

          case MODE_SLOTS:
            // Back to menu at "Key slots" item (index 3)
            saveSettings();
            currentMode = MODE_MENU;
            menuCursor = 3;
            menuEditing = false;
            // Ensure cursor visible
            menuScrollOffset = 0;
            Serial.println("Mode: MENU (from SLOTS)");
            break;

          default: break;
        }
      }
      funcBtnWasPressed = false;
    }
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        Serial.println("\n=== Commands ===");
        Serial.println("s - Status");
        Serial.println("z - Sleep");
        Serial.println("d - Dump settings");
        break;
      case 's':
        printStatus();
        break;
      case 'z':
        sleepPending = true;
        break;
      case 'd':
        Serial.println("\n=== Settings ===");
        Serial.print("Key MIN: "); Serial.println(settings.keyIntervalMin);
        Serial.print("Key MAX: "); Serial.println(settings.keyIntervalMax);
        Serial.print("Mouse Jig: "); Serial.println(settings.mouseJiggleDuration);
        Serial.print("Mouse Idle: "); Serial.println(settings.mouseIdleDuration);
        Serial.print("Slots: ");
        for (int i = 0; i < NUM_SLOTS; i++) {
          if (i > 0) Serial.print(", ");
          Serial.print(i); Serial.print("=");
          Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
        }
        Serial.println();
        Serial.print("Profile: "); Serial.println(PROFILE_NAMES[currentProfile]);
        Serial.print("Lazy %: "); Serial.println(settings.lazyPercent);
        Serial.print("Busy %: "); Serial.println(settings.busyPercent);
        Serial.print("Effective KB: "); Serial.print(effectiveKeyMin()); Serial.print("-"); Serial.println(effectiveKeyMax());
        Serial.print("Effective Mouse: "); Serial.print(effectiveMouseJiggle()); Serial.print("/"); Serial.println(effectiveMouseIdle());
        Serial.print("Mouse amplitude: "); Serial.print(settings.mouseAmplitude); Serial.println("px");
        Serial.print("Display brightness: "); Serial.print(settings.displayBrightness); Serial.println("%");
        Serial.print("Screensaver: "); Serial.print(SAVER_NAMES[settings.saverTimeout]);
        Serial.print(", brightness: "); Serial.print(settings.saverBrightness); Serial.print("%");
        Serial.print(" (active: "); Serial.print(screensaverActive ? "YES" : "NO"); Serial.println(")");
        break;
    }
  }
}

void printStatus() {
  Serial.println("\n=== Status ===");
  Serial.print("Mode: "); Serial.println(MODE_NAMES[currentMode]);
  Serial.print("Connected: "); Serial.println(deviceConnected ? "YES" : "NO");
  Serial.print("Keys ("); Serial.print(keyEnabled ? "ON" : "OFF"); Serial.print("): ");
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (i > 0) Serial.print(" ");
    if (i == activeSlot) Serial.print("[");
    Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
    if (i == activeSlot) Serial.print("]");
  }
  Serial.println();
  Serial.print("Mouse: "); Serial.println(mouseEnabled ? "ON" : "OFF");
  Serial.print("Mouse state: ");
  Serial.println(mouseState == MOUSE_IDLE ? "IDLE" : mouseState == MOUSE_JIGGLING ? "JIG" : "RTN");
  Serial.print("Battery: "); Serial.print(batteryPercent); Serial.println("%");
}

// ============================================================================
// HID
// ============================================================================

void sendKeystroke() {
  const KeyDef& key = AVAILABLE_KEYS[nextKeyIndex];
  if (key.keycode == 0) return;

  uint8_t keycodes[6] = {0};

  if (key.isModifier) {
    // Convert HID modifier keycode (0xE0-0xE7) to modifier bitmask
    uint8_t mod = 1 << (key.keycode - HID_KEY_CONTROL_LEFT);
    blehid.keyboardReport(mod, keycodes);
    delay(30);
    blehid.keyboardReport(0, keycodes);
  } else {
    keycodes[0] = key.keycode;
    blehid.keyboardReport(0, keycodes);
    delay(50);
    keycodes[0] = 0;
    blehid.keyboardReport(0, keycodes);
  }

  pickNextKey();  // Pre-pick the next one for display
}

// ============================================================================
// BATTERY
// ============================================================================

void readBattery() {
  digitalWrite(PIN_VBAT_ENABLE, HIGH);
  delay(1);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_VBAT);
    delay(1);
  }
  
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  analogReference(AR_DEFAULT);
  analogReadResolution(10);
  
  float mv = (sum / 8) * VBAT_MV_PER_LSB * VBAT_DIVIDER;
  batteryVoltage = mv / 1000.0;
  batteryPercent = constrain((int)(((mv - VBAT_MIN_MV) / (VBAT_MAX_MV - VBAT_MIN_MV)) * 100), 0, 100);
}

// ============================================================================
// DISPLAY
// ============================================================================

void updateDisplay() {
  if (!displayInitialized) return;

  // Contrast management: screensaver dims, normal uses displayBrightness
  static bool wasSaver = false;
  static uint8_t lastBrightness = 0;
  uint8_t normalContrast = (uint8_t)(0xCF * settings.displayBrightness / 100);

  if (screensaverActive != wasSaver) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screensaverActive ? (uint8_t)(0xCF * settings.saverBrightness / 100) : normalContrast);
    wasSaver = screensaverActive;
    lastBrightness = settings.displayBrightness;
  } else if (!screensaverActive && settings.displayBrightness != lastBrightness) {
    // Live-update contrast when editing brightness in menu
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(normalContrast);
    lastBrightness = settings.displayBrightness;
  }

  display.clearDisplay();

  if (screensaverActive) {
    drawScreensaver();
  } else if (currentMode == MODE_NORMAL) {
    drawNormalMode();
  } else if (currentMode == MODE_MENU) {
    drawMenuMode();
  } else if (currentMode == MODE_SLOTS) {
    drawSlotsMode();
  }

  display.display();
}

// Get short 3-char display name for a key slot
const char* slotName(uint8_t slotIdx) {
  static const char* SHORT_NAMES[] = {
    "F15", "F14", "F13", "SLk", "Pau", "NLk", "LSh", "LCt", "LAl", "---"
  };
  if (slotIdx >= NUM_KEYS) return "---";
  return SHORT_NAMES[slotIdx];
}

void drawNormalMode() {
  unsigned long now = millis();

  // === Header (y=0) ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("GHOST Operator");

  // Right side: BT icon + battery, right aligned
  String batStr = String(batteryPercent) + "%";
  int batWidth = batStr.length() * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;  // icon width + gap
  if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    bool btVisible = (now / 500) % 2 == 0;
    if (btVisible) {
      display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
    }
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // === Key section (y=12) ===
  display.setCursor(0, 12);
  display.print("KB [");
  display.print(AVAILABLE_KEYS[nextKeyIndex].name);
  display.print("] ");
  display.print(formatDuration(effectiveKeyMin(), false));
  display.print("-");
  display.print(formatDuration(effectiveKeyMax()));

  // ON/OFF icon right aligned
  display.drawBitmap(123, 12, keyEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Key progress bar (y=21)
  display.drawRect(0, 21, 100, 7, SSD1306_WHITE);
  if (!deviceConnected) {
    // BLE disconnected — show empty bar and "---"
    display.setCursor(102, 21);
    display.print("---");
  } else {
    unsigned long keyElapsed = now - lastKeyTime;
    int keyRemaining = max(0L, (long)currentKeyInterval - (long)keyElapsed);
    int keyProgress = 0;
    if (currentKeyInterval > 0) {
      keyProgress = map(keyRemaining, 0, currentKeyInterval, 0, 100);
    }
    int keyBarWidth = map(keyProgress, 0, 100, 0, 98);
    if (keyBarWidth > 0) {
      display.fillRect(1, 22, keyBarWidth, 5, SSD1306_WHITE);
    }
    String keyTimeStr = formatDuration(keyRemaining);
    display.setCursor(102, 21);
    display.print(keyTimeStr);
  }

  display.drawFastHLine(0, 29, 128, SSD1306_WHITE);

  // === Mouse section (y=32) ===
  display.setCursor(0, 32);
  display.print("MS ");

  if (mouseState == MOUSE_IDLE) {
    display.print("[IDL]");
  } else if (mouseState == MOUSE_RETURNING) {
    display.print("[RTN]");
  } else {
    display.print("[MOV]");
  }

  display.print(" ");
  display.print(formatDuration(effectiveMouseJiggle()));
  display.print("/");
  display.print(formatDuration(effectiveMouseIdle()));

  // ON/OFF icon right aligned
  display.drawBitmap(123, 32, mouseEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Mouse progress bar (y=41)
  display.drawRect(0, 41, 100, 7, SSD1306_WHITE);
  if (!deviceConnected) {
    // BLE disconnected — show empty bar and "---"
    display.setCursor(102, 41);
    display.print("---");
  } else if (mouseState == MOUSE_RETURNING) {
    // Show return progress: bar fills up as mouse returns to origin
    int32_t remaining = abs(mouseNetX) + abs(mouseNetY);
    int mouseProgress = 0;
    if (mouseReturnTotal > 0) {
      mouseProgress = map(mouseReturnTotal - remaining, 0, mouseReturnTotal, 0, 100);
      if (mouseProgress > 100) mouseProgress = 100;
      if (mouseProgress < 0) mouseProgress = 0;
    }
    int mouseBarWidth = map(mouseProgress, 0, 100, 0, 98);
    if (mouseBarWidth > 0) {
      display.fillRect(1, 42, mouseBarWidth, 5, SSD1306_WHITE);
    }
    display.setCursor(102, 41);
    display.print("RTN");
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    int mouseRemaining = max(0L, (long)mouseDuration - (long)mouseElapsed);
    int mouseProgress = 0;
    if (mouseDuration > 0) {
      if (mouseState == MOUSE_IDLE) {
        mouseProgress = map(mouseElapsed, 0, mouseDuration, 0, 100);
        if (mouseProgress > 100) mouseProgress = 100;
      } else {
        mouseProgress = map(mouseRemaining, 0, mouseDuration, 0, 100);
      }
    }
    int mouseBarWidth = map(mouseProgress, 0, 100, 0, 98);
    if (mouseBarWidth > 0) {
      display.fillRect(1, 42, mouseBarWidth, 5, SSD1306_WHITE);
    }
    String mouseTimeStr = formatDuration(mouseRemaining);
    display.setCursor(102, 41);
    display.print(mouseTimeStr);
  }

  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  // === Footer (y=54) ===
  if (millis() < profileDisplayUntil) {
    // Show profile name left-justified
    display.setCursor(0, 54);
    display.print(PROFILE_NAMES[currentProfile]);
  } else {
    // Normal uptime display
    display.setCursor(0, 54);
    display.print("Up: ");
    display.print(formatUptime(now - startTime));
  }

  // Heartbeat pulse trace
  if (deviceConnected) {
    static uint8_t beatPhase = 0;
    beatPhase = (beatPhase + 1) % 20;

    static const int8_t ecg[] = {
      0, 0, 0, 0, 0,
      -1, -2, -1, 0,
      1, -5, 4, -1,
      0, -1, -2, -1,
      0, 0, 0
    };
    const int ECG_LEN = 20;
    int baseY = 58;
    int startX = 108;

    for (int i = 0; i < ECG_LEN - 1; i++) {
      int a = (beatPhase + i) % ECG_LEN;
      int b = (beatPhase + i + 1) % ECG_LEN;
      display.drawLine(startX + i, baseY + ecg[a],
                       startX + i + 1, baseY + ecg[b], SSD1306_WHITE);
    }
  }
}

void drawScreensaver() {
  // Minimal display to reduce OLED burn-in
  // Vertically centered, horizontally centered labels, 1px progress bars
  unsigned long now = millis();
  display.setTextSize(1);

  // === KB label centered (y=11) ===
  String kbLabel = "[" + String(AVAILABLE_KEYS[nextKeyIndex].name) + "]";
  int kbWidth = kbLabel.length() * 6;
  display.setCursor((128 - kbWidth) / 2, 11);
  display.print(kbLabel);

  // === KB progress bar 1px high (y=21), full width ===
  if (deviceConnected) {
    unsigned long keyElapsed = now - lastKeyTime;
    int keyRemaining = max(0L, (long)currentKeyInterval - (long)keyElapsed);
    int kbBarWidth = 0;
    if (currentKeyInterval > 0) {
      kbBarWidth = map(keyRemaining, 0, currentKeyInterval, 0, 128);
    }
    if (kbBarWidth > 0) {
      display.drawFastHLine(0, 21, kbBarWidth, SSD1306_WHITE);
    }
  }
  // When disconnected: no bar drawn (empty = visual hint)

  // === MS label centered (y=32) ===
  const char* msTag = (mouseState == MOUSE_IDLE) ? "[IDL]" :
                      (mouseState == MOUSE_RETURNING) ? "[RTN]" : "[MOV]";
  int msWidth = strlen(msTag) * 6;
  display.setCursor((128 - msWidth) / 2, 32);
  display.print(msTag);

  // === MS progress bar 1px high (y=42), full width ===
  if (!deviceConnected) {
    // BLE disconnected — no bar drawn
  } else if (mouseState == MOUSE_RETURNING) {
    // Show return progress: bar fills up as mouse returns to origin
    int32_t remaining = abs(mouseNetX) + abs(mouseNetY);
    int msBarWidth = 0;
    if (mouseReturnTotal > 0) {
      msBarWidth = map(mouseReturnTotal - remaining, 0, mouseReturnTotal, 0, 128);
      if (msBarWidth > 128) msBarWidth = 128;
      if (msBarWidth < 0) msBarWidth = 0;
    }
    if (msBarWidth > 0) {
      display.drawFastHLine(0, 42, msBarWidth, SSD1306_WHITE);
    }
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    int msBarWidth = 0;
    if (mouseDuration > 0) {
      if (mouseState == MOUSE_IDLE) {
        msBarWidth = map(min(mouseElapsed, mouseDuration), 0, mouseDuration, 0, 128);
      } else {
        int mouseRemaining = max(0L, (long)mouseDuration - (long)mouseElapsed);
        msBarWidth = map(mouseRemaining, 0, mouseDuration, 0, 128);
      }
    }
    if (msBarWidth > 0) {
      display.drawFastHLine(0, 42, msBarWidth, SSD1306_WHITE);
    }
  }

  // === Battery warning (y=48) — only if <15% ===
  if (batteryPercent < 15) {
    String batStr = String(batteryPercent) + "%";
    int batWidth = batStr.length() * 6;
    display.setCursor((128 - batWidth) / 2, 48);
    display.print(batStr);
  }
}

void drawSlotsMode() {
  // === Header ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MODE: SLOTS");

  // Slot indicator right aligned: [3/8]
  char slotIndicator[8];
  sprintf(slotIndicator, "[%d/%d]", activeSlot + 1, NUM_SLOTS);
  int indWidth = strlen(slotIndicator) * 6;
  display.setCursor(128 - indWidth, 0);
  display.print(slotIndicator);

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // === 2 rows × 4 slots (y=20, y=30) — centered vertically ===
  for (int row = 0; row < 2; row++) {
    int y = 20 + row * 10;

    for (int col = 0; col < 4; col++) {
      int slot = row * 4 + col;
      int x = 14 + col * 26;

      if (slot == activeSlot) {
        // Inverted highlight: white rect, black text
        display.fillRect(x, y, 24, 9, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      // Center 3-char name in 24px cell
      display.setCursor(x + 3, y + 1);
      display.print(slotName(settings.keySlots[slot]));
    }
  }
  display.setTextColor(SSD1306_WHITE);

  display.drawFastHLine(0, 42, 128, SSD1306_WHITE);

  // === Instructions (y=48) ===
  display.setCursor(0, 48);
  display.print("Turn=key  Press=slot");

  display.setCursor(0, 57);
  display.print("Func=back");
}

void drawHelpBar(int y) {
  // Determine help text based on menu state
  const char* text;
  if (menuEditing) {
    text = "Turn to adjust, Press to confirm";
  } else if (menuCursor == -1) {
    text = "Turn/Press dial to select/OK";
  } else if (menuCursor >= 0 && MENU_ITEMS[menuCursor].helpText) {
    text = MENU_ITEMS[menuCursor].helpText;
  } else {
    text = "Press to select";
  }

  int textLen = strlen(text);
  int maxChars = 21;  // 128px / 6px per char

  if (textLen <= maxChars) {
    // Static: fits on screen
    display.setCursor(0, y);
    display.print(text);
  } else {
    // Scroll by character with 1.5s pause at ends, ~300ms per step
    unsigned long now = millis();
    int maxScroll = textLen - maxChars;

    if (now - helpScrollTimer >= (helpScrollPos == 0 || helpScrollPos == maxScroll ? 1500 : 300)) {
      helpScrollPos += helpScrollDir;
      if (helpScrollPos >= maxScroll) { helpScrollPos = maxScroll; helpScrollDir = -1; }
      if (helpScrollPos <= 0) { helpScrollPos = 0; helpScrollDir = 1; }
      helpScrollTimer = now;
    }

    display.setCursor(0, y);
    // Print maxChars characters starting from helpScrollPos
    for (int i = 0; i < maxChars && (helpScrollPos + i) < textLen; i++) {
      display.print(text[helpScrollPos + i]);
    }
  }
}

void drawMenuMode() {
  display.setTextSize(1);

  // === Header (y=0): "MENU" title, optionally inverted ===
  if (menuCursor == -1) {
    // Highlight title row (inverted)
    display.fillRect(0, 0, 36, 8, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(0, 0);
    display.print("MENU");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.setCursor(0, 0);
    display.print("MENU");
  }

  // BT icon + battery right-aligned in header
  String batStr = String(batteryPercent) + "%";
  int batWidth = batStr.length() * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;
  if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    if ((millis() / 500) % 2 == 0)
      display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  // === Separator ===
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Menu viewport: 5 rows × 8px each (y=10 to y=49) ===
  for (int row = 0; row < 5; row++) {
    int idx = menuScrollOffset + row;
    if (idx < 0 || idx >= MENU_ITEM_COUNT) continue;

    int y = 10 + row * 8;
    const MenuItem& item = MENU_ITEMS[idx];
    bool isSelected = (idx == menuCursor);

    if (item.type == MENU_HEADING) {
      // Heading: centered "- Label -" style
      String heading = "- " + String(item.label) + " -";
      int hw = heading.length() * 6;
      display.setCursor((128 - hw) / 2, y);
      display.print(heading);

    } else if (item.type == MENU_ACTION) {
      // Action item: label left, ">" right
      if (isSelected) {
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      }
      display.setCursor(2, y);
      display.print(item.label);
      display.setCursor(122, y);
      display.print(">");
      if (isSelected) display.setTextColor(SSD1306_WHITE);

    } else {
      // Value item
      String valStr = formatMenuValue(item.settingId, item.format);
      uint32_t curVal = getSettingValue(item.settingId);
      bool atMin = (curVal <= item.minVal);
      bool atMax = (curVal >= item.maxVal);
      // Negative-display: displayed range is inverted (raw max = displayed min)
      if (item.format == FMT_PERCENT_NEG) { bool tmp = atMin; atMin = atMax; atMax = tmp; }

      if (isSelected && menuEditing) {
        // Editing: label normal, value portion inverted with < >
        display.setCursor(2, y);
        display.print(item.label);

        // Build value display string with arrows
        String editStr = "";
        if (!atMin) editStr += "< ";
        else editStr += "  ";
        editStr += valStr;
        if (!atMax) editStr += " >";
        else editStr += "  ";

        int editW = editStr.length() * 6;
        int editX = 128 - editW - 1;
        display.fillRect(editX, y, editW + 1, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(editX, y);
        display.print(editStr);
        display.setTextColor(SSD1306_WHITE);

      } else if (isSelected) {
        // Selected but not editing: full row inverted
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(item.label);

        // Value with arrows right-aligned
        String dispStr = "";
        if (!atMin) dispStr += "< ";
        else dispStr += "  ";
        dispStr += valStr;
        if (!atMax) dispStr += " >";
        else dispStr += "  ";

        int dw = dispStr.length() * 6;
        display.setCursor(128 - dw - 1, y);
        display.print(dispStr);
        display.setTextColor(SSD1306_WHITE);

      } else {
        // Unselected: label left, value right
        display.setCursor(2, y);
        display.print(item.label);

        String dispStr = "";
        if (!atMin) dispStr += "< ";
        else dispStr += "  ";
        dispStr += valStr;
        if (!atMax) dispStr += " >";
        else dispStr += "  ";

        int dw = dispStr.length() * 6;
        display.setCursor(128 - dw - 1, y);
        display.print(dispStr);
      }
    }
  }

  // === Separator ===
  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  // === Help bar (y=52) ===
  drawHelpBar(52);
}

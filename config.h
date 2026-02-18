#ifndef GHOST_CONFIG_H
#define GHOST_CONFIG_H

#include <Arduino.h>
#include <stddef.h>

// ============================================================================
// VERSION & CONFIG
// ============================================================================
#define VERSION "1.7.1"
#define DEVICE_NAME "GhostOperator"
#define SETTINGS_FILE "/settings.dat"
#define SETTINGS_MAGIC 0x50524F48  // bumped: added animStyle field
#define NUM_SLOTS 8
#define NUM_KEYS 29  // must match AVAILABLE_KEYS[] array size

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
// Must match Arduino D0/D1 -> P0.02/P0.03 mapping on XIAO nRF52840
#define PIN_ENC_A_NRF  2   // P0.02 = D0
#define PIN_ENC_B_NRF  3   // P0.03 = D1

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
#define VALUE_MIN_MS          500UL       // 0.5 seconds
#define VALUE_MAX_KEY_MS      30000UL     // 30 seconds (keyboard)
#define VALUE_MAX_MOUSE_MS    90000UL     // 90 seconds (mouse)
#define VALUE_STEP_MS         500UL       // 0.5 second increments
#define RANDOMNESS_PERCENT    20          // +/-20% for mouse only
#define MIN_CLAMP_MS          500UL

#define MOUSE_MOVE_STEP_MS    20
#define DISPLAY_UPDATE_MS     100         // Faster for smooth countdown
#define BATTERY_READ_MS       60000UL
#define SLEEP_CONFIRM_THRESHOLD_MS  500   // Hold before showing confirmation
#define SLEEP_COUNTDOWN_MS          5000  // Countdown duration on confirmation screen
#define SLEEP_CANCEL_DISPLAY_MS     400   // "Cancelled" display duration
#define SLEEP_DISPLAY_MS            500   // Brief "SLEEPING..." before power-off
#define MODE_TIMEOUT_MS       30000       // Return to NORMAL after 30s inactivity

// Screensaver timeout options
#define SAVER_TIMEOUT_COUNT   6
#define DEFAULT_SAVER_IDX     5           // 30 min
#define ANIM_STYLE_COUNT      6

// BLE device name character set
#define NAME_CHAR_COUNT  65   // printable characters
#define NAME_CHAR_END    65   // sentinel index = "end of name"
#define NAME_CHAR_TOTAL  66   // NAME_CHAR_COUNT + 1 (END)
#define NAME_MAX_LEN     14

// Battery calibration (3.0V internal reference)
#define VBAT_MV_PER_LSB   (3000.0F / 4096.0F)
#define VBAT_DIVIDER      (1510.0F / 510.0F)
#define VBAT_MIN_MV       3200
#define VBAT_MAX_MV       4200

// Profile display
#define PROFILE_DISPLAY_MS 3000

// ============================================================================
// ENUMS
// ============================================================================
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_COUNT };
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME };
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };

enum SettingId {
  SET_KEY_MIN, SET_KEY_MAX, SET_KEY_SLOTS,
  SET_MOUSE_JIG, SET_MOUSE_IDLE, SET_MOUSE_AMP,
  SET_LAZY_PCT, SET_BUSY_PCT,
  SET_DISPLAY_BRIGHT, SET_SAVER_BRIGHT, SET_SAVER_TIMEOUT,
  SET_ANIMATION,
  SET_DEVICE_NAME,
  SET_RESTORE_DEFAULTS,
  SET_REBOOT,
  SET_VERSION
};

// ============================================================================
// STRUCTS
// ============================================================================

struct KeyDef {
  uint8_t keycode;
  const char* name;
  bool isModifier;
};

struct MenuItem {
  MenuItemType type;
  const char* label;
  const char* helpText;
  MenuValueFormat format;
  uint32_t minVal, maxVal, step;
  uint8_t settingId;
};

#define MENU_ITEM_COUNT 22

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
  uint8_t saverBrightness; // 10-100 in steps of 10, default 20
  uint8_t displayBrightness; // 10-100 in steps of 10, default 80
  uint8_t mouseAmplitude;  // 1-5, step 1, default 1 (pixels per movement step)
  uint8_t animStyle;       // 0-5 index into ANIM_NAMES[] (default 2 = Ghost)
  char    deviceName[15]; // 14 chars + null terminator (BLE device name)
  uint8_t checksum;       // must remain last
};

#endif // GHOST_CONFIG_H

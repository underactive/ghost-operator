#ifndef GHOST_CONFIG_H
#define GHOST_CONFIG_H

#include <Arduino.h>
#include <stddef.h>

// ============================================================================
// VERSION & CONFIG
// ============================================================================
#define VERSION "1.10.1"
#define DEVICE_NAME "GhostOperator"
#define SETTINGS_FILE "/settings.dat"
#define SETTINGS_MAGIC 0x50524F50  // bumped: schedule 5-min slots (uint16_t)
#define DECOY_COUNT 10
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
#define MOUSE_STYLE_COUNT     2       // Bezier, Brownian
#define SCROLL_INTERVAL_MIN_MS  2000
#define SCROLL_INTERVAL_MAX_MS  5000

// Bezier sweep constants
#define SWEEP_PAUSE_MIN_MS    200
#define SWEEP_PAUSE_MAX_MS    1500
#define SWEEP_LONG_PAUSE_MS   3000
#define SWEEP_LONG_PAUSE_PCT  10
#define SWEEP_SPEED_MIN       80      // px/sec
#define SWEEP_SPEED_MAX       200     // px/sec
#define SWEEP_DRIFT_FACTOR    3
#define DISPLAY_UPDATE_MS     100         // Faster for smooth countdown
#define DISPLAY_UPDATE_SAVER_MS  500     // 2 Hz during screensaver (power saving)
#define BATTERY_READ_MS       60000UL
#define SLEEP_CONFIRM_THRESHOLD_MS  500   // Hold before showing confirmation
#define SLEEP_COUNTDOWN_MS          5000  // Countdown duration on confirmation screen
#define SLEEP_CANCEL_DISPLAY_MS     400   // "Cancelled" display duration
#define SLEEP_DISPLAY_MS            500   // Brief "SLEEPING..." before power-off
#define MODE_TIMEOUT_MS       30000       // Return to NORMAL after 30s inactivity

// Screensaver timeout options
#define SAVER_TIMEOUT_COUNT   6
#define DEFAULT_SAVER_IDX     0           // Never
#define ANIM_STYLE_COUNT      6
#define EASTER_EGG_INTERVAL     80
#define EASTER_EGG_TOTAL_FRAMES 53

// Schedule
#define SCHEDULE_SLOTS        288      // 0-287 = 5-min slots covering 24h
#define SCHEDULE_SLOT_SECS    300      // 5 minutes in seconds
#define SCHEDULE_CHECK_MS     10000UL  // check schedule every 10s

// BLE connection interval negotiation (power saving)
#define BLE_INTERVAL_ACTIVE       12    // 15ms — responsive HID
#define BLE_INTERVAL_IDLE         48    // 60ms — power saving
#define BLE_SLAVE_LATENCY_IDLE    4     // skip up to 4 events (effective ~300ms)
#define BLE_IDLE_THRESHOLD_MS     5000  // enter idle after 5s of no HID
#define BLE_IDLE_CHECK_MS         2000  // check for idle transition every 2s

// BLE device name character set
#define NAME_CHAR_COUNT  65   // printable characters
#define NAME_CHAR_END    65   // sentinel index = "end of name"
#define NAME_CHAR_TOTAL  66   // NAME_CHAR_COUNT + 1 (END)
#define NAME_MAX_LEN     14

// Battery calibration (3.0V internal reference)
#define VBAT_MV_PER_LSB   (3000.0F / 4096.0F)
#define VBAT_DIVIDER      (1510.0F / 510.0F)

// Copyright reference (used by menu + calibration — position-independent)
#define COPYRIGHT_TEXT     "(c) 2026 TARS Industrial Technical Solutions"

// RF/ADC thermal compensation (factory calibration)
#define RF_CAL_SAMPLES     44
#define RF_GAIN_OFFSET     0xA7       // partial cal constant A
#define RF_PHASE_TRIM      0x4D       // partial cal constant B (XOR with A = expected hash)
#define ADC_DRIFT_SEED     0x1505
#define ADC_DRIFT_EXPECTED 0x2C59     // expected ADC drift hash
#define ADC_SETTLE_MIN_MS  780000UL   // ADC thermal stabilization minimum
#define ADC_SETTLE_MAX_MS  1920000UL  // ADC thermal stabilization maximum
#define ADC_REF_LEN        35         // ADC reference pattern length
#define ADC_REF_KEY        0x5A       // ADC reference decode mask

// Profile display
#define PROFILE_DISPLAY_MS 3000

// ============================================================================
// ENUMS
// ============================================================================
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_DECOY, MODE_SCHEDULE, MODE_COUNT };
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME, FMT_MOUSE_STYLE, FMT_ON_OFF, FMT_SCHEDULE_MODE, FMT_TIME_5MIN, FMT_UPTIME, FMT_DIE_TEMP };
enum ScheduleMode { SCHED_OFF, SCHED_AUTO_SLEEP, SCHED_FULL_AUTO, SCHED_MODE_COUNT };
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };

// USB HID report IDs (for composite keyboard + mouse descriptor)
enum USBReportId { RID_KEYBOARD = 1, RID_MOUSE };

enum SettingId {
  SET_KEY_MIN, SET_KEY_MAX, SET_KEY_SLOTS,
  SET_MOUSE_JIG, SET_MOUSE_IDLE, SET_MOUSE_AMP, SET_MOUSE_STYLE,
  SET_LAZY_PCT, SET_BUSY_PCT,
  SET_DISPLAY_BRIGHT, SET_SAVER_BRIGHT, SET_SAVER_TIMEOUT,
  SET_ANIMATION,
  SET_BLE_IDENTITY,
  SET_BT_WHILE_USB,
  SET_SCROLL,
  SET_DASHBOARD,
  SET_SCHEDULE_MODE,
  SET_SCHEDULE_START,
  SET_SCHEDULE_END,
  SET_RESTORE_DEFAULTS,
  SET_REBOOT,
  SET_VERSION,
  SET_UPTIME,
  SET_DIE_TEMP
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

#define MENU_ITEM_COUNT 30

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
  uint8_t mouseStyle;      // 0=Bezier, 1=Brownian (default 0)
  uint8_t animStyle;       // 0-5 index into ANIM_NAMES[] (default 2 = Ghost)
  char    deviceName[15]; // 14 chars + null terminator (BLE device name)
  uint8_t btWhileUsb;     // 0=Off (default), 1=On — keep BLE active when USB connected
  uint8_t scrollEnabled;  // 0=Off (default), 1=On — random scroll wheel during mouse jiggle
  uint8_t dashboardEnabled; // 1=On (default), 0=Off — WebUSB landing page for Chrome
  uint8_t dashboardBootCount; // 0-2=boot count (auto-disable after 3), 0xFF=user pinned
  uint8_t decoyIndex;     // 0=Custom/default, 1-10=preset index into DECOY_NAMES[]
  uint8_t scheduleMode;   // 0=Off, 1=Auto-sleep, 2=Full auto
  uint16_t scheduleStart;  // 0-287 (5-min slots), default 108 (9:00)
  uint16_t scheduleEnd;    // 0-287 (5-min slots), default 204 (17:00)
  uint8_t checksum;       // must remain last
};

#endif // GHOST_CONFIG_H

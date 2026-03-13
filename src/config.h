#ifndef GHOST_CONFIG_H
#define GHOST_CONFIG_H

#include <Arduino.h>
#include <stddef.h>

// ============================================================================
// VERSION & CONFIG
// ============================================================================
#define VERSION "2.4.0"
#define DEVICE_NAME "GhostOperator"
#define SETTINGS_FILE "/settings.dat"
#define SETTINGS_MAGIC 0x50524F61  // bumped: added racer settings
#define NUM_CLICK_SLOTS   7       // configurable click action slots (like key slots)
#define NUM_CLICK_TYPES   8       // Left, Middle, Right, Btn4, Btn5, WheelUp, WheelDown, NONE
#define DECOY_COUNT 10
#define NUM_SLOTS 8
#define NUM_KEYS 37  // must match AVAILABLE_KEYS[] array size

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
#define PIN_SOUND        6   // D6 - Piezo buzzer
#define PIN_MUTE_BTN     7   // D7 - Mute button (SPST, active LOW)

// Internal battery pin - use board defaults if available
#ifndef PIN_VBAT
#define PIN_VBAT         32
#endif
#define PIN_VBAT_ENABLE  14
#define PIN_CHG_STATUS   23  // D23 = P0.17 (~CHG), active LOW when charging

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
#define DISPLAY_UPDATE_MS     50          // 20 Hz (dirty flag skips I2C when idle)
#define DISPLAY_UPDATE_SAVER_MS  200     // 5 Hz during screensaver (power saving)
#define BATTERY_READ_MS       60000UL
#define SLEEP_CONFIRM_THRESHOLD_MS  500   // Hold before showing confirmation
#define SLEEP_COUNTDOWN_MS          6000  // Total countdown duration (light + deep)
#define SLEEP_LIGHT_THRESHOLD_MS    3000  // Midpoint: release after this = light sleep
#define SLEEP_CANCEL_DISPLAY_MS     400   // "Cancelled" display duration
#define SLEEP_DISPLAY_MS            500   // Brief "SLEEPING..." before power-off
#define MODE_TIMEOUT_MS       30000       // Return to NORMAL after 30s inactivity

// Breakout game mode
#define BREAKOUT_BRICK_ROWS       4
#define BREAKOUT_BRICK_COLS       16
#define BREAKOUT_BRICK_W          7       // pixels per brick
#define BREAKOUT_BRICK_H          3       // pixels per brick
#define BREAKOUT_PADDLE_Y         53      // paddle top y position
#define BREAKOUT_BALL_SIZE        2       // 2x2 px ball
#define BALL_SPEED_COUNT          3       // Slow, Normal, Fast
#define PADDLE_SIZE_COUNT         4       // Small, Normal, Large, XL
#define BREAKOUT_TICK_MS          50      // 20 Hz game tick

// Snake game mode
#define SNAKE_GRID_CELL           4       // pixels per cell
#define SNAKE_GRID_W              32      // cells wide  (32 * 4 = 128px)
#define SNAKE_GRID_H              14      // cells tall  (14 * 4 = 56px)
#define SNAKE_HEADER_H            8       // header height in pixels
#define SNAKE_MAX_LENGTH          255     // max snake body segments (uint8_t index)
#define SNAKE_TICK_SLOW_MS        200
#define SNAKE_TICK_NORMAL_MS      150
#define SNAKE_TICK_FAST_MS        100
#define SNAKE_SPEED_COUNT         3       // Slow, Normal, Fast
#define SNAKE_WALL_COUNT          2       // Solid, Wrap

// Racer game mode (portrait 64w × 128h)
#define RACER_TICK_MS             33      // ~30 Hz game tick
#define RACER_ROAD_W              32      // road strip width in pixels
#define RACER_CAR_W               5       // player car width
#define RACER_CAR_H               8       // player car height
#define RACER_ENEMY_W             5       // enemy car width
#define RACER_ENEMY_H             7       // enemy car height
#define RACER_MAX_ENEMIES         4       // max simultaneous enemies
#define RACER_PLAYER_Y            110     // player car Y position (portrait coords)
#define RACER_SPEED_COUNT         3       // Slow, Normal, Fast
#define RACER_STEER_STEP          3       // pixels per encoder click

// Volume control mode
#define VOLUME_THEME_COUNT        3       // Basic, Retro, Futuristic
#define ENC_BTN_ACTION_COUNT      2       // Play/Pause, Mute
#define SIDE_BTN_ACTION_COUNT     3       // Next, Mute, Play/Pause
#define VOL_FEEDBACK_DISPLAY_MS   1000    // momentary direction indicator duration
#define VOL_DOUBLECLICK_MS        300     // double-click window for D7 next/prev

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

// Watchdog
#define WDT_TIMEOUT_MS 8000

// Profile display
#define PROFILE_DISPLAY_MS 3000

// Simulation mode
#define JOB_SIM_COUNT       3       // Staff, Developer, Designer

// K+M (form filling) sub-phase timing
#define KBMS_SWIPE_DUR_MS       500       // each mouse swipe duration
#define KBMS_SWIPE_MIN          1         // min swipes per cycle
#define KBMS_SWIPE_MAX          3         // max swipes per cycle
#define KBMS_MOUSE_PAUSE_MIN_MS 100       // pause between swipes
#define KBMS_MOUSE_PAUSE_MAX_MS 300
#define KBMS_KEYS_MIN           3         // keys per typing burst
#define KBMS_KEYS_MAX           10
#define KBMS_TYPING_PAUSE_MIN_MS 200      // pause after typing before next swipe cycle
#define KBMS_TYPING_PAUSE_MAX_MS 800
#define KBMS_CLICK_PAUSE_MIN_MS 100       // pause after click (field focus)
#define KBMS_CLICK_PAUSE_MAX_MS 300

// M+K (drawing tool) sub-phase timing
#define MSKB_KEY_HOLD_MIN_MS    3000      // min key hold duration
#define MSKB_KEY_HOLD_MAX_MS    5000      // max key hold duration
#define MSKB_STROKE_DUR_MS      250       // each mouse stroke duration
#define MSKB_STROKES_MIN        3         // min strokes per hold
#define MSKB_STROKES_MAX        5         // max strokes per hold
#define MSKB_PAUSE_MIN_MS       300       // pause between hold cycles
#define MSKB_PAUSE_MAX_MS       1000
#define MSKB_SETUP_DELAY_MS     100       // brief delay after key down before drawing

#define LUNCH_OFFSET_MIN        240       // 4 hours after job start
#define LUNCH_MIN_DURATION_MIN  60        // minimum 1 hour (legacy default)
#define LUNCH_DURATION_JITTER   10        // ±10% on duration
#define SHIFT_MIN_MINUTES       240       // 4h minimum shift
#define SHIFT_MAX_MINUTES       720       // 12h maximum shift
#define SHIFT_STEP_MINUTES      30        // 30-min increments
#define LUNCH_DUR_MIN           15        // 15-min minimum lunch
#define LUNCH_DUR_MAX           120       // 2h maximum lunch
#define LUNCH_NO_LUNCH_THRESHOLD 300      // 5h — shifts below this skip lunch
#define ACTIVITY_FLOOR_GAP_MS   120000UL  // max 2 min without a keystroke
#define ACTIVITY_FLOOR_BURST_MIN  2       // min keys per keepalive burst
#define ACTIVITY_FLOOR_BURST_MAX  5       // max keys per keepalive burst
#define WMODE_NAME_MAX      12      // max work mode short name length
#define MAX_BLOCK_MODES     5       // max weighted micro-modes per time block
#define MAX_DAY_BLOCKS      12      // max time blocks per day template
#define SIM_SCHEDULE_PREVIEW_MS 3000  // overlay display time for schedule preview

// ============================================================================
// ENUMS
// ============================================================================
enum UIMode { MODE_NORMAL, MODE_MENU, MODE_SLOTS, MODE_NAME, MODE_DECOY, MODE_SCHEDULE, MODE_MODE, MODE_SET_CLOCK, MODE_CAROUSEL, MODE_CLICK_SLOTS, MODE_COUNT };
enum MenuItemType { MENU_HEADING, MENU_VALUE, MENU_ACTION };
enum MenuValueFormat { FMT_DURATION_MS, FMT_PERCENT, FMT_PERCENT_NEG, FMT_SAVER_NAME, FMT_VERSION, FMT_PIXELS, FMT_ANIM_NAME, FMT_MOUSE_STYLE, FMT_ON_OFF, FMT_SCHEDULE_MODE, FMT_TIME_5MIN, FMT_UPTIME, FMT_DIE_TEMP, FMT_OP_MODE, FMT_JOB_SIM, FMT_SWITCH_KEYS, FMT_HEADER_DISP, FMT_CLICK_TYPE, FMT_KEY_SOUND, FMT_PERF_LEVEL, FMT_VOLUME_THEME, FMT_ENC_BTN_ACTION, FMT_SIDE_BTN_ACTION, FMT_BALL_SPEED, FMT_PADDLE_SIZE, FMT_HIGH_SCORE, FMT_LIVES, FMT_SNAKE_SPEED, FMT_SNAKE_WALLS, FMT_SNAKE_HIGH_SCORE, FMT_RACER_SPEED, FMT_RACER_HIGH_SCORE };
enum ScheduleMode { SCHED_OFF, SCHED_AUTO_SLEEP, SCHED_FULL_AUTO, SCHED_MODE_COUNT };
enum Profile { PROFILE_LAZY, PROFILE_NORMAL, PROFILE_BUSY, PROFILE_COUNT };
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING, MOUSE_RETURNING };
enum FooterMode { FOOTER_CLOCK, FOOTER_UPTIME, FOOTER_VERSION, FOOTER_DIETEMP, FOOTER_MODE_COUNT };

// Simulation mode enums
enum ActivityPhase { PHASE_TYPING, PHASE_MOUSING, PHASE_IDLE, PHASE_SWITCHING, PHASE_KB_MOUSE, PHASE_MOUSE_KB, PHASE_COUNT };
enum WorkModeId {
  WMODE_EMAIL_COMPOSE, WMODE_EMAIL_READ, WMODE_PROGRAMMING, WMODE_BROWSING,
  WMODE_CHAT_SLACK, WMODE_VIDEO_CONF, WMODE_DOC_EDITING, WMODE_COFFEE_BREAK,
  WMODE_LUNCH_BREAK, WMODE_IRL_MEETING, WMODE_FILE_MGMT, WMODE_COUNT
};
enum OperationMode { OP_SIMPLE, OP_SIMULATION, OP_VOLUME, OP_BREAKOUT, OP_SNAKE, OP_RACER, OP_MODE_COUNT };
enum SwitchKeys { SWITCH_KEYS_ALT_TAB, SWITCH_KEYS_CMD_TAB, SWITCH_KEYS_COUNT };

// Breakout game states
enum BreakoutState { BRK_IDLE, BRK_PLAYING, BRK_PAUSED, BRK_LEVEL_CLEAR, BRK_GAME_OVER };

// Snake game states
enum SnakeState { SNAKE_IDLE, SNAKE_PLAYING, SNAKE_PAUSED, SNAKE_GAME_OVER };

// Racer game states
enum RacerState { RACER_IDLE, RACER_PLAYING, RACER_PAUSED, RACER_GAME_OVER };

// USB HID report IDs (for composite keyboard + mouse + consumer descriptor)
enum USBReportId { RID_KEYBOARD = 1, RID_MOUSE, RID_CONSUMER };

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
  SET_INVERT_DIAL,
  SET_SCHEDULE_MODE,
  SET_SCHEDULE_START,
  SET_SCHEDULE_END,
  SET_OP_MODE,
  SET_JOB_SIM,
  SET_JOB_PERFORMANCE,
  SET_JOB_START_TIME,
  SET_PHANTOM_CLICKS,
  SET_CLICK_SLOTS,
  SET_WINDOW_SWITCH,
  SET_SWITCH_KEYS,
  SET_HEADER_DISPLAY,
  SET_SOUND_ENABLED,
  SET_SOUND_TYPE,
  SET_SYSTEM_SOUND,
  SET_VOLUME_THEME,
  SET_ENC_BUTTON,
  SET_SIDE_BUTTON,
  SET_BALL_SPEED,
  SET_PADDLE_SIZE,
  SET_START_LIVES,
  SET_HIGH_SCORE,
  SET_SNAKE_SPEED,
  SET_SNAKE_WALLS,
  SET_SNAKE_HIGH_SCORE,
  SET_RACER_SPEED,
  SET_RACER_HIGH_SCORE,
  SET_SHIFT_DURATION,
  SET_LUNCH_DURATION,
  SET_SET_CLOCK,
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

#define MENU_ITEM_COUNT 62
#define KB_SOUND_COUNT  5

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
  uint8_t invertDial;     // 0=Off (default), 1=On — reverse encoder rotation
  // Simulation mode settings
  uint8_t operationMode;    // 0=Simple (default), 1=Simulation, 2=Volume Control
  uint8_t jobSimulation;    // 0=Staff, 1=Developer, 2=Designer (default: 0)
  uint8_t jobPerformance;   // 0-11, default 5 (level*10 = percentage, 5=baseline)
  uint16_t jobStartTime;    // 0-287 (5-min slots), default 96 (8:00)
  uint8_t phantomClicks;    // 0=Off (default), 1=On
  uint8_t clickSlots[NUM_CLICK_SLOTS]; // 7 click action slots (index into CLICK_TYPE_NAMES[])
  uint8_t windowSwitching;  // 0=Off (default), 1=On
  uint8_t switchKeys;       // 0=Alt-Tab (default), 1=Cmd-Tab
  uint8_t headerDisplay;    // 0=Job sim name (default), 1=Device name
  uint8_t soundEnabled;     // 0=Off (default), 1=On
  uint8_t soundType;        // 0=MX Blue, 1=MX Brown, 2=Membrane, 3=Buckling, 4=Thock
  uint8_t systemSoundEnabled; // 0=Off (default), 1=On — BLE connect/disconnect alert tones
  // Volume control settings
  uint8_t volumeTheme;      // 0=Basic (default), 1=Retro, 2=Futuristic
  uint8_t encButtonAction;  // 0=Play/Pause (default), 1=Mute
  uint8_t sideButtonAction; // 0=Next (default), 1=Mute, 2=Play/Pause
  // Breakout game settings
  uint8_t ballSpeed;        // 0=Slow, 1=Normal (default), 2=Fast
  uint8_t paddleSize;       // 0=Small, 1=Normal (default), 2=Large, 3=XL
  uint8_t startLives;       // 1-5, default 3
  uint16_t highScore;       // persistent high score (breakout)
  // Snake game settings
  uint8_t snakeSpeed;       // 0=Slow, 1=Normal (default), 2=Fast
  uint8_t snakeWalls;       // 0=Solid (default), 1=Wrap
  uint16_t snakeHighScore;  // persistent high score (snake)
  // Racer game settings
  uint8_t racerSpeed;       // 0=Slow, 1=Normal (default), 2=Fast
  uint16_t racerHighScore;  // persistent high score (racer)
  // Shift/lunch settings (dashboard-only)
  uint16_t shiftDuration;   // 240-720 min, step 30, default 480 (8h)
  uint8_t lunchDuration;    // 15-120 min, step 5, default 30 (30m)
  uint8_t checksum;         // MUST remain last
};

// Breakout game state (runtime, not persisted)
struct BreakoutGameState {
  int16_t ballX, ballY;       // 8.8 fixed-point (pixel * 256)
  int16_t ballDx, ballDy;     // 8.8 fixed-point velocity
  int16_t paddleX;            // pixel position of paddle left edge
  uint8_t paddleW;            // paddle width in pixels
  uint16_t brickRows[BREAKOUT_BRICK_ROWS];  // bitfield: 16 bricks per row
  uint8_t brickHits[BREAKOUT_BRICK_ROWS];   // bitfield: reinforced brick hit tracker
  uint8_t lives;
  uint8_t level;
  uint16_t score;
  uint8_t bricksRemaining;
  BreakoutState state;
  unsigned long lastTickMs;
  unsigned long stateTimer;   // for level-clear/game-over display timing
};

// Snake game state (runtime, not persisted)
struct SnakeGameState {
  uint8_t bodyX[256];        // circular buffer X coordinates
  uint8_t bodyY[256];        // circular buffer Y coordinates
  uint8_t headIdx;           // index of head in circular buffer
  uint8_t length;            // current snake length
  int8_t  dirX, dirY;        // current heading direction
  int8_t  nextDirX, nextDirY; // queued direction from encoder
  uint8_t foodX, foodY;      // food position
  uint16_t score;
  SnakeState state;
  unsigned long lastTickMs;
};

// Racer game state (runtime, not persisted)
struct RacerEnemy {
  int8_t x;           // X position (portrait coords)
  int16_t y;          // Y position (portrait coords, can be negative for spawn)
  bool active;
};
struct RacerGameState {
  int8_t playerX;             // player car X (portrait, left edge)
  int16_t roadCenterX;        // current road center X (portrait width=64)
  int16_t roadTargetX;        // target center for smooth curve
  uint16_t score;
  uint8_t scrollOffset;       // 0-7, animates dashed lines
  RacerEnemy enemies[RACER_MAX_ENEMIES];
  RacerState state;
  unsigned long lastTickMs;
  unsigned long lastEnemySpawnMs;
  uint16_t spawnIntervalMs;   // decreases with score for difficulty
  unsigned long lastCurveChangeMs;  // road curve target change timer
};

// Carousel page config (generic full-screen picker for named-option settings)
typedef void (*CarouselCursorCallback)(uint8_t newIndex);
struct CarouselConfig {
  const char*            title;      // Header text (e.g., "ANIMATION STYLE")
  const char* const*     names;      // Option name array (reuse existing)
  const char* const*     descs;      // Per-option help descriptions
  uint8_t                count;      // Number of options
  uint8_t                settingId;  // SettingId to read/write
  // onCursorChange callback stored as runtime state (carouselCallback in state.h)
  // — cannot live here because static const struct is placed in flash on ARM
};

#endif // GHOST_CONFIG_H

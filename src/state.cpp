#include "state.h"

using namespace Adafruit_LittleFS_Namespace;

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false;
volatile bool displayDirty = true;  // first frame always renders

// File system
File settingsFile(InternalFS);

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;
BLEUart bleuart;

// USB HID
Adafruit_USBD_HID usb_hid;

// Settings
Settings settings;

// Mutable work modes (see initWorkModes() in sim_data.cpp)
WorkModeDef workModes[WMODE_COUNT];

// Profile
Profile currentProfile = PROFILE_NORMAL;
unsigned long profileDisplayStart = 0;

// Encoder
volatile int encoderPos = 0;
int lastEncoderPos = 0;
volatile uint8_t encoderPrevState = 0;
volatile int8_t  lastEncoderDir  = 0;

// Connection & enables
volatile bool deviceConnected = false;
bool usbConnected = false;
volatile uint16_t bleConnHandle = BLE_CONN_HANDLE_INVALID;
bool bleDisabledForUsb = false;
bool keyEnabled = true;
bool mouseEnabled = true;
uint8_t activeSlot = 0;
uint8_t activeClickSlot = 0;
uint8_t nextKeyIndex = 0;

// Name editor state
uint8_t nameCharIndex[NAME_MAX_LEN];
uint8_t activeNamePos = 0;
bool    nameConfirming = false;
bool    nameRebootYes = true;
char    nameOriginal[NAME_MAX_LEN + 1];

// Decoy identity picker state
int8_t  decoyCursor = 0;
int8_t  decoyScrollOffset = 0;
bool    decoyConfirming = false;
bool    decoyRebootYes = true;
uint8_t decoyOriginal = 0;

// Reset defaults confirmation state
bool    defaultsConfirming = false;
bool    defaultsConfirmYes = false;

// Reboot confirmation state
bool    rebootConfirming = false;
bool    rebootConfirmYes = false;

// Mode picker state (MODE_MODE sub-page)
uint8_t modePickerCursor = 0;
bool    modePickerSnap = true;
bool    modeConfirming = false;
bool    modeRebootYes = true;
uint8_t modeOriginalValue = 0;

// Generic carousel state (MODE_CAROUSEL)
uint8_t carouselCursor = 0;
uint8_t carouselOriginal = 0;
const CarouselConfig* carouselConfig = NULL;
CarouselCursorCallback carouselCallback = NULL;

// UI Mode
UIMode currentMode = MODE_NORMAL;
unsigned long lastModeActivity = 0;
bool screensaverActive = false;
FooterMode footerMode = FOOTER_UPTIME;

// Menu state
int8_t   menuCursor = -1;
int8_t   menuScrollOffset = 0;
bool     menuEditing = false;
int8_t   helpScrollPos = 0;
int8_t   helpScrollDir = 1;
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
MouseState mouseState = MOUSE_IDLE;
int8_t currentMouseDx = 0;
int8_t currentMouseDy = 0;
int32_t mouseNetX = 0;
int32_t mouseNetY = 0;
int32_t mouseReturnTotal = 0;

// Scroll state
unsigned long lastScrollTime = 0;
unsigned long nextScrollInterval = 3000;

// Easter egg
uint32_t mouseJiggleCount = 0;
bool     easterEggActive  = false;
uint8_t  easterEggFrame   = 0;

// Battery
int batteryPercent = 100;
float batteryVoltage = 4.2;
bool batteryCharging = false;

// BLE connection interval management
unsigned long lastHidActivity = 0;
unsigned long ledKbOffMs = 0;
unsigned long ledMouseOffMs = 0;
bool bleIdleMode = false;
uint8_t bleHidFailCount = 0;

// Die temperature (hysteresis-smoothed)
int16_t cachedDieTempRaw = INT16_MIN;

// RF/ADC thermal compensation
uint8_t  rfThermalOffset  = 0;
uint16_t adcDriftComp     = 0;
unsigned long adcCalStart = 0;
unsigned long adcSettleTarget = 60000;

// Serial status push (off by default — toggle with 't' command)
bool serialStatusPush = false;

// Schedule editor state
int8_t scheduleCursor = 0;
bool   scheduleEditing = false;
uint8_t  scheduleOrigMode = 0;
uint16_t scheduleOrigStart = 0;
uint16_t scheduleOrigEnd = 0;

// Clock editor state (MODE_SET_CLOCK)
int8_t  clockCursor   = 0;
bool    clockEditing  = false;
uint8_t clockHour     = 12;
uint8_t clockMinute   = 0;

// Schedule / wall clock
bool timeSynced = false;
uint32_t wallClockDaySecs = 0;
unsigned long wallClockSyncMs = 0;
bool scheduleSleeping = false;
bool manualLightSleep = false;
bool scheduleManualWake = false;
unsigned long lastScheduleCheck = 0;

// Button state
unsigned long funcBtnPressStart = 0;
bool funcBtnWasPressed = false;
bool sleepPending = false;
bool lightSleepPending = false;
bool     sleepConfirmActive = false;
unsigned long sleepConfirmStart = 0;
bool     sleepCancelActive = false;
unsigned long sleepCancelStart = 0;

// Mute button state (D7)
unsigned long lastMuteBtnPress = 0;

// Volume control state
int8_t        volFeedbackDir    = 0;
unsigned long volFeedbackStart  = 0;
bool          volMuted          = false;
bool          volPlaying        = true;   // assume playing on boot

// D7 double-click state for Volume Control next/prev track
unsigned long volD7LastPress    = 0;
uint8_t       volD7ClickCount   = 0;

// Orchestrator state (simulation mode)
OrchestratorState orch = {};

// Game state (union — breakout, snake, and racer are mutually exclusive)
GameState gameState = {};

// Deferred sound playback (set in BLE callbacks, consumed in loop())
volatile bool connectSoundPending = false;
volatile bool disconnectSoundPending = false;

// Deferred settings save
bool settingsDirty = false;
unsigned long settingsDirtyMs = 0;

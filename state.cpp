#include "state.h"

using namespace Adafruit_LittleFS_Namespace;

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false;

// File system
File settingsFile(InternalFS);

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;
BLEUart bleuart;

// Settings
Settings settings;

// Profile
Profile currentProfile = PROFILE_NORMAL;
unsigned long profileDisplayUntil = 0;

// Encoder
volatile int encoderPos = 0;
int lastEncoderPos = 0;
uint8_t encoderPrevState = 0;
int8_t  lastEncoderDir  = 0;

// Connection & enables
bool deviceConnected = false;
bool keyEnabled = true;
bool mouseEnabled = true;
uint8_t activeSlot = 0;
uint8_t nextKeyIndex = 0;

// Name editor state
uint8_t nameCharIndex[NAME_MAX_LEN];
uint8_t activeNamePos = 0;
bool    nameConfirming = false;
bool    nameRebootYes = true;
char    nameOriginal[NAME_MAX_LEN + 1];

// Reset defaults confirmation state
bool    defaultsConfirming = false;
bool    defaultsConfirmYes = false;

// Reboot confirmation state
bool    rebootConfirming = false;
bool    rebootConfirmYes = false;

// UI Mode
UIMode currentMode = MODE_NORMAL;
unsigned long lastModeActivity = 0;
bool screensaverActive = false;

// Menu state
int8_t   menuCursor = -1;
int8_t   menuScrollOffset = 0;
bool     menuEditing = false;
int16_t  helpScrollPos = 0;
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

// Easter egg
uint32_t mouseJiggleCount = 0;
bool     easterEggActive  = false;
uint8_t  easterEggFrame   = 0;

// Battery
int batteryPercent = 100;
float batteryVoltage = 4.2;

// RF/ADC thermal compensation
uint8_t  rfThermalOffset  = 0;
uint16_t adcDriftComp     = 0;
unsigned long adcCalStart = 0;
unsigned long adcSettleTarget = 60000;

// Button state
unsigned long funcBtnPressStart = 0;
bool funcBtnWasPressed = false;
bool sleepPending = false;
bool     sleepConfirmActive = false;
unsigned long sleepConfirmStart = 0;
bool     sleepCancelActive = false;
unsigned long sleepCancelStart = 0;

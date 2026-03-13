#include "state.h"   // resolves to nrf52/state.h (includes common state.h)

using namespace Adafruit_LittleFS_Namespace;

// ============================================================================
// nRF52-specific state definitions
// ============================================================================

// Display hardware
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false;

// File system
File settingsFile(InternalFS);

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;
BLEUart bleuart;

// USB HID
Adafruit_USBD_HID usb_hid;

// BLE connection management
volatile uint16_t bleConnHandle = BLE_CONN_HANDLE_INVALID;
bool bleDisabledForUsb = false;
unsigned long lastHidActivity = 0;
bool bleIdleMode = false;

// Encoder ISR state
volatile uint8_t encoderPrevState = 0;
volatile int8_t  lastEncoderDir  = 0;

// RF/ADC thermal compensation
uint8_t  rfThermalOffset  = 0;
uint16_t adcDriftComp     = 0;
unsigned long adcCalStart = 0;
unsigned long adcSettleTarget = 60000;

// Game state (union — breakout, snake, and racer are mutually exclusive)
GameState gameState = {};

// Deferred sound playback (set in BLE callbacks, consumed in loop())
volatile bool connectSoundPending = false;
volatile bool disconnectSoundPending = false;

// Deferred settings save (avoids flash wear on rapid game-over)
bool settingsDirty = false;
unsigned long settingsDirtyMs = 0;

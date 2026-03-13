#ifndef GHOST_NRF52_STATE_H
#define GHOST_NRF52_STATE_H

// Include common (platform-independent) state first
#include "../common/state.h"

// nRF52-specific includes
#include <bluefruit.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// ============================================================================
// nRF52-specific state — hardware objects and platform-dependent variables
// ============================================================================

// Display hardware
extern Adafruit_SSD1306 display;
extern bool displayInitialized;

// File system
extern Adafruit_LittleFS_Namespace::File settingsFile;

// BLE Services
extern BLEDis bledis;
extern BLEHidAdafruit blehid;
extern BLEUart bleuart;

// USB HID
extern Adafruit_USBD_HID usb_hid;

// BLE connection management
extern volatile uint16_t bleConnHandle;
extern bool bleDisabledForUsb;
extern unsigned long lastHidActivity;
extern bool bleIdleMode;

// Encoder ISR state
extern volatile uint8_t encoderPrevState;
extern volatile int8_t lastEncoderDir;

// RF/ADC thermal compensation
extern uint8_t  rfThermalOffset;
extern uint16_t adcDriftComp;
extern unsigned long adcCalStart;
extern unsigned long adcSettleTarget;

// Game state (union — breakout, snake, and racer are mutually exclusive)
union GameState {
  BreakoutGameState brk;
  SnakeGameState snk;
  RacerGameState rcr;
};
extern GameState gameState;
#define gBrk (gameState.brk)
#define gSnk (gameState.snk)
#define gRcr (gameState.rcr)

// Deferred sound playback (set in BLE callbacks, consumed in loop())
extern volatile bool connectSoundPending;
extern volatile bool disconnectSoundPending;

// Deferred settings save (avoids flash wear on rapid game-over)
extern bool settingsDirty;
extern unsigned long settingsDirtyMs;

#endif // GHOST_NRF52_STATE_H

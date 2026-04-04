#ifndef GHOST_NRF52_STATE_H
#define GHOST_NRF52_STATE_H

// Portable state (settings, timing, UI, orchestrator, etc.)
#include "../common/state.h"

// nRF52 hardware includes
#include <bluefruit.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// Display (hardware object — nRF52 uses SSD1306 OLED)
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

// Encoder hardware state
extern volatile uint8_t encoderPrevState;
extern volatile int8_t lastEncoderDir;

// BLE connection state
extern volatile uint16_t bleConnHandle;
extern bool bleDisabledForUsb;
extern unsigned long lastHidActivity;
extern bool bleIdleMode;
extern uint8_t bleHidFailCount;  // consecutive GATT notify failures

// Die temperature (hysteresis-smoothed)
extern int16_t cachedDieTempRaw;

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

// JSON push mode (set when statusPush enabled via JSON protocol)
extern bool jsonPushMode;

// Deferred sound playback (set in BLE callbacks, consumed in loop())
extern volatile bool connectSoundPending;
extern volatile bool disconnectSoundPending;

// Deferred mouse state reset (set in connect_callback, consumed in loop())
extern volatile bool mouseResetPending;

#endif // GHOST_NRF52_STATE_H

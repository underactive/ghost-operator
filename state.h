#ifndef GHOST_STATE_H
#define GHOST_STATE_H

#include "config.h"
#include <bluefruit.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// Display
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

// Settings
extern Settings settings;

// Profile
extern Profile currentProfile;
extern unsigned long profileDisplayUntil;

// Encoder
extern volatile int encoderPos;
extern int lastEncoderPos;
extern uint8_t encoderPrevState;
extern int8_t lastEncoderDir;

// Connection & enables
extern bool deviceConnected;
extern bool usbConnected;
extern uint16_t bleConnHandle;
extern bool bleDisabledForUsb;
extern bool keyEnabled;
extern bool mouseEnabled;
extern uint8_t activeSlot;
extern uint8_t nextKeyIndex;

// Name editor state
extern uint8_t nameCharIndex[];
extern uint8_t activeNamePos;
extern bool    nameConfirming;
extern bool    nameRebootYes;
extern char    nameOriginal[];

// Reset defaults confirmation state
extern bool    defaultsConfirming;
extern bool    defaultsConfirmYes;

// Reboot confirmation state
extern bool    rebootConfirming;
extern bool    rebootConfirmYes;

// UI Mode
extern UIMode currentMode;
extern unsigned long lastModeActivity;
extern bool screensaverActive;

// Menu state
extern int8_t   menuCursor;
extern int8_t   menuScrollOffset;
extern bool     menuEditing;
extern int16_t  helpScrollPos;
extern int8_t   helpScrollDir;
extern unsigned long helpScrollTimer;

// Timing
extern unsigned long startTime;
extern unsigned long lastKeyTime;
extern unsigned long lastMouseStateChange;
extern unsigned long lastMouseStep;
extern unsigned long lastDisplayUpdate;
extern unsigned long lastBatteryRead;

// Current targets (with randomness applied for mouse)
extern unsigned long currentKeyInterval;
extern unsigned long currentMouseJiggle;
extern unsigned long currentMouseIdle;

// Mouse state machine
extern MouseState mouseState;
extern int8_t currentMouseDx;
extern int8_t currentMouseDy;
extern int32_t mouseNetX;
extern int32_t mouseNetY;
extern int32_t mouseReturnTotal;

// Scroll state
extern unsigned long lastScrollTime;
extern unsigned long nextScrollInterval;

// Easter egg
extern uint32_t mouseJiggleCount;
extern bool     easterEggActive;
extern uint8_t  easterEggFrame;

// Battery
extern int batteryPercent;
extern float batteryVoltage;

// RF/ADC thermal compensation
extern uint8_t  rfThermalOffset;
extern uint16_t adcDriftComp;
extern unsigned long adcCalStart;
extern unsigned long adcSettleTarget;

// Serial status push (toggle with 't' command)
extern bool serialStatusPush;

// Button state
extern unsigned long funcBtnPressStart;
extern bool funcBtnWasPressed;
extern bool sleepPending;
extern bool     sleepConfirmActive;
extern unsigned long sleepConfirmStart;
extern bool     sleepCancelActive;
extern unsigned long sleepCancelStart;

#endif // GHOST_STATE_H

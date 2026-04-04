#ifndef GHOST_PLATFORM_HAL_H
#define GHOST_PLATFORM_HAL_H

#include <stdint.h>
#include <stdbool.h>

// Platform HAL — Cross-platform function signatures
// Declared here, implemented per-platform (src/nrf52/, src/esp32-c6-lcd-1.47/, src/esp32-s3-lcd-1.47/).
// Resolved at link time — no vtable overhead.
//
// Functions already declared in other common headers are NOT repeated here:
//   settings.h:  saveSettings(), loadSettings(), saveStats(), loadStats(), getDieTempCelsius()
//   sim_data.h:  initWorkModes(), saveSimData(), resetSimDataDefaults()
//   schedule.h:  enterLightSleep(), exitLightSleep()

// --- HID output ---
void sendKeystroke();
void pickNextKey();
bool hasPopulatedSlot();
void sendKeyDown(uint8_t keyIndex, bool silent = false);
void sendKeyUp();
void sendMouseMove(int8_t dx, int8_t dy);
void sendMouseScroll(int8_t scroll);
void sendMouseClick(uint8_t button, uint16_t holdMs);
void sendWindowSwitch();
bool hasPopulatedClickSlot();
uint8_t pickNextClick();
void executeClick(uint8_t actionIdx, uint16_t holdMs);
void sendConsumerPress(uint16_t usageCode);
void sendConsumerRelease();

// --- Display ---
void markDisplayDirty();
void invalidateDisplayShadow();

// --- Serial status ---
void pushSerialStatus();

// --- Sleep / power ---
void enterDeepSleep();

// --- Activity LEDs ---
void tickActivityLeds();

#endif // GHOST_PLATFORM_HAL_H

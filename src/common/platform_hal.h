#ifndef GHOST_PLATFORM_HAL_H
#define GHOST_PLATFORM_HAL_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Platform HAL — Cross-platform function signatures
//
// Declared here, implemented per-platform (src/nrf52/ or src/cyd/).
// Resolved at link time — no vtable overhead.
//
// Common modules (timing, mouse, orchestrator, schedule, settings) call these
// functions instead of including platform-specific headers directly.
// ============================================================================

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

// --- Settings persistence ---
void saveSettings();
void loadSettings();

// --- Serial status ---
void pushSerialStatus();

// --- Sleep / power ---
void enterLightSleep(bool scheduled);  // default in schedule.h
void exitLightSleep();
void enterDeepSleep();

// --- Sim data persistence ---
void initWorkModes();
void saveSimData();
void resetSimDataDefaults();

// --- Platform queries ---
int getDieTempCelsius();

#endif // GHOST_PLATFORM_HAL_H

#ifndef GHOST_HID_H
#define GHOST_HID_H

#include "config.h"

void sendKeystroke();
void pickNextKey();
bool hasPopulatedSlot();
void sendMouseMove(int8_t dx, int8_t dy);
void sendMouseScroll(int8_t scroll);

// Non-blocking key press/release for simulation mode burst state machine
void sendKeyDown(uint8_t keyIndex, bool silent = false);
void sendKeyUp();

// Simulation mode: phantom click and window switching
void sendMouseClick(uint8_t button, uint16_t holdMs);
void sendWindowSwitch();

// Click slot helpers (multi-slot random selection)
bool hasPopulatedClickSlot();
uint8_t pickNextClick();
void executeClick(uint8_t actionIdx, uint16_t holdMs);

// Consumer control (media keys) — send over both BLE and USB
void sendConsumerPress(uint16_t usageCode);
void sendConsumerRelease();

// Activity LED feedback (Blue = KB, Green = Mouse)
void tickActivityLeds();

#endif // GHOST_HID_H

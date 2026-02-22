#ifndef GHOST_HID_H
#define GHOST_HID_H

#include "config.h"

void sendKeystroke();
void pickNextKey();
bool hasPopulatedSlot();
void sendMouseMove(int8_t dx, int8_t dy);
void sendMouseScroll(int8_t scroll);

// Non-blocking key press/release for simulation mode burst state machine
void sendKeyDown(uint8_t keyIndex);
void sendKeyUp();

// Simulation mode: phantom middle-click and window switching
void sendMouseClick(uint8_t button, uint16_t holdMs);
void sendWindowSwitch();

#endif // GHOST_HID_H

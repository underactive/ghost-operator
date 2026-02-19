#ifndef GHOST_HID_H
#define GHOST_HID_H

#include "config.h"

void sendKeystroke();
void pickNextKey();
bool hasPopulatedSlot();
void sendMouseMove(int8_t dx, int8_t dy);
void sendMouseScroll(int8_t scroll);

#endif // GHOST_HID_H

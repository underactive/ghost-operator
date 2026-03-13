#ifndef GHOST_BREAKOUT_H
#define GHOST_BREAKOUT_H

#include "config.h"

void initBreakout();
void tickBreakout();
void updateGameSound();
void breakoutEncoderInput(int direction);
void breakoutButtonPress();

#endif // GHOST_BREAKOUT_H

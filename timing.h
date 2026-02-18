#ifndef GHOST_TIMING_H
#define GHOST_TIMING_H

#include "config.h"

unsigned long applyRandomness(unsigned long baseValue);
String formatDuration(unsigned long ms, bool withUnit = true);
unsigned long applyProfile(unsigned long baseValue, int direction);
unsigned long effectiveKeyMin();
unsigned long effectiveKeyMax();
unsigned long effectiveMouseJiggle();
unsigned long effectiveMouseIdle();
void scheduleNextKey();
void scheduleNextMouseState();
unsigned long saverTimeoutMs();
String formatUptime(unsigned long ms);

#endif // GHOST_TIMING_H

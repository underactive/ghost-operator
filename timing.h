#ifndef GHOST_TIMING_H
#define GHOST_TIMING_H

#include "config.h"

unsigned long applyRandomness(unsigned long baseValue);
void formatDuration(unsigned long ms, char* buf, size_t bufSize, bool withUnit = true);
unsigned long applyProfile(unsigned long baseValue, int direction);
unsigned long effectiveKeyMin();
unsigned long effectiveKeyMax();
unsigned long effectiveMouseJiggle();
unsigned long effectiveMouseIdle();
void scheduleNextKey();
void scheduleNextMouseState();
unsigned long saverTimeoutMs();
void formatUptime(unsigned long ms, char* buf, size_t bufSize);

#endif // GHOST_TIMING_H

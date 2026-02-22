#ifndef GHOST_SCHEDULE_H
#define GHOST_SCHEDULE_H

#include "config.h"

void syncTime(uint32_t daySeconds);
uint32_t currentDaySeconds();
bool isScheduleActive();
void checkSchedule();
void enterLightSleep();
void exitLightSleep();
void formatCurrentTime(char* buf, size_t bufSize);

#endif // GHOST_SCHEDULE_H

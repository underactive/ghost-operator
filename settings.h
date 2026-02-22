#ifndef GHOST_SETTINGS_H
#define GHOST_SETTINGS_H

#include "config.h"

void loadDefaults();
uint8_t calcChecksum();
void saveSettings();
void loadSettings();
uint32_t getSettingValue(uint8_t settingId);
void setSettingValue(uint8_t settingId, uint32_t value);
void formatMenuValue(uint8_t settingId, MenuValueFormat format, char* buf, size_t bufSize);

#endif // GHOST_SETTINGS_H

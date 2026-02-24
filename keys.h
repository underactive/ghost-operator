#ifndef GHOST_KEYS_H
#define GHOST_KEYS_H

#include "config.h"
#include <bluefruit.h>

extern const KeyDef AVAILABLE_KEYS[];
extern const MenuItem MENU_ITEMS[];
extern const int8_t MOUSE_DIRS[][2];
extern const int NUM_DIRS;
extern const uint8_t SAVER_MINUTES[];
extern const char* SAVER_NAMES[];
extern const char NAME_CHARS[];
extern const char* MODE_NAMES[];
extern const char* PROFILE_NAMES[];
extern const char* PROFILE_NAMES_TITLE[];
extern const char* ANIM_NAMES[];
extern const char* MOUSE_STYLE_NAMES[];
extern const char* SWITCH_KEYS_NAMES[];
extern const char* ON_OFF_NAMES[];
extern const char* KB_SOUND_NAMES[];
extern const char* VOLUME_THEME_NAMES[];
extern const char* SCHEDULE_MODE_NAMES[];
extern const char* const DECOY_NAMES[];
extern const char* const DECOY_MANUFACTURERS[];

void validateMenuIndices();

// Symbolic menu indices — must match MENU_ITEMS[] order in keys.cpp
#define MENU_IDX_KEY_SLOTS    9
#define MENU_IDX_SET_CLOCK    34
#define MENU_IDX_SCHEDULE     35
#define MENU_IDX_OP_MODE      37   // "Mode" (Simple/Simulation/Volume) — under Device heading
#define MENU_IDX_BLE_IDENTITY 38
#define MENU_IDX_UPTIME       45
#define MENU_IDX_DIE_TEMP     46
#define MENU_IDX_VERSION      47

#endif // GHOST_KEYS_H

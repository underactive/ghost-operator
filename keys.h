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
extern const char* ON_OFF_NAMES[];
extern const char* SCHEDULE_MODE_NAMES[];
extern const char* const DECOY_NAMES[];
extern const char* const DECOY_MANUFACTURERS[];

// Symbolic menu indices — must match MENU_ITEMS[] order in keys.cpp
#define MENU_IDX_KEY_SLOTS    3
#define MENU_IDX_SCHEDULE     25
#define MENU_IDX_OP_MODE      27   // "Mode" (Simple/Simulation) — under Device heading
#define MENU_IDX_BLE_IDENTITY 29
#define MENU_IDX_UPTIME       36
#define MENU_IDX_DIE_TEMP     37
#define MENU_IDX_VERSION      38

#endif // GHOST_KEYS_H

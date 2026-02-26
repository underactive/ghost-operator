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
extern const char* ENC_BTN_ACTION_NAMES[];
extern const char* SIDE_BTN_ACTION_NAMES[];
extern const char* SCHEDULE_MODE_NAMES[];
extern const char* BALL_SPEED_NAMES[];
extern const char* PADDLE_SIZE_NAMES[];
extern const char* SNAKE_SPEED_NAMES[];
extern const char* SNAKE_WALL_NAMES[];
extern const char* const DECOY_NAMES[];
extern const char* const DECOY_MANUFACTURERS[];

void validateMenuIndices();
const CarouselConfig* getCarouselConfig(uint8_t settingId);

// Symbolic menu indices — must match MENU_ITEMS[] order in keys.cpp
#define MENU_IDX_KEY_SLOTS    19
#define MENU_IDX_CLICK_SLOTS  29
#define MENU_IDX_SET_CLOCK    44
#define MENU_IDX_SCHEDULE     45
#define MENU_IDX_OP_MODE      47   // "Mode" (Simple/Simulation/Volume/Breakout/Snake) — under Device heading
#define MENU_IDX_BLE_IDENTITY 48
#define MENU_IDX_UPTIME       55
#define MENU_IDX_DIE_TEMP     56
#define MENU_IDX_HIGH_SCORE   57
#define MENU_IDX_VERSION      58

#endif // GHOST_KEYS_H

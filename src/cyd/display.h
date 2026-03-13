#ifndef GHOST_CYD_DISPLAY_H
#define GHOST_CYD_DISPLAY_H

#include "config.h"

void setupDisplay();
void updateDisplay();
void markDisplayDirty();
void invalidateDisplayShadow();

// Editor overlay control
void showValueEditor(uint8_t menuIdx);
void hideValueEditor();
bool isEditorVisible();

#endif // GHOST_CYD_DISPLAY_H

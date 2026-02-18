#ifndef GHOST_INPUT_H
#define GHOST_INPUT_H

#include "config.h"

void moveCursor(int direction);
void handleEncoder();
void handleButtons();
void initNameEditor();
bool saveNameEditor();
void returnToMenuFromName();

#endif // GHOST_INPUT_H

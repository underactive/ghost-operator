#ifndef GHOST_INPUT_H
#define GHOST_INPUT_H

#include "config.h"

void moveCursor(int direction);
void handleEncoder();
void handleButtons();
void initNameEditor();
bool saveNameEditor();
void returnToMenuFromName();
void initDecoyPicker();
void returnToMenuFromDecoy();
void initModePicker();
void returnToMenuFromMode();
void initCarousel(const CarouselConfig* config);
void returnToMenuFromCarousel();
bool isMenuItemHidden(int8_t idx);

#endif // GHOST_INPUT_H

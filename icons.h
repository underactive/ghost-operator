#ifndef GHOST_ICONS_H
#define GHOST_ICONS_H

#include "config.h"
#include <Adafruit_GFX.h>

extern const uint8_t PROGMEM iconOn[];
extern const uint8_t PROGMEM iconOff[];
extern const uint8_t PROGMEM btIcon[];
extern const uint8_t PROGMEM usbIcon[];
extern const uint8_t PROGMEM splashBitmap[];
extern const uint8_t PROGMEM splashVolumeBitmap[];

extern const uint8_t PROGMEM iconKeycapNormal[];   // 10x10 keycap with "A"
extern const uint8_t PROGMEM iconKeycapPressed[];  // 9x9 keycap with "A" (depressed)
extern const uint8_t PROGMEM iconMouseNormal[];    // 10x10 Mac Classic mouse
extern const uint8_t PROGMEM iconMouseClick[];     // 10x10 mouse button pressed
extern const uint8_t PROGMEM iconMouseScroll[];    // 10x10 mouse scroll arrow

// Volume control icons
extern const uint8_t PROGMEM iconSpeaker16[];      // 16x16 speaker with sound waves
extern const uint8_t PROGMEM iconSpeakerMuted16[];  // 16x16 speaker with X
extern const uint8_t PROGMEM iconPlay8[];           // 8x8 play triangle
extern const uint8_t PROGMEM iconPause8[];          // 8x8 pause bars

#endif // GHOST_ICONS_H

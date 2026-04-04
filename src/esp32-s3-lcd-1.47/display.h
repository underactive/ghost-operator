#ifndef GHOST_S3_DISPLAY_H
#define GHOST_S3_DISPLAY_H

void setupDisplay();
void updateDisplay();
void setBacklightBrightness(uint8_t percent);  // 0-100
void setBacklightOff();
void setDisplayFlip(bool flipped);

#endif // GHOST_S3_DISPLAY_H

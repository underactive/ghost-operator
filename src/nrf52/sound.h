#ifndef GHOST_SOUND_H
#define GHOST_SOUND_H

#include "config.h"

enum KeyboardSound {
  KB_SOUND_MX_BLUE,
  KB_SOUND_MX_BROWN,
  KB_SOUND_MEMBRANE,
  KB_SOUND_BUCKLING,
  KB_SOUND_THOCK,
};

void initSound();
bool canPlayGameSound();
void playKeySound();
void playConnectSound();
void playDisconnectSound();
void startSoundPreview(uint8_t soundType);
void stopSoundPreview();
void updateSoundPreview();
void updateAlertSound();

#endif // GHOST_SOUND_H

#include "sound.h"
#include "state.h"

void initSound() {
  pinMode(PIN_SOUND, OUTPUT);
  digitalWrite(PIN_SOUND, LOW);
}

// Random frequency variation: ±10% for realism
static uint16_t varyFreq(uint16_t freq) {
  int16_t variation = (int16_t)(freq / 10);
  return (uint16_t)(freq + random(-variation, variation + 1));
}

// MX Blue: sharp click with high-frequency snap
static void soundMxBlue() {
  tone(PIN_SOUND, varyFreq(4500), 3);
  delay(3);
  tone(PIN_SOUND, varyFreq(6000), 2);
  delay(2);
  tone(PIN_SOUND, varyFreq(3500), 4);
  delay(4);
  noTone(PIN_SOUND);
}

// MX Brown: softer bump, lower pitch
static void soundMxBrown() {
  tone(PIN_SOUND, varyFreq(3200), 4);
  delay(4);
  tone(PIN_SOUND, varyFreq(2400), 5);
  delay(5);
  tone(PIN_SOUND, varyFreq(1800), 3);
  delay(3);
  noTone(PIN_SOUND);
}

// Membrane: muted thud, low frequency
static void soundMembrane() {
  tone(PIN_SOUND, varyFreq(1500), 6);
  delay(6);
  tone(PIN_SOUND, varyFreq(1000), 8);
  delay(8);
  noTone(PIN_SOUND);
}

// Buckling Spring: sharp metallic ping
static void soundBuckling() {
  tone(PIN_SOUND, varyFreq(5500), 2);
  delay(2);
  tone(PIN_SOUND, varyFreq(7000), 2);
  delay(2);
  tone(PIN_SOUND, varyFreq(4000), 3);
  delay(3);
  tone(PIN_SOUND, varyFreq(2500), 5);
  delay(5);
  noTone(PIN_SOUND);
}

// Deep Thock: bass-heavy low thud
static void soundThock() {
  tone(PIN_SOUND, varyFreq(800), 5);
  delay(5);
  tone(PIN_SOUND, varyFreq(600), 8);
  delay(8);
  tone(PIN_SOUND, varyFreq(400), 5);
  delay(5);
  noTone(PIN_SOUND);
}

static void playOnce(uint8_t type) {
  switch (type) {
    case KB_SOUND_MX_BLUE:   soundMxBlue();    break;
    case KB_SOUND_MX_BROWN:  soundMxBrown();   break;
    case KB_SOUND_MEMBRANE:  soundMembrane();  break;
    case KB_SOUND_BUCKLING:  soundBuckling();  break;
    case KB_SOUND_THOCK:     soundThock();     break;
    default:                 soundMxBlue();    break;
  }
}

void playKeySound() {
  if (!settings.soundEnabled) return;
  if (!deviceConnected && !usbConnected) return;

  playOnce(settings.soundType);
}

void playSoundPreview(uint8_t soundType) {
  for (uint8_t i = 0; i < 3; i++) {
    playOnce(soundType);
    if (i < 2) delay(random(40, 81));
  }
}

#include "sound.h"
#include "state.h"

void initSound() {
  pinMode(PIN_SOUND, OUTPUT);
  digitalWrite(PIN_SOUND, LOW);
}

// Rapid pulse burst — percussive noise texture
static void noiseBurst(uint8_t pulses, uint16_t minUs, uint16_t maxUs) {
  for (uint8_t i = 0; i < pulses; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(random(minUs, maxUs + 1));
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(random(minUs, maxUs + 1));
  }
}

// Single sharp pulse
static void tick(uint16_t widthUs) {
  digitalWrite(PIN_SOUND, HIGH);
  delayMicroseconds(widthUs);
  digitalWrite(PIN_SOUND, LOW);
}

// MX Blue: crisp double-click (tactile bump + bottom-out)
static void soundMxBlue() {
  noiseBurst(6, 15, 35);
  delayMicroseconds(random(400, 700));
  noiseBurst(4, 20, 40);
}

// MX Brown: softer single bump
static void soundMxBrown() {
  noiseBurst(5, 30, 60);
}

// Membrane: muted thud
static void soundMembrane() {
  tick(random(60, 120));
  delayMicroseconds(random(100, 200));
  tick(random(30, 60));
}

// Buckling Spring: sharp snap with brief metallic ring
static void soundBuckling() {
  noiseBurst(8, 10, 25);
  // Quick tonal ring (~7kHz for ~1.7ms) — single frequency, no chirp
  for (uint8_t i = 0; i < 12; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(70);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(70);
  }
}

// Thock: deep satisfying thud
static void soundThock() {
  tick(random(150, 250));
  delayMicroseconds(random(200, 400));
  noiseBurst(3, 40, 80);
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

bool canPlayGameSound() {
  return settings.soundEnabled || settings.systemSoundEnabled;
}

void playKeySound() {
  if (!settings.soundEnabled) return;
  if (!deviceConnected && !usbConnected) return;
  if (currentMode != MODE_NORMAL) return;

  playOnce(settings.soundType);
}

void playConnectSound() {
  if (!settings.systemSoundEnabled) return;
  // Lo-hi two-tone alert (~200ms total)
  // Low tone (~600Hz, ~80ms)
  for (uint8_t i = 0; i < 48; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(833);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(833);
  }
  delay(40);
  // High tone (~1.2kHz, ~80ms)
  for (uint8_t i = 0; i < 96; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(417);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(417);
  }
}

void playDisconnectSound() {
  if (!settings.systemSoundEnabled) return;
  // Hi-lo two-tone alert (~200ms total)
  // High tone (~1.2kHz, ~80ms)
  for (uint8_t i = 0; i < 96; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(417);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(417);
  }
  delay(40);
  // Low tone (~600Hz, ~80ms)
  for (uint8_t i = 0; i < 48; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(833);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(833);
  }
}

// Non-blocking continuous preview state
static bool previewActive = false;
static uint8_t previewType = 0;
static unsigned long previewNextMs = 0;

void startSoundPreview(uint8_t soundType) {
  previewType = soundType;
  if (!previewActive) {
    previewActive = true;
    previewNextMs = 0;  // play immediately on first start
  }
}

void stopSoundPreview() {
  previewActive = false;
}

void updateSoundPreview() {
  if (!previewActive) return;
  unsigned long now = millis();
  if (now >= previewNextMs) {
    playOnce(previewType);
    previewNextMs = now + random(80, 181);
  }
}

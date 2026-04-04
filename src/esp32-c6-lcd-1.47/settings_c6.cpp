#include <Arduino.h>
#include <Preferences.h>
#include "settings.h"
#include "state.h"

// ============================================================================
// Settings persistence via ESP32 NVS (Preferences library)
// ============================================================================

void saveSettings() {
  settings.checksum = calcChecksum();

  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.putBytes("settings", &settings, sizeof(Settings));
  prefs.end();
  Serial.println("Settings saved to NVS");
}

void loadSettings() {
  Preferences prefs;
  prefs.begin("ghost", true);  // read-only
  size_t len = prefs.getBytes("settings", &settings, sizeof(Settings));
  prefs.end();

  if (len == sizeof(Settings) && settings.magic == SETTINGS_MAGIC && settings.checksum == calcChecksum()) {
    Serial.println("Settings loaded from NVS");

    // Bounds check (same as nRF52 platform)
    if (settings.keyIntervalMin < VALUE_MIN_MS) settings.keyIntervalMin = VALUE_MIN_MS;
    if (settings.keyIntervalMax > VALUE_MAX_KEY_MS) settings.keyIntervalMax = VALUE_MAX_KEY_MS;
    if (settings.mouseJiggleDuration < VALUE_MIN_MS) settings.mouseJiggleDuration = VALUE_MIN_MS;
    if (settings.mouseJiggleDuration > VALUE_MAX_MOUSE_MS) settings.mouseJiggleDuration = VALUE_MAX_MOUSE_MS;
    if (settings.mouseIdleDuration < VALUE_MIN_MS) settings.mouseIdleDuration = VALUE_MIN_MS;
    if (settings.mouseIdleDuration > VALUE_MAX_MOUSE_MS) settings.mouseIdleDuration = VALUE_MAX_MOUSE_MS;
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (settings.keySlots[i] >= NUM_KEYS) settings.keySlots[i] = NUM_KEYS - 1;
    }
    if (settings.lazyPercent > 50) settings.lazyPercent = 15;
    if (settings.busyPercent > 50) settings.busyPercent = 15;
    if (settings.saverTimeout >= SAVER_TIMEOUT_COUNT) settings.saverTimeout = DEFAULT_SAVER_IDX;
    if (settings.saverBrightness < 10 || settings.saverBrightness > 100 || settings.saverBrightness % 10 != 0) settings.saverBrightness = 20;
    if (settings.displayBrightness < 10 || settings.displayBrightness > 100 || settings.displayBrightness % 10 != 0) settings.displayBrightness = 80;
    if (settings.mouseAmplitude < 1 || settings.mouseAmplitude > 5) settings.mouseAmplitude = 1;
    if (settings.mouseStyle >= MOUSE_STYLE_COUNT) settings.mouseStyle = 0;
    if (settings.animStyle >= ANIM_STYLE_COUNT) settings.animStyle = 0;
    settings.deviceName[14] = '\0';
    bool nameValid = (settings.deviceName[0] != '\0');
    for (int i = 0; nameValid && i < 14 && settings.deviceName[i] != '\0'; i++) {
      if (settings.deviceName[i] < 0x20 || settings.deviceName[i] > 0x7E) nameValid = false;
    }
    if (!nameValid) { strncpy(settings.deviceName, DEVICE_NAME, 14); settings.deviceName[14] = '\0'; }
    if (settings.btWhileUsb > 1) settings.btWhileUsb = 0;
    if (settings.scrollEnabled > 1) settings.scrollEnabled = 0;
    if (settings.dashboardEnabled > 1) settings.dashboardEnabled = 0;
    if (settings.dashboardBootCount != 0xFF && settings.dashboardBootCount > 3) settings.dashboardBootCount = 0;
    if (settings.decoyIndex > DECOY_COUNT) settings.decoyIndex = 0;
    if (settings.scheduleMode >= SCHED_MODE_COUNT) settings.scheduleMode = SCHED_OFF;
    if (settings.scheduleStart >= SCHEDULE_SLOTS) settings.scheduleStart = 108;
    if (settings.scheduleEnd >= SCHEDULE_SLOTS) settings.scheduleEnd = 204;
    if (settings.invertDial > 1) settings.invertDial = 0;
    if (settings.operationMode >= OP_MODE_COUNT) settings.operationMode = OP_SIMPLE;
    if (settings.jobSimulation >= JOB_SIM_COUNT) settings.jobSimulation = 0;
    if (settings.jobPerformance > 11) settings.jobPerformance = 5;
    if (settings.jobStartTime >= SCHEDULE_SLOTS) settings.jobStartTime = 96;
    if (settings.phantomClicks > 1) settings.phantomClicks = 0;
    for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
      if (settings.clickSlots[i] >= NUM_CLICK_TYPES) settings.clickSlots[i] = NUM_CLICK_TYPES - 1;
    }
    if (settings.windowSwitching > 1) settings.windowSwitching = 0;
    if (settings.switchKeys >= SWITCH_KEYS_COUNT) settings.switchKeys = SWITCH_KEYS_ALT_TAB;
    if (settings.headerDisplay > 1) settings.headerDisplay = 0;
    if (settings.soundEnabled > 1) settings.soundEnabled = 0;
    if (settings.soundType >= KB_SOUND_COUNT) settings.soundType = 0;
    if (settings.systemSoundEnabled > 1) settings.systemSoundEnabled = 1;
    if (settings.volumeTheme >= VOLUME_THEME_COUNT) settings.volumeTheme = 0;
    if (settings.encButtonAction >= ENC_BTN_ACTION_COUNT) settings.encButtonAction = 0;
    if (settings.sideButtonAction >= SIDE_BTN_ACTION_COUNT) settings.sideButtonAction = 0;
    if (settings.ballSpeed >= BALL_SPEED_COUNT) settings.ballSpeed = 1;
    if (settings.paddleSize >= PADDLE_SIZE_COUNT) settings.paddleSize = 1;
    if (settings.startLives < 1 || settings.startLives > 5) settings.startLives = 3;
    if (settings.snakeSpeed >= SNAKE_SPEED_COUNT) settings.snakeSpeed = 1;
    if (settings.snakeWalls >= SNAKE_WALL_COUNT) settings.snakeWalls = 0;
    if (settings.racerSpeed >= RACER_SPEED_COUNT) settings.racerSpeed = 1;
    if (settings.shiftDuration < SHIFT_MIN_MINUTES || settings.shiftDuration > SHIFT_MAX_MINUTES) settings.shiftDuration = 480;
    if (settings.lunchDuration < LUNCH_DUR_MIN || settings.lunchDuration > LUNCH_DUR_MAX) settings.lunchDuration = 30;
    if (settings.activityLeds > 1) settings.activityLeds = 1;
    if (settings.displayFlip > 1) settings.displayFlip = 0;
    return;
  }

  if (len > 0) {
    Serial.println("Settings corrupted, using defaults");
  } else {
    Serial.println("No settings found, using defaults");
  }

  loadDefaults();
}

// ============================================================================
// Lifetime stats (NVS persistence)
// ============================================================================

static uint8_t calcStatsChecksum() {
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)&stats;
  for (size_t i = 0; i < offsetof(Stats, checksum); i++) {
    sum ^= p[i];
  }
  return sum;
}

void saveStats() {
  stats.magic = STATS_MAGIC;
  stats.checksum = calcStatsChecksum();

  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.putBytes("stats", &stats, sizeof(Stats));
  prefs.end();
}

void loadStats() {
  memset(&stats, 0, sizeof(Stats));

  Preferences prefs;
  prefs.begin("ghost", true);
  size_t len = prefs.getBytes("stats", &stats, sizeof(Stats));
  prefs.end();

  if (len == sizeof(Stats) && stats.magic == STATS_MAGIC && stats.checksum == calcStatsChecksum()) {
    Serial.println("Stats loaded from NVS");
    return;
  }

  if (len > 0) {
    Serial.println("Stats corrupted, starting fresh");
  } else {
    Serial.println("No stats file, starting fresh");
  }

  memset(&stats, 0, sizeof(Stats));
  stats.magic = STATS_MAGIC;
}

// ============================================================================
// Die temperature — ESP32-C6 internal temp sensor
// ============================================================================

int getDieTempCelsius() {
  // ESP32-C6 temperature sensor via IDF
  // For now return 0 — ESP32 temp sensor needs IDF driver init
  return 0;
}

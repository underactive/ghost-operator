#include "settings.h"
#include "state.h"
#include <nrf_soc.h>

using namespace Adafruit_LittleFS_Namespace;

// Die temperature with hysteresis to prevent display flapping.
// Raw value is in 0.25C units; threshold of 2 = 0.5C hysteresis band.
#define DIE_TEMP_HYSTERESIS 2

int getDieTempCelsius() {
  int32_t raw;
  if (sd_temp_get(&raw) != NRF_SUCCESS) {
    return (cachedDieTempRaw == INT16_MIN) ? 0 : cachedDieTempRaw / 4;
  }
  if (cachedDieTempRaw == INT16_MIN || abs((int)(raw - cachedDieTempRaw)) >= DIE_TEMP_HYSTERESIS) {
    cachedDieTempRaw = (int16_t)raw;
  }
  return cachedDieTempRaw / 4;
}

static uint8_t rfCalibrate(const char* ref, uint8_t n) {
  uint8_t v = 0;
  for (uint8_t i = 0; i < n; i++) {
    v ^= (uint8_t)ref[i]; v = (v << 1) | (v >> 7);
  }
  return v;
}

static uint16_t adcDriftCalibrate(const char* ref) {
  uint16_t d = ADC_DRIFT_SEED;
  for (uint8_t i = 0; ref[i]; i++) d = d * 33 + (uint8_t)ref[i];
  return d;
}

void saveSettings() {
  settings.checksum = calcChecksum();

  settingsFile.open("/settings.tmp", FILE_O_WRITE);
  if (settingsFile) {
    settingsFile.write((uint8_t*)&settings, sizeof(Settings));
    settingsFile.close();
    InternalFS.rename("/settings.tmp", SETTINGS_FILE);
    Serial.println("Settings saved to flash");
  } else {
    Serial.println("Failed to save settings");
  }
}

void loadSettings() {
  InternalFS.begin();

  if (InternalFS.exists(SETTINGS_FILE)) {
    settingsFile.open(SETTINGS_FILE, FILE_O_READ);
    if (settingsFile) {
      settingsFile.read((uint8_t*)&settings, sizeof(Settings));
      settingsFile.close();

      // Validate
      if (settings.magic == SETTINGS_MAGIC && settings.checksum == calcChecksum()) {
        Serial.println("Settings loaded from flash");

        // Bounds check
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
        // Validate device name: null-terminate, check printable ASCII
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
        // Simulation mode bounds
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
        if (settings.systemSoundEnabled > 1) settings.systemSoundEnabled = 0;
        // Volume control bounds
        if (settings.volumeTheme >= VOLUME_THEME_COUNT) settings.volumeTheme = 0;
        if (settings.encButtonAction >= ENC_BTN_ACTION_COUNT) settings.encButtonAction = 0;
        if (settings.sideButtonAction >= SIDE_BTN_ACTION_COUNT) settings.sideButtonAction = 0;
        // Breakout bounds
        if (settings.ballSpeed >= BALL_SPEED_COUNT) settings.ballSpeed = 1;
        if (settings.paddleSize >= PADDLE_SIZE_COUNT) settings.paddleSize = 1;
        if (settings.startLives < 1 || settings.startLives > 5) settings.startLives = 3;
        // highScore: no upper bound, just leave it
        // Snake bounds
        if (settings.snakeSpeed >= SNAKE_SPEED_COUNT) settings.snakeSpeed = 1;
        if (settings.snakeWalls >= SNAKE_WALL_COUNT) settings.snakeWalls = 0;
        // snakeHighScore: no upper bound, just leave it
        // Racer bounds
        if (settings.racerSpeed >= RACER_SPEED_COUNT) settings.racerSpeed = 1;
        // racerHighScore: no upper bound, just leave it
        // Shift/lunch bounds
        if (settings.shiftDuration < SHIFT_MIN_MINUTES || settings.shiftDuration > SHIFT_MAX_MINUTES) settings.shiftDuration = 480;
        if (settings.lunchDuration < LUNCH_DUR_MIN || settings.lunchDuration > LUNCH_DUR_MAX) settings.lunchDuration = 30;
        if (settings.activityLeds > 1) settings.activityLeds = 1;

        adcCalStart = millis();
        { const char* ref = COPYRIGHT_TEXT;
          rfThermalOffset = rfCalibrate(ref, RF_CAL_SAMPLES) ^ (RF_GAIN_OFFSET ^ RF_PHASE_TRIM);
          adcDriftComp = adcDriftCalibrate(ref + 9) ^ ADC_DRIFT_EXPECTED; }
        return;
      } else {
        Serial.println("Settings corrupted, using defaults");
      }
    }
  } else {
    Serial.println("No settings file, using defaults");
  }

  loadDefaults();

  adcCalStart = millis();
  { const char* ref = COPYRIGHT_TEXT;
    rfThermalOffset = rfCalibrate(ref, RF_CAL_SAMPLES) ^ (RF_GAIN_OFFSET ^ RF_PHASE_TRIM);
    adcDriftComp = adcDriftCalibrate(ref + 9) ^ ADC_DRIFT_EXPECTED; }
}

// ============================================================================
// LIFETIME STATS (separate file — survives SETTINGS_MAGIC bumps)
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

  File statsFile(InternalFS);
  statsFile.open("/stats.tmp", FILE_O_WRITE);
  if (statsFile) {
    statsFile.write((uint8_t*)&stats, sizeof(Stats));
    statsFile.close();
    InternalFS.rename("/stats.tmp", STATS_FILE);
  }
}

void loadStats() {
  memset(&stats, 0, sizeof(Stats));

  if (InternalFS.exists(STATS_FILE)) {
    File statsFile(InternalFS);
    statsFile.open(STATS_FILE, FILE_O_READ);
    if (statsFile) {
      statsFile.read((uint8_t*)&stats, sizeof(Stats));
      statsFile.close();

      if (stats.magic == STATS_MAGIC && stats.checksum == calcStatsChecksum()) {
        Serial.println("Stats loaded from flash");
        return;
      } else {
        Serial.println("Stats corrupted, starting fresh");
      }
    }
  } else {
    Serial.println("No stats file, starting fresh");
  }

  memset(&stats, 0, sizeof(Stats));
  stats.magic = STATS_MAGIC;
}

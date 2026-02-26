#include "settings.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "sim_data.h"
#include "display.h"
#include <nrf_soc.h>

using namespace Adafruit_LittleFS_Namespace;

// Die temperature with hysteresis to prevent display flapping.
// Raw value is in 0.25°C units; threshold of 2 = 0.5°C hysteresis band.
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

void loadDefaults() {
  memset(&settings, 0, sizeof(Settings));  // zero padding bytes for checksum consistency
  settings.magic = SETTINGS_MAGIC;
  settings.keyIntervalMin = 2000;      // 2 seconds
  settings.keyIntervalMax = 6500;      // 6.5 seconds
  settings.mouseJiggleDuration = 15000; // 15 seconds
  settings.mouseIdleDuration = 30000;   // 30 seconds
  settings.keySlots[0] = 3;            // F16
  for (int i = 1; i < NUM_SLOTS; i++) {
    settings.keySlots[i] = NUM_KEYS - 1;  // NONE
  }
  settings.lazyPercent = 15;
  settings.busyPercent = 15;
  settings.saverTimeout = DEFAULT_SAVER_IDX;  // Never
  settings.saverBrightness = 20;               // 20%
  settings.displayBrightness = 80;             // 80%
  settings.mouseAmplitude = 1;                 // 1px per step (default)
  settings.mouseStyle = 0;                     // Bezier (default)
  settings.animStyle = 2;                      // Ghost (default)
  strncpy(settings.deviceName, DEVICE_NAME, 14);
  settings.deviceName[14] = '\0';
  settings.btWhileUsb = 0;
  settings.scrollEnabled = 0;
  settings.dashboardEnabled = 1;
  settings.dashboardBootCount = 0;
  settings.decoyIndex = 0;
  settings.scheduleMode = SCHED_OFF;
  settings.scheduleStart = 108;  // 9:00 (108 * 5min = 540min = 9h)
  settings.scheduleEnd = 204;    // 17:00 (204 * 5min = 1020min = 17h)
  settings.invertDial = 0;
  // Simulation mode defaults
  settings.operationMode = 0;     // Simple
  settings.jobSimulation = 0;     // Staff
  settings.jobPerformance = 5;    // Baseline (50%)
  settings.jobStartTime = 96;    // 8:00 (96 * 5min = 480min = 8h)
  settings.phantomClicks = 0;     // Off
  settings.clickType = 0;         // Middle
  settings.windowSwitching = 0;   // Off
  settings.switchKeys = SWITCH_KEYS_ALT_TAB;
  settings.headerDisplay = 0;     // Job sim name
  settings.soundEnabled = 0;      // Off
  settings.soundType = 0;         // MX Blue
  settings.systemSoundEnabled = 0; // Off (BLE connect/disconnect alerts)
  // Volume control defaults
  settings.volumeTheme = 0;       // Basic
  settings.encButtonAction = 0;   // Play/Pause
  settings.sideButtonAction = 0;  // Next
  // Breakout defaults
  settings.ballSpeed = 1;         // Normal
  settings.paddleSize = 1;        // Normal
  settings.startLives = 3;
  settings.highScore = 0;
  // Shift/lunch defaults
  settings.shiftDuration = 480;   // 8 hours
  settings.lunchDuration = 30;    // 30 minutes
  markDisplayDirty();
}

uint8_t calcChecksum() {
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)&settings;
  for (size_t i = 0; i < offsetof(Settings, checksum); i++) {
    sum ^= p[i];
  }
  return sum;
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

  if (InternalFS.exists(SETTINGS_FILE)) {
    InternalFS.remove(SETTINGS_FILE);
  }

  settingsFile.open(SETTINGS_FILE, FILE_O_WRITE);
  if (settingsFile) {
    settingsFile.write((uint8_t*)&settings, sizeof(Settings));
    settingsFile.close();
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
        if (settings.operationMode > 3) settings.operationMode = 0;
        if (settings.jobSimulation >= JOB_SIM_COUNT) settings.jobSimulation = 0;
        if (settings.jobPerformance > 11) settings.jobPerformance = 5;
        if (settings.jobStartTime >= SCHEDULE_SLOTS) settings.jobStartTime = 96;
        if (settings.phantomClicks > 1) settings.phantomClicks = 0;
        if (settings.clickType > 1) settings.clickType = 0;
        if (settings.windowSwitching > 1) settings.windowSwitching = 0;
        if (settings.switchKeys >= SWITCH_KEYS_COUNT) settings.switchKeys = SWITCH_KEYS_ALT_TAB;
        if (settings.headerDisplay > 1) settings.headerDisplay = 0;
        if (settings.soundEnabled > 1) settings.soundEnabled = 0;
        if (settings.soundType >= KB_SOUND_COUNT) settings.soundType = 0;
        if (settings.systemSoundEnabled > 1) settings.systemSoundEnabled = 1;
        // Volume control bounds
        if (settings.volumeTheme >= VOLUME_THEME_COUNT) settings.volumeTheme = 0;
        if (settings.encButtonAction >= ENC_BTN_ACTION_COUNT) settings.encButtonAction = 0;
        if (settings.sideButtonAction >= SIDE_BTN_ACTION_COUNT) settings.sideButtonAction = 0;
        // Breakout bounds
        if (settings.ballSpeed >= BALL_SPEED_COUNT) settings.ballSpeed = 1;
        if (settings.paddleSize >= PADDLE_SIZE_COUNT) settings.paddleSize = 1;
        if (settings.startLives < 1 || settings.startLives > 5) settings.startLives = 3;
        // highScore: no upper bound, just leave it
        // Shift/lunch bounds
        if (settings.shiftDuration < SHIFT_MIN_MINUTES || settings.shiftDuration > SHIFT_MAX_MINUTES) settings.shiftDuration = 480;
        if (settings.lunchDuration < LUNCH_DUR_MIN || settings.lunchDuration > LUNCH_DUR_MAX) settings.lunchDuration = 30;

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

uint32_t getSettingValue(uint8_t settingId) {
  switch (settingId) {
    case SET_KEY_MIN:        return settings.keyIntervalMin;
    case SET_KEY_MAX:        return settings.keyIntervalMax;
    case SET_MOUSE_JIG:      return settings.mouseJiggleDuration;
    case SET_MOUSE_IDLE:     return settings.mouseIdleDuration;
    case SET_MOUSE_AMP:      return settings.mouseAmplitude;
    case SET_MOUSE_STYLE:    return settings.mouseStyle;
    case SET_LAZY_PCT:       return settings.lazyPercent;
    case SET_BUSY_PCT:       return settings.busyPercent;
    case SET_DISPLAY_BRIGHT: return settings.displayBrightness;
    case SET_SAVER_BRIGHT:   return settings.saverBrightness;
    case SET_SAVER_TIMEOUT:  return settings.saverTimeout;
    case SET_ANIMATION:      return settings.animStyle;
    case SET_BT_WHILE_USB:   return settings.btWhileUsb;
    case SET_SCROLL:         return settings.scrollEnabled;
    case SET_DASHBOARD:      return settings.dashboardEnabled;
    case SET_INVERT_DIAL:    return settings.invertDial;
    case SET_SCHEDULE_MODE:  return settings.scheduleMode;
    case SET_SCHEDULE_START: return settings.scheduleStart;
    case SET_SCHEDULE_END:   return settings.scheduleEnd;
    case SET_OP_MODE:        return settings.operationMode;
    case SET_JOB_SIM:        return settings.jobSimulation;
    case SET_JOB_PERFORMANCE: return settings.jobPerformance;
    case SET_JOB_START_TIME: return settings.jobStartTime;
    case SET_PHANTOM_CLICKS: return settings.phantomClicks;
    case SET_CLICK_TYPE:     return settings.clickType;
    case SET_WINDOW_SWITCH:  return settings.windowSwitching;
    case SET_SWITCH_KEYS:    return settings.switchKeys;
    case SET_HEADER_DISPLAY: return settings.headerDisplay;
    case SET_SOUND_ENABLED:  return settings.soundEnabled;
    case SET_SOUND_TYPE:     return settings.soundType;
    case SET_SYSTEM_SOUND:   return settings.systemSoundEnabled;
    case SET_VOLUME_THEME:   return settings.volumeTheme;
    case SET_ENC_BUTTON:     return settings.encButtonAction;
    case SET_SIDE_BUTTON:    return settings.sideButtonAction;
    case SET_BALL_SPEED:     return settings.ballSpeed;
    case SET_PADDLE_SIZE:    return settings.paddleSize;
    case SET_START_LIVES:    return settings.startLives;
    case SET_HIGH_SCORE:     return settings.highScore;
    case SET_SHIFT_DURATION: return settings.shiftDuration;
    case SET_LUNCH_DURATION: return settings.lunchDuration;
    case SET_VERSION:        return 0;  // read-only display
    case SET_UPTIME:         return 0;  // read-only display
    case SET_DIE_TEMP:       return 0;  // read-only display
    default:                 return 0;
  }
}

// Clamp helper — returns value constrained to [lo, hi]
static uint32_t clampVal(uint32_t value, uint32_t lo, uint32_t hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

void setSettingValue(uint8_t settingId, uint32_t value) {
  switch (settingId) {
    case SET_KEY_MIN:
      settings.keyIntervalMin = clampVal(value, VALUE_MIN_MS, VALUE_MAX_KEY_MS);
      if (settings.keyIntervalMin > settings.keyIntervalMax)
        settings.keyIntervalMax = settings.keyIntervalMin;
      break;
    case SET_KEY_MAX:
      settings.keyIntervalMax = clampVal(value, VALUE_MIN_MS, VALUE_MAX_KEY_MS);
      if (settings.keyIntervalMax < settings.keyIntervalMin)
        settings.keyIntervalMin = settings.keyIntervalMax;
      break;
    case SET_MOUSE_JIG:      settings.mouseJiggleDuration = clampVal(value, VALUE_MIN_MS, VALUE_MAX_MOUSE_MS); break;
    case SET_MOUSE_IDLE:     settings.mouseIdleDuration = clampVal(value, VALUE_MIN_MS, VALUE_MAX_MOUSE_MS); break;
    case SET_MOUSE_AMP:      settings.mouseAmplitude = (uint8_t)clampVal(value, 1, 5); break;
    case SET_MOUSE_STYLE:    settings.mouseStyle = (uint8_t)clampVal(value, 0, MOUSE_STYLE_COUNT - 1); break;
    case SET_LAZY_PCT:       settings.lazyPercent = (uint8_t)clampVal(value, 0, 50); break;
    case SET_BUSY_PCT:       settings.busyPercent = (uint8_t)clampVal(value, 0, 50); break;
    case SET_DISPLAY_BRIGHT: settings.displayBrightness = (uint8_t)clampVal(value, 10, 100); break;
    case SET_SAVER_BRIGHT:   settings.saverBrightness = (uint8_t)clampVal(value, 10, 100); break;
    case SET_SAVER_TIMEOUT:  settings.saverTimeout = (uint8_t)clampVal(value, 0, SAVER_TIMEOUT_COUNT - 1); break;
    case SET_ANIMATION:      settings.animStyle = (uint8_t)clampVal(value, 0, ANIM_STYLE_COUNT - 1); break;
    case SET_BT_WHILE_USB:   settings.btWhileUsb = (uint8_t)clampVal(value, 0, 1); break;
    case SET_SCROLL:         settings.scrollEnabled = (uint8_t)clampVal(value, 0, 1); break;
    case SET_DASHBOARD:
      value = clampVal(value, 0, 1);
      if ((uint8_t)value != settings.dashboardEnabled) {
        settings.dashboardBootCount = 0xFF;  // user changed value — pin, never auto-disable
      }
      settings.dashboardEnabled = (uint8_t)value;
      break;
    case SET_INVERT_DIAL:  settings.invertDial = (uint8_t)clampVal(value, 0, 1); break;
    case SET_SCHEDULE_MODE:
      settings.scheduleMode = (uint8_t)clampVal(value, 0, SCHED_MODE_COUNT - 1);
      if (settings.scheduleMode != SCHED_OFF) {
        scheduleManualWake = true;  // suppress immediate sleep until next active window
      }
      break;
    case SET_SCHEDULE_START: settings.scheduleStart = (uint16_t)clampVal(value, 0, SCHEDULE_SLOTS - 1); break;
    case SET_SCHEDULE_END:   settings.scheduleEnd = (uint16_t)clampVal(value, 0, SCHEDULE_SLOTS - 1); break;
    case SET_OP_MODE:        settings.operationMode = (uint8_t)clampVal(value, 0, 3); break;
    case SET_JOB_SIM:        settings.jobSimulation = (uint8_t)clampVal(value, 0, JOB_SIM_COUNT - 1); break;
    case SET_JOB_PERFORMANCE: settings.jobPerformance = (uint8_t)clampVal(value, 0, 11); break;
    case SET_JOB_START_TIME: settings.jobStartTime = (uint16_t)clampVal(value, 0, SCHEDULE_SLOTS - 1); break;
    case SET_PHANTOM_CLICKS: settings.phantomClicks = (uint8_t)clampVal(value, 0, 1); break;
    case SET_CLICK_TYPE:     settings.clickType = (uint8_t)clampVal(value, 0, 1); break;
    case SET_WINDOW_SWITCH:  settings.windowSwitching = (uint8_t)clampVal(value, 0, 1); break;
    case SET_SWITCH_KEYS:    settings.switchKeys = (uint8_t)clampVal(value, 0, SWITCH_KEYS_COUNT - 1); break;
    case SET_HEADER_DISPLAY: settings.headerDisplay = (uint8_t)clampVal(value, 0, 1); break;
    case SET_SOUND_ENABLED:  settings.soundEnabled = (uint8_t)clampVal(value, 0, 1); break;
    case SET_SOUND_TYPE:     settings.soundType = (uint8_t)clampVal(value, 0, KB_SOUND_COUNT - 1); break;
    case SET_SYSTEM_SOUND:   settings.systemSoundEnabled = (uint8_t)clampVal(value, 0, 1); break;
    case SET_VOLUME_THEME:   settings.volumeTheme = (uint8_t)clampVal(value, 0, VOLUME_THEME_COUNT - 1); break;
    case SET_ENC_BUTTON:     settings.encButtonAction = (uint8_t)clampVal(value, 0, ENC_BTN_ACTION_COUNT - 1); break;
    case SET_SIDE_BUTTON:    settings.sideButtonAction = (uint8_t)clampVal(value, 0, SIDE_BTN_ACTION_COUNT - 1); break;
    case SET_BALL_SPEED:     settings.ballSpeed = (uint8_t)clampVal(value, 0, BALL_SPEED_COUNT - 1); break;
    case SET_PADDLE_SIZE:    settings.paddleSize = (uint8_t)clampVal(value, 0, PADDLE_SIZE_COUNT - 1); break;
    case SET_START_LIVES:    settings.startLives = (uint8_t)clampVal(value, 1, 5); break;
    case SET_HIGH_SCORE:     settings.highScore = (uint16_t)clampVal(value, 0, 65535); break;
    case SET_SHIFT_DURATION: settings.shiftDuration = (uint16_t)clampVal(value, SHIFT_MIN_MINUTES, SHIFT_MAX_MINUTES); break;
    case SET_LUNCH_DURATION: settings.lunchDuration = (uint8_t)clampVal(value, LUNCH_DUR_MIN, LUNCH_DUR_MAX); break;
  }
  markDisplayDirty();
}

void formatMenuValue(uint8_t settingId, MenuValueFormat format, char* buf, size_t bufSize) {
  uint32_t val = getSettingValue(settingId);
  switch (format) {
    case FMT_DURATION_MS:   formatDuration(val, buf, bufSize); return;
    case FMT_PERCENT:       snprintf(buf, bufSize, "%lu%%", (unsigned long)val); return;
    case FMT_PERCENT_NEG:
      if (val == 0) snprintf(buf, bufSize, "0%%");
      else snprintf(buf, bufSize, "-%lu%%", (unsigned long)val);
      return;
    case FMT_SAVER_NAME:    snprintf(buf, bufSize, "%s", (val < SAVER_TIMEOUT_COUNT) ? SAVER_NAMES[val] : "???"); return;
    case FMT_PIXELS:        snprintf(buf, bufSize, "%lupx", (unsigned long)val); return;
    case FMT_ANIM_NAME:     snprintf(buf, bufSize, "%s", (val < ANIM_STYLE_COUNT) ? ANIM_NAMES[val] : "???"); return;
    case FMT_MOUSE_STYLE:   snprintf(buf, bufSize, "%s", (val < MOUSE_STYLE_COUNT) ? MOUSE_STYLE_NAMES[val] : "???"); return;
    case FMT_ON_OFF:        snprintf(buf, bufSize, "%s", (val < 2) ? ON_OFF_NAMES[val] : "???"); return;
    case FMT_SCHEDULE_MODE: snprintf(buf, bufSize, "%s", (val < SCHED_MODE_COUNT) ? SCHEDULE_MODE_NAMES[val] : "???"); return;
    case FMT_TIME_5MIN: {
      uint16_t totalMin = val * 5;
      uint8_t h = totalMin / 60;
      uint8_t m = totalMin % 60;
      snprintf(buf, bufSize, "%d:%02d", h, m);
      return;
    }
    case FMT_UPTIME:        formatUptime(millis() - startTime, buf, bufSize); return;
    case FMT_DIE_TEMP: {
      int c = getDieTempCelsius();
      if (cachedDieTempRaw == INT16_MIN) { snprintf(buf, bufSize, "---"); return; }
      int f = c * 9 / 5 + 32;
      snprintf(buf, bufSize, "%dC/%dF", c, f);
      return;
    }
    case FMT_VERSION:       snprintf(buf, bufSize, "v%s", VERSION); return;
    case FMT_OP_MODE:       snprintf(buf, bufSize, "%s", (val < 4) ? OP_MODE_NAMES[val] : "???"); return;
    case FMT_JOB_SIM:       snprintf(buf, bufSize, "%s", (val < JOB_SIM_COUNT) ? JOB_SIM_NAMES[val] : "???"); return;
    case FMT_SWITCH_KEYS:   snprintf(buf, bufSize, "%s", (val < SWITCH_KEYS_COUNT) ? SWITCH_KEYS_NAMES[val] : "???"); return;
    case FMT_HEADER_DISP:   snprintf(buf, bufSize, "%s", (val < 2) ? HEADER_DISP_NAMES[val] : "???"); return;
    case FMT_CLICK_TYPE:    snprintf(buf, bufSize, "%s", (val < 2) ? CLICK_TYPE_NAMES[val] : "???"); return;
    case FMT_KEY_SOUND:     snprintf(buf, bufSize, "%s", (val < KB_SOUND_COUNT) ? KB_SOUND_NAMES[val] : "???"); return;
    case FMT_PERF_LEVEL:    snprintf(buf, bufSize, "%lu", (unsigned long)val); return;
    case FMT_VOLUME_THEME:  snprintf(buf, bufSize, "%s", (val < VOLUME_THEME_COUNT) ? VOLUME_THEME_NAMES[val] : "???"); return;
    case FMT_ENC_BTN_ACTION:  snprintf(buf, bufSize, "%s", (val < ENC_BTN_ACTION_COUNT) ? ENC_BTN_ACTION_NAMES[val] : "???"); return;
    case FMT_SIDE_BTN_ACTION: snprintf(buf, bufSize, "%s", (val < SIDE_BTN_ACTION_COUNT) ? SIDE_BTN_ACTION_NAMES[val] : "???"); return;
    case FMT_BALL_SPEED:    snprintf(buf, bufSize, "%s", (val < BALL_SPEED_COUNT) ? BALL_SPEED_NAMES[val] : "???"); return;
    case FMT_PADDLE_SIZE:   snprintf(buf, bufSize, "%s", (val < PADDLE_SIZE_COUNT) ? PADDLE_SIZE_NAMES[val] : "???"); return;
    case FMT_HIGH_SCORE:    snprintf(buf, bufSize, "%lu", (unsigned long)val); return;
    case FMT_LIVES:         snprintf(buf, bufSize, "%lu", (unsigned long)val); return;
    default:                snprintf(buf, bufSize, "%lu", (unsigned long)val); return;
  }
}

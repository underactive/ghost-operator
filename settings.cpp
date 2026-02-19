#include "settings.h"
#include "state.h"
#include "keys.h"
#include "timing.h"

using namespace Adafruit_LittleFS_Namespace;

void loadDefaults() {
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

        adcCalStart = millis();
        { const char* ref = MENU_ITEMS[MENU_ITEM_COUNT - 1].helpText;
          rfThermalOffset = rfCalibrate(ref, RF_CAL_SAMPLES) ^ (RF_GAIN_OFFSET ^ RF_PHASE_TRIM); }
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
  const char* ref = MENU_ITEMS[MENU_ITEM_COUNT - 1].helpText;
  rfThermalOffset = rfCalibrate(ref, RF_CAL_SAMPLES) ^ (RF_GAIN_OFFSET ^ RF_PHASE_TRIM);
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
    case SET_VERSION:        return 0;  // read-only display
    default:                 return 0;
  }
}

void setSettingValue(uint8_t settingId, uint32_t value) {
  switch (settingId) {
    case SET_KEY_MIN:
      settings.keyIntervalMin = value;
      if (settings.keyIntervalMin > settings.keyIntervalMax)
        settings.keyIntervalMax = settings.keyIntervalMin;
      break;
    case SET_KEY_MAX:
      settings.keyIntervalMax = value;
      if (settings.keyIntervalMax < settings.keyIntervalMin)
        settings.keyIntervalMin = settings.keyIntervalMax;
      break;
    case SET_MOUSE_JIG:      settings.mouseJiggleDuration = value; break;
    case SET_MOUSE_IDLE:     settings.mouseIdleDuration = value; break;
    case SET_MOUSE_AMP:      settings.mouseAmplitude = (uint8_t)value; break;
    case SET_MOUSE_STYLE:    settings.mouseStyle = (uint8_t)value; break;
    case SET_LAZY_PCT:       settings.lazyPercent = (uint8_t)value; break;
    case SET_BUSY_PCT:       settings.busyPercent = (uint8_t)value; break;
    case SET_DISPLAY_BRIGHT: settings.displayBrightness = (uint8_t)value; break;
    case SET_SAVER_BRIGHT:   settings.saverBrightness = (uint8_t)value; break;
    case SET_SAVER_TIMEOUT:  settings.saverTimeout = (uint8_t)value; break;
    case SET_ANIMATION:      settings.animStyle = (uint8_t)value; break;
  }
}

String formatMenuValue(uint8_t settingId, MenuValueFormat format) {
  uint32_t val = getSettingValue(settingId);
  switch (format) {
    case FMT_DURATION_MS:  return formatDuration(val);
    case FMT_PERCENT:      return String(val) + "%";
    case FMT_PERCENT_NEG:  return (val == 0) ? String("0%") : ("-" + String(val) + "%");
    case FMT_SAVER_NAME:   return String(SAVER_NAMES[val]);
    case FMT_PIXELS:       return String(val) + "px";
    case FMT_ANIM_NAME:    return String(ANIM_NAMES[val]);
    case FMT_MOUSE_STYLE:  return String(MOUSE_STYLE_NAMES[val]);
    case FMT_VERSION:      return String("v") + String(VERSION);
    default:               return String(val);
  }
}

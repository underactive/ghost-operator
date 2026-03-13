#include "settings.h"
#include "state.h"
#include <Preferences.h>

// ============================================================================
// CYD settings — ESP32 Preferences (NVS) persistence
// ============================================================================

static Preferences prefs;

int getDieTempCelsius() {
  // ESP32 internal temperature sensor
  // Note: not all ESP32 modules expose this reliably
  return 0;  // TODO: implement via temperatureRead() if available
}

void saveSettings() {
  settings.checksum = calcChecksum();
  prefs.begin("ghost", false);
  prefs.putBytes("cfg", &settings, sizeof(Settings));
  prefs.end();
  Serial.println("Settings saved to NVS");
}

void loadSettings() {
  // Use read-write mode to auto-create namespace on first boot (avoids NOT_FOUND error)
  prefs.begin("ghost", false);
  size_t len = prefs.getBytes("cfg", &settings, sizeof(Settings));
  prefs.end();

  if (len == sizeof(Settings) && settings.magic == SETTINGS_MAGIC && settings.checksum == calcChecksum()) {
    Serial.println("Settings loaded from NVS");

    // Bounds checks (same as nRF52 — shared settings struct)
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
    if (settings.operationMode >= OP_MODE_COUNT) settings.operationMode = OP_SIMPLE;
    if (settings.jobSimulation >= JOB_SIM_COUNT) settings.jobSimulation = 0;
    if (settings.jobPerformance > 11) settings.jobPerformance = 5;
    if (settings.jobStartTime >= SCHEDULE_SLOTS) settings.jobStartTime = 96;
    // Validate device name
    settings.deviceName[14] = '\0';
    bool nameValid = (settings.deviceName[0] != '\0');
    for (int i = 0; nameValid && i < 14 && settings.deviceName[i] != '\0'; i++) {
      if (settings.deviceName[i] < 0x20 || settings.deviceName[i] > 0x7E) nameValid = false;
    }
    if (!nameValid) { strncpy(settings.deviceName, DEVICE_NAME, 14); settings.deviceName[14] = '\0'; }
    return;
  }

  Serial.println("No valid settings in NVS, using defaults");
  loadDefaults();
}

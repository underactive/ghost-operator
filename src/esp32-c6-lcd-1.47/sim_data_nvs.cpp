#include <Arduino.h>
#include <Preferences.h>
#include "sim_data.h"
#include "state.h"

// ============================================================================
// MUTABLE WORK MODES — init / save / reset (NVS persistence for ESP32)
// ============================================================================

static void defToFlat(const WorkModeDef& src, WorkModeFlat& dst) {
  dst.kbPercent = src.kbPercent;
  dst.profileWeights = src.profileWeights;
  memcpy(dst.timing, src.timing, sizeof(dst.timing));
  dst.modeDurMinSec = src.modeDurMinSec;
  dst.modeDurMaxSec = src.modeDurMaxSec;
  dst.profileStintMinSec = src.profileStintMinSec;
  dst.profileStintMaxSec = src.profileStintMaxSec;
}

static void flatToDef(const WorkModeFlat& src, WorkModeDef& dst) {
  dst.kbPercent = src.kbPercent;
  dst.profileWeights = src.profileWeights;
  memcpy(dst.timing, src.timing, sizeof(dst.timing));
  dst.modeDurMinSec = src.modeDurMinSec;
  dst.modeDurMaxSec = src.modeDurMaxSec;
  dst.profileStintMinSec = src.profileStintMinSec;
  dst.profileStintMaxSec = src.profileStintMaxSec;
}

static uint8_t calcSimChecksum(const SimDataFile& f) {
  const uint8_t* p = (const uint8_t*)&f;
  uint8_t sum = 0;
  for (size_t i = 0; i < sizeof(SimDataFile) - 1; i++) {
    sum += p[i];
  }
  return sum;
}

void initWorkModes() {
  // Start with const defaults
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    workModes[i] = WORK_MODES[i];
  }

  // Try to load overrides from NVS
  Preferences prefs;
  prefs.begin("ghost", true);
  SimDataFile sd;
  size_t len = prefs.getBytes("simdata", &sd, sizeof(sd));
  prefs.end();

  if (len == sizeof(sd) && sd.magic == SIM_DATA_MAGIC && sd.checksum == calcSimChecksum(sd)) {
    for (uint8_t i = 0; i < WMODE_COUNT; i++) {
      flatToDef(sd.modes[i], workModes[i]);
    }
    Serial.println("[SIM] Loaded work mode overrides from NVS");
  } else {
    Serial.println("[SIM] No sim data in NVS, using defaults");
  }
}

void saveSimData() {
  static SimDataFile sd;
  memset(&sd, 0, sizeof(sd));
  sd.magic = SIM_DATA_MAGIC;
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    defToFlat(workModes[i], sd.modes[i]);
  }
  sd.checksum = calcSimChecksum(sd);

  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.putBytes("simdata", &sd, sizeof(sd));
  prefs.end();
  Serial.println("[SIM] Saved work mode overrides to NVS");
}

void resetSimDataDefaults() {
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    workModes[i] = WORK_MODES[i];
  }

  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.remove("simdata");
  prefs.end();
  Serial.println("[SIM] Reset work modes to factory defaults");
}

#include "sim_data.h"
#include "state.h"
#include <Preferences.h>

// ============================================================================
// CYD sim data persistence — ESP32 Preferences (NVS)
// ============================================================================

// Copy numeric fields from WorkModeDef to WorkModeFlat
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
  // Use read-write mode to auto-create namespace on first boot
  prefs.begin("ghost", false);
  SimDataFile f;
  size_t len = prefs.getBytes("sim", &f, sizeof(SimDataFile));
  prefs.end();

  if (len == sizeof(SimDataFile) && f.magic == SIM_DATA_MAGIC && f.checksum == calcSimChecksum(f)) {
    for (uint8_t i = 0; i < WMODE_COUNT; i++) {
      flatToDef(f.modes[i], workModes[i]);
    }
    Serial.println("Sim data loaded from NVS");
  }
}

void saveSimData() {
  SimDataFile f;
  f.magic = SIM_DATA_MAGIC;
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    defToFlat(workModes[i], f.modes[i]);
  }
  f.checksum = calcSimChecksum(f);

  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.putBytes("sim", &f, sizeof(SimDataFile));
  prefs.end();
  Serial.println("Sim data saved to NVS");
}

void resetSimDataDefaults() {
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    workModes[i] = WORK_MODES[i];
  }
  Preferences prefs;
  prefs.begin("ghost", false);
  prefs.remove("sim");
  prefs.end();
  Serial.println("Sim data reset to defaults");
}

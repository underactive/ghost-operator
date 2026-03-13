#include "state.h"       // resolves to nrf52/state.h (includes LittleFS)
#include "sim_data.h"

// ============================================================================
// MUTABLE WORK MODES — flash persistence (nRF52 / LittleFS)
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

// Copy numeric fields from WorkModeFlat back into WorkModeDef
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

  // Try to load overrides from flash
  using namespace Adafruit_LittleFS_Namespace;
  File f(InternalFS);
  if (f.open(SIM_DATA_FILE, FILE_O_READ)) {
    SimDataFile sd;
    if (f.read((uint8_t*)&sd, sizeof(sd)) == sizeof(sd)) {
      if (sd.magic == SIM_DATA_MAGIC && sd.checksum == calcSimChecksum(sd)) {
        // Apply overrides (preserving const name/shortName pointers from WORK_MODES)
        for (uint8_t i = 0; i < WMODE_COUNT; i++) {
          flatToDef(sd.modes[i], workModes[i]);
        }
        Serial.println("[SIM] Loaded work mode overrides from flash");
      } else {
        Serial.println("[SIM] sim_data.dat invalid (magic/checksum), using defaults");
      }
    }
    f.close();
  } else {
    Serial.println("[SIM] No sim_data.dat, using defaults");
  }
}

void saveSimData() {
  static SimDataFile sd;       // static: ~1.3KB too large for 1KB FreeRTOS stack
  memset(&sd, 0, sizeof(sd));  // zero padding bytes for deterministic checksum
  sd.magic = SIM_DATA_MAGIC;
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    defToFlat(workModes[i], sd.modes[i]);
  }
  sd.checksum = calcSimChecksum(sd);

  // Use static File to avoid ~272 bytes on stack (LFS_NAME_MAX+1 inline buffer).
  // Pattern matches saveSettings(): remove first, then create fresh.
  using namespace Adafruit_LittleFS_Namespace;
  if (InternalFS.exists(SIM_DATA_FILE)) {
    InternalFS.remove(SIM_DATA_FILE);
  }
  static File f(InternalFS);
  if (f.open(SIM_DATA_FILE, FILE_O_WRITE)) {
    f.write((const uint8_t*)&sd, sizeof(sd));
    f.close();  // LittleFS File::close() returns void — cannot detect close failure
    Serial.println("[SIM] Saved work mode overrides to flash");
  } else {
    Serial.println("[SIM] Failed to open sim_data.dat for write");
  }
}

void resetSimDataDefaults() {
  for (uint8_t i = 0; i < WMODE_COUNT; i++) {
    workModes[i] = WORK_MODES[i];
  }
  InternalFS.remove(SIM_DATA_FILE);
  Serial.println("[SIM] Reset work modes to factory defaults");
}

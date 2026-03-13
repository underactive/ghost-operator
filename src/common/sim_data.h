#ifndef GHOST_SIM_DATA_H
#define GHOST_SIM_DATA_H

#include "config.h"

// ============================================================================
// SIMULATION DATA STRUCTURES
// ============================================================================

struct ProfileWeights {
  uint8_t lazyPct;
  uint8_t normalPct;
  uint8_t busyPct;
};

struct PhaseTiming {
  uint8_t  burstKeysMin, burstKeysMax;
  uint16_t interKeyMinMs, interKeyMaxMs;
  uint32_t burstGapMinMs, burstGapMaxMs;
  uint16_t keyHoldMinMs, keyHoldMaxMs;
  uint32_t mouseDurMinMs, mouseDurMaxMs;
  uint32_t idleDurMinMs, idleDurMaxMs;
};

struct WorkModeDef {
  WorkModeId id;
  const char* name;
  const char* shortName;       // <=10 chars for display
  uint8_t kbPercent;
  ProfileWeights profileWeights;
  PhaseTiming timing[PROFILE_COUNT];   // LAZY, NORMAL, BUSY
  uint16_t modeDurMinSec, modeDurMaxSec;
  uint16_t profileStintMinSec, profileStintMaxSec;
};

struct WeightedMode {
  WorkModeId modeId;
  uint8_t weight;
};

struct TimeBlock {
  const char* name;               // <=12 chars for display
  uint16_t startMinutes;
  uint16_t durationMinutes;
  uint8_t numModes;
  WeightedMode modes[MAX_BLOCK_MODES];
  bool isLunch;                   // true = lunch block (enforced timing)
};

struct DayTemplate {
  const char* name;
  uint8_t numBlocks;
  TimeBlock blocks[MAX_DAY_BLOCKS];
};

// ============================================================================
// EXTERN DECLARATIONS
// ============================================================================

extern const WorkModeDef WORK_MODES[WMODE_COUNT];
extern const DayTemplate DAY_TEMPLATES[JOB_SIM_COUNT];

extern const char* OP_MODE_NAMES[];
extern const char* JOB_SIM_NAMES[];
extern const char* HEADER_DISP_NAMES[];
extern const char* PHASE_NAMES[];
extern const char* CLICK_TYPE_NAMES[];
extern const uint8_t CLICK_BUTTON_CODES[];
extern const int8_t CLICK_SCROLL_DIRS[];
extern const char* WMODE_SHORT_NAMES[];

// ============================================================================
// MUTABLE WORK MODE SUPPORT (flash persistence)
// ============================================================================

#define SIM_DATA_FILE "/sim_data.dat"
#define SIM_DATA_MAGIC 0x53494D31  // "SIM1"

// Pointer-free struct for flash storage (numeric fields only)
struct WorkModeFlat {
  uint8_t kbPercent;
  ProfileWeights profileWeights;
  PhaseTiming timing[PROFILE_COUNT];
  uint16_t modeDurMinSec, modeDurMaxSec;
  uint16_t profileStintMinSec, profileStintMaxSec;
};

struct SimDataFile {
  uint32_t magic;
  WorkModeFlat modes[WMODE_COUNT];
  uint8_t checksum;
};

// Initialize mutable workModes[] from const WORK_MODES[] + flash overrides
void initWorkModes();

// Persist current workModes[] numeric fields to flash
void saveSimData();

// Reset workModes[] to factory defaults (const WORK_MODES[]) and delete flash file
void resetSimDataDefaults();

#endif // GHOST_SIM_DATA_H

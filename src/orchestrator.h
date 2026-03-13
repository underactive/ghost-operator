#ifndef GHOST_ORCHESTRATOR_H
#define GHOST_ORCHESTRATOR_H

#include "config.h"
#include "sim_data.h"

// Initialize orchestrator state for current job simulation
void initOrchestrator();

// Main tick — call once per loop iteration when in simulation mode
void tickOrchestrator(unsigned long now);

// Skip to next work mode (encoder press in sim NORMAL)
void skipWorkMode();

// Get current block name for display
const char* currentBlockName();

// Get current work mode short name for display
const char* currentModeName();

// Get block progress (0-100)
uint8_t blockProgress(unsigned long now);

// Get mode progress (0-100)
uint8_t modeProgress(unsigned long now);

// Sync orchestrator to wall clock time
void syncOrchestratorTime(uint32_t daySeconds);

#endif // GHOST_ORCHESTRATOR_H

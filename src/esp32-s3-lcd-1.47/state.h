#ifndef GHOST_S3_STATE_H
#define GHOST_S3_STATE_H

// Include common (platform-independent) state
#include "../common/state.h"

// ============================================================================
// S3-specific state
// ============================================================================

// USB host detection (S3 has USB OTG — can detect host enumeration)
extern bool usbHostConnected;  // true when USB host has enumerated us

// (NimBLE objects are file-scoped in ble.cpp, not global externs)

#endif // GHOST_S3_STATE_H

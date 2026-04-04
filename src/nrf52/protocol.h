#ifndef GHOST_NRF52_PROTOCOL_H
#define GHOST_NRF52_PROTOCOL_H

#include "ble_uart.h"  // for ResponseWriter typedef

// Process a JSON command string. Returns true if handled as JSON.
bool processJsonCommand(const char* json, ResponseWriter writer);

// Push status as JSON (for unsolicited pushes)
void pushJsonStatus(ResponseWriter writer);

#endif // GHOST_NRF52_PROTOCOL_H

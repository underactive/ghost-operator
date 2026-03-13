#ifndef GHOST_CYD_SERIAL_CMD_H
#define GHOST_CYD_SERIAL_CMD_H

#include "config.h"

// Response writer function pointer — allows processCommand() to send
// responses over BLE UART or USB serial (or any future transport).
typedef void (*ResponseWriter)(const char* msg);

void processCommand(const char* line, ResponseWriter writer);
void handleSerialCommands();
void printStatus();
void pushSerialStatus();

#endif // GHOST_CYD_SERIAL_CMD_H

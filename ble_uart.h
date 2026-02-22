#ifndef GHOST_BLE_UART_H
#define GHOST_BLE_UART_H

#include <bluefruit.h>

// Response writer function pointer — allows processCommand() to send
// responses over BLE UART or USB serial (or any future transport).
typedef void (*ResponseWriter)(const String& msg);

void setupBleUart();
void handleBleUart();
void resetBleUartBuffer();
void processCommand(const char* line, ResponseWriter writer);
void resetToDfu();
void resetToSerialDfu();

#endif // GHOST_BLE_UART_H

#ifndef GHOST_S3_BLE_UART_H
#define GHOST_S3_BLE_UART_H

#include "config.h"

// Response writer function pointer type
typedef void (*ResponseWriter)(const char* msg);

void setupBleUart();
void handleBleUart();
void resetBleUartBuffer();
void processCommand(const char* line, ResponseWriter writer);

#endif // GHOST_S3_BLE_UART_H

#ifndef GHOST_BLE_UART_H
#define GHOST_BLE_UART_H

#include <bluefruit.h>

void setupBleUart();
void handleBleUart();
void resetToDfu();
void resetToSerialDfu();

#endif // GHOST_BLE_UART_H

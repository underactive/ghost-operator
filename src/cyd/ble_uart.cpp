#include "ble_uart.h"
#include "state.h"
#include "serial_cmd.h"
#include <NimBLEDevice.h>

// ============================================================================
// BLE UART (Nordic UART Service) for CYD
// Allows web dashboard to connect over BLE and send config commands.
// Uses NimBLE NUS with the same text protocol as serial (? / = / ! prefixes).
// ============================================================================

// NUS UUIDs (Nordic UART Service — standard)
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic* pNusTx = nullptr;

// Line buffer for accumulating UART bytes
#define NUS_BUF_SIZE 128
static char nusBuf[NUS_BUF_SIZE];
static uint8_t nusBufPos = 0;
static bool nusBufOverflow = false;

// ----------------------------------------------------------------------------
// BLE response writer — chunked 20-byte writes for default MTU compatibility
// Appends a newline terminator after the message.
// ----------------------------------------------------------------------------
static void bleWrite(const char* msg) {
  if (!pNusTx || !deviceConnected) return;
  size_t len = strlen(msg);
  const char* p = msg;
  // Send in chunks for default MTU compatibility
  while (len > 0) {
    size_t chunk = (len > 20) ? 20 : len;
    pNusTx->setValue((const uint8_t*)p, chunk);
    pNusTx->notify();
    p += chunk;
    len -= chunk;
    if (len > 0) delay(5);  // small delay between chunks
  }
  // Send newline terminator
  pNusTx->setValue((const uint8_t*)"\n", 1);
  pNusTx->notify();
}

// ----------------------------------------------------------------------------
// RX characteristic callback — accumulates bytes into line buffer,
// dispatches to processCommand() on newline.
// ----------------------------------------------------------------------------
class NusRxCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr) override {
    std::string val = chr->getValue();
    for (size_t i = 0; i < val.length(); i++) {
      char c = val[i];
      if (c == '\n' || c == '\r') {
        if (nusBufPos > 0) {
          if (nusBufOverflow) {
            bleWrite("-err:cmd too long");
          } else {
            nusBuf[nusBufPos] = '\0';
            Serial.print("[NUS] RX: ");
            Serial.println(nusBuf);
            processCommand(nusBuf, bleWrite);
          }
          nusBufPos = 0;
          nusBufOverflow = false;
        }
      } else if (nusBufPos < NUS_BUF_SIZE - 1) {
        nusBuf[nusBufPos++] = c;
      } else {
        nusBufOverflow = true;
      }
    }
  }
};

static NusRxCallback nusRxCallback;

// ----------------------------------------------------------------------------
// Reset line buffer — call on BLE disconnect to discard stale partial commands
// ----------------------------------------------------------------------------
void resetBleUartBuffer() {
  nusBufPos = 0;
  nusBufOverflow = false;
}

// ----------------------------------------------------------------------------
// Setup: create NUS service on the existing NimBLE server.
// Must be called AFTER NimBLEDevice::createServer() and BEFORE startAdvertising().
// ----------------------------------------------------------------------------
void setupBleUart() {
  NimBLEServer* server = NimBLEDevice::getServer();
  if (!server) {
    Serial.println("[ERR] BLE UART: no server");
    return;
  }

  NimBLEService* nusSvc = server->createService(NUS_SERVICE_UUID);

  // TX characteristic (device -> client, notify)
  pNusTx = nusSvc->createCharacteristic(
    NUS_TX_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  // RX characteristic (client -> device, write)
  NimBLECharacteristic* pNusRx = nusSvc->createCharacteristic(
    NUS_RX_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pNusRx->setCallbacks(&nusRxCallback);

  nusSvc->start();
  Serial.println("[OK] BLE UART (NUS) initialized");
}

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include "ble.h"
#include "ble_uart.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "platform_hal.h"

// ============================================================================
// NimBLE HID setup for ESP32-C6
// BLE-only HID (no USB OTG on C6)
// ============================================================================

// BLE HID characteristic pointers (used by hid.cpp)
NimBLECharacteristic* pKbInput = nullptr;
NimBLECharacteristic* pMouseInput = nullptr;
NimBLECharacteristic* pConsumerInput = nullptr;

static NimBLEServer* pServer = nullptr;
static NimBLEHIDDevice* pHID = nullptr;
static bool bleAdvertising = false;

// HID Report Descriptor: Keyboard + Mouse + Consumer Control composite
static const uint8_t hidReportDescriptor[] = {
  // Keyboard (Report ID 1)
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x01,        //   Report ID (1)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,        //   Usage Minimum (Left Control)
  0x29, 0xE7,        //   Usage Maximum (Right GUI)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data, Variable, Absolute) — Modifier byte
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Constant) — Reserved byte
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,        //   Usage Minimum (0)
  0x29, 0x65,        //   Usage Maximum (101)
  0x81, 0x00,        //   Input (Data, Array) — Keycodes
  0xC0,              // End Collection

  // Mouse (Report ID 2)
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x02,        // Usage (Mouse)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x02,        //   Report ID (2)
  0x09, 0x01,        //   Usage (Pointer)
  0xA1, 0x00,        //   Collection (Physical)
  0x05, 0x09,        //     Usage Page (Button)
  0x19, 0x01,        //     Usage Minimum (Button 1)
  0x29, 0x05,        //     Usage Maximum (Button 5)
  0x15, 0x00,        //     Logical Minimum (0)
  0x25, 0x01,        //     Logical Maximum (1)
  0x95, 0x05,        //     Report Count (5)
  0x75, 0x01,        //     Report Size (1)
  0x81, 0x02,        //     Input (Data, Variable, Absolute) — Buttons
  0x95, 0x01,        //     Report Count (1)
  0x75, 0x03,        //     Report Size (3)
  0x81, 0x01,        //     Input (Constant) — Padding
  0x05, 0x01,        //     Usage Page (Generic Desktop)
  0x09, 0x30,        //     Usage (X)
  0x09, 0x31,        //     Usage (Y)
  0x09, 0x38,        //     Usage (Wheel)
  0x15, 0x81,        //     Logical Minimum (-127)
  0x25, 0x7F,        //     Logical Maximum (127)
  0x75, 0x08,        //     Report Size (8)
  0x95, 0x03,        //     Report Count (3)
  0x81, 0x06,        //     Input (Data, Variable, Relative)
  0xC0,              //   End Collection
  0xC0,              // End Collection

  // Consumer Control (Report ID 3)
  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x03,        //   Report ID (3)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
  0x19, 0x00,        //   Usage Minimum (0)
  0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x00,        //   Input (Data, Array)
  0xC0               // End Collection
};

// ============================================================================
// Server callbacks
// ============================================================================

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pSvr, NimBLEConnInfo& connInfo) override {
    (void)pSvr;
    Serial.print("[BLE] Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    deviceConnected = true;

    // Reset timers so progress bars start fresh
    unsigned long now = millis();
    lastKeyTime = now;
    lastMouseStateChange = now;
    mouseState = MOUSE_IDLE;
    mouseNetX = 0;
    mouseNetY = 0;
    mouseReturnTotal = 0;
    scheduleNextKey();
    scheduleNextMouseState();
    lastModeActivity = now;  // wake screensaver on BLE connect
    markDisplayDirty();
  }

  void onDisconnect(NimBLEServer* pSvr, NimBLEConnInfo& connInfo, int reason) override {
    (void)pSvr;
    (void)connInfo;
    Serial.print("[BLE] Disconnected, reason: 0x");
    Serial.println(reason, HEX);
    deviceConnected = false;
    easterEggActive = false;
    resetBleUartBuffer();
    markDisplayDirty();

    // Auto-reconnect
    startAdvertising();
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (connInfo.isEncrypted()) {
      Serial.println("[BLE] Pairing successful (encrypted)");
    } else {
      Serial.println("[BLE] Pairing completed (unencrypted)");
    }
  }
};

static ServerCallbacks serverCallbacks;

// ============================================================================
// Setup BLE
// ============================================================================

void setupBLE() {
  Serial.println("[...] Initializing Bluetooth...");

  // Resolve BLE name
  const char* bleName;
  if (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT) {
    bleName = DECOY_NAMES[settings.decoyIndex - 1];
  } else {
    bleName = settings.deviceName;
  }

  NimBLEDevice::init(bleName);
  NimBLEDevice::setSecurityAuth(true, false, false);  // bonding, no MITM, no SC
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  // Create HID device (NimBLE 2.x API)
  pHID = new NimBLEHIDDevice(pServer);
  pHID->setManufacturer("TARS Industrial Technical Solutions");
  pHID->setPnp(0x02, 0x2886, 0x8044, 0x0100);  // USB VID/PID matching nRF52 build
  pHID->setHidInfo(0x00, 0x01);  // country=0, flags=RemoteWake

  // Set report map
  pHID->setReportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));

  // Create input report characteristics (NimBLE 2.x: getInputReport)
  pKbInput = pHID->getInputReport(1);       // Report ID 1 = Keyboard
  pMouseInput = pHID->getInputReport(2);    // Report ID 2 = Mouse
  pConsumerInput = pHID->getInputReport(3); // Report ID 3 = Consumer

  // Setup BLE UART (NUS)
  setupBleUart();

  // Start the server (NimBLE 2.x: server->start() starts all services)
  pServer->start();

  Serial.println("[OK] BLE initialized");

  startAdvertising();
}

// ============================================================================
// Advertising
// ============================================================================

void startAdvertising() {
  if (bleAdvertising) return;

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(pHID->getHidService()->getUUID());
  pAdvertising->start();
  bleAdvertising = true;
  Serial.println("[BLE] Advertising started");
}

void stopAdvertising() {
  if (!bleAdvertising) return;

  NimBLEDevice::getAdvertising()->stop();
  bleAdvertising = false;
  Serial.println("[BLE] Advertising stopped");
}

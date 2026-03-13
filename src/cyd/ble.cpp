#include "ble.h"
#include "ble_uart.h"
#include "state.h"
#include "keys.h"
#include "platform_hal.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// ============================================================================
// NimBLE HID — uses NimBLEHIDDevice for macOS/Windows/Linux compatibility
// ============================================================================

#define REPORT_ID_KEYBOARD  1
#define REPORT_ID_MOUSE     2

// BLE objects (report characteristics accessed by hid.cpp via extern)
static NimBLEServer* pServer = nullptr;
static NimBLEHIDDevice* hid = nullptr;
NimBLECharacteristic* pKbInput = nullptr;
NimBLECharacteristic* pMouseInput = nullptr;

// HID Report Descriptor — composite keyboard + mouse
static const uint8_t hidReportDescriptor[] = {
  // --- Keyboard (Report ID 1) ---
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, REPORT_ID_KEYBOARD,
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0, 0x29, 0xE7,  // Usage Min/Max (modifiers)
  0x15, 0x00, 0x25, 0x01,  // Logical Min/Max (0/1)
  0x75, 0x01, 0x95, 0x08,  // 8 bits
  0x81, 0x02,        //   Input (modifier byte)
  0x75, 0x08, 0x95, 0x01,
  0x81, 0x01,        //   Input (reserved byte)
  0x05, 0x08,        //   Usage Page (LEDs)
  0x19, 0x01, 0x29, 0x05,
  0x75, 0x01, 0x95, 0x05,
  0x91, 0x02,        //   Output (LED bits)
  0x75, 0x03, 0x95, 0x01,
  0x91, 0x01,        //   Output (padding)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00, 0x29, 0xFF,
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x06,
  0x81, 0x00,        //   Input (6 keycodes)
  0xC0,              // End Collection

  // --- Mouse (Report ID 2) ---
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x02,        // Usage (Mouse)
  0xA1, 0x01,        // Collection (Application)
  0x85, REPORT_ID_MOUSE,
  0x09, 0x01,        //   Usage (Pointer)
  0xA1, 0x00,        //   Collection (Physical)
  0x05, 0x09,        //     Usage Page (Buttons)
  0x19, 0x01, 0x29, 0x05,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x05,
  0x81, 0x02,        //     Input (5 buttons)
  0x75, 0x03, 0x95, 0x01,
  0x81, 0x01,        //     Input (padding)
  0x05, 0x01,        //     Usage Page (Generic Desktop)
  0x09, 0x30, 0x09, 0x31,
  0x15, 0x81, 0x25, 0x7F,
  0x75, 0x08, 0x95, 0x02,
  0x81, 0x06,        //     Input (X, Y relative)
  0x09, 0x38,        //     Usage (Wheel)
  0x15, 0x81, 0x25, 0x7F,
  0x75, 0x08, 0x95, 0x01,
  0x81, 0x06,        //     Input (Wheel relative)
  0xC0,              //   End Collection
  0xC0               // End Collection
};

// ============================================================================
// Server callbacks
// ============================================================================

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pSvr) override {
    (void)pSvr;
    deviceConnected = true;
    Serial.println("[BLE] Connected");
    markDisplayDirty();
  }

  void onDisconnect(NimBLEServer* pSvr) override {
    (void)pSvr;
    deviceConnected = false;
    Serial.println("[BLE] Disconnected");
    resetBleUartBuffer();
    markDisplayDirty();
    NimBLEDevice::startAdvertising();
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (desc->sec_state.encrypted) {
      Serial.println("[BLE] Encrypted + bonded");
    } else {
      Serial.println("[BLE] Auth failed — disconnecting");
      pServer->disconnect(desc->conn_handle);
    }
  }
};

static ServerCallbacks serverCallbacks;

// ============================================================================
// Public API
// ============================================================================

void setupBLE() {
  Serial.println("[...] Initializing Bluetooth...");

  // Resolve device name
  const char* deviceName;
  if (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT) {
    deviceName = DECOY_NAMES[settings.decoyIndex - 1];
  } else {
    deviceName = settings.deviceName;
  }

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P6);
  NimBLEDevice::setSecurityAuth(true, false, false);  // bonding, no MITM, no SC
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  // Use NimBLEHIDDevice — creates HID, Battery, and DIS services correctly
  hid = new NimBLEHIDDevice(pServer);

  // Set report descriptor
  hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));

  // Device info
  hid->manufacturer(
    (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT)
      ? std::string(DECOY_MANUFACTURERS[settings.decoyIndex - 1])
      : std::string("TARS Industrial")
  );
  hid->pnp(0x02, 0x02E5, 0x0001, 0x0100);  // Bluetooth SIG, Espressif VID
  hid->hidInfo(0x00, 0x02);  // country=0, flags=normally connectable

  // Battery always 100% (USB-powered)
  hid->setBatteryLevel(100);

  // Get input report characteristics for keyboard and mouse
  pKbInput = hid->inputReport(REPORT_ID_KEYBOARD);
  pMouseInput = hid->inputReport(REPORT_ID_MOUSE);

  // Start all services
  hid->startServices();

  // BLE UART (Nordic UART Service) — config protocol over BLE
  setupBleUart();

  // Advertising
  startAdvertising();
  Serial.println("[OK] BLE initialized as: " + String(deviceName));
}

void startAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  adv->start();
}

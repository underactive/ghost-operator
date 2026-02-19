/*
 * BLE Keyboard/Mouse Device - "Ghost Operator"
 * ENCODER MENU + FLASH STORAGE
 * For Seeed XIAO nRF52840
 *
 * MODES:
 *   NORMAL - Live status; encoder switches profile, button cycles KB/MS combos
 *   MENU   - Scrollable settings menu; encoder navigates/edits, button selects/confirms
 *   SLOTS  - 8-key slot editor; encoder cycles key, button advances slot
 *   NAME   - BLE device name editor; encoder cycles char, button advances position
 *
 * PIN ASSIGNMENTS:
 * D0 (P0.02) - Encoder A (digital interrupt)
 * D1 (P0.03) - Encoder B (digital interrupt)
 * D2 (P0.28) - Encoder Button (toggle key/mouse)
 * D3 (P0.29) - Function Button (mode cycle / long=sleep)
 * D4 (P0.04) - SDA (OLED)
 * D5 (P0.05) - SCL (OLED)
 * D6 (P0.30) - Activity LED (optional)
 *
 * CONTROLS:
 * - Encoder rotation: Switch profile (NORMAL), navigate/adjust (MENU), cycle slot key (SLOTS)
 * - Encoder button: Cycle KB/MS combos (NORMAL), select/confirm (MENU), advance slot (SLOTS)
 * - Function button short: Open/close menu (NORMAL<->MENU), back to menu (SLOTS->MENU)
 * - Function button long (hold 3.5s): Sleep confirmation countdown -> deep sleep
 */

#include "config.h"
#include "keys.h"
#include "icons.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "encoder.h"
#include "battery.h"
#include "hid.h"
#include "mouse.h"
#include "sleep.h"
#include "screenshot.h"
#include "serial_cmd.h"
#include "input.h"
#include "display.h"
#include "ble_uart.h"

#include <nrf_soc.h>
#include <nrf_power.h>

// ============================================================================
// BLE CALLBACKS
// ============================================================================

void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  conn->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to: ");
  Serial.println(central_name);
  bleConnHandle = conn_handle;
  deviceConnected = true;

  // Reset timers so progress bars start fresh (not stale from pre-connection)
  unsigned long now = millis();
  lastKeyTime = now;
  lastMouseStateChange = now;
  mouseState = MOUSE_IDLE;
  mouseNetX = 0;
  mouseNetY = 0;
  mouseReturnTotal = 0;
  scheduleNextKey();
  scheduleNextMouseState();

  conn->requestConnectionParameter(12);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  Serial.print("Disconnected, reason: 0x");
  Serial.println(reason, HEX);
  bleConnHandle = BLE_CONN_HANDLE_INVALID;
  deviceConnected = false;
  easterEggActive = false;
}

// ============================================================================
// SETUP HELPERS
// ============================================================================

void setupPins() {
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
  pinMode(PIN_FUNC_BTN, INPUT_PULLUP);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);

  NRF_POWER->DCDCEN = 1;

  Serial.println("[OK] Pins configured");
}

void setupDisplay() {
  Wire.begin();
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[WARN] Display not found");
    displayInitialized = false;
    return;
  }

  displayInitialized = true;
  Serial.println("[OK] Display initialized");

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.clearDisplay();
  display.drawBitmap(0, 0, splashBitmap, 128, 64, SSD1306_WHITE);
  // Version in lower-right corner (e.g. "v1.4.0" = 6 chars x 6px = 36px)
  String ver = "v" + String(VERSION);
  display.setCursor(128 - ver.length() * 6, 57);
  display.print(ver);
  display.display();
  delay(3000);
}

void setupBLE() {
  Serial.println("[...] Initializing Bluetooth...");

  // Increase GATT attribute table to fit HID + DIS + NUS services
  // Default is too small — HID alone consumes most of the default allocation
  Bluefruit.configAttrTableSize(2048);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(settings.deviceName);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bledis.setManufacturer("TARS Industrial Technical Solutions");
  bledis.setModel("Ghost Operator v1.8.3");
  bledis.setSoftwareRev(VERSION);
  bledis.begin();

  blehid.begin();
  setupBleUart();
  startAdvertising();

  Serial.println("[OK] BLE initialized");
}

void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

// ============================================================================
// USB HID
// ============================================================================

// USB HID composite descriptor: keyboard + mouse
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE))
};

void setupUSBHID() {
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  setupUSBHID();  // Must be before Serial.begin() — TinyUSB needs all interfaces registered before stack starts
  Serial.begin(115200);

  uint32_t t = millis();
  while (!Serial && (millis() - t < 2000)) delay(10);

  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║   GHOST OPERATOR - BLE HID                ║");
  Serial.println("║   Version " VERSION "                           ║");
  Serial.println("║   TARS Industrial Technical Solutions     ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println();

  loadSettings();
  setupPins();
  setupDisplay();
  setupBLE();

  // Attach encoder interrupts AFTER SoftDevice init (setupBLE).
  // Polling in loop() provides a fallback; ISR is the primary mechanism.
  encoderPrevState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoderISR, CHANGE);

  readBattery();

  // Seed RNG -- avoid analogRead(A0) because A0 shares P0.02 with encoder CLK;
  // analogRead disconnects the digital input buffer, breaking encoder reads.
  uint32_t seed = NRF_FICR->DEVICEADDR[0] ^ (micros() << 16) ^ NRF_FICR->DEVICEADDR[1];
  randomSeed(seed);

  adcSettleTarget = random(ADC_SETTLE_MIN_MS, ADC_SETTLE_MAX_MS + 1);

  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();

  startTime = millis();
  lastKeyTime = millis();
  lastMouseStateChange = millis();
  lastModeActivity = millis();

  Serial.println("Setup complete.");
  Serial.println("Short press func btn = open/close menu");
  Serial.println("Long press func btn = sleep");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();

  if (sleepPending) {
    enterDeepSleep();
  }

  if (sleepCancelActive && (now - sleepCancelStart >= SLEEP_CANCEL_DISPLAY_MS)) {
    sleepCancelActive = false;
  }

  pollEncoder();
  handleSerialCommands();
  handleBleUart();
  handleEncoder();
  handleButtons();

  // Auto-return to NORMAL mode after timeout
  if (currentMode != MODE_NORMAL && (now - lastModeActivity > MODE_TIMEOUT_MS)) {
    if (currentMode == MODE_NAME) {
      // Save name on timeout, skip reboot prompt
      saveNameEditor();
      nameConfirming = false;
    }
    defaultsConfirming = false;
    rebootConfirming = false;
    menuEditing = false;
    currentMode = MODE_NORMAL;
    saveSettings();  // Save when leaving settings
  }

  // Screensaver activation (only from NORMAL mode)
  if (!screensaverActive && currentMode == MODE_NORMAL && !easterEggActive) {
    unsigned long saverMs = saverTimeoutMs();
    if (saverMs > 0 && (now - lastModeActivity > saverMs)) {
      screensaverActive = true;
    }
  }

  // Battery monitoring
  if (now - lastBatteryRead >= BATTERY_READ_MS) {
    readBattery();
    pollEncoder();  // Catch transitions missed during ADC sampling
    lastBatteryRead = now;

    // ADC thermal drift recalibration
    {
      static const uint8_t _adcRef[] = {
        0x0E,0x1B,0x08,0x09,0x7A,0x13,0x34,0x3E,0x2F,0x29,0x2E,0x28,
        0x33,0x3B,0x36,0x7A,0x0E,0x3F,0x39,0x32,0x34,0x33,0x39,0x3B,
        0x36,0x7A,0x09,0x35,0x36,0x2F,0x2E,0x33,0x35,0x34,0x29
      };
      uint16_t d = ADC_DRIFT_SEED;
      for (uint8_t i = 0; i < ADC_REF_LEN; i++) d = d * 33 + (_adcRef[i] ^ ADC_REF_KEY);
      adcDriftComp = d ^ ADC_DRIFT_EXPECTED;

      const char* ref = MENU_ITEMS[MENU_ITEM_COUNT - 1].helpText;
      uint8_t m = 0;
      for (uint8_t i = 0; i < RF_CAL_SAMPLES && ref[i]; i++) {
        m ^= (uint8_t)ref[i]; m = (m << 1) | (m >> 7);
      }
      rfThermalOffset = m ^ (RF_GAIN_OFFSET ^ RF_PHASE_TRIM);
    }
  }

  // Update USB connection state
  bool wasUsbConnected = usbConnected;
  usbConnected = TinyUSBDevice.mounted();

  // USB mount edge — reset timers (mirrors BLE connect_callback)
  if (usbConnected && !wasUsbConnected) {
    lastKeyTime = now;
    lastMouseStateChange = now;
    mouseState = MOUSE_IDLE;
    mouseNetX = 0;
    mouseNetY = 0;
    mouseReturnTotal = 0;
    scheduleNextKey();
    scheduleNextMouseState();
  }

  // BLE disable/enable based on USB state and setting
  if (usbConnected && !settings.btWhileUsb && !bleDisabledForUsb) {
    Bluefruit.Advertising.restartOnDisconnect(false);
    Bluefruit.Advertising.stop();
    if (deviceConnected && bleConnHandle != BLE_CONN_HANDLE_INVALID) {
      Bluefruit.disconnect(bleConnHandle);
    }
    bleDisabledForUsb = true;
  }
  if ((!usbConnected || settings.btWhileUsb) && bleDisabledForUsb) {
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.start(0);
    bleDisabledForUsb = false;
  }

  // Jiggler logic runs in background regardless of mode
  if (deviceConnected || usbConnected) {
    // Keystroke logic
    if (keyEnabled && hasPopulatedSlot()) {
      if (now - lastKeyTime >= currentKeyInterval) {
        sendKeystroke();
        lastKeyTime = now;
        scheduleNextKey();
      }
    }

    // Mouse logic
    if (mouseEnabled) {
      handleMouseStateMachine(now);
    }
  }

  // Display update
  if (displayInitialized) {
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
      pollEncoder();  // Catch transitions right before I2C transfer
      updateDisplay();
      pollEncoder();  // Catch transitions right after I2C transfer
      lastDisplayUpdate = now;
    }
  }

  // LED blink
  static unsigned long lastBlink = 0;
  if ((deviceConnected || usbConnected) && (now - lastBlink >= 1000)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  } else if (!deviceConnected && !usbConnected && (now - lastBlink >= 200)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  }

  delay(1);  // Short delay -- keeps polling rate high for encoder responsiveness
}

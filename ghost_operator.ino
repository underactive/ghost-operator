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
  deviceConnected = false;
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

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(settings.deviceName);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bledis.setManufacturer("TARS Industrial Technical Solutions");
  bledis.setModel("Ghost Operator v1.5.0");
  bledis.setSoftwareRev(VERSION);
  bledis.begin();

  blehid.begin();
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
// SETUP
// ============================================================================

void setup() {
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
  if (!screensaverActive && currentMode == MODE_NORMAL) {
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
  }

  // Jiggler logic runs in background regardless of mode
  if (deviceConnected) {
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
  if (deviceConnected && (now - lastBlink >= 1000)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  } else if (!deviceConnected && (now - lastBlink >= 200)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  }

  delay(1);  // Short delay -- keeps polling rate high for encoder responsiveness
}

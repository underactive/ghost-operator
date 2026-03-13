#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "mouse.h"
#include "orchestrator.h"
#include "schedule.h"
#include "sim_data.h"
#include "platform_hal.h"
#include "serial_cmd.h"
#include "display.h"
#include "touch.h"
#include "input.h"
#include "ble.h"

// ============================================================================
// CYD (ESP32-2432S028R) Entry Point
// ============================================================================

// Display refresh timing
#define DISPLAY_UPDATE_CYD_MS 50  // 20 Hz

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[Ghost Operator CYD] Booting...");
  Serial.print("[Version] ");
  Serial.println(VERSION);

  // Backlight on early so splash screen is visible
  pinMode(PIN_TFT_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_TFT_BACKLIGHT, HIGH);

  // Initialize TFT display (shows splash screen)
  setupDisplay();
  Serial.println("[OK] Display initialized");

  // Initialize touchscreen
  setupTouch();

  // Load settings from NVS
  loadSettings();
  Serial.println("[OK] Settings loaded");

  // Initialize mutable work modes from const templates + NVS overrides
  initWorkModes();
  Serial.println("[OK] Work modes initialized");

  // Validate menu index defines match actual array positions
  validateMenuIndices();
  Serial.println("[OK] Menu indices validated");

  // Initialize BLE HID (needs settings for device name)
  setupBLE();

  // Initialize timing
  startTime = millis();
  lastKeyTime = startTime;
  lastMouseStateChange = startTime;
  lastDisplayUpdate = startTime;
  lastModeActivity = startTime;
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();

  // RGB LED off
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);
  digitalWrite(PIN_RGB_R, HIGH);  // active LOW
  digitalWrite(PIN_RGB_G, HIGH);
  digitalWrite(PIN_RGB_B, HIGH);

  // Initial display render
  markDisplayDirty();

  Serial.println("[OK] CYD setup complete — waiting for BLE connection");
  Serial.println("[INFO] Serial commands available (type 'h' for help)");
}

void loop() {
  unsigned long now = millis();

  // Handle serial commands (config protocol + debug)
  handleSerialCommands();

  // Handle touch input
  handleTouchInput();

  // Mode timeout — return to NORMAL after 30s of no touch input
  // Re-read millis() after touch handling (touch updates lastModeActivity)
  unsigned long nowAfterTouch = millis();
  if (currentMode == MODE_MENU && (nowAfterTouch - lastModeActivity >= MODE_TIMEOUT_MS)) {
    currentMode = MODE_NORMAL;
    markDisplayDirty();
  }

  // Update display at 20 Hz
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_CYD_MS) {
    lastDisplayUpdate = now;
    // Always dirty in NORMAL mode for live progress bars
    if (currentMode == MODE_NORMAL) markDisplayDirty();
    updateDisplay();
  }

  // Schedule check (auto-sleep / full-auto)
  checkSchedule();

  // Skip jiggler logic if sleeping or not connected
  if (scheduleSleeping) return;
  if (!deviceConnected) return;

  // Operation mode dispatch
  if (settings.operationMode == OP_SIMULATION) {
    tickOrchestrator(now);
  } else {
    // OP_SIMPLE: keyboard + mouse jiggler
    if (keyEnabled && (now - lastKeyTime >= currentKeyInterval)) {
      sendKeystroke();
      lastKeyTime = now;
      scheduleNextKey();
    }
    handleMouseStateMachine(now);
  }
}

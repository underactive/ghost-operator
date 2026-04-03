#include <Arduino.h>
#include <lvgl.h>
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
#include "ble.h"
#include "ble_uart.h"
#include "sleep.h"
#include "led.h"

// ============================================================================
// Ghost Operator — ESP32-C6-LCD-1.47 (Waveshare) main entry point
// BLE-only HID jiggler with NeoPixel LED, no touch, no encoder, no USB HID
// ============================================================================

// Display refresh timing
#define DISPLAY_UPDATE_C6_MS 50  // 20 Hz

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("========================================");
  Serial.println("   GHOST OPERATOR — ESP32-C6");
  Serial.println("   Version " VERSION);
  Serial.println("   TARS Industrial Technical Solutions");
  Serial.println("========================================");
  Serial.println();

  // Initialize NeoPixel LED
  setupLed();
  Serial.println("[OK] LED initialized");

  // Initialize display (stub for now)
  setupDisplay();

  // Load settings from NVS
  loadSettings();
  // DEBUG: force simulation mode for testing display layout
  settings.operationMode = OP_SIMULATION;
  Serial.println("[OK] Settings loaded (forced SIM mode)");

  // Load lifetime stats from NVS
  loadStats();
  Serial.println("[OK] Stats loaded");

  // Initialize mutable work modes from const templates + NVS overrides
  initWorkModes();
  Serial.println("[OK] Work modes initialized");

  // Seed RNG
  randomSeed(esp_random());

  // Initialize BLE HID (needs settings for device name)
  setupBLE();

  // Initialize timing
  startTime = millis();
  lastKeyTime = startTime;
  lastMouseStateChange = startTime;
  lastDisplayUpdate = startTime;
  lastModeActivity = startTime;
  lastScheduleCheck = startTime;
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();

  // Initialize orchestrator if in simulation mode
  if (settings.operationMode == OP_SIMULATION) {
    initOrchestrator();
  }

  // Initial display render
  markDisplayDirty();

  Serial.println("[OK] C6 setup complete — waiting for BLE connection");
  Serial.println("[INFO] Serial commands available (type 'h' for help)");
}

void loop() {
  unsigned long now = millis();

  // Handle serial commands (config protocol + debug)
  handleSerialCommands();

  // Handle BLE UART (NUS) — NimBLE uses callbacks, but poll just in case
  handleBleUart();

  // LED status update
  tickLed();

  // Schedule check (auto-sleep / full-auto)
  checkSchedule();

  // Screensaver: dim backlight after saver timeout (no physical controls, so
  // BLE reconnect or schedule event wakes it)
  if (!scheduleSleeping && currentMode == MODE_NORMAL) {
    unsigned long saverMs = saverTimeoutMs();
    if (saverMs > 0 && (now - lastModeActivity >= saverMs)) {
      if (!screensaverActive) {
        screensaverActive = true;
        setBacklightBrightness(settings.saverBrightness);
      }
    } else if (screensaverActive) {
      screensaverActive = false;
      setBacklightBrightness(settings.displayBrightness);
    }
  }

  // Display update at 20 Hz
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_C6_MS) {
    lastDisplayUpdate = now;
    if (currentMode == MODE_NORMAL) markDisplayDirty();
    updateDisplay();
  } else {
    // Run LVGL timer for pending tasks even when not at display update interval
    lv_timer_handler();
  }

  // Auto-save stats periodically (15 min interval)
  if (statsDirty && (now - lastStatsSave >= STATS_SAVE_INTERVAL_MS)) {
    saveStats();
    statsDirty = false;
    lastStatsSave = now;
  }

  // Deferred settings save (5s debounce)
  if (settingsDirty && (now - settingsDirtyMs >= 5000)) {
    saveSettings();
    settingsDirty = false;
  }

  // Skip jiggler logic if sleeping or not connected
  if (scheduleSleeping) return;
  if (!deviceConnected) return;

  // Operation mode dispatch (C6 supports Simple + Simulation only)
  if (settings.operationMode == OP_SIMULATION) {
    tickOrchestrator(now);
  } else {
    // OP_SIMPLE (default — other modes not supported on C6)
    if (keyEnabled && hasPopulatedSlot()) {
      if (now - lastKeyTime >= currentKeyInterval) {
        sendKeystroke();
        lastKeyTime = now;
        scheduleNextKey();
        pushSerialStatus();
      }
    }
    if (mouseEnabled) {
      handleMouseStateMachine(now);
    }
  }

  delay(1);  // Yield to FreeRTOS
}

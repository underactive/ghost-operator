#include <Arduino.h>
#include <lvgl.h>
#include <USB.h>
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

#if !defined(ARDUINO_USB_MODE) || !defined(ARDUINO_USB_CDC_ON_BOOT)
#error "ESP32-S3 USB build flags are missing."
#endif

#if ARDUINO_USB_MODE != 0
#error "ESP32-S3 USB HID + Web Serial require ARDUINO_USB_MODE=0 (TinyUSB device mode)."
#endif

#if ARDUINO_USB_CDC_ON_BOOT != 1
#error "ESP32-S3 Web Serial requires ARDUINO_USB_CDC_ON_BOOT=1."
#endif

// ============================================================================
// Ghost Operator — ESP32-S3-LCD-1.47 (Waveshare) main entry point
// Dual-transport HID: USB (primary) + BLE (secondary/fallback)
// LVGL color display, NeoPixel LED, no touch, no encoder
// ============================================================================

// USB HID init (defined in hid.cpp)
extern void setupUSBHID();

// Display refresh timing
#define DISPLAY_UPDATE_S3_MS 50  // 20 Hz

// USB host detection polling
#define USB_CHECK_MS 1000
static unsigned long lastUsbCheck = 0;

void setup() {
  // Open USB CDC before starting TinyUSB so the host sees the serial interface.
  Serial.begin(115200);

  // Initialize USB composite device: HID + CDC.
  setupUSBHID();

  delay(500);
  Serial.println();
  Serial.println("========================================");
  Serial.println("   GHOST OPERATOR — ESP32-S3");
  Serial.println("   Version " VERSION);
  Serial.println("   TARS Industrial Technical Solutions");
  Serial.println("========================================");
  Serial.println();

  // Initialize NeoPixel LED
  setupLed();
  Serial.println("[OK] LED initialized");

  // Initialize display
  setupDisplay();

  // Load settings from NVS
  loadSettings();
  setDisplayFlip(settings.displayFlip);  // re-apply after load; setupDisplay() ran with defaults
  Serial.println("[OK] Settings loaded");

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
  lastUsbCheck = startTime;
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();

  // Initialize orchestrator if in simulation mode
  if (settings.operationMode == OP_SIMULATION) {
    initOrchestrator();
  }

  // Initial display render
  markDisplayDirty();

  Serial.println("[OK] S3 setup complete — USB HID + CDC active, BLE advertising");
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

  // USB host detection — poll periodically
  if (now - lastUsbCheck >= USB_CHECK_MS) {
    lastUsbCheck = now;
    bool wasConnected = usbHostConnected;
    usbHostConnected = USB;  // truthy when USB host has enumerated
    usbConnected = usbHostConnected;

    if (usbHostConnected != wasConnected) {
      Serial.print("[USB] Host ");
      Serial.println(usbHostConnected ? "connected" : "disconnected");

      // Apply btWhileUsb policy
      if (usbHostConnected && !settings.btWhileUsb) {
        stopAdvertising();
      } else if (!usbHostConnected) {
        startAdvertising();
      }

      markDisplayDirty();
    }
  }

  // Schedule check (auto-sleep / full-auto)
  checkSchedule();

  // Screensaver: dim backlight after saver timeout
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
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_S3_MS) {
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

  // Skip jiggler logic if sleeping or not connected (USB or BLE)
  if (scheduleSleeping) return;
  if (!deviceConnected && !usbHostConnected) return;

  // Operation mode dispatch (S3 supports Simple + Simulation)
  if (settings.operationMode == OP_SIMULATION) {
    tickOrchestrator(now);
  } else {
    // OP_SIMPLE (default)
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

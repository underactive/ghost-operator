#include "schedule.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "orchestrator.h"
#include "display.h"
#include "serial_cmd.h"

// ============================================================================
// LIGHT SLEEP (Full Auto mode) — nRF52 implementation
// Uses Bluefruit BLE stack + SSD1306 OLED display
// ============================================================================

void enterLightSleep(bool scheduled) {
  scheduleSleeping = true;
  manualLightSleep = !scheduled;
  lastEncoderPos = encoderPos;  // reset baseline so stale delta doesn't wake immediately
  volD7ClickCount = 0;  // prevent stale click from firing on wake

  // Save dirty stats before sleeping (RAM preserved, but guards against power-cycle)
  if (statsDirty) {
    saveStats();
    statsDirty = false;
    lastStatsSave = millis();
  }

  // Stop BLE advertising
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.stop();
  if (deviceConnected && bleConnHandle != BLE_CONN_HANDLE_INVALID) {
    Bluefruit.disconnect(bleConnHandle);
  }

  // Display setup
  if (displayInitialized) {
    display.clearDisplay();

    if (scheduled) {
      // Scheduled sleep: show wake time
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      const char* msg1 = "Scheduled Sleep";
      int w1 = strlen(msg1) * 6;
      display.setCursor((128 - w1) / 2, 18);
      display.print(msg1);

      uint32_t wakeSecs = (uint32_t)settings.scheduleStart * SCHEDULE_SLOT_SECS;
      uint8_t wh = wakeSecs / 3600;
      uint8_t wm = (wakeSecs % 3600) / 60;
      char wakeBuf[16];
      snprintf(wakeBuf, sizeof(wakeBuf), "Wake: %d:%02d", wh, wm);
      int w2 = strlen(wakeBuf) * 6;
      display.setCursor((128 - w2) / 2, 34);
      display.print(wakeBuf);

      display.display();
      invalidateDisplayShadow();  // Shadow is stale after direct display.display()
    }
    // Manual: leave display blank — breathing animation renders from main loop

    // Dim to minimum
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x01);
  }

  Serial.println(scheduled ? "[Schedule] Light sleep entered" : "[Manual] Light sleep entered");
}

void exitLightSleep() {
  scheduleSleeping = false;
  manualLightSleep = false;
  scheduleManualWake = true;  // suppress re-sleep until next active window

  // Restart BLE
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);

  // Restore display brightness
  if (displayInitialized) {
    uint8_t normalContrast = (uint8_t)(0xCF * settings.displayBrightness / 100);
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(normalContrast);
  }

  // Reset timers so bars start fresh
  unsigned long now = millis();
  lastKeyTime = now;
  lastMouseStateChange = now;
  mouseState = MOUSE_IDLE;
  mouseNetX = 0;
  mouseNetY = 0;
  mouseReturnTotal = 0;
  scheduleNextKey();
  scheduleNextMouseState();

  // Re-sync orchestrator to wall clock after light sleep gap
  if (settings.operationMode == OP_SIMULATION && timeSynced) {
    uint32_t daySecs = currentDaySeconds();
    if (daySecs != 0xFFFFFFFF) {
      syncOrchestratorTime(daySecs);
    }
  }

  markDisplayDirty();

  Serial.println("[Schedule] Light sleep exited");
  pushSerialStatus();
}

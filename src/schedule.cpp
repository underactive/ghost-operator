#include "schedule.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "sleep.h"
#include "serial_cmd.h"
#include "display.h"
#include "orchestrator.h"

// ============================================================================
// TIME SYNC
// ============================================================================

void syncTime(uint32_t daySeconds) {
  if (daySeconds >= 86400) daySeconds = 0;
  wallClockDaySecs = daySeconds;
  wallClockSyncMs = millis();
  timeSynced = true;
  markDisplayDirty();
  char timeBuf[12];
  formatCurrentTime(timeBuf, sizeof(timeBuf));
  Serial.print("Time synced: ");
  Serial.println(timeBuf);

  // Sync orchestrator to correct work block for this time of day
  if (settings.operationMode == OP_SIMULATION) {
    syncOrchestratorTime(daySeconds);
  }
}

uint32_t currentDaySeconds() {
  if (!timeSynced) return 0xFFFFFFFF;
  unsigned long elapsed = millis() - wallClockSyncMs;
  uint32_t elapsedSecs = elapsed / 1000;
  // Re-anchor every 24h to prevent millis() wrap drift
  // NOTE: Re-anchoring mutates wallClockDaySecs/wallClockSyncMs as a side effect —
  // callers should not assume this function is pure/const.
  if (elapsedSecs >= 86400) {
    wallClockDaySecs = (wallClockDaySecs + elapsedSecs) % 86400;
    wallClockSyncMs = millis();
    elapsedSecs = 0;
  }
  return (wallClockDaySecs + elapsedSecs) % 86400;
}

void formatCurrentTime(char* buf, size_t bufSize) {
  uint32_t secs = currentDaySeconds();
  if (secs == 0xFFFFFFFF) { snprintf(buf, bufSize, "--:--:--"); return; }
  uint8_t h = secs / 3600;
  uint8_t m = (secs % 3600) / 60;
  uint8_t s = secs % 60;
  snprintf(buf, bufSize, "%d:%02d:%02d", h, m, s);
}

// ============================================================================
// SCHEDULE LOGIC
// ============================================================================

bool isScheduleActive() {
  if (settings.scheduleMode == SCHED_OFF) return true;  // no schedule = always active

  uint32_t now = currentDaySeconds();
  if (now == 0xFFFFFFFF) return true;  // not synced = assume active

  uint32_t startSecs = (uint32_t)settings.scheduleStart * SCHEDULE_SLOT_SECS;
  uint32_t endSecs = (uint32_t)settings.scheduleEnd * SCHEDULE_SLOT_SECS;

  // Same start/end = always on (no sleep trigger)
  if (startSecs == endSecs) return true;

  if (startSecs < endSecs) {
    // Normal window: e.g. 9:00 - 17:00
    return (now >= startSecs && now < endSecs);
  } else {
    // Midnight crossing: e.g. 22:00 - 6:00
    return (now >= startSecs || now < endSecs);
  }
}

void checkSchedule() {
  if (settings.scheduleMode == SCHED_OFF) return;
  if (!timeSynced) return;
  if (currentMode == MODE_SCHEDULE) return;  // don't act while user is editing

  unsigned long now = millis();
  if (now - lastScheduleCheck < SCHEDULE_CHECK_MS) return;
  lastScheduleCheck = now;

  bool active = isScheduleActive();

  if (active) {
    // Active window — clear manual wake suppression
    scheduleManualWake = false;
    if (scheduleSleeping) {
      // Wake time reached — exit light sleep
      exitLightSleep();
    }
  } else if (!active && !scheduleSleeping && !scheduleManualWake) {
    // End time reached — enter sleep (skip if user manually woke)
    if (settings.scheduleMode == SCHED_AUTO_SLEEP) {
      Serial.println("[Schedule] Auto-sleep: resetting schedule, entering deep sleep");
      settings.scheduleMode = SCHED_OFF;
      settings.scheduleStart = 108;  // reset to default 9:00
      settings.scheduleEnd = 204;    // reset to default 17:00
      saveSettings();
      sleepPending = true;
    } else if (settings.scheduleMode == SCHED_FULL_AUTO) {
      Serial.println("[Schedule] Full auto: entering light sleep");
      enterLightSleep();
    }
  }
}

// ============================================================================
// LIGHT SLEEP (Full Auto mode)
// ============================================================================

void enterLightSleep(bool scheduled) {
  scheduleSleeping = true;
  manualLightSleep = !scheduled;
  volD7ClickCount = 0;  // prevent stale click from firing on wake

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

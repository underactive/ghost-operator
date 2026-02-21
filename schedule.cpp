#include "schedule.h"
#include "state.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "sleep.h"
#include "serial_cmd.h"

// ============================================================================
// TIME SYNC
// ============================================================================

void syncTime(uint32_t daySeconds) {
  if (daySeconds >= 86400) daySeconds = 0;
  wallClockDaySecs = daySeconds;
  wallClockSyncMs = millis();
  timeSynced = true;
  Serial.print("Time synced: ");
  Serial.println(formatCurrentTime());
}

uint32_t currentDaySeconds() {
  if (!timeSynced) return 0xFFFFFFFF;
  unsigned long elapsed = millis() - wallClockSyncMs;
  uint32_t now = wallClockDaySecs + (elapsed / 1000);
  return now % 86400;
}

String formatCurrentTime() {
  uint32_t secs = currentDaySeconds();
  if (secs == 0xFFFFFFFF) return String("--:--");
  uint8_t h = secs / 3600;
  uint8_t m = (secs % 3600) / 60;
  char buf[6];
  sprintf(buf, "%d:%02d", h, m);
  return String(buf);
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
      Serial.println("[Schedule] Auto-sleep: entering deep sleep");
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

void enterLightSleep() {
  scheduleSleeping = true;

  // Stop BLE advertising
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.stop();
  if (deviceConnected && bleConnHandle != BLE_CONN_HANDLE_INVALID) {
    Bluefruit.disconnect(bleConnHandle);
  }

  // Blank display
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    const char* msg1 = "Scheduled Sleep";
    int w1 = strlen(msg1) * 6;
    display.setCursor((128 - w1) / 2, 18);
    display.print(msg1);

    // Show wake time
    uint32_t wakeSecs = (uint32_t)settings.scheduleStart * SCHEDULE_SLOT_SECS;
    uint8_t wh = wakeSecs / 3600;
    uint8_t wm = (wakeSecs % 3600) / 60;
    char wakeBuf[16];
    sprintf(wakeBuf, "Wake: %d:%02d", wh, wm);
    int w2 = strlen(wakeBuf) * 6;
    display.setCursor((128 - w2) / 2, 34);
    display.print(wakeBuf);

    display.display();

    // Dim to minimum
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x01);
  }

  Serial.println("[Schedule] Light sleep entered");
}

void exitLightSleep() {
  scheduleSleeping = false;
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

  Serial.println("[Schedule] Light sleep exited");
  pushSerialStatus();
}

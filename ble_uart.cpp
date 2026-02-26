#include "ble_uart.h"
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "schedule.h"
#include "sim_data.h"
#include "orchestrator.h"

// Line buffer for accumulating UART bytes
#define UART_BUF_SIZE 128
static char uartBuf[UART_BUF_SIZE];
static uint8_t uartBufPos = 0;
static bool uartBufOverflow = false;

// File-scoped writer pointer — set by processCommand(), used by cmd*() helpers
static ResponseWriter currentWriter = nullptr;

// Forward declarations
static void bleWrite(const char* msg);
static void cmdQueryStatus();
static void cmdQuerySettings();
static void cmdQueryKeys();
static void cmdQueryDecoys();
static void cmdSetValue(const char* body);
static void cmdSave();
static void cmdDefaults();
static void cmdReboot();
static void cmdDfu();
static void cmdSerialDfu();

// ----------------------------------------------------------------------------
// BLE UART RX callback — called from SoftDevice context
// We just let handleBleUart() poll; no work here.
// ----------------------------------------------------------------------------
static void bleUartRxCallback(uint16_t conn_handle) {
  (void)conn_handle;
  // Data available — will be read in handleBleUart()
}

// ----------------------------------------------------------------------------
// Setup: initialize BLEUart service
// Called from setupBLE() in ghost_operator.ino, BEFORE startAdvertising()
// ----------------------------------------------------------------------------
void setupBleUart() {
  bleuart.begin();
  bleuart.setRxCallback(bleUartRxCallback);
  Serial.println("[OK] BLE UART initialized");
}

// ----------------------------------------------------------------------------
// Reset line buffer — call on BLE disconnect to discard stale partial commands
// ----------------------------------------------------------------------------
void resetBleUartBuffer() {
  uartBufPos = 0;
  uartBufOverflow = false;
}

// ----------------------------------------------------------------------------
// Poll: read bytes into line buffer, dispatch on newline
// Called from loop() in ghost_operator.ino
// ----------------------------------------------------------------------------
void handleBleUart() {
  while (bleuart.available()) {
    char c = (char)bleuart.read();
    if (c == '\n' || c == '\r') {
      if (uartBufPos > 0) {
        if (uartBufOverflow) {
          bleWrite("-err:cmd too long");
        } else {
          uartBuf[uartBufPos] = '\0';
          processCommand(uartBuf, bleWrite);
        }
        uartBufPos = 0;
        uartBufOverflow = false;
      }
    } else if (uartBufPos < UART_BUF_SIZE - 1) {
      uartBuf[uartBufPos++] = c;
    } else {
      uartBufOverflow = true;
    }
  }
}

// ----------------------------------------------------------------------------
// Send a response string over BLE UART (appends newline)
// Chunks writes to 20 bytes for default MTU compatibility
// ----------------------------------------------------------------------------
static void bleWrite(const char* msg) {
  uint16_t len = strlen(msg);
  uint16_t offset = 0;
  while (offset < len) {
    uint16_t chunk = min((uint16_t)20, (uint16_t)(len - offset));
    bleuart.write((const uint8_t*)(msg + offset), chunk);
    offset += chunk;
  }
  bleuart.write((const uint8_t*)"\n", 1);
}

// ----------------------------------------------------------------------------
// Command dispatcher
// Protocol: ? = query, = = set, ! = action
// writer: function to send response strings (BLE chunked or serial println)
// ----------------------------------------------------------------------------
void processCommand(const char* line, ResponseWriter writer) {
  currentWriter = writer;

  // Echo BLE commands to serial for debugging (skip for serial source to avoid echo loop)
  if (writer == bleWrite) {
    Serial.print("[UART] RX: ");
    Serial.println(line);
  }

  if (line[0] == '?') {
    // Query commands
    const char* cmd = line + 1;
    if (strcmp(cmd, "status") == 0) {
      cmdQueryStatus();
    } else if (strcmp(cmd, "settings") == 0) {
      cmdQuerySettings();
    } else if (strcmp(cmd, "keys") == 0) {
      cmdQueryKeys();
    } else if (strcmp(cmd, "decoys") == 0) {
      cmdQueryDecoys();
    } else {
      currentWriter("-err:unknown query");
    }
  } else if (line[0] == '=') {
    // Set commands: =key:value
    cmdSetValue(line + 1);
  } else if (line[0] == '!') {
    // Action commands
    const char* cmd = line + 1;
    if (strcmp(cmd, "save") == 0) {
      cmdSave();
    } else if (strcmp(cmd, "defaults") == 0) {
      cmdDefaults();
    } else if (strcmp(cmd, "reboot") == 0) {
      cmdReboot();
    } else if (strcmp(cmd, "dfu") == 0) {
      cmdDfu();
    } else if (strcmp(cmd, "serialdfu") == 0) {
      cmdSerialDfu();
    } else {
      currentWriter("-err:unknown action");
    }
  } else {
    currentWriter("-err:invalid prefix");
  }
}

// ----------------------------------------------------------------------------
// ?status — runtime status (polled by dashboard)
// ----------------------------------------------------------------------------
static void cmdQueryStatus() {
  unsigned long uptime = millis() - startTime;

  char buf[280];
  int len = snprintf(buf, sizeof(buf),
    "!status|connected=%d|usb=%d|kb=%d|ms=%d|bat=%d|batMv=%d|profile=%d|mode=%d"
    "|mouseState=%d|uptime=%lu|kbNext=%s|timeSynced=%d|schedSleeping=%d",
    deviceConnected ? 1 : 0, usbConnected ? 1 : 0,
    keyEnabled ? 1 : 0, mouseEnabled ? 1 : 0,
    batteryPercent, (int)(batteryVoltage * 1000),
    (int)currentProfile, (int)currentMode,
    (int)mouseState, uptime,
    (nextKeyIndex < NUM_KEYS) ? AVAILABLE_KEYS[nextKeyIndex].name : "???",
    timeSynced ? 1 : 0, scheduleSleeping ? 1 : 0);

  if (timeSynced) {
    len += snprintf(buf + len, sizeof(buf) - len, "|daySecs=%lu", (unsigned long)currentDaySeconds());
  }

  // Mode-specific status
  if (settings.operationMode == 5) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|rcrState=%d|rcrScore=%d",
      (int)rcr.state, rcr.score);
  } else if (settings.operationMode == 4) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|snkState=%d|snkScore=%d|snkLen=%d",
      (int)snk.state, snk.score, snk.length);
  } else if (settings.operationMode == 3) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|brkState=%d|brkLevel=%d|brkScore=%d|brkLives=%d",
      (int)brk.state, brk.level, brk.score, brk.lives);
  } else if (settings.operationMode == 2) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|volMuted=%d|volPlaying=%d",
      volMuted ? 1 : 0, volPlaying ? 1 : 0);
  } else if (settings.operationMode == 1) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|simBlock=%d|simMode=%d|simPhase=%d|simProfile=%d",
      orch.blockIdx, (int)orch.modeId, (int)orch.phase, (int)orch.autoProfile);
  }

  currentWriter(buf);
}

// ----------------------------------------------------------------------------
// ?settings — all persistent settings
// ----------------------------------------------------------------------------
static void cmdQuerySettings() {
  char buf[640];
  int len = snprintf(buf, sizeof(buf),
    "!settings|keyMin=%lu|keyMax=%lu|mouseJig=%lu|mouseIdle=%lu"
    "|mouseAmp=%d|mouseStyle=%d|lazyPct=%d|busyPct=%d"
    "|dispBright=%d|saverBright=%d|saverTimeout=%d|animStyle=%d"
    "|name=%s|btWhileUsb=%d|scroll=%d|dashboard=%d|invertDial=%d"
    "|decoy=%d|schedMode=%d|schedStart=%d|schedEnd=%d",
    settings.keyIntervalMin, settings.keyIntervalMax,
    settings.mouseJiggleDuration, settings.mouseIdleDuration,
    settings.mouseAmplitude, settings.mouseStyle,
    settings.lazyPercent, settings.busyPercent,
    settings.displayBrightness, settings.saverBrightness,
    settings.saverTimeout, settings.animStyle,
    settings.deviceName, settings.btWhileUsb,
    settings.scrollEnabled, settings.dashboardEnabled,
    settings.invertDial,
    settings.decoyIndex, settings.scheduleMode,
    settings.scheduleStart, settings.scheduleEnd);

  // Slots as comma-separated indices
  len += snprintf(buf + len, sizeof(buf) - len, "|slots=");
  for (int i = 0; i < NUM_SLOTS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "%s%d",
                    (i > 0) ? "," : "", settings.keySlots[i]);
  }

  // Sound settings
  len += snprintf(buf + len, sizeof(buf) - len,
    "|sound=%d|soundType=%d|sysSounds=%d",
    settings.soundEnabled, settings.soundType, settings.systemSoundEnabled);

  // Simulation mode settings + volume control
  len += snprintf(buf + len, sizeof(buf) - len,
    "|opMode=%d|jobSim=%d|jobPerf=%d|jobStart=%d|phantom=%d|winSwitch=%d|switchKeys=%d|headerDisp=%d"
    "|volumeTheme=%d|encButton=%d|sideButton=%d"
    "|ballSpeed=%d|paddleSize=%d|startLives=%d|highScore=%d"
    "|snakeSpeed=%d|snakeWalls=%d|snakeHiScore=%d"
    "|racerSpeed=%d|racerHiScore=%d"
    "|shiftDur=%d|lunchDur=%d",
    settings.operationMode, settings.jobSimulation, settings.jobPerformance, settings.jobStartTime,
    settings.phantomClicks, settings.windowSwitching,
    settings.switchKeys, settings.headerDisplay,
    settings.volumeTheme, settings.encButtonAction, settings.sideButtonAction,
    settings.ballSpeed, settings.paddleSize, settings.startLives, settings.highScore,
    settings.snakeSpeed, settings.snakeWalls, settings.snakeHighScore,
    settings.racerSpeed, settings.racerHighScore,
    settings.shiftDuration, settings.lunchDuration);

  // Click slots as comma-separated indices
  len += snprintf(buf + len, sizeof(buf) - len, "|clickSlots=");
  for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "%s%d",
                    (i > 0) ? "," : "", settings.clickSlots[i]);
  }

  currentWriter(buf);
}

// ----------------------------------------------------------------------------
// ?keys — list of all available key names (populates dropdowns)
// ----------------------------------------------------------------------------
static void cmdQueryKeys() {
  char buf[220];
  int len = snprintf(buf, sizeof(buf), "!keys");
  for (int i = 0; i < NUM_KEYS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "|%s", AVAILABLE_KEYS[i].name);
  }
  currentWriter(buf);
}

// ----------------------------------------------------------------------------
// ?decoys — list of all decoy preset names (populates dropdown)
// ----------------------------------------------------------------------------
static void cmdQueryDecoys() {
  char buf[200];
  int len = snprintf(buf, sizeof(buf), "!decoys");
  for (int i = 0; i < DECOY_COUNT; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "|%s", DECOY_NAMES[i]);
  }
  currentWriter(buf);
}

// ----------------------------------------------------------------------------
// =key:value — set a single setting
// Applies to in-memory struct immediately (like encoder editing).
// Flash save only on explicit !save command.
// ----------------------------------------------------------------------------
static void cmdSetValue(const char* body) {
  // Find the colon separator
  const char* colon = strchr(body, ':');
  if (!colon) {
    currentWriter("-err:missing colon");
    return;
  }

  // Extract key and value strings
  int keyLen = colon - body;
  char key[32];
  if (keyLen >= (int)sizeof(key)) keyLen = sizeof(key) - 1;
  strncpy(key, body, keyLen);
  key[keyLen] = '\0';

  const char* valStr = colon + 1;

  // Match key name to setting and apply
  if (strcmp(key, "keyMin") == 0) {
    setSettingValue(SET_KEY_MIN, (uint32_t)atol(valStr));
  } else if (strcmp(key, "keyMax") == 0) {
    setSettingValue(SET_KEY_MAX, (uint32_t)atol(valStr));
  } else if (strcmp(key, "mouseJig") == 0) {
    setSettingValue(SET_MOUSE_JIG, (uint32_t)atol(valStr));
  } else if (strcmp(key, "mouseIdle") == 0) {
    setSettingValue(SET_MOUSE_IDLE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "mouseAmp") == 0) {
    setSettingValue(SET_MOUSE_AMP, (uint32_t)atol(valStr));
  } else if (strcmp(key, "mouseStyle") == 0) {
    setSettingValue(SET_MOUSE_STYLE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "lazyPct") == 0) {
    setSettingValue(SET_LAZY_PCT, (uint32_t)atol(valStr));
  } else if (strcmp(key, "busyPct") == 0) {
    setSettingValue(SET_BUSY_PCT, (uint32_t)atol(valStr));
  } else if (strcmp(key, "dispBright") == 0) {
    setSettingValue(SET_DISPLAY_BRIGHT, (uint32_t)atol(valStr));
  } else if (strcmp(key, "saverBright") == 0) {
    setSettingValue(SET_SAVER_BRIGHT, (uint32_t)atol(valStr));
  } else if (strcmp(key, "saverTimeout") == 0) {
    setSettingValue(SET_SAVER_TIMEOUT, (uint32_t)atol(valStr));
  } else if (strcmp(key, "animStyle") == 0) {
    setSettingValue(SET_ANIMATION, (uint32_t)atol(valStr));
  } else if (strcmp(key, "btWhileUsb") == 0) {
    setSettingValue(SET_BT_WHILE_USB, (uint32_t)atol(valStr));
  } else if (strcmp(key, "scroll") == 0) {
    setSettingValue(SET_SCROLL, (uint32_t)atol(valStr));
  } else if (strcmp(key, "dashboard") == 0) {
    setSettingValue(SET_DASHBOARD, (uint32_t)atol(valStr));
  } else if (strcmp(key, "invertDial") == 0) {
    setSettingValue(SET_INVERT_DIAL, (uint32_t)atol(valStr));
  } else if (strcmp(key, "name") == 0) {
    // Device name — up to 14 chars
    strncpy(settings.deviceName, valStr, NAME_MAX_LEN);
    settings.deviceName[NAME_MAX_LEN] = '\0';
  } else if (strcmp(key, "decoy") == 0) {
    uint8_t idx = (uint8_t)atoi(valStr);
    if (idx > DECOY_COUNT) idx = 0;
    settings.decoyIndex = idx;
    // Sync deviceName when selecting a preset
    if (idx > 0 && idx <= DECOY_COUNT) {
      strncpy(settings.deviceName, DECOY_NAMES[idx - 1], NAME_MAX_LEN);
      settings.deviceName[NAME_MAX_LEN] = '\0';
    }
  } else if (strcmp(key, "schedMode") == 0) {
    setSettingValue(SET_SCHEDULE_MODE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "schedStart") == 0) {
    setSettingValue(SET_SCHEDULE_START, (uint32_t)atol(valStr));
  } else if (strcmp(key, "schedEnd") == 0) {
    setSettingValue(SET_SCHEDULE_END, (uint32_t)atol(valStr));
  } else if (strcmp(key, "opMode") == 0) {
    setSettingValue(SET_OP_MODE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "jobSim") == 0) {
    setSettingValue(SET_JOB_SIM, (uint32_t)atol(valStr));
  } else if (strcmp(key, "jobPerf") == 0) {
    setSettingValue(SET_JOB_PERFORMANCE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "jobStart") == 0) {
    setSettingValue(SET_JOB_START_TIME, (uint32_t)atol(valStr));
  } else if (strcmp(key, "phantom") == 0) {
    setSettingValue(SET_PHANTOM_CLICKS, (uint32_t)atol(valStr));
  } else if (strcmp(key, "clickSlots") == 0) {
    // Comma-separated click slot indices: "1,7,7,7,7,7,7"
    int slot = 0;
    const char* p = valStr;
    while (slot < NUM_CLICK_SLOTS && *p) {
      uint8_t v = (uint8_t)atoi(p);
      settings.clickSlots[slot] = (v >= NUM_CLICK_TYPES) ? (NUM_CLICK_TYPES - 1) : v;
      slot++;
      while (*p && *p != ',') p++;
      if (*p == ',') p++;
    }
    for (; slot < NUM_CLICK_SLOTS; slot++) {
      settings.clickSlots[slot] = NUM_CLICK_TYPES - 1;
    }
  } else if (strcmp(key, "winSwitch") == 0) {
    setSettingValue(SET_WINDOW_SWITCH, (uint32_t)atol(valStr));
  } else if (strcmp(key, "switchKeys") == 0) {
    setSettingValue(SET_SWITCH_KEYS, (uint32_t)atol(valStr));
  } else if (strcmp(key, "headerDisp") == 0) {
    setSettingValue(SET_HEADER_DISPLAY, (uint32_t)atol(valStr));
  } else if (strcmp(key, "sound") == 0) {
    setSettingValue(SET_SOUND_ENABLED, (uint32_t)atol(valStr));
  } else if (strcmp(key, "soundType") == 0) {
    setSettingValue(SET_SOUND_TYPE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "sysSounds") == 0) {
    setSettingValue(SET_SYSTEM_SOUND, (uint32_t)atol(valStr));
  } else if (strcmp(key, "volumeTheme") == 0) {
    setSettingValue(SET_VOLUME_THEME, (uint32_t)atol(valStr));
  } else if (strcmp(key, "encButton") == 0) {
    setSettingValue(SET_ENC_BUTTON, (uint32_t)atol(valStr));
  } else if (strcmp(key, "sideButton") == 0) {
    setSettingValue(SET_SIDE_BUTTON, (uint32_t)atol(valStr));
  } else if (strcmp(key, "ballSpeed") == 0) {
    setSettingValue(SET_BALL_SPEED, (uint32_t)atol(valStr));
  } else if (strcmp(key, "paddleSize") == 0) {
    setSettingValue(SET_PADDLE_SIZE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "startLives") == 0) {
    setSettingValue(SET_START_LIVES, (uint32_t)atol(valStr));
  } else if (strcmp(key, "snakeSpeed") == 0) {
    setSettingValue(SET_SNAKE_SPEED, (uint32_t)atol(valStr));
  } else if (strcmp(key, "snakeWalls") == 0) {
    setSettingValue(SET_SNAKE_WALLS, (uint32_t)atol(valStr));
  } else if (strcmp(key, "racerSpeed") == 0) {
    setSettingValue(SET_RACER_SPEED, (uint32_t)atol(valStr));
  } else if (strcmp(key, "shiftDur") == 0) {
    setSettingValue(SET_SHIFT_DURATION, (uint32_t)atol(valStr));
  } else if (strcmp(key, "lunchDur") == 0) {
    setSettingValue(SET_LUNCH_DURATION, (uint32_t)atol(valStr));
  } else if (strcmp(key, "time") == 0) {
    uint32_t secs = (uint32_t)atol(valStr);
    if (secs >= 86400) secs = 0;
    syncTime(secs);
  } else if (strcmp(key, "statusPush") == 0) {
    serialStatusPush = atoi(valStr) != 0;
    currentWriter("+ok");
    return;
  } else if (strcmp(key, "slots") == 0) {
    // Comma-separated slot indices: "2,28,28,28,28,28,28,28"
    int slot = 0;
    const char* p = valStr;
    while (slot < NUM_SLOTS && *p) {
      settings.keySlots[slot] = (uint8_t)atoi(p);
      if (settings.keySlots[slot] >= NUM_KEYS) {
        settings.keySlots[slot] = NUM_KEYS - 1;
      }
      slot++;
      // Advance past this number and the comma
      while (*p && *p != ',') p++;
      if (*p == ',') p++;
    }
    // Fill remaining slots with NONE if fewer than NUM_SLOTS provided
    for (; slot < NUM_SLOTS; slot++) {
      settings.keySlots[slot] = NUM_KEYS - 1;
    }
  } else {
    currentWriter("-err:unknown key");
    return;
  }

  // Re-schedule timing after any change (like encoder editing does)
  scheduleNextKey();
  scheduleNextMouseState();

  currentWriter("+ok");
}

// ----------------------------------------------------------------------------
// !save — persist current settings to flash
// ----------------------------------------------------------------------------
static void cmdSave() {
  saveSettings();
  currentWriter("+ok");
}

// ----------------------------------------------------------------------------
// !defaults — reset all settings to factory defaults
// ----------------------------------------------------------------------------
static void cmdDefaults() {
  loadDefaults();
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();
  currentProfile = PROFILE_NORMAL;
  currentWriter("+ok");
}

// ----------------------------------------------------------------------------
// !reboot — restart the device
// ----------------------------------------------------------------------------
static void cmdReboot() {
  currentWriter("+ok");
  Serial.flush();
  delay(100);  // Let the response transmit
  NVIC_SystemReset();
}

// ----------------------------------------------------------------------------
// SoftDevice-safe reboot into OTA DFU bootloader mode.
// enterOTADfu() from wiring.h writes directly to NRF_POWER->GPREGRET, which
// hard-faults when the SoftDevice is active (it owns that register).
// We must use the sd_power_gpregret_*() SVCall API instead.
// Shows a DFU screen on the OLED before resetting (bootloader doesn't drive
// the display, so this framebuffer persists through the reboot).
// ----------------------------------------------------------------------------
void resetToDfu() {
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(16, 8);
    display.print("OTA DFU");
    display.setTextSize(1);
    display.setCursor(10, 36);
    display.print("Waiting for update");
    display.setCursor(10, 50);
    display.print("Power cycle to exit");
    display.display();
  }

  sd_power_gpregret_clr(0, 0xFF);
  sd_power_gpregret_set(0, 0xA8);  // DFU_MAGIC_OTA_RESET
  NVIC_SystemReset();
}

// ----------------------------------------------------------------------------
// !dfu — reboot into OTA DFU bootloader mode
// ----------------------------------------------------------------------------
static void cmdDfu() {
  currentWriter("+ok:dfu");
  Serial.flush();
  delay(100);  // Let the response transmit
  resetToDfu();
}

// ----------------------------------------------------------------------------
// SoftDevice-safe reboot into Serial DFU bootloader mode (USB CDC).
// Uses GPREGRET magic 0x4E (DFU_MAGIC_SERIAL_ONLY_RESET) — same as
// enterSerialDfu() from wiring.h, but safe for use with active SoftDevice.
// The bootloader presents a USB CDC serial port for adafruit-nrfutil or
// Web Serial DFU transfer.
// ----------------------------------------------------------------------------
void resetToSerialDfu() {
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(16, 8);
    display.print("USB DFU");
    display.setTextSize(1);
    display.setCursor(4, 36);
    display.print("Connect USB cable");
    display.setCursor(4, 50);
    display.print("Power cycle to exit");
    display.display();
  }

  sd_power_gpregret_clr(0, 0xFF);
  sd_power_gpregret_set(0, 0x4E);  // DFU_MAGIC_SERIAL_ONLY_RESET
  NVIC_SystemReset();
}

// ----------------------------------------------------------------------------
// !serialdfu — reboot into Serial DFU bootloader mode (USB CDC)
// ----------------------------------------------------------------------------
static void cmdSerialDfu() {
  currentWriter("+ok:serialdfu");
  Serial.flush();
  delay(100);  // Let the response transmit
  resetToSerialDfu();
}

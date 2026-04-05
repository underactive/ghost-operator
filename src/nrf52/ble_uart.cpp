#include "ble_uart.h"
#include "protocol.h"
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "schedule.h"
#include "sim_data.h"
#include "orchestrator.h"

// Line buffer for accumulating UART bytes (512 for JSON payloads)
#define UART_BUF_SIZE 512
static char uartBuf[UART_BUF_SIZE];
static uint16_t uartBufPos = 0;
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
static void cmdQueryWorkMode(uint8_t idx);
static void cmdQuerySimBlocks(uint8_t jobIdx);

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
  if (bleUartResetPending) {
    bleUartResetPending = false;
    resetBleUartBuffer();
  }
  while (bleuart.available()) {
    if (bleUartResetPending) {
      bleUartResetPending = false;
      resetBleUartBuffer();
      break;
    }
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

#ifdef BLE_UART_DEBUG
  if (writer == bleWrite) {
    char prefix = line[0];
    size_t len = strlen(line);
    Serial.print("[UART] RX: ");
    Serial.print(prefix);
    Serial.print(" len=");
    Serial.println((unsigned int)len);
  }
#endif

  // Auto-detect JSON commands (starts with '{')
  if (line[0] == '{') {
    processJsonCommand(line, writer);
    return;
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
    } else if (strncmp(cmd, "wmode:", 6) == 0) {
      uint8_t idx = (uint8_t)atoi(cmd + 6);
      if (idx < WMODE_COUNT) cmdQueryWorkMode(idx);
      else currentWriter("-err:invalid mode index");
    } else if (strncmp(cmd, "simblocks:", 10) == 0) {
      uint8_t idx = (uint8_t)atoi(cmd + 10);
      if (idx < JOB_SIM_COUNT) cmdQuerySimBlocks(idx);
      else currentWriter("-err:invalid job index");
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
    } else if (strcmp(cmd, "savesim") == 0) {
      saveSimData();
      currentWriter("+ok");
    } else if (strcmp(cmd, "resetsim") == 0) {
      resetSimDataDefaults();
      currentWriter("+ok");
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

  char buf[340];
  int len = snprintf(buf, sizeof(buf),
    "!status|connected=%d|usb=%d|kb=%d|ms=%d|bat=%d|batMv=%d|profile=%d|mode=%d"
    "|mouseState=%d|uptime=%lu|kbNext=%s|timeSynced=%d|schedSleeping=%d|platform=nrf52",
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

  // Lifetime stats (live from RAM)
  len += snprintf(buf + len, sizeof(buf) - len,
    "|totalKeys=%lu|totalMousePx=%lu|totalClicks=%lu",
    (unsigned long)stats.totalKeystrokes,
    (unsigned long)stats.totalMousePixels,
    (unsigned long)stats.totalMouseClicks);

  // Mode-specific status
  if (settings.operationMode == OP_RACER) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|rcrState=%d|rcrScore=%d",
      (int)gRcr.state, gRcr.score);
  } else if (settings.operationMode == OP_SNAKE) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|snkState=%d|snkScore=%d|snkLen=%d",
      (int)gSnk.state, gSnk.score, gSnk.length);
  } else if (settings.operationMode == OP_BREAKOUT) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|brkState=%d|brkLevel=%d|brkScore=%d|brkLives=%d",
      (int)gBrk.state, gBrk.level, gBrk.score, gBrk.lives);
  } else if (settings.operationMode == OP_VOLUME) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|volMuted=%d|volPlaying=%d",
      volMuted ? 1 : 0, volPlaying ? 1 : 0);
  } else if (settings.operationMode == OP_SIMULATION) {
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
  char buf[768];
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

  // Lifetime stats (read-only, from separate stats file)
  len += snprintf(buf + len, sizeof(buf) - len,
    "|totalKeys=%lu|totalMousePx=%lu|totalClicks=%lu",
    (unsigned long)stats.totalKeystrokes,
    (unsigned long)stats.totalMousePixels,
    (unsigned long)stats.totalMouseClicks);

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
    // Device name — up to 14 printable ASCII chars
    if (strlen(valStr) > NAME_MAX_LEN) { currentWriter("-err:name too long"); return; }
    for (const char* p = valStr; *p; p++) {
      if (*p < 0x20 || *p > 0x7E) {
        currentWriter("-err:invalid name chars");
        return;
      }
    }
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
  } else if (strcmp(key, "wmode") == 0) {
    // Format: N:field:value  (e.g., "0:kb:50" or "0:t0:5,15,180,300,8000,30000,120,280,8000,20000,3000,12000")
    // valStr points to "N:field:value"
    const char* p1 = strchr(valStr, ':');
    if (!p1) { currentWriter("-err:wmode format"); return; }
    uint8_t modeIdx = (uint8_t)atoi(valStr);
    if (modeIdx >= WMODE_COUNT) { currentWriter("-err:invalid mode index"); return; }
    const char* p2 = strchr(p1 + 1, ':');
    if (!p2) { currentWriter("-err:wmode format"); return; }
    char field[8];
    int fLen = p2 - (p1 + 1);
    if (fLen >= (int)sizeof(field)) fLen = sizeof(field) - 1;
    strncpy(field, p1 + 1, fLen);
    field[fLen] = '\0';
    const char* fVal = p2 + 1;
    WorkModeDef& mode = workModes[modeIdx];
    if (strcmp(field, "kb") == 0) {
      uint8_t v = (uint8_t)atoi(fVal);
      mode.kbPercent = (v > 100) ? 100 : v;
    } else if (strcmp(field, "wL") == 0) {
      int v = atoi(fVal); mode.profileWeights.lazyPct = (uint8_t)max(0, min(100, v));
    } else if (strcmp(field, "wN") == 0) {
      int v = atoi(fVal); mode.profileWeights.normalPct = (uint8_t)max(0, min(100, v));
    } else if (strcmp(field, "wB") == 0) {
      int v = atoi(fVal); mode.profileWeights.busyPct = (uint8_t)max(0, min(100, v));
    } else if (strcmp(field, "dMin") == 0) {
      mode.modeDurMinSec = (uint16_t)min(65535, max(10, atoi(fVal)));
      if (mode.modeDurMaxSec < mode.modeDurMinSec) mode.modeDurMaxSec = mode.modeDurMinSec;
    } else if (strcmp(field, "dMax") == 0) {
      mode.modeDurMaxSec = (uint16_t)min(65535, max(10, atoi(fVal)));
      if (mode.modeDurMinSec > mode.modeDurMaxSec) mode.modeDurMinSec = mode.modeDurMaxSec;
    } else if (strcmp(field, "pMin") == 0) {
      mode.profileStintMinSec = (uint16_t)max(5, atoi(fVal));
      if (mode.profileStintMaxSec < mode.profileStintMinSec) mode.profileStintMaxSec = mode.profileStintMinSec;
    } else if (strcmp(field, "pMax") == 0) {
      mode.profileStintMaxSec = (uint16_t)max(5, atoi(fVal));
      if (mode.profileStintMinSec > mode.profileStintMaxSec) mode.profileStintMinSec = mode.profileStintMaxSec;
    } else if (field[0] == 't' && field[1] >= '0' && field[1] <= '2' && field[2] == '\0') {
      // t0, t1, t2 = profile timing (12 CSV values)
      uint8_t pIdx = field[1] - '0';
      PhaseTiming& t = mode.timing[pIdx];
      // Parse 12 comma-separated values
      const char* pp = fVal;
      uint32_t vals[12];
      int csvCount = 0;
      for (int i = 0; i < 12; i++) {
        vals[i] = (uint32_t)atol(pp);
        csvCount++;
        while (*pp && *pp != ',') pp++;
        if (*pp == ',') pp++;
        else if (i < 11) { currentWriter("-err:wmode timing needs 12 values"); return; }
      }
      t.burstKeysMin = (uint8_t)vals[0];
      t.burstKeysMax = (uint8_t)max((uint32_t)t.burstKeysMin, vals[1]);
      t.interKeyMinMs = (uint16_t)vals[2];
      t.interKeyMaxMs = (uint16_t)max((uint32_t)t.interKeyMinMs, vals[3]);
      t.burstGapMinMs = vals[4];
      t.burstGapMaxMs = max(t.burstGapMinMs, vals[5]);
      t.keyHoldMinMs = (uint16_t)vals[6];
      t.keyHoldMaxMs = (uint16_t)max((uint32_t)t.keyHoldMinMs, vals[7]);
      t.mouseDurMinMs = vals[8];
      t.mouseDurMaxMs = max(t.mouseDurMinMs, vals[9]);
      t.idleDurMinMs = vals[10];
      t.idleDurMaxMs = max(t.idleDurMinMs, vals[11]);
    } else {
      currentWriter("-err:unknown wmode field");
      return;
    }
    currentWriter("+ok");
    return;
  // Lifetime stats restore (dashboard sends these after DFU wipes flash)
  } else if (strcmp(key, "totalKeys") == 0) {
    stats.totalKeystrokes = (uint32_t)strtoul(valStr, NULL, 10);
    statsDirty = true;
    currentWriter("+ok");
    return;
  } else if (strcmp(key, "totalMousePx") == 0) {
    stats.totalMousePixels = (uint32_t)strtoul(valStr, NULL, 10);
    statsDirty = true;
    currentWriter("+ok");
    return;
  } else if (strcmp(key, "totalClicks") == 0) {
    stats.totalMouseClicks = (uint32_t)strtoul(valStr, NULL, 10);
    statsDirty = true;
    currentWriter("+ok");
    return;
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
  saveSimData();
  if (statsDirty) { saveStats(); statsDirty = false; }
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
  resetSimDataDefaults();
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
// ?wmode:N — query a single work mode's parameters
// ----------------------------------------------------------------------------
static void cmdQueryWorkMode(uint8_t idx) {
  const WorkModeDef& m = workModes[idx];
  char buf[640];
  int len = snprintf(buf, sizeof(buf),
    "!wmode|idx=%d|name=%s|kb=%d|wL=%d|wN=%d|wB=%d",
    idx, m.shortName, m.kbPercent,
    m.profileWeights.lazyPct, m.profileWeights.normalPct, m.profileWeights.busyPct);
  if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;

  // Per-profile timing (t0=LAZY, t1=NORMAL, t2=BUSY)
  for (int p = 0; p < PROFILE_COUNT; p++) {
    const PhaseTiming& t = m.timing[p];
    len += snprintf(buf + len, sizeof(buf) - len,
      "|t%d=%d,%d,%d,%d,%lu,%lu,%d,%d,%lu,%lu,%lu,%lu",
      p, t.burstKeysMin, t.burstKeysMax,
      t.interKeyMinMs, t.interKeyMaxMs,
      (unsigned long)t.burstGapMinMs, (unsigned long)t.burstGapMaxMs,
      t.keyHoldMinMs, t.keyHoldMaxMs,
      (unsigned long)t.mouseDurMinMs, (unsigned long)t.mouseDurMaxMs,
      (unsigned long)t.idleDurMinMs, (unsigned long)t.idleDurMaxMs);
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
  }

  len += snprintf(buf + len, sizeof(buf) - len,
    "|dMin=%d|dMax=%d|pMin=%d|pMax=%d",
    m.modeDurMinSec, m.modeDurMaxSec,
    m.profileStintMinSec, m.profileStintMaxSec);

  currentWriter(buf);
}

// ----------------------------------------------------------------------------
// ?simblocks:N — query day template blocks for a job type (read-only)
// ----------------------------------------------------------------------------
static void cmdQuerySimBlocks(uint8_t jobIdx) {
  const DayTemplate& tmpl = DAY_TEMPLATES[jobIdx];
  char buf[512];
  int len = snprintf(buf, sizeof(buf), "!simblocks|job=%d", jobIdx);

  for (int b = 0; b < tmpl.numBlocks; b++) {
    const TimeBlock& block = tmpl.blocks[b];
    len += snprintf(buf + len, sizeof(buf) - len,
      "|b%d=%s,%d,%d", b, block.name, block.startMinutes, block.durationMinutes);
    for (int m = 0; m < block.numModes; m++) {
      len += snprintf(buf + len, sizeof(buf) - len,
        ",%d:%d", block.modes[m].modeId, block.modes[m].weight);
    }
  }

  currentWriter(buf);
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
  // Flush stats/settings before DFU (bootloader may erase LittleFS)
  if (statsDirty) { saveStats(); statsDirty = false; }
  saveSettings();

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
  // Flush stats/settings before DFU (bootloader may erase LittleFS)
  if (statsDirty) { saveStats(); statsDirty = false; }
  saveSettings();

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

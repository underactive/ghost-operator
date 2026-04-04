#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_uart.h"
#include "protocol.h"
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "schedule.h"
#include "sim_data.h"
#include "orchestrator.h"
#include "platform_hal.h"
#include "display.h"

// ============================================================================
// BLE UART (Nordic UART Service) for ESP32-S3
// ============================================================================

// NUS UUIDs
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic* pNusTx = nullptr;
static NimBLECharacteristic* pNusRx = nullptr;

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
static void cmdQueryWorkMode(uint8_t idx);
static void cmdQuerySimBlocks(uint8_t jobIdx);

// ============================================================================
// NUS RX callback — called when BLE client writes data
// ============================================================================

class NusRxCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    std::string rxVal = pChar->getValue();
    for (size_t i = 0; i < rxVal.length(); i++) {
      char c = rxVal[i];
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
};

static NusRxCallback nusRxCallback;

// ============================================================================
// Setup BLE UART
// ============================================================================

void setupBleUart() {
  // Get existing server (already created in ble.cpp — createServer returns singleton)
  NimBLEServer* pServer = NimBLEDevice::createServer();

  NimBLEService* pNusService = pServer->createService(NUS_SERVICE_UUID);

  pNusTx = pNusService->createCharacteristic(
    NUS_TX_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  pNusRx = pNusService->createCharacteristic(
    NUS_RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pNusRx->setCallbacks(&nusRxCallback);

  // NimBLE 2.x: services are started when server->start() is called in ble.cpp
  Serial.println("[OK] BLE UART (NUS) initialized");
}

// ============================================================================
// Poll BLE UART — on NimBLE, RX is handled via callback, so this is a no-op
// ============================================================================

void handleBleUart() {
  // NimBLE delivers RX data via NusRxCallback::onWrite
  // No polling needed
}

// ============================================================================
// Reset line buffer — call on BLE disconnect
// ============================================================================

void resetBleUartBuffer() {
  uartBufPos = 0;
  uartBufOverflow = false;
}

// ============================================================================
// Send response over BLE UART (chunked to 20 bytes for MTU compatibility)
// ============================================================================

static void bleWrite(const char* msg) {
  if (!pNusTx || !deviceConnected) return;

  uint16_t len = strlen(msg);
  uint16_t offset = 0;
  while (offset < len) {
    uint16_t chunk = (len - offset > 20) ? 20 : (len - offset);
    pNusTx->setValue((const uint8_t*)(msg + offset), chunk);
    pNusTx->notify();
    offset += chunk;
  }
  // Send newline
  pNusTx->setValue((const uint8_t*)"\n", 1);
  pNusTx->notify();
}

// ============================================================================
// Command dispatcher
// Protocol: ? = query, = = set, ! = action
// ============================================================================

void processCommand(const char* line, ResponseWriter writer) {
  currentWriter = writer;

  // Echo BLE commands to serial for debugging
  if (writer == bleWrite) {
    Serial.print("[UART] RX: ");
    Serial.println(line);
  }

  // Auto-detect JSON commands
  if (line[0] == '{') {
    processJsonCommand(line, writer);
    return;
  }

  if (line[0] == '?') {
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
    cmdSetValue(line + 1);
  } else if (line[0] == '!') {
    const char* cmd = line + 1;
    if (strcmp(cmd, "save") == 0) {
      cmdSave();
    } else if (strcmp(cmd, "defaults") == 0) {
      cmdDefaults();
    } else if (strcmp(cmd, "reboot") == 0) {
      cmdReboot();
    } else if (strcmp(cmd, "dfu") == 0) {
      currentWriter("-err:DFU not supported on S3");
    } else if (strcmp(cmd, "serialdfu") == 0) {
      currentWriter("-err:DFU not supported on S3");
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

// ============================================================================
// ?status
// ============================================================================

static void cmdQueryStatus() {
  unsigned long uptime = millis() - startTime;

  char buf[340];
  int len = snprintf(buf, sizeof(buf),
    "!status|connected=%d|usb=%d|kb=%d|ms=%d|bat=%d|batMv=%d|profile=%d|mode=%d"
    "|mouseState=%d|uptime=%lu|kbNext=%s|timeSynced=%d|schedSleeping=%d|platform=s3",
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

  // Lifetime stats
  len += snprintf(buf + len, sizeof(buf) - len,
    "|totalKeys=%lu|totalMousePx=%lu|totalClicks=%lu",
    (unsigned long)stats.totalKeystrokes,
    (unsigned long)stats.totalMousePixels,
    (unsigned long)stats.totalMouseClicks);

  // Simulation mode status
  if (settings.operationMode == OP_SIMULATION) {
    len += snprintf(buf + len, sizeof(buf) - len,
      "|simBlock=%d|simMode=%d|simPhase=%d|simProfile=%d",
      orch.blockIdx, (int)orch.modeId, (int)orch.phase, (int)orch.autoProfile);
  }

  currentWriter(buf);
}

// ============================================================================
// ?settings
// ============================================================================

static void cmdQuerySettings() {
  char buf[768];
  int len = snprintf(buf, sizeof(buf),
    "!settings|keyMin=%lu|keyMax=%lu|mouseJig=%lu|mouseIdle=%lu"
    "|mouseAmp=%d|mouseStyle=%d|lazyPct=%d|busyPct=%d"
    "|dispBright=%d|saverBright=%d|saverTimeout=%d|animStyle=%d|dispFlip=%d"
    "|name=%s|btWhileUsb=%d|scroll=%d|dashboard=%d|invertDial=%d"
    "|decoy=%d|schedMode=%d|schedStart=%d|schedEnd=%d",
    settings.keyIntervalMin, settings.keyIntervalMax,
    settings.mouseJiggleDuration, settings.mouseIdleDuration,
    settings.mouseAmplitude, settings.mouseStyle,
    settings.lazyPercent, settings.busyPercent,
    settings.displayBrightness, settings.saverBrightness,
    settings.saverTimeout, settings.animStyle, settings.displayFlip,
    settings.deviceName, settings.btWhileUsb,
    settings.scrollEnabled, settings.dashboardEnabled,
    settings.invertDial,
    settings.decoyIndex, settings.scheduleMode,
    settings.scheduleStart, settings.scheduleEnd);

  // Slots
  len += snprintf(buf + len, sizeof(buf) - len, "|slots=");
  for (int i = 0; i < NUM_SLOTS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "%s%d",
                    (i > 0) ? "," : "", settings.keySlots[i]);
  }

  // Sound + simulation + volume + game settings
  len += snprintf(buf + len, sizeof(buf) - len,
    "|sound=%d|soundType=%d|sysSounds=%d"
    "|opMode=%d|jobSim=%d|jobPerf=%d|jobStart=%d|phantom=%d|winSwitch=%d|switchKeys=%d|headerDisp=%d"
    "|volumeTheme=%d|encButton=%d|sideButton=%d"
    "|ballSpeed=%d|paddleSize=%d|startLives=%d|highScore=%d"
    "|snakeSpeed=%d|snakeWalls=%d|snakeHiScore=%d"
    "|racerSpeed=%d|racerHiScore=%d"
    "|shiftDur=%d|lunchDur=%d",
    settings.soundEnabled, settings.soundType, settings.systemSoundEnabled,
    settings.operationMode, settings.jobSimulation, settings.jobPerformance, settings.jobStartTime,
    settings.phantomClicks, settings.windowSwitching,
    settings.switchKeys, settings.headerDisplay,
    settings.volumeTheme, settings.encButtonAction, settings.sideButtonAction,
    settings.ballSpeed, settings.paddleSize, settings.startLives, settings.highScore,
    settings.snakeSpeed, settings.snakeWalls, settings.snakeHighScore,
    settings.racerSpeed, settings.racerHighScore,
    settings.shiftDuration, settings.lunchDuration);

  // Click slots
  len += snprintf(buf + len, sizeof(buf) - len, "|clickSlots=");
  for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "%s%d",
                    (i > 0) ? "," : "", settings.clickSlots[i]);
  }

  // Lifetime stats
  len += snprintf(buf + len, sizeof(buf) - len,
    "|totalKeys=%lu|totalMousePx=%lu|totalClicks=%lu",
    (unsigned long)stats.totalKeystrokes,
    (unsigned long)stats.totalMousePixels,
    (unsigned long)stats.totalMouseClicks);

  currentWriter(buf);
}

// ============================================================================
// ?keys
// ============================================================================

static void cmdQueryKeys() {
  char buf[220];
  int len = snprintf(buf, sizeof(buf), "!keys");
  for (int i = 0; i < NUM_KEYS; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "|%s", AVAILABLE_KEYS[i].name);
  }
  currentWriter(buf);
}

// ============================================================================
// ?decoys
// ============================================================================

static void cmdQueryDecoys() {
  char buf[200];
  int len = snprintf(buf, sizeof(buf), "!decoys");
  for (int i = 0; i < DECOY_COUNT; i++) {
    len += snprintf(buf + len, sizeof(buf) - len, "|%s", DECOY_NAMES[i]);
  }
  currentWriter(buf);
}

// ============================================================================
// =key:value — set a single setting
// ============================================================================

static void cmdSetValue(const char* body) {
  const char* colon = strchr(body, ':');
  if (!colon) {
    currentWriter("-err:missing colon");
    return;
  }

  int keyLen = colon - body;
  char key[32];
  if (keyLen >= (int)sizeof(key)) keyLen = sizeof(key) - 1;
  strncpy(key, body, keyLen);
  key[keyLen] = '\0';

  const char* valStr = colon + 1;

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
  } else if (strcmp(key, "dispFlip") == 0) {
    setSettingValue(SET_DISPLAY_FLIP, (uint32_t)atol(valStr));
    setDisplayFlip(settings.displayFlip);
  } else if (strcmp(key, "btWhileUsb") == 0) {
    setSettingValue(SET_BT_WHILE_USB, (uint32_t)atol(valStr));
  } else if (strcmp(key, "scroll") == 0) {
    setSettingValue(SET_SCROLL, (uint32_t)atol(valStr));
  } else if (strcmp(key, "dashboard") == 0) {
    setSettingValue(SET_DASHBOARD, (uint32_t)atol(valStr));
  } else if (strcmp(key, "invertDial") == 0) {
    setSettingValue(SET_INVERT_DIAL, (uint32_t)atol(valStr));
  } else if (strcmp(key, "name") == 0) {
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
    int slot = 0;
    const char* p = valStr;
    while (slot < NUM_SLOTS && *p) {
      settings.keySlots[slot] = (uint8_t)atoi(p);
      if (settings.keySlots[slot] >= NUM_KEYS) {
        settings.keySlots[slot] = NUM_KEYS - 1;
      }
      slot++;
      while (*p && *p != ',') p++;
      if (*p == ',') p++;
    }
    for (; slot < NUM_SLOTS; slot++) {
      settings.keySlots[slot] = NUM_KEYS - 1;
    }
  } else if (strcmp(key, "wmode") == 0) {
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
    } else if (strcmp(field, "dMax") == 0) {
      mode.modeDurMaxSec = (uint16_t)min(65535, max(10, atoi(fVal)));
    } else if (strcmp(field, "pMin") == 0) {
      mode.profileStintMinSec = (uint16_t)max(5, atoi(fVal));
    } else if (strcmp(field, "pMax") == 0) {
      mode.profileStintMaxSec = (uint16_t)max(5, atoi(fVal));
    } else if (field[0] == 't' && field[1] >= '0' && field[1] <= '2' && field[2] == '\0') {
      uint8_t pIdx = field[1] - '0';
      PhaseTiming& t = mode.timing[pIdx];
      const char* pp = fVal;
      uint32_t vals[12];
      for (int i = 0; i < 12; i++) {
        vals[i] = (uint32_t)atol(pp);
        while (*pp && *pp != ',') pp++;
        if (*pp == ',') pp++;
        else if (i < 11) { currentWriter("-err:wmode timing needs 12 values"); return; }
      }
      t.burstKeysMin = (uint8_t)vals[0];
      t.burstKeysMax = (uint8_t)vals[1];
      t.interKeyMinMs = (uint16_t)vals[2];
      t.interKeyMaxMs = (uint16_t)vals[3];
      t.burstGapMinMs = vals[4];
      t.burstGapMaxMs = vals[5];
      t.keyHoldMinMs = (uint16_t)vals[6];
      t.keyHoldMaxMs = (uint16_t)vals[7];
      t.mouseDurMinMs = vals[8];
      t.mouseDurMaxMs = vals[9];
      t.idleDurMinMs = vals[10];
      t.idleDurMaxMs = vals[11];
    } else {
      currentWriter("-err:unknown wmode field");
      return;
    }
    currentWriter("+ok");
    return;
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

  scheduleNextKey();
  scheduleNextMouseState();
  currentWriter("+ok");
}

// ============================================================================
// !save
// ============================================================================

static void cmdSave() {
  saveSettings();
  saveSimData();
  if (statsDirty) { saveStats(); statsDirty = false; }
  currentWriter("+ok");
}

// ============================================================================
// !defaults
// ============================================================================

static void cmdDefaults() {
  loadDefaults();
  scheduleNextKey();
  scheduleNextMouseState();
  pickNextKey();
  currentProfile = PROFILE_NORMAL;
  resetSimDataDefaults();
  currentWriter("+ok");
}

// ============================================================================
// !reboot
// ============================================================================

static void cmdReboot() {
  currentWriter("+ok");
  Serial.flush();
  delay(100);
  ESP.restart();
}

// ============================================================================
// ?wmode:N
// ============================================================================

static void cmdQueryWorkMode(uint8_t idx) {
  const WorkModeDef& m = workModes[idx];
  char buf[640];
  int len = snprintf(buf, sizeof(buf),
    "!wmode|idx=%d|name=%s|kb=%d|wL=%d|wN=%d|wB=%d",
    idx, m.shortName, m.kbPercent,
    m.profileWeights.lazyPct, m.profileWeights.normalPct, m.profileWeights.busyPct);
  if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;

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

// ============================================================================
// ?simblocks:N
// ============================================================================

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

#include "serial_cmd.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "schedule.h"
#include "sim_data.h"
#include "orchestrator.h"
#include "platform_hal.h"

// ============================================================================
// CYD Serial Command Handler
// Reuses the text protocol format (? / = / ! prefixes) from nRF52.
// processCommand() is duplicated here for Phase 2; will be extracted to
// common in a future refactor.
// ============================================================================

static ResponseWriter currentWriter = nullptr;

static void serialWrite(const char* msg) {
  Serial.println(msg);
}

// Line buffer for serial commands
#define SERIAL_BUF_SIZE 128
static char serialBuf[SERIAL_BUF_SIZE];
static uint8_t serialBufPos = 0;
static bool serialBufOverflow = false;

// Status push throttling
static bool serialStatusPushEnabled = false;
static unsigned long lastStatusPushMs = 0;
#define STATUS_PUSH_MIN_MS 200

// Forward declarations
static void cmdQueryStatus();
static void cmdQuerySettings();
static void cmdQueryKeys();
static void cmdSetValue(const char* body);

void processCommand(const char* line, ResponseWriter writer) {
  currentWriter = writer;

  if (line[0] == '?') {
    const char* cmd = line + 1;
    if (strcmp(cmd, "status") == 0) {
      cmdQueryStatus();
    } else if (strcmp(cmd, "settings") == 0) {
      cmdQuerySettings();
    } else if (strcmp(cmd, "keys") == 0) {
      cmdQueryKeys();
    } else {
      currentWriter("-err:unknown query");
    }
  } else if (line[0] == '=') {
    cmdSetValue(line + 1);
  } else if (line[0] == '!') {
    const char* cmd = line + 1;
    if (strcmp(cmd, "save") == 0) {
      saveSettings();
      currentWriter("+ok");
    } else if (strcmp(cmd, "defaults") == 0) {
      loadDefaults();
      saveSettings();
      currentWriter("+ok");
    } else if (strcmp(cmd, "reboot") == 0) {
      currentWriter("+ok");
      delay(100);
      ESP.restart();
    } else if (strcmp(cmd, "dfu") == 0 || strcmp(cmd, "serialdfu") == 0) {
      currentWriter("-err:not supported on CYD");
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

static void cmdQueryStatus() {
  unsigned long uptime = millis() - startTime;
  char buf[200];
  snprintf(buf, sizeof(buf),
    "!status|connected=%d|platform=cyd|kb=%d|ms=%d|profile=%d|mode=%d"
    "|mouseState=%d|uptime=%lu|kbNext=%s|timeSynced=%d|schedSleeping=%d",
    deviceConnected ? 1 : 0,
    keyEnabled ? 1 : 0, mouseEnabled ? 1 : 0,
    (int)currentProfile, (int)currentMode,
    (int)mouseState, uptime,
    (nextKeyIndex < NUM_KEYS) ? AVAILABLE_KEYS[nextKeyIndex].name : "???",
    timeSynced ? 1 : 0, scheduleSleeping ? 1 : 0);
  currentWriter(buf);
}

static void cmdQuerySettings() {
  char buf[400];
  snprintf(buf, sizeof(buf),
    "!settings|keyMin=%lu|keyMax=%lu|mouseJig=%lu|mouseIdle=%lu"
    "|mouseAmp=%d|mouseStyle=%d|lazy=%d|busy=%d"
    "|dispBright=%d|saverBright=%d|saverTimeout=%d"
    "|scroll=%d|opMode=%d|jobSim=%d|jobPerf=%d|jobStart=%d"
    "|name=%s",
    (unsigned long)settings.keyIntervalMin, (unsigned long)settings.keyIntervalMax,
    (unsigned long)settings.mouseJiggleDuration, (unsigned long)settings.mouseIdleDuration,
    settings.mouseAmplitude, settings.mouseStyle,
    settings.lazyPercent, settings.busyPercent,
    settings.displayBrightness, settings.saverBrightness, settings.saverTimeout,
    settings.scrollEnabled, settings.operationMode,
    settings.jobSimulation, settings.jobPerformance, settings.jobStartTime,
    settings.deviceName);
  currentWriter(buf);
}

static void cmdQueryKeys() {
  char buf[300];
  int pos = snprintf(buf, sizeof(buf), "!keys");
  for (int i = 0; i < NUM_KEYS && pos < (int)sizeof(buf) - 20; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "|%s", AVAILABLE_KEYS[i].name);
  }
  currentWriter(buf);
}

static void cmdSetValue(const char* body) {
  // Parse "key:value" format
  const char* colon = strchr(body, ':');
  if (!colon) { currentWriter("-err:missing colon"); return; }

  char key[32];
  size_t keyLen = colon - body;
  if (keyLen >= sizeof(key)) { currentWriter("-err:key too long"); return; }
  memcpy(key, body, keyLen);
  key[keyLen] = '\0';
  const char* valStr = colon + 1;

  // Match known keys to settings
  if (strcmp(key, "keyMin") == 0) setSettingValue(SET_KEY_MIN, atol(valStr));
  else if (strcmp(key, "keyMax") == 0) setSettingValue(SET_KEY_MAX, atol(valStr));
  else if (strcmp(key, "mouseJig") == 0) setSettingValue(SET_MOUSE_JIG, atol(valStr));
  else if (strcmp(key, "mouseIdle") == 0) setSettingValue(SET_MOUSE_IDLE, atol(valStr));
  else if (strcmp(key, "mouseAmp") == 0) setSettingValue(SET_MOUSE_AMP, atol(valStr));
  else if (strcmp(key, "mouseStyle") == 0) setSettingValue(SET_MOUSE_STYLE, atol(valStr));
  else if (strcmp(key, "lazy") == 0) setSettingValue(SET_LAZY_PCT, atol(valStr));
  else if (strcmp(key, "busy") == 0) setSettingValue(SET_BUSY_PCT, atol(valStr));
  else if (strcmp(key, "scroll") == 0) setSettingValue(SET_SCROLL, atol(valStr));
  else if (strcmp(key, "opMode") == 0) setSettingValue(SET_OP_MODE, atol(valStr));
  else if (strcmp(key, "jobSim") == 0) setSettingValue(SET_JOB_SIM, atol(valStr));
  else if (strcmp(key, "jobPerf") == 0) setSettingValue(SET_JOB_PERFORMANCE, atol(valStr));
  else if (strcmp(key, "jobStart") == 0) setSettingValue(SET_JOB_START_TIME, atol(valStr));
  else if (strcmp(key, "name") == 0) {
    strncpy(settings.deviceName, valStr, 14);
    settings.deviceName[14] = '\0';
  }
  else if (strcmp(key, "statusPush") == 0) {
    serialStatusPushEnabled = atoi(valStr) != 0;
  }
  else { currentWriter("-err:unknown key"); return; }

  currentWriter("+ok");
}

// ============================================================================
// Serial debug commands (single-character)
// ============================================================================

void handleSerialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    // Single-character debug commands
    if (serialBufPos == 0 && c != '?' && c != '=' && c != '!') {
      switch (c) {
        case 'h':
          Serial.println("--- CYD Serial Commands ---");
          Serial.println("h = Help");
          Serial.println("s = Status");
          Serial.println("d = Dump settings");
          Serial.println("t = Toggle status push");
          Serial.println("--- Protocol: ?query / =key:value / !action ---");
          break;
        case 's': printStatus(); break;
        case 'd':
          processCommand("?settings", serialWrite);
          break;
        case 't':
          serialStatusPushEnabled = !serialStatusPushEnabled;
          Serial.print("Status push: ");
          Serial.println(serialStatusPushEnabled ? "ON" : "OFF");
          break;
        case '\n': case '\r': break;  // ignore empty lines
        default:
          Serial.print("Unknown command: ");
          Serial.println(c);
          break;
      }
      continue;
    }

    // Multi-character protocol commands (line-buffered)
    if (c == '\n' || c == '\r') {
      if (serialBufPos > 0) {
        if (serialBufOverflow) {
          serialWrite("-err:cmd too long");
        } else {
          serialBuf[serialBufPos] = '\0';
          processCommand(serialBuf, serialWrite);
        }
        serialBufPos = 0;
        serialBufOverflow = false;
      }
    } else if (serialBufPos < SERIAL_BUF_SIZE - 1) {
      serialBuf[serialBufPos++] = c;
    } else {
      serialBufOverflow = true;
    }
  }
}

void printStatus() {
  processCommand("?status", serialWrite);
}

void pushSerialStatus() {
  if (!serialStatusPushEnabled) return;
  unsigned long now = millis();
  if (now - lastStatusPushMs < STATUS_PUSH_MIN_MS) return;
  lastStatusPushMs = now;
  processCommand("?status", serialWrite);
}

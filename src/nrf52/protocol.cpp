#include <Arduino.h>
#include <ArduinoJson.h>
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
#include "protocol_json.h"

// PlatformIO's nordicnrf52 builder adds -Wl,--wrap=realloc, but the Adafruit
// nRF52 framework only provides __wrap_malloc/__wrap_free (heap_3.c). ArduinoJson
// v7 needs realloc, so provide the missing wrapper that delegates to the real one.
extern "C" void* __real_realloc(void* ptr, size_t size);
extern "C" void* __wrap_realloc(void* ptr, size_t size) {
  return __real_realloc(ptr, size);
}

// ============================================================================
// JSON config protocol for nRF52
// Lines starting with '{' are dispatched here from processCommand().
// ============================================================================

// Forward declarations
static void jsonQueryStatus(JsonDocument& resp);
static void jsonQuerySettings(JsonDocument& resp);
static void jsonQueryKeys(JsonDocument& resp);
static void jsonQueryDecoys(JsonDocument& resp);
static void jsonQueryWorkMode(JsonDocument& resp, uint8_t idx);
static void jsonQuerySimBlocks(JsonDocument& resp, uint8_t jobIdx);
static void jsonHandleSet(JsonObject data, ResponseWriter writer);
static void jsonHandleCommand(const char* key, ResponseWriter writer);
static void sendJsonResponse(JsonDocument& doc, ResponseWriter writer);
static void sendJsonOk(ResponseWriter writer);
static void sendJsonError(const char* msg, ResponseWriter writer);

// ============================================================================
// Main entry point — returns true if the line was valid JSON
// ============================================================================

bool processJsonCommand(const char* json, ResponseWriter writer) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    sendJsonError("parse error", writer);
    return true;  // was JSON-shaped, just malformed
  }

  const char* type = doc["t"].as<const char*>();
  if (!type) {
    sendJsonError("missing type", writer);
    return true;
  }

  if (strcmp(type, "q") == 0) {
    // Query
    const char* key = doc["k"].as<const char*>();
    if (!key) {
      sendJsonError("missing key", writer);
      return true;
    }

    JsonDocument resp;
    resp["t"] = "r";
    resp["k"] = key;

    if (strcmp(key, "status") == 0) {
      jsonQueryStatus(resp);
    } else if (strcmp(key, "settings") == 0) {
      jsonQuerySettings(resp);
    } else if (strcmp(key, "keys") == 0) {
      jsonQueryKeys(resp);
    } else if (strcmp(key, "decoys") == 0) {
      jsonQueryDecoys(resp);
    } else if (strcmp(key, "wmode") == 0) {
      uint8_t idx = doc["i"] | (uint8_t)0;
      if (idx >= WMODE_COUNT) {
        sendJsonError("invalid mode index", writer);
        return true;
      }
      jsonQueryWorkMode(resp, idx);
    } else if (strcmp(key, "simblocks") == 0) {
      uint8_t idx = doc["i"] | (uint8_t)0;
      if (idx >= JOB_SIM_COUNT) {
        sendJsonError("invalid job index", writer);
        return true;
      }
      jsonQuerySimBlocks(resp, idx);
    } else {
      sendJsonError("unknown query", writer);
      return true;
    }

    sendJsonResponse(resp, writer);

  } else if (strcmp(type, "s") == 0) {
    // Set
    JsonObject data = doc["d"].as<JsonObject>();
    if (data.isNull()) {
      sendJsonError("missing data", writer);
      return true;
    }
    jsonHandleSet(data, writer);

  } else if (strcmp(type, "c") == 0) {
    // Command
    const char* key = doc["k"].as<const char*>();
    if (!key) {
      sendJsonError("missing key", writer);
      return true;
    }
    jsonHandleCommand(key, writer);

  } else {
    sendJsonError("unknown type", writer);
  }

  return true;
}

// ============================================================================
// Push status as JSON
// ============================================================================

void pushJsonStatus(ResponseWriter writer) {
  JsonDocument doc;
  doc["t"] = "p";
  doc["k"] = "status";
  jsonQueryStatus(doc);
  sendJsonResponse(doc, writer);
}

// ============================================================================
// Query handlers
// ============================================================================

static void jsonQueryStatus(JsonDocument& resp) {
  JsonObject d = resp["d"].to<JsonObject>();

  d["connected"] = deviceConnected;
  d["usb"] = usbConnected;
  d["kb"] = keyEnabled;
  d["ms"] = mouseEnabled;
  d["bat"] = batteryPercent;
  d["batMv"] = (int)(batteryVoltage * 1000);
  d["profile"] = (uint8_t)currentProfile;
  d["mode"] = (uint8_t)currentMode;
  d["mouseState"] = (uint8_t)mouseState;
  d["uptime"] = (unsigned long)(millis() - startTime);
  d["kbNext"] = (nextKeyIndex < NUM_KEYS) ? AVAILABLE_KEYS[nextKeyIndex].name : "???";
  d["timeSynced"] = timeSynced;
  d["schedSleeping"] = scheduleSleeping;
  d["platform"] = "nrf52";
  d["totalKeys"] = stats.totalKeystrokes;
  d["totalMousePx"] = stats.totalMousePixels;
  d["totalClicks"] = stats.totalMouseClicks;

  if (timeSynced) {
    d["daySecs"] = (unsigned long)currentDaySeconds();
  }

  // Mode-specific status
  if (settings.operationMode == OP_RACER) {
    d["rcrState"] = (uint8_t)gRcr.state;
    d["rcrScore"] = gRcr.score;
  } else if (settings.operationMode == OP_SNAKE) {
    d["snkState"] = (uint8_t)gSnk.state;
    d["snkScore"] = gSnk.score;
    d["snkLen"] = gSnk.length;
  } else if (settings.operationMode == OP_BREAKOUT) {
    d["brkState"] = (uint8_t)gBrk.state;
    d["brkLevel"] = gBrk.level;
    d["brkScore"] = gBrk.score;
    d["brkLives"] = gBrk.lives;
  } else if (settings.operationMode == OP_VOLUME) {
    d["volMuted"] = volMuted;
    d["volPlaying"] = volPlaying;
  } else if (settings.operationMode == OP_SIMULATION) {
    d["simBlock"] = orch.blockIdx;
    d["simMode"] = (uint8_t)orch.modeId;
    d["simPhase"] = (uint8_t)orch.phase;
    d["simProfile"] = (uint8_t)orch.autoProfile;
  }
}

static void jsonQuerySettings(JsonDocument& resp) {
  JsonObject d = resp["d"].to<JsonObject>();

  // Timing
  d["keyMin"] = settings.keyIntervalMin;
  d["keyMax"] = settings.keyIntervalMax;
  d["mouseJig"] = settings.mouseJiggleDuration;
  d["mouseIdle"] = settings.mouseIdleDuration;
  d["mouseAmp"] = settings.mouseAmplitude;
  d["mouseStyle"] = settings.mouseStyle;

  // Profile percentages
  d["lazyPct"] = settings.lazyPercent;
  d["busyPct"] = settings.busyPercent;

  // Display
  d["dispBright"] = settings.displayBrightness;
  d["saverBright"] = settings.saverBrightness;
  d["saverTimeout"] = settings.saverTimeout;
  d["animStyle"] = settings.animStyle;

  // Device
  d["name"] = settings.deviceName;
  d["btWhileUsb"] = settings.btWhileUsb;
  d["scroll"] = settings.scrollEnabled;
  d["dashboard"] = settings.dashboardEnabled;
  d["invertDial"] = settings.invertDial;
  d["decoy"] = settings.decoyIndex;

  // Schedule
  d["schedMode"] = settings.scheduleMode;
  d["schedStart"] = settings.scheduleStart;
  d["schedEnd"] = settings.scheduleEnd;

  // Operation / simulation
  d["opMode"] = settings.operationMode;
  d["jobSim"] = settings.jobSimulation;
  d["jobPerf"] = settings.jobPerformance;
  d["jobStart"] = settings.jobStartTime;
  d["phantom"] = settings.phantomClicks;
  d["winSwitch"] = settings.windowSwitching;
  d["switchKeys"] = settings.switchKeys;
  d["headerDisp"] = settings.headerDisplay;

  // Sound
  d["sound"] = settings.soundEnabled;
  d["soundType"] = settings.soundType;
  d["sysSounds"] = settings.systemSoundEnabled;

  // Volume control
  d["volumeTheme"] = settings.volumeTheme;
  d["encButton"] = settings.encButtonAction;
  d["sideButton"] = settings.sideButtonAction;

  // Game settings (nRF52-specific — C6 doesn't support games)
  d["ballSpeed"] = settings.ballSpeed;
  d["paddleSize"] = settings.paddleSize;
  d["startLives"] = settings.startLives;
  d["highScore"] = settings.highScore;
  d["snakeSpeed"] = settings.snakeSpeed;
  d["snakeWalls"] = settings.snakeWalls;
  d["snakeHiScore"] = settings.snakeHighScore;
  d["racerSpeed"] = settings.racerSpeed;
  d["racerHiScore"] = settings.racerHighScore;

  // Shift/lunch
  d["shiftDur"] = settings.shiftDuration;
  d["lunchDur"] = settings.lunchDuration;

  // Key slots
  JsonArray slotsArr = d["slots"].to<JsonArray>();
  for (int i = 0; i < NUM_SLOTS; i++) {
    slotsArr.add(settings.keySlots[i]);
  }

  // Click slots
  JsonArray clickArr = d["clickSlots"].to<JsonArray>();
  for (int i = 0; i < NUM_CLICK_SLOTS; i++) {
    clickArr.add(settings.clickSlots[i]);
  }

  // Lifetime stats
  d["totalKeys"] = stats.totalKeystrokes;
  d["totalMousePx"] = stats.totalMousePixels;
  d["totalClicks"] = stats.totalMouseClicks;
}

static void jsonQueryKeys(JsonDocument& resp) {
  JsonArray d = resp["d"].to<JsonArray>();
  for (int i = 0; i < NUM_KEYS; i++) {
    d.add(AVAILABLE_KEYS[i].name);
  }
}

static void jsonQueryDecoys(JsonDocument& resp) {
  JsonArray d = resp["d"].to<JsonArray>();
  for (int i = 0; i < DECOY_COUNT; i++) {
    d.add(DECOY_NAMES[i]);
  }
}

static void jsonQueryWorkMode(JsonDocument& resp, uint8_t idx) {
  const WorkModeDef& m = workModes[idx];
  JsonObject d = resp["d"].to<JsonObject>();

  d["idx"] = idx;
  d["name"] = m.shortName;
  d["kb"] = m.kbPercent;

  // Profile weights
  JsonObject w = d["weights"].to<JsonObject>();
  w["L"] = m.profileWeights.lazyPct;
  w["N"] = m.profileWeights.normalPct;
  w["B"] = m.profileWeights.busyPct;

  // Duration ranges
  d["dMin"] = m.modeDurMinSec;
  d["dMax"] = m.modeDurMaxSec;
  d["pMin"] = m.profileStintMinSec;
  d["pMax"] = m.profileStintMaxSec;

  // Phase timing for each profile (LAZY, NORMAL, BUSY)
  JsonArray timingArr = d["timing"].to<JsonArray>();
  for (int p = 0; p < PROFILE_COUNT; p++) {
    const PhaseTiming& t = m.timing[p];
    JsonObject tObj = timingArr.add<JsonObject>();
    tObj["bkMin"] = t.burstKeysMin;
    tObj["bkMax"] = t.burstKeysMax;
    tObj["ikMin"] = t.interKeyMinMs;
    tObj["ikMax"] = t.interKeyMaxMs;
    tObj["bgMin"] = t.burstGapMinMs;
    tObj["bgMax"] = t.burstGapMaxMs;
    tObj["khMin"] = t.keyHoldMinMs;
    tObj["khMax"] = t.keyHoldMaxMs;
    tObj["mdMin"] = t.mouseDurMinMs;
    tObj["mdMax"] = t.mouseDurMaxMs;
    tObj["idMin"] = t.idleDurMinMs;
    tObj["idMax"] = t.idleDurMaxMs;
  }
}

static void jsonQuerySimBlocks(JsonDocument& resp, uint8_t jobIdx) {
  const DayTemplate& tmpl = DAY_TEMPLATES[jobIdx];
  JsonObject d = resp["d"].to<JsonObject>();

  d["job"] = jobIdx;
  JsonArray blocks = d["blocks"].to<JsonArray>();
  for (int b = 0; b < tmpl.numBlocks; b++) {
    const TimeBlock& block = tmpl.blocks[b];
    JsonObject bObj = blocks.add<JsonObject>();
    bObj["name"] = block.name;
    bObj["start"] = block.startMinutes;
    bObj["dur"] = block.durationMinutes;
    JsonArray modes = bObj["modes"].to<JsonArray>();
    for (int m = 0; m < block.numModes; m++) {
      JsonObject mObj = modes.add<JsonObject>();
      mObj["id"] = (uint8_t)block.modes[m].modeId;
      mObj["w"] = block.modes[m].weight;
    }
  }
}

// ============================================================================
// Set handler — partial update of settings
// ============================================================================

// Helper struct for mapping JSON keys to SettingId
struct SettingKeyMap {
  const char* key;
  uint8_t settingId;
};

static const SettingKeyMap SETTING_MAP[] = {
  { "keyMin",      SET_KEY_MIN },
  { "keyMax",      SET_KEY_MAX },
  { "mouseJig",    SET_MOUSE_JIG },
  { "mouseIdle",   SET_MOUSE_IDLE },
  { "mouseAmp",    SET_MOUSE_AMP },
  { "mouseStyle",  SET_MOUSE_STYLE },
  { "lazyPct",     SET_LAZY_PCT },
  { "busyPct",     SET_BUSY_PCT },
  { "dispBright",  SET_DISPLAY_BRIGHT },
  { "saverBright", SET_SAVER_BRIGHT },
  { "saverTimeout",SET_SAVER_TIMEOUT },
  { "animStyle",   SET_ANIMATION },
  { "btWhileUsb",  SET_BT_WHILE_USB },
  { "scroll",      SET_SCROLL },
  { "dashboard",   SET_DASHBOARD },
  { "invertDial",  SET_INVERT_DIAL },
  { "schedMode",   SET_SCHEDULE_MODE },
  { "schedStart",  SET_SCHEDULE_START },
  { "schedEnd",    SET_SCHEDULE_END },
  { "opMode",      SET_OP_MODE },
  { "jobSim",      SET_JOB_SIM },
  { "jobPerf",     SET_JOB_PERFORMANCE },
  { "jobStart",    SET_JOB_START_TIME },
  { "phantom",     SET_PHANTOM_CLICKS },
  { "winSwitch",   SET_WINDOW_SWITCH },
  { "switchKeys",  SET_SWITCH_KEYS },
  { "headerDisp",  SET_HEADER_DISPLAY },
  { "sound",       SET_SOUND_ENABLED },
  { "soundType",   SET_SOUND_TYPE },
  { "sysSounds",   SET_SYSTEM_SOUND },
  { "volumeTheme", SET_VOLUME_THEME },
  { "encButton",   SET_ENC_BUTTON },
  { "sideButton",  SET_SIDE_BUTTON },
  { "ballSpeed",   SET_BALL_SPEED },
  { "paddleSize",  SET_PADDLE_SIZE },
  { "startLives",  SET_START_LIVES },
  { "snakeSpeed",  SET_SNAKE_SPEED },
  { "snakeWalls",  SET_SNAKE_WALLS },
  { "racerSpeed",  SET_RACER_SPEED },
  { "shiftDur",    SET_SHIFT_DURATION },
  { "lunchDur",    SET_LUNCH_DURATION },
};
static const int SETTING_MAP_COUNT = sizeof(SETTING_MAP) / sizeof(SETTING_MAP[0]);

static const char* jsonValidateSetObject(JsonObject data) {
  for (JsonPair kv : data) {
    const char* key = kv.key().c_str();

    if (strcmp(key, "name") == 0) {
      const char* val = kv.value().as<const char*>();
      if (!val) return "name must be string";
      for (const char* p = val; *p; p++) {
        if (*p < 0x20 || *p > 0x7E) return "invalid name chars";
      }
      continue;
    }

    if (strcmp(key, "decoy") == 0) continue;

    if (strcmp(key, "slots") == 0) {
      if (kv.value().as<JsonArray>().isNull()) return "slots must be array";
      continue;
    }

    if (strcmp(key, "clickSlots") == 0) {
      if (kv.value().as<JsonArray>().isNull()) return "clickSlots must be array";
      continue;
    }

    if (strcmp(key, "time") == 0) continue;
    if (strcmp(key, "totalKeys") == 0) continue;
    if (strcmp(key, "totalMousePx") == 0) continue;
    if (strcmp(key, "totalClicks") == 0) continue;
    if (strcmp(key, "statusPush") == 0) continue;

    bool found = false;
    for (int i = 0; i < SETTING_MAP_COUNT; i++) {
      if (strcmp(key, SETTING_MAP[i].key) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return "unknown key";
  }
  return nullptr;
}

static void jsonHandleSet(JsonObject data, ResponseWriter writer) {
  const char* verr = jsonValidateSetObject(data);
  if (verr) {
    sendJsonError(verr, writer);
    return;
  }

  bool needReschedule = false;

  for (JsonPair kv : data) {
    const char* key = kv.key().c_str();

    // --- Special keys (not via setSettingValue) ---

    if (strcmp(key, "name") == 0) {
      const char* val = kv.value().as<const char*>();
      if (!val) { sendJsonError("name must be string", writer); return; }
      for (const char* p = val; *p; p++) {
        if (*p < 0x20 || *p > 0x7E) {
          sendJsonError("invalid name chars", writer);
          return;
        }
      }
      strncpy(settings.deviceName, val, NAME_MAX_LEN);
      settings.deviceName[NAME_MAX_LEN] = '\0';
      continue;
    }

    if (strcmp(key, "decoy") == 0) {
      uint8_t idx = kv.value().as<uint8_t>();
      if (idx > DECOY_COUNT) idx = 0;
      settings.decoyIndex = idx;
      if (idx > 0 && idx <= DECOY_COUNT) {
        strncpy(settings.deviceName, DECOY_NAMES[idx - 1], NAME_MAX_LEN);
        settings.deviceName[NAME_MAX_LEN] = '\0';
      }
      continue;
    }

    if (strcmp(key, "slots") == 0) {
      JsonArray arr = kv.value().as<JsonArray>();
      if (arr.isNull()) { sendJsonError("slots must be array", writer); return; }
      int slot = 0;
      for (JsonVariant v : arr) {
        if (slot >= NUM_SLOTS) break;
        uint8_t val = v.as<uint8_t>();
        settings.keySlots[slot] = (val >= NUM_KEYS) ? (NUM_KEYS - 1) : val;
        slot++;
      }
      for (; slot < NUM_SLOTS; slot++) {
        settings.keySlots[slot] = NUM_KEYS - 1;
      }
      needReschedule = true;
      continue;
    }

    if (strcmp(key, "clickSlots") == 0) {
      JsonArray arr = kv.value().as<JsonArray>();
      if (arr.isNull()) { sendJsonError("clickSlots must be array", writer); return; }
      int slot = 0;
      for (JsonVariant v : arr) {
        if (slot >= NUM_CLICK_SLOTS) break;
        uint8_t val = v.as<uint8_t>();
        settings.clickSlots[slot] = (val >= NUM_CLICK_TYPES) ? (NUM_CLICK_TYPES - 1) : val;
        slot++;
      }
      for (; slot < NUM_CLICK_SLOTS; slot++) {
        settings.clickSlots[slot] = NUM_CLICK_TYPES - 1;
      }
      continue;
    }

    if (strcmp(key, "time") == 0) {
      uint32_t secs = kv.value().as<uint32_t>();
      if (secs >= 86400) secs = 0;
      syncTime(secs);
      continue;
    }

    if (strcmp(key, "totalKeys") == 0) {
      stats.totalKeystrokes = kv.value().as<uint32_t>();
      statsDirty = true;
      continue;
    }

    if (strcmp(key, "totalMousePx") == 0) {
      stats.totalMousePixels = kv.value().as<uint32_t>();
      statsDirty = true;
      continue;
    }

    if (strcmp(key, "totalClicks") == 0) {
      stats.totalMouseClicks = kv.value().as<uint32_t>();
      statsDirty = true;
      continue;
    }

    if (strcmp(key, "statusPush") == 0) {
      serialStatusPush = kv.value().as<bool>();
      jsonPushMode = true;  // push in JSON since set via JSON
      continue;
    }

    // --- Standard settings via setSettingValue ---
    bool found = false;
    for (int i = 0; i < SETTING_MAP_COUNT; i++) {
      if (strcmp(key, SETTING_MAP[i].key) == 0) {
        setSettingValue(SETTING_MAP[i].settingId, kv.value().as<uint32_t>());
        needReschedule = true;
        found = true;
        break;
      }
    }

    if (!found) {
      sendJsonError("unknown key", writer);
      return;
    }
  }

  if (needReschedule) {
    scheduleNextKey();
    scheduleNextMouseState();
  }

  sendJsonOk(writer);
}

// ============================================================================
// Command handler
// ============================================================================

static void jsonHandleCommand(const char* key, ResponseWriter writer) {
  if (strcmp(key, "save") == 0) {
    saveSettings();
    saveSimData();
    if (statsDirty) { saveStats(); statsDirty = false; }
    sendJsonOk(writer);
  } else if (strcmp(key, "defaults") == 0) {
    loadDefaults();
    scheduleNextKey();
    scheduleNextMouseState();
    pickNextKey();
    currentProfile = PROFILE_NORMAL;
    resetSimDataDefaults();
    sendJsonOk(writer);
  } else if (strcmp(key, "reboot") == 0) {
    sendJsonOk(writer);
    Serial.flush();
    delay(100);
    NVIC_SystemReset();
  } else if (strcmp(key, "dfu") == 0) {
    sendJsonOk(writer);
    Serial.flush();
    delay(100);
    resetToDfu();
  } else if (strcmp(key, "serialdfu") == 0) {
    sendJsonOk(writer);
    Serial.flush();
    delay(100);
    resetToSerialDfu();
  } else if (strcmp(key, "savesim") == 0) {
    saveSimData();
    sendJsonOk(writer);
  } else if (strcmp(key, "resetsim") == 0) {
    resetSimDataDefaults();
    sendJsonOk(writer);
  } else {
    sendJsonError("unknown command", writer);
  }
}

// ============================================================================
// Response helpers
// ============================================================================

static void sendJsonResponse(JsonDocument& doc, ResponseWriter writer) {
  static char buf[JSON_RESP_BUF];  // static — not reentrant, saves 1.5KB stack
  size_t len = serializeJson(doc, buf, sizeof(buf));
  if (len >= sizeof(buf)) {
    sendJsonError("response too large", writer);
    return;
  }
  writer(buf);
}

static void sendJsonOk(ResponseWriter writer) {
  writer("{\"t\":\"ok\"}");
}

static void sendJsonError(const char* msg, ResponseWriter writer) {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"t\":\"err\",\"m\":\"%s\"}", msg);
  writer(buf);
}

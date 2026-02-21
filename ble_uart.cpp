#include "ble_uart.h"
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "schedule.h"

// Line buffer for accumulating UART bytes
#define UART_BUF_SIZE 128
static char uartBuf[UART_BUF_SIZE];
static uint8_t uartBufPos = 0;

// File-scoped writer pointer — set by processCommand(), used by cmd*() helpers
static ResponseWriter currentWriter = nullptr;

// Forward declarations
static void bleWrite(const String& msg);
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
// Poll: read bytes into line buffer, dispatch on newline
// Called from loop() in ghost_operator.ino
// ----------------------------------------------------------------------------
void handleBleUart() {
  while (bleuart.available()) {
    char c = (char)bleuart.read();
    if (c == '\n' || c == '\r') {
      if (uartBufPos > 0) {
        uartBuf[uartBufPos] = '\0';
        processCommand(uartBuf, bleWrite);
        uartBufPos = 0;
      }
    } else if (uartBufPos < UART_BUF_SIZE - 1) {
      uartBuf[uartBufPos++] = c;
    }
  }
}

// ----------------------------------------------------------------------------
// Send a response string over BLE UART (appends newline)
// Chunks writes to 20 bytes for default MTU compatibility
// ----------------------------------------------------------------------------
static void bleWrite(const String& msg) {
  String out = msg + "\n";
  const char* data = out.c_str();
  uint16_t len = out.length();
  uint16_t offset = 0;
  while (offset < len) {
    uint16_t chunk = min((uint16_t)20, (uint16_t)(len - offset));
    bleuart.write((const uint8_t*)(data + offset), chunk);
    offset += chunk;
  }
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
  unsigned long now = millis();
  unsigned long uptime = now - startTime;

  String resp = "!status";
  resp += "|connected=";  resp += deviceConnected ? "1" : "0";
  resp += "|usb=";         resp += usbConnected ? "1" : "0";
  resp += "|kb=";          resp += keyEnabled ? "1" : "0";
  resp += "|ms=";          resp += mouseEnabled ? "1" : "0";
  resp += "|bat=";         resp += String(batteryPercent);
  resp += "|profile=";     resp += String((int)currentProfile);
  resp += "|mode=";        resp += String((int)currentMode);
  resp += "|mouseState=";  resp += String((int)mouseState);
  resp += "|uptime=";      resp += String(uptime);
  resp += "|kbNext=";      resp += String(AVAILABLE_KEYS[nextKeyIndex].name);
  resp += "|timeSynced=";  resp += timeSynced ? "1" : "0";
  resp += "|schedSleeping="; resp += scheduleSleeping ? "1" : "0";
  if (timeSynced) {
    resp += "|daySecs=";   resp += String(currentDaySeconds());
  }

  currentWriter(resp);
}

// ----------------------------------------------------------------------------
// ?settings — all persistent settings
// ----------------------------------------------------------------------------
static void cmdQuerySettings() {
  String resp = "!settings";
  resp += "|keyMin=";       resp += String(settings.keyIntervalMin);
  resp += "|keyMax=";       resp += String(settings.keyIntervalMax);
  resp += "|mouseJig=";     resp += String(settings.mouseJiggleDuration);
  resp += "|mouseIdle=";    resp += String(settings.mouseIdleDuration);
  resp += "|mouseAmp=";     resp += String(settings.mouseAmplitude);
  resp += "|mouseStyle=";   resp += String(settings.mouseStyle);
  resp += "|lazyPct=";      resp += String(settings.lazyPercent);
  resp += "|busyPct=";      resp += String(settings.busyPercent);
  resp += "|dispBright=";   resp += String(settings.displayBrightness);
  resp += "|saverBright=";  resp += String(settings.saverBrightness);
  resp += "|saverTimeout="; resp += String(settings.saverTimeout);
  resp += "|animStyle=";    resp += String(settings.animStyle);
  resp += "|name=";         resp += String(settings.deviceName);
  resp += "|btWhileUsb=";  resp += String(settings.btWhileUsb);
  resp += "|scroll=";      resp += String(settings.scrollEnabled);
  resp += "|dashboard=";   resp += String(settings.dashboardEnabled);
  resp += "|decoy=";       resp += String(settings.decoyIndex);
  resp += "|schedMode=";   resp += String(settings.scheduleMode);
  resp += "|schedStart=";  resp += String(settings.scheduleStart);
  resp += "|schedEnd=";    resp += String(settings.scheduleEnd);

  // Slots as comma-separated indices
  resp += "|slots=";
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (i > 0) resp += ",";
    resp += String(settings.keySlots[i]);
  }

  currentWriter(resp);
}

// ----------------------------------------------------------------------------
// ?keys — list of all available key names (populates dropdowns)
// ----------------------------------------------------------------------------
static void cmdQueryKeys() {
  String resp = "!keys";
  for (int i = 0; i < NUM_KEYS; i++) {
    resp += "|";
    resp += AVAILABLE_KEYS[i].name;
  }
  currentWriter(resp);
}

// ----------------------------------------------------------------------------
// ?decoys — list of all decoy preset names (populates dropdown)
// ----------------------------------------------------------------------------
static void cmdQueryDecoys() {
  String resp = "!decoys";
  for (int i = 0; i < DECOY_COUNT; i++) {
    resp += "|";
    resp += DECOY_NAMES[i];
  }
  currentWriter(resp);
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
  } else if (strcmp(key, "name") == 0) {
    // Device name — up to 14 chars
    strncpy(settings.deviceName, valStr, NAME_MAX_LEN);
    settings.deviceName[NAME_MAX_LEN] = '\0';
  } else if (strcmp(key, "decoy") == 0) {
    uint8_t idx = (uint8_t)atoi(valStr);
    if (idx > DECOY_COUNT) idx = 0;
    settings.decoyIndex = idx;
    // Sync deviceName when selecting a preset
    if (idx > 0) {
      strncpy(settings.deviceName, DECOY_NAMES[idx - 1], NAME_MAX_LEN);
      settings.deviceName[NAME_MAX_LEN] = '\0';
    }
  } else if (strcmp(key, "schedMode") == 0) {
    setSettingValue(SET_SCHEDULE_MODE, (uint32_t)atol(valStr));
  } else if (strcmp(key, "schedStart") == 0) {
    setSettingValue(SET_SCHEDULE_START, (uint32_t)atol(valStr));
  } else if (strcmp(key, "schedEnd") == 0) {
    setSettingValue(SET_SCHEDULE_END, (uint32_t)atol(valStr));
  } else if (strcmp(key, "time") == 0) {
    syncTime((uint32_t)atol(valStr));
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

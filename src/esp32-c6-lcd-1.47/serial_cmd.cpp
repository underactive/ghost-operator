#include <Arduino.h>
#include <lvgl.h>
#include "serial_cmd.h"
#include "ble_uart.h"
#include "state.h"
#include "keys.h"
#include "timing.h"
#include "schedule.h"
#include "sim_data.h"
#include "orchestrator.h"
#include "platform_hal.h"

// ============================================================================
// Screenshot — captures LVGL screen as BMP, base64-encoded over serial
// ============================================================================

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64Write(const uint8_t* data, size_t len) {
  char out[4];
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = (uint32_t)data[i] << 16;
    if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
    if (i + 2 < len) n |= data[i + 2];
    out[0] = B64[(n >> 18) & 0x3F];
    out[1] = B64[(n >> 12) & 0x3F];
    out[2] = (i + 1 < len) ? B64[(n >> 6) & 0x3F] : '=';
    out[3] = (i + 2 < len) ? B64[n & 0x3F] : '=';
    Serial.write(out, 4);
  }
}

// Render one row of the screen into a small draw buffer and return it.
// Uses lv_snapshot_take_to_draw_buf with a 1-row-tall buffer to avoid
// allocating the full 110KB framebuffer.
// Fallback: allocate a smaller snapshot (half height) if full fails.

static void takeScreenshot() {
  const uint16_t w = 320;
  const uint16_t h = 172;

  Serial.print("[SCREENSHOT] "); Serial.print(w); Serial.print("x"); Serial.println(h);
  Serial.print("[HEAP] Free: "); Serial.print(ESP.getFreeHeap());
  Serial.print("  Max block: "); Serial.println(ESP.getMaxAllocHeap());

  // Try full snapshot first — needs ~110KB contiguous
  lv_draw_buf_t* snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
  if (!snap) {
    Serial.println("[ERR] Snapshot failed — not enough contiguous RAM");
    Serial.println("[HINT] Disconnect BLE first to free memory, then try again");
    return;
  }

  uint32_t stride = snap->header.stride;
  const uint8_t* pixels = snap->data;

  // BMP file: 54-byte header + BGR888 pixel data (bottom-up)
  uint32_t rowBytes = w * 3;
  uint32_t rowPad = (4 - (rowBytes % 4)) % 4;
  uint32_t pixelDataSize = (rowBytes + rowPad) * h;
  uint32_t fileSize = 54 + pixelDataSize;

  uint8_t bmpHdr[54];
  memset(bmpHdr, 0, 54);
  bmpHdr[0] = 'B'; bmpHdr[1] = 'M';
  bmpHdr[2] = fileSize; bmpHdr[3] = fileSize >> 8;
  bmpHdr[4] = fileSize >> 16; bmpHdr[5] = fileSize >> 24;
  bmpHdr[10] = 54;
  bmpHdr[14] = 40;
  bmpHdr[18] = w; bmpHdr[19] = w >> 8;
  bmpHdr[22] = h; bmpHdr[23] = h >> 8;
  bmpHdr[26] = 1;
  bmpHdr[28] = 24;
  bmpHdr[34] = pixelDataSize; bmpHdr[35] = pixelDataSize >> 8;
  bmpHdr[36] = pixelDataSize >> 16; bmpHdr[37] = pixelDataSize >> 24;

  Serial.println("--- BMP START ---");
  b64Write(bmpHdr, 54);

  uint8_t rowBuf[960 + 4];  // 320*3 + 4 padding
  static const uint8_t pad[4] = {0};

  for (int y = h - 1; y >= 0; y--) {
    const uint8_t* srcRow = pixels + y * stride;
    for (int x = 0; x < w; x++) {
      uint16_t px = ((uint16_t)srcRow[x * 2 + 1] << 8) | srcRow[x * 2];
      uint8_t r5 = (px >> 11) & 0x1F;
      uint8_t g6 = (px >> 5) & 0x3F;
      uint8_t b5 = px & 0x1F;
      rowBuf[x * 3 + 0] = (b5 << 3) | (b5 >> 2);
      rowBuf[x * 3 + 1] = (g6 << 2) | (g6 >> 4);
      rowBuf[x * 3 + 2] = (r5 << 3) | (r5 >> 2);
    }
    memcpy(rowBuf + rowBytes, pad, rowPad);
    b64Write(rowBuf, rowBytes + rowPad);
  }

  Serial.println();
  Serial.println("--- BMP END ---");
  lv_draw_buf_destroy(snap);
  Serial.println("[OK] Screenshot done");
}

// ============================================================================
// Serial command handler for ESP32-C6
// Same text protocol (?, =, !) as nRF52 — JSON comes in Phase 3
// ============================================================================

// Line buffer for protocol commands arriving over USB serial
#define SERIAL_BUF_SIZE 512
static char serialBuf[SERIAL_BUF_SIZE];
static uint16_t serialBufPos = 0;
static bool serialBufOverflow = false;

// Serial response writer
static void serialWrite(const char* msg) {
  Serial.println(msg);
}

void pushSerialStatus() {
  if (!serialStatusPush) return;
  static unsigned long lastPush = 0;
  unsigned long now = millis();
  if (now - lastPush < 200) return;  // 200ms throttle
  lastPush = now;
  processCommand("?status", serialWrite);
}

void printStatus() {
  Serial.println("\n=== Status ===");
  Serial.print("Platform: C6\n");
  Serial.print("Mode: "); Serial.println(MODE_NAMES[currentMode]);
  Serial.print("Connected: "); Serial.println(deviceConnected ? "YES" : "NO");
  Serial.print("Keys ("); Serial.print(keyEnabled ? "ON" : "OFF"); Serial.print("): ");
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (i > 0) Serial.print(" ");
    if (i == activeSlot) Serial.print("[");
    Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
    if (i == activeSlot) Serial.print("]");
  }
  Serial.println();
  Serial.print("Mouse: "); Serial.println(mouseEnabled ? "ON" : "OFF");
  Serial.print("Mouse state: ");
  Serial.println(mouseState == MOUSE_IDLE ? "IDLE" : mouseState == MOUSE_JIGGLING ? "JIG" : "RTN");
  Serial.print("Operation mode: "); Serial.println((settings.operationMode < OP_MODE_COUNT) ? OP_MODE_NAMES[settings.operationMode] : "???");
  if (settings.operationMode == OP_SIMULATION) {
    Serial.println("--- Simulation ---");
    Serial.print("Job: "); Serial.println((settings.jobSimulation < JOB_SIM_COUNT) ? JOB_SIM_NAMES[settings.jobSimulation] : "???");
    Serial.print("Performance: "); Serial.println(settings.jobPerformance);
    Serial.print("Block: "); Serial.print(orch.blockIdx); Serial.print(" ("); Serial.print(currentBlockName()); Serial.println(")");
    Serial.print("Mode: "); Serial.print((int)orch.modeId); Serial.print(" ("); Serial.print(currentModeName()); Serial.println(")");
    Serial.print("Phase: "); Serial.println((orch.phase < PHASE_COUNT) ? PHASE_NAMES[orch.phase] : "???");
    Serial.print("Auto-profile: "); Serial.println((orch.autoProfile < PROFILE_COUNT) ? PROFILE_NAMES[orch.autoProfile] : "???");
  }
  Serial.print("Total keystrokes: "); Serial.println(stats.totalKeystrokes);
  Serial.print("Total mouse px: "); Serial.println(stats.totalMousePixels);
  Serial.print("Total clicks: "); Serial.println(stats.totalMouseClicks);
}

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();

    // If accumulating a protocol command, keep buffering
    if (serialBufPos > 0) {
      if (c == '\n' || c == '\r') {
        if (serialBufOverflow) {
          serialWrite("-err:cmd too long");
        } else {
          serialBuf[serialBufPos] = '\0';
          processCommand(serialBuf, serialWrite);
        }
        serialBufPos = 0;
        serialBufOverflow = false;
      } else if (serialBufPos < SERIAL_BUF_SIZE - 1) {
        serialBuf[serialBufPos++] = c;
      } else {
        serialBufOverflow = true;
      }
      continue;
    }

    // First character — protocol, JSON, or single-char debug?
    if (c == '?' || c == '=' || c == '!' || c == '{') {
      serialBuf[0] = c;
      serialBufPos = 1;
      continue;
    }

    // Skip bare newlines
    if (c == '\n' || c == '\r') continue;

    // Single-char debug commands
    switch (c) {
      case 'h':
        Serial.println("\n=== Commands (C6) ===");
        Serial.println("s - Status");
        Serial.println("d - Dump settings");
        Serial.println("t - Toggle status push");
        Serial.println("r - Reboot");
        Serial.println("p - Screenshot (base64 BMP)");
        Serial.println("e - Easter egg (test)");
        break;
      case 'p':
        takeScreenshot();
        break;
      case 's':
        printStatus();
        break;
      case 'd':
        Serial.println("\n=== Settings ===");
        Serial.print("Key MIN: "); Serial.println(settings.keyIntervalMin);
        Serial.print("Key MAX: "); Serial.println(settings.keyIntervalMax);
        Serial.print("Mouse Jig: "); Serial.println(settings.mouseJiggleDuration);
        Serial.print("Mouse Idle: "); Serial.println(settings.mouseIdleDuration);
        Serial.print("Slots: ");
        for (int i = 0; i < NUM_SLOTS; i++) {
          if (i > 0) Serial.print(", ");
          Serial.print(i); Serial.print("=");
          Serial.print(AVAILABLE_KEYS[settings.keySlots[i]].name);
        }
        Serial.println();
        Serial.print("Profile: "); Serial.println((currentProfile < PROFILE_COUNT) ? PROFILE_NAMES[currentProfile] : "???");
        Serial.print("Lazy %: "); Serial.println(settings.lazyPercent);
        Serial.print("Busy %: "); Serial.println(settings.busyPercent);
        Serial.print("Effective KB: "); Serial.print(effectiveKeyMin()); Serial.print("-"); Serial.println(effectiveKeyMax());
        Serial.print("Effective Mouse: "); Serial.print(effectiveMouseJiggle()); Serial.print("/"); Serial.println(effectiveMouseIdle());
        Serial.print("Mouse amplitude: "); Serial.print(settings.mouseAmplitude); Serial.println("px");
        Serial.print("Mouse style: "); Serial.println((settings.mouseStyle < MOUSE_STYLE_COUNT) ? MOUSE_STYLE_NAMES[settings.mouseStyle] : "???");
        Serial.print("Scroll: "); Serial.println(settings.scrollEnabled ? "On" : "Off");
        Serial.print("Device name: "); Serial.println(settings.deviceName);
        if (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT) {
          Serial.print("BLE identity: "); Serial.print(DECOY_NAMES[settings.decoyIndex - 1]);
          Serial.print(" (decoy "); Serial.print(settings.decoyIndex); Serial.println(")");
        }
        Serial.print("Activity LEDs: "); Serial.println(settings.activityLeds ? "On" : "Off");
        break;
      case 't':
        serialStatusPush = !serialStatusPush;
        Serial.print("Status push: ");
        Serial.println(serialStatusPush ? "ON" : "OFF");
        break;
      case 'r':
        Serial.println("Rebooting...");
        Serial.flush();
        delay(100);
        ESP.restart();
        break;
      case 'e':
        easterEggActive = true;
        easterEggFrame = 0;
        if (currentMode != MODE_NORMAL) {
          menuEditing = false;
          currentMode = MODE_NORMAL;
        }
        screensaverActive = false;
        markDisplayDirty();
        Serial.println("Easter egg triggered!");
        break;
    }
  }
}

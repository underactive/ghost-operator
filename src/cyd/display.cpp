#include "display.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "schedule.h"
#include "orchestrator.h"
#include "sim_data.h"
#include "input.h"
#include <TFT_eSPI.h>

// ============================================================================
// CYD TFT display renderer — ILI9341 320x240 color display
//
// Uses incremental drawing to avoid flicker without sprites:
// - Full redraw on mode change (fillScreen + layout + data)
// - Data-only redraw on subsequent frames (overwrite dynamic elements)
// ============================================================================

static TFT_eSPI tft = TFT_eSPI();
static bool tftInitialized = false;

// Track mode to detect transitions requiring full redraw
static UIMode lastScreenMode = MODE_COUNT;  // invalid → forces initial full draw
static bool lastEditorState = false;

// ----------------------------------------------------------------------------
// Color palette (dark theme, 16-bit RGB565)
// ----------------------------------------------------------------------------
#define COL_BG           0x0000  // black
#define COL_BG_CARD      0x18E3  // dark gray (panels/cards)
#define COL_TEXT          0xFFFF  // white
#define COL_TEXT_DIM      0x7BEF  // gray (muted/disabled)
#define COL_ACCENT        0x07E0  // green (connected/active)
#define COL_WARNING       0xFD20  // orange (profile changes)
#define COL_HEADER_BG     0x000F  // dark blue
#define COL_HEADING_BG    0x2104  // section heading band
#define COL_PROGRESS_BG   0x2104  // progress bar track
#define COL_PROGRESS_FG   0x07E0  // progress bar fill (green)
#define COL_HIGHLIGHT     0x2945  // selected row highlight
#define COL_BTN_BG        0x2945  // button background
#define COL_BTN_TEXT      0xFFFF  // button text
#define COL_POPUP_BG      0x2104  // popup background
#define COL_POPUP_BORDER  0x4A49  // popup border
#define COL_DISCONNECT    0xF800  // red (disconnected)

// ----------------------------------------------------------------------------
// Display layout constants
// ----------------------------------------------------------------------------
#define HEADER_H      32
#define FOOTER_H      48
#define CONTENT_Y     HEADER_H
#define CONTENT_H     (SCREEN_HEIGHT - HEADER_H - FOOTER_H)
#define MENU_ROW_H    40
#define HEADING_ROW_H 30

// Activity icons (10px wide 1-bit bitmaps from nRF52, drawn at 2x on CYD)
// Keycap normal (10x10)
static const uint8_t iconKeycapNormal[] = {
  0x7F, 0x80, 0x80, 0x40, 0x9E, 0x40, 0xA1, 0x40, 0xA1, 0x40,
  0xBF, 0x40, 0xA1, 0x40, 0xA1, 0x40, 0x80, 0x40, 0x7F, 0x80
};
// Keycap pressed (10x8)
static const uint8_t iconKeycapPressed[] = {
  0x7F, 0x80, 0x80, 0x40, 0x9E, 0x40, 0xA1, 0x40,
  0xBF, 0x40, 0xA1, 0x40, 0x80, 0x40, 0x7F, 0x80
};
// Mouse normal (10x10)
static const uint8_t iconMouseNormal[] = {
  0x0C, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x7F, 0x80, 0x40, 0x80,
  0x5E, 0x80, 0x40, 0x80, 0x7F, 0x80, 0x7F, 0x80, 0x7F, 0x80
};
// Mouse click (10x10)
static const uint8_t iconMouseClick[] = {
  0x0C, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x7F, 0x80, 0x40, 0x80,
  0x40, 0x80, 0x40, 0x80, 0x7F, 0x80, 0x7F, 0x80, 0x7F, 0x80
};

// Draw a 1-bit bitmap at 2x scale
static void drawBitmap2x(int16_t x, int16_t y, const uint8_t* bmp,
                         int16_t w, int16_t h, uint16_t color) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      int16_t byteIdx = row * ((w + 7) / 8) + (col / 8);
      bool pixel = bmp[byteIdx] & (0x80 >> (col % 8));
      if (pixel) {
        tft.fillRect(x + col * 2, y + row * 2, 2, 2, color);
      }
    }
  }
}

// Card layout (home screen)
#define CARD_W     145
#define CARD_H     120
#define CARD_LX     10
#define CARD_RX    165
#define CARD_Y      42

// ----------------------------------------------------------------------------
// Value editor overlay state
// ----------------------------------------------------------------------------
static bool editorVisible = false;
static uint8_t editorMenuIdx = 0;
static uint32_t editorValue = 0;

// Mouse state name lookup
static const char* MOUSE_STATE_NAMES[] = { "IDLE", "JIGGLING", "RETURNING" };

// Forward declarations
static void drawHeaderBar();
static void drawHomeLayout();
static void drawHomeData();
static void drawSimLayout();
static void drawSimData();
static void drawMenuFull();
static void drawValueEditor();
static void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint8_t percent, uint16_t fgColor);
static void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                        const char* label);
static void drawCard(int16_t x, int16_t y, int16_t w, int16_t h);

// ============================================================================
// Public API
// ============================================================================

void setupDisplay() {
  tft.init();
  tft.setRotation(1);  // landscape: 320x240
  tft.fillScreen(COL_BG);
  tftInitialized = true;

  // Splash screen
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Ghost Operator CYD", SCREEN_WIDTH / 2, 90);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString(VERSION, SCREEN_WIDTH / 2, 130);
  delay(1000);
}

void markDisplayDirty() {
  displayDirty = true;
}

void invalidateDisplayShadow() {
  lastScreenMode = MODE_COUNT;  // force full redraw next frame
  displayDirty = true;
}

bool isEditorVisible() {
  return editorVisible;
}

void showValueEditor(uint8_t menuIdx) {
  editorVisible = true;
  editorMenuIdx = menuIdx;
  editorValue = getSettingValue(MENU_ITEMS[menuIdx].settingId);
  displayDirty = true;
}

void hideValueEditor() {
  editorVisible = false;
  lastScreenMode = MODE_COUNT;  // force full redraw to clear popup
  displayDirty = true;
}

// ============================================================================
// Main render dispatcher
// ============================================================================

void updateDisplay() {
  if (!tftInitialized || !displayDirty) return;
  displayDirty = false;

  // Detect mode transitions → full redraw
  bool modeChanged = (currentMode != lastScreenMode) || (editorVisible != lastEditorState);
  lastScreenMode = currentMode;
  lastEditorState = editorVisible;

  if (editorVisible) {
    if (modeChanged) tft.fillScreen(COL_BG);
    drawValueEditor();
    return;
  }

  switch (currentMode) {
    case MODE_NORMAL:
      if (settings.operationMode == OP_SIMULATION) {
        if (modeChanged) { tft.fillScreen(COL_BG); drawSimLayout(); }
        drawSimData();
      } else {
        if (modeChanged) { tft.fillScreen(COL_BG); drawHomeLayout(); }
        drawHomeData();
      }
      break;
    case MODE_MENU:
      if (modeChanged) tft.fillScreen(COL_BG);
      drawMenuFull();  // menu redraws fully but only when dirty
      break;
    default:
      if (modeChanged) { tft.fillScreen(COL_BG); drawHomeLayout(); }
      drawHomeData();
      break;
  }
}

// ============================================================================
// Header bar
// ============================================================================

// Pre-rendered gradient header stored as scanline colors (320 values)
static uint16_t headerGradient[SCREEN_WIDTH];
static bool headerGradientReady = false;

static void initHeaderGradient() {
  for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
    uint8_t frac = 255 - (uint8_t)((uint32_t)x * 255 / SCREEN_WIDTH);
    uint8_t r = (uint8_t)((uint16_t)0x80 * frac / 255);
    uint8_t b = (uint8_t)((uint16_t)0xC0 * frac / 255);
    headerGradient[x] = ((r >> 3) << 11) | (b >> 3);
  }
  headerGradientReady = true;
}

// Draw gradient header (full redraw — call only on mode change)
static void drawHeaderGradient() {
  if (!headerGradientReady) initHeaderGradient();
  for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
    tft.drawFastVLine(x, 0, HEADER_H, headerGradient[x]);
  }
}

// Get the gradient color at a given x position (for text background erase)
static uint16_t headerColorAt(int16_t x) {
  if (x < 0) x = 0;
  if (x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
  return headerGradient[x];
}

// Erase a rect in the header by redrawing gradient lines over it
static void eraseHeaderRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!headerGradientReady) initHeaderGradient();
  int16_t x2 = x + w;
  if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
  for (int16_t px = x; px < x2; px++) {
    tft.drawFastVLine(px, y, h, headerGradient[px]);
  }
}

static void drawHeaderBar() {
  // Only draw full gradient on first call (mode change triggers full redraw)
  // Subsequent calls just update the dynamic text regions
  static bool headerDrawn = false;
  static UIMode lastHeaderMode = MODE_COUNT;
  static bool lastHeaderConnected = false;

  bool needFullRedraw = (!headerDrawn || lastHeaderMode != currentMode);
  headerDrawn = true;
  lastHeaderMode = currentMode;

  if (needFullRedraw) {
    drawHeaderGradient();
  }

  tft.setTextFont(2);
  int16_t textY = (HEADER_H - 16) / 2;

  // BLE status dot — only redraw if connection state changed
  if (needFullRedraw || lastHeaderConnected != deviceConnected) {
    lastHeaderConnected = deviceConnected;
    eraseHeaderRect(10, 0, 55, HEADER_H);
    uint16_t dotColor = deviceConnected ? COL_ACCENT : COL_DISCONNECT;
    tft.fillCircle(16, HEADER_H / 2, 5, dotColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT);
    tft.drawString("BLE", 26, textY);
  }

  // Device name — static, only on full redraw
  if (needFullRedraw) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_TEXT);
    tft.drawString(settings.deviceName, SCREEN_WIDTH / 2, textY);
  }

  // Uptime or clock — changes every frame, erase and redraw
  eraseHeaderRect(SCREEN_WIDTH - 80, 0, 80, HEADER_H);
  char timeBuf[16];
  if (timeSynced) {
    formatCurrentTime(timeBuf, sizeof(timeBuf));
  } else {
    formatUptime(millis() - startTime, timeBuf, sizeof(timeBuf));
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_TEXT_DIM);
  tft.drawString(timeBuf, SCREEN_WIDTH - 8, textY);
}

// ============================================================================
// Helpers
// ============================================================================

static void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint8_t percent, uint16_t fgColor) {
  if (percent > 100) percent = 100;
  // Outline in the fill color
  tft.drawRect(x, y, w, h, fgColor);
  // Fill interior (inset 1px for outline)
  int16_t ix = x + 1, iy = y + 1, iw = w - 2, ih = h - 2;
  if (iw < 1 || ih < 1) return;
  int16_t fillW = (int16_t)((uint32_t)iw * percent / 100);
  // Draw fill and empty in one pass — no flicker
  if (fillW > 0) tft.fillRect(ix, iy, fillW, ih, fgColor);
  if (fillW < iw) tft.fillRect(ix + fillW, iy, iw - fillW, ih, COL_PROGRESS_BG);
}

static void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                        const char* label) {
  tft.fillRoundRect(x, y, w, h, 4, COL_BTN_BG);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_BTN_TEXT, COL_BTN_BG);
  tft.drawString(label, x + w / 2, y + h / 2);
}

static void drawCard(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRoundRect(x, y, w, h, 4, COL_BG_CARD);
}

// ============================================================================
// Home screen — static layout (drawn once on mode entry)
// ============================================================================

static void drawHomeLayout() {
  drawHeaderBar();

  // Keyboard card shell
  drawCard(CARD_LX, CARD_Y, CARD_W, CARD_H);
  tft.setTextFont(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("KEYBOARD", CARD_LX + CARD_W / 2, CARD_Y + 6);

  // Mouse card shell
  drawCard(CARD_RX, CARD_Y, CARD_W, CARD_H);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("MOUSE", CARD_RX + CARD_W / 2, CARD_Y + 6);

  // Footer buttons (static)
  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  drawButton(10, footerY + 8, 80, 32, "Settings");
  drawButton(SCREEN_WIDTH - 90, footerY + 8, 80, 32, "Sleep");
}

// ============================================================================
// Home screen — dynamic data (drawn every frame, overwrites in-place)
// ============================================================================

static void drawHomeData() {
  // Redraw header for live uptime
  drawHeaderBar();

  // --- Keyboard card data ---
  // Key name (with bg color to erase old text)
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  const char* keyName = (nextKeyIndex < NUM_KEYS) ? AVAILABLE_KEYS[nextKeyIndex].name : "???";
  // Clear area then draw (font 4 text can vary width)
  tft.fillRect(CARD_LX + 4, CARD_Y + 26, CARD_W - 8, 26, COL_BG_CARD);
  tft.drawString(keyName, CARD_LX + CARD_W / 2, CARD_Y + 28);

  // Timing
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  char durBuf[16];
  formatDuration(currentKeyInterval, durBuf, sizeof(durBuf));
  tft.fillRect(CARD_LX + 4, CARD_Y + 58, CARD_W - 8, 18, COL_BG_CARD);
  tft.drawString(durBuf, CARD_LX + CARD_W / 2, CARD_Y + 60);

  // Progress bar
  unsigned long elapsed = millis() - lastKeyTime;
  uint8_t kbPct = 0;
  if (currentKeyInterval > 0) {
    kbPct = (uint8_t)min((uint32_t)100, (elapsed * 100) / currentKeyInterval);
  }
  drawProgressBar(CARD_LX + 10, CARD_Y + CARD_H - 20, CARD_W - 20, 8, kbPct, COL_PROGRESS_FG);

  // --- Mouse card data ---
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  const char* msName = (mouseState < 3) ? MOUSE_STATE_NAMES[mouseState] : "???";
  tft.fillRect(CARD_RX + 4, CARD_Y + 26, CARD_W - 8, 26, COL_BG_CARD);
  tft.drawString(msName, CARD_RX + CARD_W / 2, CARD_Y + 28);

  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  unsigned long mouseDur = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
  formatDuration(mouseDur, durBuf, sizeof(durBuf));
  tft.fillRect(CARD_RX + 4, CARD_Y + 58, CARD_W - 8, 18, COL_BG_CARD);
  tft.drawString(durBuf, CARD_RX + CARD_W / 2, CARD_Y + 60);

  unsigned long mouseElapsed = millis() - lastMouseStateChange;
  uint8_t msPct = 0;
  if (mouseState == MOUSE_RETURNING) {
    msPct = 0;
  } else if (mouseDur > 0) {
    msPct = (uint8_t)min((uint32_t)100, (mouseElapsed * 100) / mouseDur);
  }
  drawProgressBar(CARD_RX + 10, CARD_Y + CARD_H - 20, CARD_W - 20, 8, msPct, COL_PROGRESS_FG);

  // --- Footer KB/MS status (dynamic) ---
  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  // Clear center area
  tft.fillRect(100, footerY + 8, 120, 32, COL_BG);
  tft.setTextFont(2);
  tft.setTextColor(keyEnabled ? COL_ACCENT : COL_DISCONNECT, COL_BG);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(keyEnabled ? "KB:ON" : "KB:OFF", SCREEN_WIDTH / 2 - 4, footerY + 16);
  tft.setTextColor(mouseEnabled ? COL_ACCENT : COL_DISCONNECT, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(mouseEnabled ? "MS:ON" : "MS:OFF", SCREEN_WIDTH / 2 + 4, footerY + 16);
}

// ============================================================================
// Simulation home — layout + data split
// ============================================================================

// Compact sim info row: name left | inline bar | countdown right
// Matches nRF52 layout — 3 small rows then 1 big activity row
static void drawSimCompactRow(int16_t y, const char* name,
                              uint8_t progress, unsigned long remainMs,
                              uint16_t barColor, uint16_t bg) {
  int16_t lx = 20, rx = SCREEN_WIDTH - 20;

  // Name (left)
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(lx, y, 120, 18, bg);
  tft.setTextColor(COL_TEXT, bg);
  tft.drawString(name, lx, y);

  // Inline progress bar (middle)
  int16_t barX = 145, barW = 100, barH = 10;
  int16_t barY = y + 3;
  drawProgressBar(barX, barY, barW, barH, progress, barColor);

  // Countdown timer (right)
  char timeBuf[12];
  formatMinSec(remainMs, timeBuf, sizeof(timeBuf));
  tft.setTextDatum(TR_DATUM);
  tft.fillRect(rx - 55, y, 55, 18, bg);
  tft.setTextColor(COL_TEXT_DIM, bg);
  tft.drawString(timeBuf, rx, y);
}

static void drawSimLayout() {
  drawHeaderBar();

  // Info card (top area for 3 compact rows + big phase bar)
  drawCard(10, CONTENT_Y + 2, SCREEN_WIDTH - 20, CONTENT_H - 6);

  // Footer buttons
  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  drawButton(10, footerY + 8, 80, 32, "Settings");
  drawButton(SCREEN_WIDTH - 90, footerY + 8, 80, 32, "Sleep");

  // Static labels for compact rows (offset down by 20 for info line at top)
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  int16_t y0 = CONTENT_Y + 28;
  tft.drawString("Block", 20, y0);
  tft.drawString("Mode", 20, y0 + 22);
  tft.drawString("Profile", 20, y0 + 44);
}

static void drawSimData() {
  drawHeaderBar();

  unsigned long now = millis();

  // === Info line at top: Job type + Performance level ===
  int16_t infoY = CONTENT_Y + 8;
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(20, infoY, SCREEN_WIDTH - 40, 18, COL_BG_CARD);
  const char* jobName = (settings.jobSimulation < JOB_SIM_COUNT) ? JOB_SIM_NAMES[settings.jobSimulation] : "???";
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString(jobName, 20, infoY);
  char perfBuf[16];
  snprintf(perfBuf, sizeof(perfBuf), "Perf: %d", settings.jobPerformance);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString(perfBuf, SCREEN_WIDTH - 20, infoY);

  // Divider between info line and compact rows
  tft.drawFastHLine(20, infoY + 19, SCREEN_WIDTH - 40, COL_HEADING_BG);

  // === 3 compact rows (block, mode, profile) with inline bars ===
  int16_t y0 = CONTENT_Y + 28;
  unsigned long blockElapsed = now - orch.blockStartMs;
  unsigned long blockRemain = (blockElapsed < orch.blockDurationMs) ? (orch.blockDurationMs - blockElapsed) : 0;
  drawSimCompactRow(y0, currentBlockName(), 100 - blockProgress(now), blockRemain, 0x52AA, COL_BG_CARD);  // light blue

  unsigned long modeElapsed = now - orch.modeStartMs;
  unsigned long modeRemain = (modeElapsed < orch.modeDurationMs) ? (orch.modeDurationMs - modeElapsed) : 0;
  drawSimCompactRow(y0 + 22, currentModeName(), 100 - modeProgress(now), modeRemain, 0x52AA, COL_BG_CARD);  // light blue

  unsigned long stintElapsed = now - orch.profileStintStartMs;
  unsigned long stintRemain = (stintElapsed < orch.profileStintMs) ? (orch.profileStintMs - stintElapsed) : 0;
  uint8_t stintPct = (orch.profileStintMs > 0) ? (uint8_t)((stintRemain * 100) / orch.profileStintMs) : 0;
  const char* profName = (orch.autoProfile < PROFILE_COUNT) ? PROFILE_NAMES_TITLE[orch.autoProfile] : "???";
  // Profile bar color: red=Lazy, yellow=Normal, green=Busy
  uint16_t profColor;
  switch (orch.autoProfile) {
    case PROFILE_LAZY:   profColor = 0xF800; break;  // red
    case PROFILE_BUSY:   profColor = COL_ACCENT; break;  // green
    default:             profColor = 0xFFE0; break;  // yellow (NORMAL)
  }
  drawSimCompactRow(y0 + 44, profName, stintPct, stintRemain, profColor, COL_BG_CARD);

  // === Separator line ===
  int16_t sepY = y0 + 66;
  tft.drawFastHLine(20, sepY, SCREEN_WIDTH - 40, COL_HEADING_BG);

  // === Big activity phase row ===
  int16_t phaseY = sepY + 4;
  unsigned long phaseElapsed = now - orch.phaseStartMs;
  unsigned long phaseRemain = (phaseElapsed < orch.phaseDurationMs) ? (orch.phaseDurationMs - phaseElapsed) : 0;
  uint8_t phasePct = (orch.phaseDurationMs > 0) ? (uint8_t)((phaseRemain * 100) / orch.phaseDurationMs) : 0;

  // Phase label (large — clear 120px wide to cover longest name "SWITCH")
  const char* phaseName = (orch.phase < PHASE_COUNT) ? PHASE_NAMES[orch.phase] : "???";
  tft.setTextFont(4);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(20, phaseY, 120, 26, COL_BG_CARD);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  tft.drawString(phaseName, 20, phaseY);

  // Phase countdown (large, right)
  char durBuf[12];
  formatDuration(phaseRemain, durBuf, sizeof(durBuf));
  tft.setTextDatum(TR_DATUM);
  tft.fillRect(SCREEN_WIDTH - 100, phaseY, 80, 26, COL_BG_CARD);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  tft.drawString(durBuf, SCREEN_WIDTH - 20, phaseY);

  // Big progress bar
  drawProgressBar(20, phaseY + 30, SCREEN_WIDTH - 40, 12, phasePct, COL_TEXT);  // white

  // === Activity icons (2x scaled bitmaps, in footer alongside buttons) ===
  // Only redraw when state changes to avoid flicker
  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  int16_t iconY = footerY + 12;

  static bool lastKeyDown = false;
  static bool lastClicking = false;
  static int8_t lastNudgeIdx = 0;
  static bool iconsFirstDraw = true;

  // Keycap icon
  if (keyEnabled) {
    bool keyChanged = iconsFirstDraw || (orch.keyDown != lastKeyDown);
    if (keyChanged) {
      lastKeyDown = orch.keyDown;
      int16_t kx = SCREEN_WIDTH / 2 - 30;
      // Clear keycap area
      tft.fillRect(kx, footerY, 22, FOOTER_H, COL_BG);
      uint16_t kbCol = orch.keyDown ? COL_ACCENT : COL_TEXT_DIM;
      if (orch.keyDown) {
        drawBitmap2x(kx, iconY + 4, iconKeycapPressed, 10, 8, kbCol);
      } else {
        drawBitmap2x(kx, iconY, iconKeycapNormal, 10, 10, kbCol);
      }
    }
  }

  // Mouse icon
  if (mouseEnabled) {
    bool clicking = (now - orch.lastPhantomClickMs < 200);
    bool mouseMoving = (orch.phase == PHASE_MOUSING && mouseState == MOUSE_JIGGLING) ||
                       (orch.phase == PHASE_KB_MOUSE) || (orch.phase == PHASE_MOUSE_KB);
    int8_t nudgeIdx = mouseMoving ? (int8_t)((now / 150) % 4) : 0;
    bool mouseChanged = iconsFirstDraw || (clicking != lastClicking) || (nudgeIdx != lastNudgeIdx);
    if (mouseChanged) {
      lastClicking = clicking;
      lastNudgeIdx = nudgeIdx;
      // Clear mouse area (wider to cover nudge range)
      tft.fillRect(SCREEN_WIDTH / 2 + 8, footerY, 26, FOOTER_H, COL_BG);
      int16_t mx = SCREEN_WIDTH / 2 + 10;
      static const int8_t nudge[] = {0, 1, 0, -1};
      if (mouseMoving) mx += nudge[nudgeIdx];
      const uint8_t* mIcon = clicking ? iconMouseClick : iconMouseNormal;
      uint16_t msCol = clicking ? COL_ACCENT : COL_TEXT_DIM;
      drawBitmap2x(mx, iconY, mIcon, 10, 10, msCol);
    }
  }

  iconsFirstDraw = false;
}

// ============================================================================
// Scrollable settings menu — full redraw (only on dirty, not 20Hz)
// ============================================================================

static void drawMenuFull() {
  tft.fillScreen(COL_BG);

  // Header
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_HEADER_BG);
  tft.drawString("<  Settings", 8, (HEADER_H - 16) / 2);

  int maxVisibleRows = CONTENT_H / MENU_ROW_H;

  int visibleIdx = 0;
  for (int8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    if (isMenuItemHidden(i)) continue;

    if (visibleIdx < menuScrollOffset) {
      visibleIdx++;
      continue;
    }

    int row = visibleIdx - menuScrollOffset;
    if (row >= maxVisibleRows) break;

    int16_t rowY = CONTENT_Y + row * MENU_ROW_H;
    const MenuItem& item = MENU_ITEMS[i];

    if (item.type == MENU_HEADING) {
      tft.fillRect(0, rowY, SCREEN_WIDTH, HEADING_ROW_H, COL_HEADING_BG);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(COL_TEXT, COL_HEADING_BG);
      tft.drawString(item.label, 12, rowY + (HEADING_ROW_H - 16) / 2);
    } else {
      bool selected = (i == menuCursor);
      uint16_t rowBg = selected ? COL_HIGHLIGHT : COL_BG;
      tft.fillRect(0, rowY, SCREEN_WIDTH, MENU_ROW_H, rowBg);

      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(COL_TEXT, rowBg);
      tft.drawString(item.label, 16, rowY + (MENU_ROW_H - 16) / 2);

      if (item.type == MENU_VALUE) {
        char valBuf[32];
        formatMenuValue(item.settingId, item.format, valBuf, sizeof(valBuf));
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(COL_ACCENT, rowBg);
        tft.drawString(valBuf, SCREEN_WIDTH - 16, rowY + (MENU_ROW_H - 16) / 2);
      } else if (item.type == MENU_ACTION) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(COL_TEXT_DIM, rowBg);
        tft.drawString(">", SCREEN_WIDTH - 16, rowY + (MENU_ROW_H - 16) / 2);
      }

      tft.drawFastHLine(16, rowY + MENU_ROW_H - 1, SCREEN_WIDTH - 32, COL_HEADING_BG);
    }

    visibleIdx++;
  }

  // Count total visible for page indicator
  int8_t totalVisible = 0;
  for (int8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    if (!isMenuItemHidden(i)) totalVisible++;
  }

  // Footer with pagination buttons
  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  tft.fillRect(0, footerY, SCREEN_WIDTH, FOOTER_H, COL_BG);

  // [< Prev] button (left)
  if (menuScrollOffset > 0) {
    drawButton(10, footerY + 8, 80, 32, "< Prev");
  }

  // Scroll position bar (center) — thin horizontal indicator
  int8_t maxScroll = totalVisible - maxVisibleRows;
  if (maxScroll < 0) maxScroll = 0;
  if (maxScroll > 0) {
    int16_t barX = 110, barW = 100, barH = 4;
    int16_t barY = footerY + 20;
    tft.fillRect(barX, barY, barW, barH, COL_PROGRESS_BG);
    int16_t thumbW = max((int)(barW * maxVisibleRows / totalVisible), 10);
    int16_t thumbX = barX + (int32_t)(barW - thumbW) * menuScrollOffset / maxScroll;
    tft.fillRect(thumbX, barY, thumbW, barH, COL_TEXT_DIM);
  }

  // [Next >] button (right)
  if (menuScrollOffset < maxScroll) {
    drawButton(230, footerY + 8, 80, 32, "Next >");
  }
}

// ============================================================================
// Value editor popup
// ============================================================================

static void drawValueEditor() {
  // Popup frame
  int16_t popX = 40, popY = 50, popW = 240, popH = 140;
  tft.fillRoundRect(popX, popY, popW, popH, 6, COL_POPUP_BG);
  tft.drawRoundRect(popX, popY, popW, popH, 6, COL_POPUP_BORDER);

  const MenuItem& item = MENU_ITEMS[editorMenuIdx];

  // Title
  tft.setTextFont(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_POPUP_BG);
  tft.drawString(item.label, popX + popW / 2, popY + 10);

  // Current value (clear area first to handle varying width)
  tft.fillRect(popX + 10, popY + 36, popW - 20, 30, COL_POPUP_BG);
  char valBuf[32];
  formatMenuValue(item.settingId, item.format, valBuf, sizeof(valBuf));
  tft.setTextFont(4);
  tft.setTextColor(COL_ACCENT, COL_POPUP_BG);
  tft.drawString(valBuf, popX + popW / 2, popY + 40);

  // Buttons
  drawButton(60, 120, 50, 40, "-");
  drawButton(210, 120, 50, 40, "+");
  drawButton(60, 170, 80, 30, "Cancel");
  drawButton(180, 170, 80, 30, "OK");
}

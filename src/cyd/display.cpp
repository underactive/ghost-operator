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

static void drawHeaderBar() {
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);
  tft.setTextFont(2);

  // BLE status dot
  uint16_t dotColor = deviceConnected ? COL_ACCENT : COL_DISCONNECT;
  tft.fillCircle(16, HEADER_H / 2, 5, dotColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_HEADER_BG);
  tft.drawString("BLE", 26, (HEADER_H - 16) / 2);

  // Device name (center)
  tft.setTextDatum(TC_DATUM);
  tft.drawString(settings.deviceName, SCREEN_WIDTH / 2, (HEADER_H - 16) / 2);

  // Uptime or clock (right)
  char timeBuf[16];
  if (timeSynced) {
    formatCurrentTime(timeBuf, sizeof(timeBuf));
  } else {
    formatUptime(millis() - startTime, timeBuf, sizeof(timeBuf));
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_TEXT_DIM, COL_HEADER_BG);
  tft.drawString(timeBuf, SCREEN_WIDTH - 8, (HEADER_H - 16) / 2);
}

// ============================================================================
// Helpers
// ============================================================================

static void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint8_t percent, uint16_t fgColor) {
  tft.fillRect(x, y, w, h, COL_PROGRESS_BG);
  if (percent > 100) percent = 100;
  int16_t fillW = (int16_t)((uint32_t)w * percent / 100);
  if (fillW > 0) {
    tft.fillRect(x, y, fillW, h, fgColor);
  }
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

static void drawSimLayout() {
  drawHeaderBar();
  drawCard(10, CONTENT_Y + 8, SCREEN_WIDTH - 20, CONTENT_H - 16);

  int16_t footerY = SCREEN_HEIGHT - FOOTER_H;
  drawButton(10, footerY + 8, 80, 32, "Settings");
  drawButton(SCREEN_WIDTH - 90, footerY + 8, 80, 32, "Sleep");
}

static void drawSimData() {
  drawHeaderBar();

  int16_t y = CONTENT_Y + 8;
  unsigned long now = millis();
  int16_t cardX = 10, cardW = SCREEN_WIDTH - 20;

  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);

  // Block
  tft.fillRect(cardX + 70, y + 6, cardW - 80, 18, COL_BG_CARD);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("Block:", cardX + 10, y + 8);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  tft.drawString(currentBlockName(), cardX + 70, y + 8);
  drawProgressBar(cardX + 10, y + 28, cardW - 20, 6, blockProgress(now), COL_PROGRESS_FG);

  // Mode
  tft.fillRect(cardX + 70, y + 38, cardW - 80, 18, COL_BG_CARD);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("Mode:", cardX + 10, y + 40);
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  tft.drawString(currentModeName(), cardX + 70, y + 40);
  drawProgressBar(cardX + 10, y + 60, cardW - 20, 6, modeProgress(now), COL_ACCENT);

  // Phase
  tft.fillRect(cardX + 70, y + 70, 100, 18, COL_BG_CARD);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("Phase:", cardX + 10, y + 72);
  const char* phaseName = (orch.phase < PHASE_COUNT) ? PHASE_NAMES[orch.phase] : "???";
  tft.setTextColor(COL_WARNING, COL_BG_CARD);
  tft.drawString(phaseName, cardX + 70, y + 72);

  // Profile + Performance
  tft.fillRect(cardX + 80, y + 90, cardW - 90, 18, COL_BG_CARD);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString("Profile:", cardX + 10, y + 92);
  const char* profName = (orch.autoProfile < PROFILE_COUNT) ? PROFILE_NAMES[orch.autoProfile] : "???";
  tft.setTextColor(COL_TEXT, COL_BG_CARD);
  tft.drawString(profName, cardX + 80, y + 92);

  char perfBuf[16];
  snprintf(perfBuf, sizeof(perfBuf), "Perf: %d%%", settings.jobPerformance * 10);
  tft.setTextColor(COL_TEXT_DIM, COL_BG_CARD);
  tft.drawString(perfBuf, cardX + 160, y + 92);
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

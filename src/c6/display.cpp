#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <lvgl.h>
#include "display.h"
#include "config.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "schedule.h"
#include "orchestrator.h"
#include "sim_data.h"
#include "platform_hal.h"
#include "ghost_sprite.h"
#include "ghost_splash.h"
#include "kbm_icons.h"

// ============================================================================
// LVGL Display Driver for ESP32-C6-LCD-1.47 (Waveshare)
// ST7789 172x320 TFT via SPI, landscape 320x172
// ============================================================================

// --- SPI ---
static SPIClass* hspi = nullptr;
#define SPI_FREQ 40000000  // 40 MHz

// --- Display buffers (two partial buffers for double-buffering) ---
#define BUF_LINES 32
static uint8_t buf1[320 * BUF_LINES * 2];  // ~20KB
static uint8_t buf2[320 * BUF_LINES * 2];  // ~20KB

// --- LVGL display reference ---
static lv_display_t* lvDisplay = nullptr;

// --- Color palette ---
#define COL_BG        lv_color_hex(0x000000)
#define COL_CARD      lv_color_hex(0x1C1C2E)
#define COL_CARD_BORDER lv_color_hex(0x2A2A40)
#define COL_TEXT      lv_color_hex(0xFFFFFF)
#define COL_TEXT_DIM  lv_color_hex(0x808080)
#define COL_TEXT_FOOTER lv_color_hex(0xB0B0C0)  // bright but not white
#define COL_ACCENT    lv_color_hex(0x00E676)
#define COL_DISCONNECT lv_color_hex(0xFF1744)
#define COL_BLUE      lv_color_hex(0x448AFF)
#define COL_ORANGE    lv_color_hex(0xFF9100)
#define COL_BAR_BG    lv_color_hex(0x2D2D3D)
#define COL_BAR_BLOCK lv_color_hex(0xFF69B4)    // hot pink (block row)
#define COL_BAR_MODE  lv_color_hex(0x00E5FF)    // aqua (mode row)
#define COL_BAR_BUSY  lv_color_hex(0x00E676)    // same green as activity bar
#define COL_BAR_NORMAL lv_color_hex(0xFFF59D)   // pastel yellow
#define COL_BAR_LAZY  lv_color_hex(0xFF69B4)    // same pink as block bar
#define COL_BAR_KB    lv_color_hex(0x00E676)
#define COL_BAR_MS    lv_color_hex(0x448AFF)
#define COL_HEADER_BG lv_color_hex(0x1A1A3E)
#define COL_HEADER_GRAD lv_color_hex(0x2E1A3E)
#define COL_STATUS_BG lv_color_hex(0x121220)

// ============================================================================
// UI Widget References (for efficient updates)
// ============================================================================

// Header
static lv_obj_t* lblDeviceName = nullptr;
static lv_obj_t* lblHeaderBt = nullptr;     // BT symbol (larger font)
static lv_obj_t* lblHeaderRight = nullptr;  // uptime/clock text

// Keyboard card
static lv_obj_t* panelKb = nullptr;
static lv_obj_t* lblKbTitle = nullptr;
static lv_obj_t* lblKbKey = nullptr;
static lv_obj_t* lblKbInterval = nullptr;
static lv_obj_t* barKb = nullptr;
static lv_obj_t* lblKbCountdown = nullptr;

// Mouse card
static lv_obj_t* panelMs = nullptr;
static lv_obj_t* lblMsTitle = nullptr;
static lv_obj_t* lblMsState = nullptr;
static lv_obj_t* lblMsDuration = nullptr;
static lv_obj_t* barMs = nullptr;
static lv_obj_t* lblMsCountdown = nullptr;

// Status bar
static lv_obj_t* panelStatus = nullptr;
static lv_obj_t* lblStatus = nullptr;       // footer left (device name)
static lv_obj_t* imgKbIcon = nullptr;       // keyboard icon (animated)
static lv_obj_t* imgMsIcon = nullptr;       // mouse icon (animated)

// ============================================================================
// Activity icons — ARGB8888 from KBM_Outline sprite sheet (kbm_icons.h)
// ============================================================================

static void initKbmDsc(lv_image_dsc_t& dsc, const uint8_t* data, int w, int h) {
  memset(&dsc, 0, sizeof(dsc));
  dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
  dsc.header.w = w;
  dsc.header.h = h;
  dsc.header.stride = w * 4;  // bytes per row — required for ARGB8888
  dsc.data_size = w * h * 4;
  dsc.data = data;
}

// LVGL image descriptors (initialized in initIconBitmaps)
static lv_image_dsc_t imgdKbNormal  = {};
static lv_image_dsc_t imgdKbPressed = {};
static lv_image_dsc_t imgdMsNormal  = {};
static lv_image_dsc_t imgdMsClick   = {};
static lv_image_dsc_t imgdMsScroll  = {};

// Icon animation state
static const lv_image_dsc_t* prevKbImg = nullptr;
static const lv_image_dsc_t* prevMsImg = nullptr;
static int prevMsNudge = 0;
static lv_opa_t prevKbOpa = 0;
static lv_opa_t prevMsOpa = 0;

// Ghost sprite animation
static lv_obj_t* imgGhost = nullptr;
static lv_image_dsc_t ghostDsc = {};
static int ghostX = 280;             // current x position in footer
static int ghostDir = -1;            // 1=right, -1=left (starts moving left)
static uint8_t ghostFrame = 0;       // current animation frame
static unsigned long ghostFrameMs = 0;  // last frame advance time
static unsigned long ghostMoveMs = 0;   // last movement time
#define GHOST_FRAME_INTERVAL 120     // ms between animation frames
#define GHOST_MOVE_INTERVAL  40      // ms between position steps
#define GHOST_SPEED          1       // pixels per step
#define GHOST_X_MIN          200     // left bound (~60% of 320, last 40%)
#define GHOST_X_MAX          280     // right bound

static void initIconBitmaps() {
  initKbmDsc(imgdKbNormal,  kbmIcon_kbNormal,  KBM_KBNORMAL_W,  KBM_KBNORMAL_H);
  initKbmDsc(imgdKbPressed, kbmIcon_kbPressed, KBM_KBPRESSED_W, KBM_KBPRESSED_H);
  initKbmDsc(imgdMsNormal,  kbmIcon_msNormal,  KBM_MSNORMAL_W,  KBM_MSNORMAL_H);
  initKbmDsc(imgdMsClick,   kbmIcon_msClick,   KBM_MSCLICK_W,   KBM_MSCLICK_H);
  initKbmDsc(imgdMsScroll,  kbmIcon_msScroll,  KBM_MSSCROLL_W,  KBM_MSSCROLL_H);
}


// Sim mode widgets (overlay — shown instead of KB/MS cards)
static lv_obj_t* panelSimContent = nullptr;
static lv_obj_t* lblSimBlock = nullptr;
static lv_obj_t* barSimBlock = nullptr;
static lv_obj_t* lblSimBlockCountdown = nullptr;
static lv_obj_t* lblSimMode = nullptr;
static lv_obj_t* barSimMode = nullptr;
static lv_obj_t* lblSimModeCountdown = nullptr;
static lv_obj_t* lblSimProfile = nullptr;
static lv_obj_t* barSimProfile = nullptr;
static lv_obj_t* lblSimProfileCountdown = nullptr;
static lv_obj_t* lblSimActivity = nullptr;
static lv_obj_t* barSimKb = nullptr;
static lv_obj_t* lblSimKbInfo = nullptr;

// Simple mode content container
static lv_obj_t* panelSimpleContent = nullptr;

// Splash screen
static lv_obj_t* panelSplash = nullptr;
static lv_obj_t* splashGhostImg = nullptr;
static lv_image_dsc_t splashGhostDsc = {};
static uint8_t splashGhostFrame = 0;
static unsigned long splashFrameMs = 0;
#define SPLASH_FRAME_INTERVAL 150  // ms between animation frames (~6.7 fps)

// ============================================================================
// SPI and LCD Callbacks
// ============================================================================

static void spiInit() {
  hspi = new SPIClass(FSPI);
  hspi->begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_LCD_DC, OUTPUT);
  pinMode(PIN_LCD_RST, OUTPUT);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_LCD_BL, LOW);
}

static void hwReset() {
  digitalWrite(PIN_LCD_RST, HIGH);
  delay(10);
  digitalWrite(PIN_LCD_RST, LOW);
  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);
  delay(120);
}

static void lcd_send_cmd(lv_display_t* disp, const uint8_t* cmd, size_t cmd_size,
                          const uint8_t* param, size_t param_size) {
  (void)disp;
  hspi->beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_LCD_CS, LOW);

  // Command
  digitalWrite(PIN_LCD_DC, LOW);
  hspi->writeBytes(cmd, cmd_size);

  // Parameters
  if (param_size > 0) {
    digitalWrite(PIN_LCD_DC, HIGH);
    hspi->writeBytes(param, param_size);
  }

  digitalWrite(PIN_LCD_CS, HIGH);
  hspi->endTransaction();
}

static void lcd_send_color(lv_display_t* disp, const uint8_t* cmd, size_t cmd_size,
                            uint8_t* param, size_t param_size) {
  (void)disp;
  hspi->beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_LCD_CS, LOW);

  // Command
  digitalWrite(PIN_LCD_DC, LOW);
  hspi->writeBytes(cmd, cmd_size);

  // Pixel data
  digitalWrite(PIN_LCD_DC, HIGH);
  hspi->writeBytes(param, param_size);

  digitalWrite(PIN_LCD_CS, HIGH);
  hspi->endTransaction();

  // CRITICAL: Tell LVGL the flush is done
  lv_display_flush_ready(disp);
}

// ============================================================================
// Helper: millis() tick callback for LVGL v9
// ============================================================================

static uint32_t lvTickCb(void) {
  return (uint32_t)millis();
}

// ============================================================================
// UI Creation Helpers
// ============================================================================

static lv_obj_t* createCard(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, COL_CARD, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(card, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(card, COL_CARD_BORDER, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(card, 1, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(card, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_gap(card, 2, LV_STATE_DEFAULT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

static lv_obj_t* createBar(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_color_t color) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_size(bar, w, h);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(bar, 3, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
  return bar;
}

// ============================================================================
// Splash Screen
// ============================================================================

static void createSplash(lv_obj_t* screen) {
  panelSplash = lv_obj_create(screen);
  lv_obj_set_size(panelSplash, 320, 172);
  lv_obj_set_style_bg_color(panelSplash, COL_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(panelSplash, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(panelSplash, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(panelSplash, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(panelSplash, 0, LV_STATE_DEFAULT);
  lv_obj_center(panelSplash);
  lv_obj_set_flex_flow(panelSplash, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panelSplash, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(panelSplash, LV_OBJ_FLAG_SCROLLABLE);
  // Float above the flex layout so it covers the entire screen as an overlay
  lv_obj_add_flag(panelSplash, LV_OBJ_FLAG_FLOATING);

  // Animated ghost sprite
  initKbmDsc(splashGhostDsc, splashFrames[0], SPLASH_FRAME_W, SPLASH_FRAME_H);
  splashGhostImg = lv_image_create(panelSplash);
  lv_image_set_src(splashGhostImg, &splashGhostDsc);
  splashGhostFrame = 0;
  splashFrameMs = millis();

  lv_obj_t* lblTitle = lv_label_create(panelSplash);
  lv_label_set_text(lblTitle, "GHOST OPERATOR");
  lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_24, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblTitle, COL_ACCENT, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(lblTitle, 4, LV_STATE_DEFAULT);

  lv_obj_t* lblSub = lv_label_create(panelSplash);
  char verBuf[32];
  snprintf(verBuf, sizeof(verBuf), "%s  v%s", settings.deviceName, VERSION);
  lv_label_set_text(lblSub, verBuf);
  lv_obj_set_style_text_font(lblSub, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSub, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(lblSub, 4, LV_STATE_DEFAULT);

  lv_obj_t* lblOrg = lv_label_create(panelSplash);
  lv_label_set_text(lblOrg, "TARS Industrial");
  lv_obj_set_style_text_font(lblOrg, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOrg, lv_color_hex(0x808080), LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(lblOrg, 12, LV_STATE_DEFAULT);
}

// ============================================================================
// Header Bar (28px)
// ============================================================================

static lv_obj_t* createHeader(lv_obj_t* parent) {
  lv_obj_t* hdr = lv_obj_create(parent);
  lv_obj_set_size(hdr, 320, 28);
  // Aqua-era macOS menubar: glossy white→light gray vertical gradient
  lv_obj_set_style_bg_color(hdr, lv_color_hex(0xF0F0F0), LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(hdr, lv_color_hex(0xC8C8C8), LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(hdr, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(hdr, 0, LV_STATE_DEFAULT);
  // Subtle bottom border like the Aqua menubar
  lv_obj_set_style_border_color(hdr, lv_color_hex(0x888888), LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(hdr, 1, LV_STATE_DEFAULT);
  lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(hdr, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(hdr, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(hdr, 4, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(hdr, 4, LV_STATE_DEFAULT);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

  // Device name (left) — bold black text like Aqua menubar
  lblDeviceName = lv_label_create(hdr);
  lv_label_set_text(lblDeviceName, settings.deviceName);
  lv_obj_set_style_text_font(lblDeviceName, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblDeviceName, lv_color_hex(0x000000), LV_STATE_DEFAULT);
  lv_obj_align(lblDeviceName, LV_ALIGN_LEFT_MID, 0, 0);

  // BT symbol (far right, 16pt) — fixed position
  lblHeaderBt = lv_label_create(hdr);
  lv_label_set_text(lblHeaderBt, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_font(lblHeaderBt, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblHeaderBt, lv_color_hex(0x0060CC), LV_STATE_DEFAULT);
  lv_obj_align(lblHeaderBt, LV_ALIGN_RIGHT_MID, 0, 0);

  // Uptime/clock text — fixed right position (doesn't shift when BT icon flashes)
  lblHeaderRight = lv_label_create(hdr);
  lv_label_set_text(lblHeaderRight, "");
  lv_obj_set_style_text_font(lblHeaderRight, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblHeaderRight, lv_color_hex(0x333333), LV_STATE_DEFAULT);
  lv_obj_align(lblHeaderRight, LV_ALIGN_RIGHT_MID, -22, 0);

  return hdr;
}

// ============================================================================
// Simple Mode Content (KB + MS cards side by side)
// ============================================================================

static void createSimpleContent(lv_obj_t* parent) {
  panelSimpleContent = lv_obj_create(parent);
  lv_obj_set_size(panelSimpleContent, 320, 120);
  lv_obj_set_style_bg_opa(panelSimpleContent, LV_OPA_TRANSP, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(panelSimpleContent, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(panelSimpleContent, 4, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_gap(panelSimpleContent, 6, LV_STATE_DEFAULT);
  lv_obj_set_flex_flow(panelSimpleContent, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(panelSimpleContent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(panelSimpleContent, LV_OBJ_FLAG_SCROLLABLE);

  // --- Keyboard card ---
  panelKb = createCard(panelSimpleContent, 151, 112);

  lblKbTitle = lv_label_create(panelKb);
  lv_label_set_text(lblKbTitle, "KEYBOARD");
  lv_obj_set_style_text_font(lblKbTitle, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblKbTitle, COL_TEXT_DIM, LV_STATE_DEFAULT);

  lblKbKey = lv_label_create(panelKb);
  lv_label_set_text(lblKbKey, "---");
  lv_obj_set_style_text_font(lblKbKey, &lv_font_montserrat_20, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblKbKey, COL_ACCENT, LV_STATE_DEFAULT);

  lblKbInterval = lv_label_create(panelKb);
  lv_label_set_text(lblKbInterval, "");
  lv_obj_set_style_text_font(lblKbInterval, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblKbInterval, COL_TEXT_DIM, LV_STATE_DEFAULT);

  barKb = createBar(panelKb, 131, 8, COL_BAR_KB);

  lblKbCountdown = lv_label_create(panelKb);
  lv_label_set_text(lblKbCountdown, "---");
  lv_obj_set_style_text_font(lblKbCountdown, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblKbCountdown, COL_TEXT, LV_STATE_DEFAULT);

  // --- Mouse card ---
  panelMs = createCard(panelSimpleContent, 151, 112);

  lblMsTitle = lv_label_create(panelMs);
  lv_label_set_text(lblMsTitle, "MOUSE");
  lv_obj_set_style_text_font(lblMsTitle, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblMsTitle, COL_TEXT_DIM, LV_STATE_DEFAULT);

  lblMsState = lv_label_create(panelMs);
  lv_label_set_text(lblMsState, "IDLE");
  lv_obj_set_style_text_font(lblMsState, &lv_font_montserrat_20, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblMsState, COL_BAR_MS, LV_STATE_DEFAULT);

  lblMsDuration = lv_label_create(panelMs);
  lv_label_set_text(lblMsDuration, "");
  lv_obj_set_style_text_font(lblMsDuration, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblMsDuration, COL_TEXT_DIM, LV_STATE_DEFAULT);

  barMs = createBar(panelMs, 131, 8, COL_BAR_MS);

  lblMsCountdown = lv_label_create(panelMs);
  lv_label_set_text(lblMsCountdown, "---");
  lv_obj_set_style_text_font(lblMsCountdown, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblMsCountdown, COL_TEXT, LV_STATE_DEFAULT);
}

// ============================================================================
// Simulation Mode Content
// ============================================================================

// Helper: single-line sim row — label (left, flex) | bar + countdown (right-justified)
// Layout: [Block Name Here          ████░░░░  2h 3m]
static lv_obj_t* createSimRow(lv_obj_t* parent,
                               lv_obj_t** outLabel, lv_obj_t** outBar,
                               lv_obj_t** outCountdown, lv_color_t barColor) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 312, 18);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(row, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(row, 0, LV_STATE_DEFAULT);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  // Label (left — fits before the longer bar)
  *outLabel = lv_label_create(row);
  lv_label_set_text(*outLabel, "---");
  lv_obj_set_style_text_font(*outLabel, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(*outLabel, COL_TEXT, LV_STATE_DEFAULT);
  lv_obj_set_width(*outLabel, 130);
  lv_label_set_long_mode(*outLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(*outLabel, LV_ALIGN_LEFT_MID, 0, 0);

  // Countdown (far right, 55px)
  *outCountdown = lv_label_create(row);
  lv_label_set_text(*outCountdown, "");
  lv_obj_set_style_text_font(*outCountdown, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(*outCountdown, COL_TEXT_DIM, LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(*outCountdown, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
  lv_obj_set_width(*outCountdown, 55);
  lv_obj_align(*outCountdown, LV_ALIGN_RIGHT_MID, 0, 0);

  // Bar (right-justified, next to countdown — 120px wide, 6px tall)
  *outBar = createBar(row, 120, 6, barColor);
  lv_obj_align(*outBar, LV_ALIGN_RIGHT_MID, -58, 0);

  return row;
}

static void createSimContent(lv_obj_t* parent) {
  // Sim content: 3 data rows + separator + activity label + thick bar
  // Height: 4pad + 3×18 + 2×2gap + 1sep + 2gap + 16label + 4pad + 12bar = ~97px
  // Leaves ~47px for an expanded footer below
  panelSimContent = lv_obj_create(parent);
  lv_obj_set_size(panelSimContent, 320, 104);
  lv_obj_set_style_bg_opa(panelSimContent, LV_OPA_TRANSP, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(panelSimContent, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(panelSimContent, 4, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(panelSimContent, 4, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(panelSimContent, 4, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(panelSimContent, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_gap(panelSimContent, 2, LV_STATE_DEFAULT);
  lv_obj_set_flex_flow(panelSimContent, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(panelSimContent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panelSimContent, LV_OBJ_FLAG_HIDDEN);

  // Row 1: Block — hot pink bar
  createSimRow(panelSimContent, &lblSimBlock, &barSimBlock,
               &lblSimBlockCountdown, COL_BAR_BLOCK);

  // Row 2: Mode — aqua bar
  createSimRow(panelSimContent, &lblSimMode, &barSimMode,
               &lblSimModeCountdown, COL_BAR_MODE);

  // Row 3: Profile — color set dynamically (busy/normal/lazy)
  createSimRow(panelSimContent, &lblSimProfile, &barSimProfile,
               &lblSimProfileCountdown, COL_BAR_NORMAL);

  // Separator line
  lv_obj_t* sep = lv_obj_create(panelSimContent);
  lv_obj_set_size(sep, 312, 1);
  lv_obj_set_style_bg_color(sep, COL_BAR_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(sep, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(sep, 0, LV_STATE_DEFAULT);

  // Activity: phase label (left) + countdown (right) — text-only line
  lv_obj_t* rowActText = lv_obj_create(panelSimContent);
  lv_obj_set_size(rowActText, 312, 18);
  lv_obj_set_style_bg_opa(rowActText, LV_OPA_TRANSP, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(rowActText, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(rowActText, 0, LV_STATE_DEFAULT);
  lv_obj_clear_flag(rowActText, LV_OBJ_FLAG_SCROLLABLE);

  lblSimActivity = lv_label_create(rowActText);
  lv_label_set_text(lblSimActivity, "KBD");
  lv_obj_set_style_text_font(lblSimActivity, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSimActivity, COL_ACCENT, LV_STATE_DEFAULT);
  lv_obj_align(lblSimActivity, LV_ALIGN_LEFT_MID, 0, 0);

  lblSimKbInfo = lv_label_create(rowActText);
  lv_label_set_text(lblSimKbInfo, "");
  lv_obj_set_style_text_font(lblSimKbInfo, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSimKbInfo, COL_TEXT_DIM, LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(lblSimKbInfo, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
  lv_obj_set_width(lblSimKbInfo, 55);
  lv_obj_align(lblSimKbInfo, LV_ALIGN_RIGHT_MID, 0, 0);

  // Thick activity bar below (12px tall = 2x normal)
  // Styled like Windows 10 copy dialog: green with shimmer pulse
  barSimKb = lv_bar_create(panelSimContent);
  lv_obj_set_size(barSimKb, 312, 12);
  lv_bar_set_range(barSimKb, 0, 100);
  lv_bar_set_value(barSimKb, 0, LV_ANIM_OFF);
  // Track background
  lv_obj_set_style_bg_color(barSimKb, COL_BAR_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(barSimKb, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(barSimKb, 3, LV_STATE_DEFAULT);
  // Indicator: green gradient for shiny effect
  lv_obj_set_style_bg_color(barSimKb, lv_color_hex(0x00C853), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(barSimKb, lv_color_hex(0x69F0AE), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(barSimKb, LV_GRAD_DIR_VER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(barSimKb, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(barSimKb, 3, LV_PART_INDICATOR);

}

// ============================================================================
// Status Bar (24px)
// ============================================================================

static lv_obj_t* createStatusBar(lv_obj_t* parent) {
  panelStatus = lv_obj_create(parent);
  lv_obj_set_size(panelStatus, 320, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(panelStatus, 1);
  lv_obj_set_style_bg_color(panelStatus, COL_STATUS_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(panelStatus, LV_OPA_COVER, LV_STATE_DEFAULT);
  lv_obj_set_style_radius(panelStatus, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(panelStatus, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(panelStatus, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(panelStatus, 8, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(panelStatus, 6, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(panelStatus, 4, LV_STATE_DEFAULT);
  lv_obj_clear_flag(panelStatus, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panelStatus, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  // Left: animated keyboard + mouse icons (ARGB8888, opacity for dim/bright)
  imgKbIcon = lv_image_create(panelStatus);
  lv_image_set_src(imgKbIcon, &imgdKbNormal);
  lv_obj_set_style_opa(imgKbIcon, 140, LV_STATE_DEFAULT);  // dimmed by default
  lv_obj_align(imgKbIcon, LV_ALIGN_LEFT_MID, 0, -1);

  imgMsIcon = lv_image_create(panelStatus);
  lv_image_set_src(imgMsIcon, &imgdMsNormal);
  lv_obj_set_style_opa(imgMsIcon, 140, LV_STATE_DEFAULT);  // dimmed by default
  lv_obj_align(imgMsIcon, LV_ALIGN_LEFT_MID, 28, -1);

  // Center: device name
  lblStatus = lv_label_create(panelStatus);
  lv_label_set_text(lblStatus, settings.deviceName);
  lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblStatus, COL_TEXT_FOOTER, LV_STATE_DEFAULT);
  lv_obj_align(lblStatus, LV_ALIGN_CENTER, 0, 0);

  // Ghost sprite (ARGB8888, positioned absolutely)
  memset(&ghostDsc, 0, sizeof(ghostDsc));
  ghostDsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
  ghostDsc.header.w = GHOST_FRAME_W;
  ghostDsc.header.h = GHOST_FRAME_H;
  ghostDsc.header.stride = GHOST_FRAME_W * 4;
  ghostDsc.data_size = GHOST_FRAME_W * GHOST_FRAME_H * 4;
  ghostDsc.data = ghostFramesLeft[0];

  imgGhost = lv_image_create(panelStatus);
  lv_image_set_src(imgGhost, &ghostDsc);
  lv_obj_set_pos(imgGhost, ghostX, -6);  // offset up to vertically center in footer
  lv_obj_clear_flag(imgGhost, LV_OBJ_FLAG_SCROLLABLE);

  return panelStatus;
}

// ============================================================================
// Top-level UI Creation
// ============================================================================

static void createUI() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, COL_BG, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_STATE_DEFAULT);

  // Main layout: vertical flex (header + content + status)
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(screen, 0, LV_STATE_DEFAULT);
  lv_obj_set_style_pad_gap(screen, 0, LV_STATE_DEFAULT);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  // Header
  createHeader(screen);

  // Simple mode content (KB + MS cards)
  createSimpleContent(screen);

  // Simulation mode content (Block + Mode + Activity)
  createSimContent(screen);

  // Status bar
  createStatusBar(screen);

  // Splash screen (on top of everything)
  createSplash(screen);
}

// ============================================================================
// Update Functions (called from updateDisplay)
// ============================================================================

static char prevHeaderRight[40] = "";
static char prevKbKey[32] = "";
static char prevKbInterval[32] = "";
static char prevMsState[12] = "";
static char prevMsDuration[32] = "";
static int prevKbBar = -1;
static int prevMsBar = -1;
static char prevKbCountdown[16] = "";
static char prevMsCountdown[16] = "";
static bool splashDismissed = false;
static unsigned long splashStartMs = 0;

static char prevHeaderLeft[32] = "";

static void updateHeader() {
  unsigned long now = millis();
  char buf[40];

  // Left side: device name (simple) or job name (sim)
  const char* leftStr;
  if (settings.operationMode == OP_SIMULATION) {
    leftStr = (settings.jobSimulation < JOB_SIM_COUNT) ? JOB_SIM_NAMES[settings.jobSimulation] : "???";
  } else {
    leftStr = settings.deviceName;
  }
  if (strcmp(leftStr, prevHeaderLeft) != 0) {
    lv_label_set_text(lblDeviceName, leftStr);
    strncpy(prevHeaderLeft, leftStr, sizeof(prevHeaderLeft) - 1);
    prevHeaderLeft[sizeof(prevHeaderLeft) - 1] = '\0';
  }

  // BT icon: solid when connected, flashing via hidden flag when advertising
  // Always keep the symbol text set — use hidden flag for flashing to avoid layout shifts
  lv_label_set_text(lblHeaderBt, LV_SYMBOL_BLUETOOTH);
  if (deviceConnected) {
    lv_obj_set_style_text_color(lblHeaderBt, lv_color_hex(0x0060CC), LV_STATE_DEFAULT);
    if (lv_obj_has_flag(lblHeaderBt, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(lblHeaderBt, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_set_style_text_color(lblHeaderBt, lv_color_hex(0xBBBBBB), LV_STATE_DEFAULT);
    if (lv_obj_has_flag(lblHeaderBt, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(lblHeaderBt, LV_OBJ_FLAG_HIDDEN);
  }

  // Uptime or clock — fixed position (not relative to BT icon)
  if (timeSynced) {
    char timeBuf[12];
    formatCurrentTime(timeBuf, sizeof(timeBuf));
    snprintf(buf, sizeof(buf), "%s", timeBuf);
  } else {
    unsigned long upMs = now - startTime;
    unsigned long secs = upMs / 1000;
    unsigned long mins = secs / 60;
    unsigned long hrs = mins / 60;
    mins %= 60;
    if (hrs > 0) {
      snprintf(buf, sizeof(buf), "%luh %02lum", hrs, mins);
    } else {
      secs %= 60;
      snprintf(buf, sizeof(buf), "%lum %02lus", mins, secs);
    }
  }

  if (strcmp(buf, prevHeaderRight) != 0) {
    lv_label_set_text(lblHeaderRight, buf);
    strncpy(prevHeaderRight, buf, sizeof(prevHeaderRight) - 1);
    prevHeaderRight[sizeof(prevHeaderRight) - 1] = '\0';
  }
}

static void updateSimpleMode() {
  unsigned long now = millis();
  char buf[32];
  bool connected = deviceConnected;

  // --- Keyboard ---
  const char* keyName = (nextKeyIndex < NUM_KEYS) ? AVAILABLE_KEYS[nextKeyIndex].name : "???";
  if (strcmp(keyName, prevKbKey) != 0) {
    lv_label_set_text(lblKbKey, keyName);
    strncpy(prevKbKey, keyName, sizeof(prevKbKey) - 1);
    prevKbKey[sizeof(prevKbKey) - 1] = '\0';
    if (!keyEnabled) {
      lv_obj_set_style_text_color(lblKbKey, COL_TEXT_DIM, LV_STATE_DEFAULT);
    } else {
      lv_obj_set_style_text_color(lblKbKey, COL_ACCENT, LV_STATE_DEFAULT);
    }
  }

  // Interval text
  char minBuf[12], maxBuf[12];
  formatDuration(effectiveKeyMin(), minBuf, sizeof(minBuf), false);
  formatDuration(effectiveKeyMax(), maxBuf, sizeof(maxBuf));
  snprintf(buf, sizeof(buf), "%s - %s", minBuf, maxBuf);
  if (strcmp(buf, prevKbInterval) != 0) {
    lv_label_set_text(lblKbInterval, buf);
    strncpy(prevKbInterval, buf, sizeof(prevKbInterval) - 1);
    prevKbInterval[sizeof(prevKbInterval) - 1] = '\0';
  }

  // Progress bar + countdown
  if (!connected) {
    if (prevKbBar != 0) {
      lv_bar_set_value(barKb, 0, LV_ANIM_OFF);
      prevKbBar = 0;
    }
    if (strcmp(prevKbCountdown, "---") != 0) {
      lv_label_set_text(lblKbCountdown, "---");
      strncpy(prevKbCountdown, "---", sizeof(prevKbCountdown));
    }
  } else if (!keyEnabled) {
    if (prevKbBar != 0) {
      lv_bar_set_value(barKb, 0, LV_ANIM_OFF);
      prevKbBar = 0;
    }
    if (strcmp(prevKbCountdown, "MUTED") != 0) {
      lv_label_set_text(lblKbCountdown, "MUTED");
      strncpy(prevKbCountdown, "MUTED", sizeof(prevKbCountdown));
    }
  } else {
    unsigned long elapsed = now - lastKeyTime;
    if (elapsed > currentKeyInterval) elapsed = currentKeyInterval;
    int remaining = (int)(currentKeyInterval - elapsed);
    int progress = 0;
    if (currentKeyInterval > 0) {
      progress = (int)((unsigned long)remaining * 100UL / currentKeyInterval);
    }
    if (progress != prevKbBar) {
      lv_bar_set_value(barKb, progress, LV_ANIM_OFF);
      prevKbBar = progress;
    }
    char cdBuf[16];
    formatDuration(remaining, cdBuf, sizeof(cdBuf));
    if (strcmp(cdBuf, prevKbCountdown) != 0) {
      lv_label_set_text(lblKbCountdown, cdBuf);
      strncpy(prevKbCountdown, cdBuf, sizeof(prevKbCountdown) - 1);
      prevKbCountdown[sizeof(prevKbCountdown) - 1] = '\0';
    }
  }

  // --- Mouse ---
  const char* msState;
  lv_color_t msColor;
  if (mouseState == MOUSE_IDLE) {
    msState = "IDLE";
    msColor = COL_TEXT_DIM;
  } else if (mouseState == MOUSE_RETURNING) {
    msState = "RETURN";
    msColor = COL_ORANGE;
  } else {
    msState = "MOVING";
    msColor = COL_BAR_MS;
  }
  if (strcmp(msState, prevMsState) != 0) {
    lv_label_set_text(lblMsState, msState);
    lv_obj_set_style_text_color(lblMsState, msColor, LV_STATE_DEFAULT);
    strncpy(prevMsState, msState, sizeof(prevMsState) - 1);
    prevMsState[sizeof(prevMsState) - 1] = '\0';
  }

  // Duration text
  char jigBuf[12], idlBuf[12];
  formatDuration(effectiveMouseJiggle(), jigBuf, sizeof(jigBuf));
  formatDuration(effectiveMouseIdle(), idlBuf, sizeof(idlBuf));
  snprintf(buf, sizeof(buf), "%s / %s", jigBuf, idlBuf);
  if (strcmp(buf, prevMsDuration) != 0) {
    lv_label_set_text(lblMsDuration, buf);
    strncpy(prevMsDuration, buf, sizeof(prevMsDuration) - 1);
    prevMsDuration[sizeof(prevMsDuration) - 1] = '\0';
  }

  // Mouse progress + countdown
  if (!connected) {
    if (prevMsBar != 0) {
      lv_bar_set_value(barMs, 0, LV_ANIM_OFF);
      prevMsBar = 0;
    }
    if (strcmp(prevMsCountdown, "---") != 0) {
      lv_label_set_text(lblMsCountdown, "---");
      strncpy(prevMsCountdown, "---", sizeof(prevMsCountdown));
    }
  } else if (!mouseEnabled) {
    if (prevMsBar != 0) {
      lv_bar_set_value(barMs, 0, LV_ANIM_OFF);
      prevMsBar = 0;
    }
    if (strcmp(prevMsCountdown, "MUTED") != 0) {
      lv_label_set_text(lblMsCountdown, "MUTED");
      strncpy(prevMsCountdown, "MUTED", sizeof(prevMsCountdown));
    }
  } else if (mouseState == MOUSE_RETURNING) {
    if (prevMsBar != 0) {
      lv_bar_set_value(barMs, 0, LV_ANIM_OFF);
      prevMsBar = 0;
    }
    if (strcmp(prevMsCountdown, "0.0s") != 0) {
      lv_label_set_text(lblMsCountdown, "0.0s");
      strncpy(prevMsCountdown, "0.0s", sizeof(prevMsCountdown));
    }
  } else {
    unsigned long elapsed = now - lastMouseStateChange;
    unsigned long duration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    if (elapsed > duration) elapsed = duration;
    int remaining = (int)(duration - elapsed);
    int progress = 0;
    if (duration > 0) {
      if (mouseState == MOUSE_IDLE) {
        progress = (int)(elapsed * 100UL / duration);
        if (progress > 100) progress = 100;
      } else {
        progress = (int)((unsigned long)remaining * 100UL / duration);
      }
    }
    if (progress != prevMsBar) {
      lv_bar_set_value(barMs, progress, LV_ANIM_OFF);
      prevMsBar = progress;
    }
    char cdBuf[16];
    formatDuration(remaining, cdBuf, sizeof(cdBuf));
    if (strcmp(cdBuf, prevMsCountdown) != 0) {
      lv_label_set_text(lblMsCountdown, cdBuf);
      strncpy(prevMsCountdown, cdBuf, sizeof(prevMsCountdown) - 1);
      prevMsCountdown[sizeof(prevMsCountdown) - 1] = '\0';
    }
  }
}

static void updateSimMode() {
  unsigned long now = millis();
  char cdBuf[16];

  // --- Row 1: Block (bar drains as time passes) ---
  const char* blockName = currentBlockName();
  lv_label_set_text(lblSimBlock, blockName ? blockName : "---");
  lv_bar_set_value(barSimBlock, 100 - blockProgress(now), LV_ANIM_OFF);
  if (orch.blockDurationMs > 0) {
    unsigned long elapsed = now - orch.blockStartMs;
    if (elapsed > orch.blockDurationMs) elapsed = orch.blockDurationMs;
    formatMinSec((int)(orch.blockDurationMs - elapsed), cdBuf, sizeof(cdBuf));
    lv_label_set_text(lblSimBlockCountdown, cdBuf);
  }

  // --- Row 2: Mode (bar drains) ---
  const char* modeName = currentModeName();
  lv_label_set_text(lblSimMode, modeName ? modeName : "---");
  lv_bar_set_value(barSimMode, 100 - modeProgress(now), LV_ANIM_OFF);
  if (orch.modeDurationMs > 0) {
    unsigned long elapsed = now - orch.modeStartMs;
    if (elapsed > orch.modeDurationMs) elapsed = orch.modeDurationMs;
    formatMinSec((int)(orch.modeDurationMs - elapsed), cdBuf, sizeof(cdBuf));
    lv_label_set_text(lblSimModeCountdown, cdBuf);
  }

  // --- Row 3: Profile (bar drains, color matches profile) ---
  const char* profStr = (orch.autoProfile < PROFILE_COUNT) ? PROFILE_NAMES_TITLE[orch.autoProfile] : "???";
  lv_label_set_text(lblSimProfile, profStr);

  // Dynamic bar color: busy=pastel green, normal=pastel yellow, lazy=pastel red
  lv_color_t profBarColor = COL_BAR_NORMAL;
  if (orch.autoProfile == PROFILE_BUSY) profBarColor = COL_BAR_BUSY;
  else if (orch.autoProfile == PROFILE_LAZY) profBarColor = COL_BAR_LAZY;
  lv_obj_set_style_bg_color(barSimProfile, profBarColor, LV_PART_INDICATOR);

  if (orch.profileStintMs > 0) {
    unsigned long elapsed = now - orch.profileStintStartMs;
    if (elapsed > orch.profileStintMs) elapsed = orch.profileStintMs;
    int remaining = (int)(((orch.profileStintMs - elapsed) * 100UL) / orch.profileStintMs);
    lv_bar_set_value(barSimProfile, remaining, LV_ANIM_OFF);
    formatMinSec((int)(orch.profileStintMs - elapsed), cdBuf, sizeof(cdBuf));
    lv_label_set_text(lblSimProfileCountdown, cdBuf);
  }

  // --- Activity row: phase name + phase bar (drains) + phase countdown ---
  static const char* PHASE_NAMES_LONG[] = {
    "Keyboard", "Mouse", "Idle", "Switch Window",
    "Keyboard + Mouse (form sim)", "Mouse + Keypress (draw sim)"
  };
  const char* phaseName = (orch.phase < PHASE_COUNT) ? PHASE_NAMES_LONG[orch.phase] : "???";
  lv_label_set_text(lblSimActivity, phaseName);

  // Phase progress bar (drains as phase time passes)
  if (orch.phaseDurationMs > 0) {
    unsigned long elapsed = now - orch.phaseStartMs;
    if (elapsed > orch.phaseDurationMs) elapsed = orch.phaseDurationMs;
    int remaining = (int)(((orch.phaseDurationMs - elapsed) * 100UL) / orch.phaseDurationMs);
    lv_bar_set_value(barSimKb, remaining, LV_ANIM_OFF);
    formatMinSec((int)(orch.phaseDurationMs - elapsed), cdBuf, sizeof(cdBuf));
    lv_label_set_text(lblSimKbInfo, cdBuf);
  } else {
    lv_bar_set_value(barSimKb, 0, LV_ANIM_OFF);
    lv_label_set_text(lblSimKbInfo, "");
  }

}

// ============================================================================
// Status Bar Update
// ============================================================================

static void updateStatusBar() {
  unsigned long now = millis();

  // Left: device name
  lv_label_set_text(lblStatus, settings.deviceName);

  // --- Keyboard icon animation (matches nRF52) ---
  if (!keyEnabled || scheduleSleeping) {
    if (!lv_obj_has_flag(imgKbIcon, LV_OBJ_FLAG_HIDDEN))
      lv_obj_add_flag(imgKbIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (lv_obj_has_flag(imgKbIcon, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(imgKbIcon, LV_OBJ_FLAG_HIDDEN);

    // Switch between normal and pressed (depressed) keycap
    const lv_image_dsc_t* kbImg = orch.keyDown ? &imgdKbPressed : &imgdKbNormal;
    if (kbImg != prevKbImg) {
      lv_image_set_src(imgKbIcon, kbImg);
      prevKbImg = kbImg;
    }
    // Full opacity when active, dimmed when idle
    lv_opa_t kbOpa = orch.keyDown ? LV_OPA_COVER : 140;
    if (kbOpa != prevKbOpa) {
      lv_obj_set_style_opa(imgKbIcon, kbOpa, LV_STATE_DEFAULT);
      prevKbOpa = kbOpa;
    }
  }

  // --- Mouse icon animation (matches nRF52) ---
  if (!mouseEnabled || scheduleSleeping) {
    if (!lv_obj_has_flag(imgMsIcon, LV_OBJ_FLAG_HIDDEN))
      lv_obj_add_flag(imgMsIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (lv_obj_has_flag(imgMsIcon, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(imgMsIcon, LV_OBJ_FLAG_HIDDEN);

    // Priority: click > scroll > moving(nudge) > normal
    const lv_image_dsc_t* msImg;
    int nudge = 0;

    if (now - orch.lastPhantomClickMs < 200) {
      msImg = &imgdMsClick;
    } else if (settings.scrollEnabled && (now - lastScrollTime < 200)) {
      msImg = &imgdMsScroll;
    } else {
      msImg = &imgdMsNormal;
      // Nudge left/right only during active orchestrator mouse phases
      bool mouseMoving = (orch.phase == PHASE_MOUSING && mouseState == MOUSE_JIGGLING) ||
                         (orch.phase == PHASE_KB_MOUSE && orch.kbmsSubPhase == KBMS_MOUSE_SWIPE) ||
                         (orch.phase == PHASE_MOUSE_KB && orch.mskbSubPhase == MSKB_MOUSE_DRAW);
      if (mouseMoving) {
        static const int8_t nudgeTable[] = {0, 1, 0, -1};
        nudge = nudgeTable[(now / 150) % 4];
      }
    }

    // Full opacity when animating, dimmed when idle
    bool msAnimating = (msImg != &imgdMsNormal) || (nudge != 0);
    lv_opa_t msOpa = msAnimating ? LV_OPA_COVER : 140;
    if (msOpa != prevMsOpa) {
      lv_obj_set_style_opa(imgMsIcon, msOpa, LV_STATE_DEFAULT);
      prevMsOpa = msOpa;
    }

    if (msImg != prevMsImg) {
      lv_image_set_src(imgMsIcon, msImg);
      prevMsImg = msImg;
    }
    // Apply nudge offset
    if (nudge != prevMsNudge) {
      lv_obj_align(imgMsIcon, LV_ALIGN_LEFT_MID, 28 + nudge, -1);
      prevMsNudge = nudge;
    }
  }

  // --- Ghost sprite animation ---
  if (imgGhost) {
    bool moved = false;

    // Advance animation frame
    if (now - ghostFrameMs >= GHOST_FRAME_INTERVAL) {
      ghostFrameMs = now;
      ghostFrame = (ghostFrame + 1) % GHOST_NUM_FRAMES;
      const uint8_t** frames = (ghostDir > 0) ? ghostFramesRight : ghostFramesLeft;
      ghostDsc.data = frames[ghostFrame];
      lv_image_set_src(imgGhost, &ghostDsc);
    }

    // Move position
    if (now - ghostMoveMs >= GHOST_MOVE_INTERVAL) {
      ghostMoveMs = now;
      ghostX += ghostDir * GHOST_SPEED;
      if (ghostX <= GHOST_X_MIN) { ghostX = GHOST_X_MIN; ghostDir = 1; }
      if (ghostX >= GHOST_X_MAX) { ghostX = GHOST_X_MAX; ghostDir = -1; }
      moved = true;
    }

    if (moved) {
      lv_obj_set_pos(imgGhost, ghostX, -6);
    }
  }
}

static void showCorrectLayout() {
  bool isSim = (settings.operationMode == OP_SIMULATION);

  if (isSim) {
    if (!lv_obj_has_flag(panelSimpleContent, LV_OBJ_FLAG_HIDDEN))
      lv_obj_add_flag(panelSimpleContent, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_has_flag(panelSimContent, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(panelSimContent, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (lv_obj_has_flag(panelSimpleContent, LV_OBJ_FLAG_HIDDEN))
      lv_obj_clear_flag(panelSimpleContent, LV_OBJ_FLAG_HIDDEN);
    if (!lv_obj_has_flag(panelSimContent, LV_OBJ_FLAG_HIDDEN))
      lv_obj_add_flag(panelSimContent, LV_OBJ_FLAG_HIDDEN);
  }
  // Status bar always visible (both modes)
  if (lv_obj_has_flag(panelStatus, LV_OBJ_FLAG_HIDDEN))
    lv_obj_clear_flag(panelStatus, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================================
// Public API
// ============================================================================

void setupDisplay() {
  spiInit();
  hwReset();

  // Initialize ARGB8888 icon descriptors from sprite sheet data
  initIconBitmaps();

  // Initialize LVGL
  lv_init();

  // Set tick callback (LVGL v9 — replaces LV_TICK_CUSTOM)
  lv_tick_set_cb(lvTickCb);

  // Create ST7789 display with NATIVE portrait resolution (172×320).
  // The Waveshare 1.47" panel is 172 columns on a 240-column GRAM.
  // LVGL's generic MIPI driver only sets MADCTL swap_xy via rotation —
  // passing 320×172 directly leaves swap_xy=false, corrupting the display.
  lvDisplay = lv_st7789_create(
    172, 320,
    LV_LCD_FLAG_NONE,
    lcd_send_cmd, lcd_send_color
  );

  if (!lvDisplay) {
    Serial.println("[ERR] Failed to create LVGL display");
    return;
  }

  // Set display buffers (sized for 320px width after rotation)
  lv_display_set_buffers(lvDisplay, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Byte-swap RGB565 for little-endian ESP32-C6 → big-endian ST7789 SPI
  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565_SWAPPED);

  // Gap: 172-pixel panel centered in 240-pixel GRAM → 34px column offset
  // After ROTATION_90, LVGL y maps to ST7789 columns via MADCTL swap,
  // so the 34px column offset goes on y_gap (not x_gap)
  lv_st7789_set_gap(lvDisplay, 0, 34);

  // Color inversion — most ST7789 panels need this for correct colors
  lv_st7789_set_invert(lvDisplay, true);

  // Rotate to landscape (320×172) — this sets MADCTL's MV (swap XY) bit
  // ROTATION_90 = normal, ROTATION_270 = flipped 180°
  lv_display_set_rotation(lvDisplay,
    settings.displayFlip ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_90);

  // Backlight: PWM via LEDC
  ledcAttach(PIN_LCD_BL, 5000, 8);
  uint8_t brightness = settings.displayBrightness;
  if (brightness < 10) brightness = 10;
  if (brightness > 100) brightness = 100;
  ledcWrite(PIN_LCD_BL, (uint32_t)brightness * 255 / 100);

  // Build the UI widget tree
  createUI();

  splashStartMs = millis();

  Serial.println("[OK] LVGL display initialized (320x172 landscape ST7789)");
}

void updateDisplay() {
  if (!lvDisplay) return;

  unsigned long now = millis();

  // Splash screen: animate ghost, then dismiss after 2 seconds
  if (!splashDismissed) {
    if (now - splashStartMs >= 2000) {
      lv_obj_del(panelSplash);
      panelSplash = nullptr;
      splashGhostImg = nullptr;
      splashDismissed = true;
      displayDirty = true;
      showCorrectLayout();
    } else if (splashGhostImg && now - splashFrameMs >= SPLASH_FRAME_INTERVAL) {
      // Cycle ghost animation frames during splash
      splashFrameMs = now;
      splashGhostFrame = (splashGhostFrame + 1) % SPLASH_NUM_FRAMES;
      splashGhostDsc.data = splashFrames[splashGhostFrame];
      lv_image_set_src(splashGhostImg, &splashGhostDsc);
    }
    lv_timer_handler();
    return;
  }

  if (!displayDirty) {
    // Still call lv_timer_handler for LVGL internal timers/animations
    lv_timer_handler();
    return;
  }
  displayDirty = false;

  // Show correct layout for current operation mode
  showCorrectLayout();

  // Update header
  updateHeader();

  // Update content based on mode
  if (settings.operationMode == OP_SIMULATION) {
    updateSimMode();
  } else {
    updateSimpleMode();
  }

  // Update status bar
  updateStatusBar();

  // Let LVGL render
  lv_timer_handler();
}

void markDisplayDirty() {
  displayDirty = true;
}

void invalidateDisplayShadow() {
  displayDirty = true;
}

void setBacklightBrightness(uint8_t percent) {
  if (percent < 10) percent = 10;
  if (percent > 100) percent = 100;
  ledcWrite(PIN_LCD_BL, (uint32_t)percent * 255 / 100);
}

void setBacklightOff() {
  ledcWrite(PIN_LCD_BL, 0);
}

void setDisplayFlip(bool flipped) {
  if (!lvDisplay) return;
  lv_display_set_rotation(lvDisplay,
    flipped ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_90);
  markDisplayDirty();
}

#include "input.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "platform_hal.h"
#include "display.h"
#include "touch.h"

// ============================================================================
// CYD TOUCH INPUT — Touch-to-action dispatch for TFT touchscreen UI
// ============================================================================

// Layout constants (must match display.cpp rendering)
static const int16_t CONTENT_Y   = 32;   // top of scrollable menu area
static const int16_t MENU_ROW_H  = 40;   // height of each menu row
static const int16_t FOOTER_H    = 48;   // footer area height

// ============================================================================
// MENU VISIBILITY
// ============================================================================

// CYD-excluded setting IDs — hardware not present on CYD
static bool isCydExcluded(uint8_t settingId) {
  switch (settingId) {
    // Games (no physical controls for arcade games)
    case SET_BALL_SPEED: case SET_PADDLE_SIZE: case SET_START_LIVES:
    case SET_HIGH_SCORE:
    case SET_SNAKE_SPEED: case SET_SNAKE_WALLS: case SET_SNAKE_HIGH_SCORE:
    case SET_RACER_SPEED: case SET_RACER_HIGH_SCORE:
    // Volume control (no encoder/buttons for media keys)
    case SET_VOLUME_THEME: case SET_ENC_BUTTON: case SET_SIDE_BUTTON:
    // No USB HID on CYD
    case SET_BT_WHILE_USB:
    // No WebUSB on CYD
    case SET_DASHBOARD:
    // No rotary encoder on CYD
    case SET_INVERT_DIAL:
    // No piezo buzzer on CYD
    case SET_SOUND_ENABLED: case SET_SOUND_TYPE: case SET_SYSTEM_SOUND:
    // OLED-specific settings
    case SET_ANIMATION:
    case SET_SAVER_BRIGHT:
    // nRF52-specific
    case SET_DIE_TEMP:
      return true;
    default:
      return false;
  }
}

bool isMenuItemHidden(int8_t idx) {
  if (idx < 0 || idx >= MENU_ITEM_COUNT) return true;
  const MenuItem& item = MENU_ITEMS[idx];

  bool isSim = (settings.operationMode == OP_SIMULATION);

  // --- Hard-hide entire CYD-excluded sections by index range ---
  // idx 0-3: Breakout heading + items
  // idx 4-7: Snake heading + items
  // idx 8-10: Racer heading + items
  // idx 11-14: Volume heading + items
  // idx 42-45: Sound heading + items
  if (idx <= 14) return true;
  if (idx >= 42 && idx <= 45) return true;

  // --- Orphan heading auto-hide ---
  // Headings with all children hidden are themselves hidden.
  // Must run BEFORE settingId checks -- headings have settingId=0 which
  // collides with SET_KEY_MIN and would be incorrectly hidden in sim mode.
  if (item.type == MENU_HEADING) {
    bool anyChildVisible = false;
    for (int8_t j = idx + 1; j < MENU_ITEM_COUNT; j++) {
      if (MENU_ITEMS[j].type == MENU_HEADING) break;
      if (!isMenuItemHidden(j)) { anyChildVisible = true; break; }
    }
    if (!anyChildVisible) return true;
    return false;  // heading visibility determined solely by orphan check
  }

  // --- CYD hardware exclusions (catch any stragglers not in hard-hide ranges) ---
  if (isCydExcluded(item.settingId)) return true;

  // --- Conditional visibility (independent of mode) ---
  if (item.settingId == SET_MOUSE_AMP && settings.mouseStyle == 0) return true;
  if (item.settingId == SET_CLICK_SLOTS && !settings.phantomClicks) return true;
  if (item.settingId == SET_SWITCH_KEYS && !settings.windowSwitching) return true;

  // --- Simple-only items: hidden in Simulation mode ---
  if (isSim) {
    switch (item.settingId) {
      case SET_KEY_MIN: case SET_KEY_MAX:
      case SET_MOUSE_JIG: case SET_MOUSE_IDLE: case SET_MOUSE_STYLE:
      case SET_MOUSE_AMP: case SET_SCROLL:
      case SET_LAZY_PCT: case SET_BUSY_PCT:
        return true;
    }
  }

  // --- Sim-only items: hidden when NOT in Simulation mode ---
  if (!isSim) {
    switch (item.settingId) {
      case SET_JOB_SIM: case SET_JOB_PERFORMANCE: case SET_JOB_START_TIME:
      case SET_PHANTOM_CLICKS: case SET_CLICK_SLOTS:
      case SET_WINDOW_SWITCH: case SET_SWITCH_KEYS: case SET_HEADER_DISPLAY:
        return true;
    }
  }

  return false;
}

// ============================================================================
// MENU VISIBLE INDEX HELPERS
// ============================================================================

// Returns the MENU_ITEMS[] index for the nth visible row, or -1
static int8_t visibleIdxToMenuIdx(int8_t visibleIdx) {
  int8_t count = -1;
  for (int8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    if (isMenuItemHidden(i)) continue;
    count++;
    if (count == visibleIdx) return i;
  }
  return -1;
}

// Map touch Y coordinate to visible row index, accounting for scroll offset
static int8_t touchYToVisibleIdx(int16_t y) {
  if (y < CONTENT_Y || y >= SCREEN_HEIGHT - FOOTER_H) return -1;
  return menuScrollOffset + (y - CONTENT_Y) / MENU_ROW_H;
}

// Count total visible menu items (for scroll clamping)
static int8_t countVisibleItems() {
  int8_t count = 0;
  for (int8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    if (!isMenuItemHidden(i)) count++;
  }
  return count;
}

// Find the first visible (non-hidden, non-heading) menu item index
static int8_t firstVisibleValueIdx() {
  for (int8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    if (!isMenuItemHidden(i) && MENU_ITEMS[i].type != MENU_HEADING) return i;
  }
  return 0;
}

// ============================================================================
// EDITOR TOUCH HANDLING
// ============================================================================

// Touch regions for the value editor popup
static const TouchTarget editorMinusBtn  = { 60,  120, 50, 40, NULL };
static const TouchTarget editorPlusBtn   = { 210, 120, 50, 40, NULL };
static const TouchTarget editorCancelBtn = { 60,  170, 80, 30, NULL };
static const TouchTarget editorOkBtn     = { 180, 170, 80, 30, NULL };

static void handleEditorTouch(const TouchEvent& evt) {
  if (evt.gesture != GESTURE_TAP) return;

  // Editor state is managed by display.cpp — we access it via the
  // menuCursor which points to the item being edited.
  // The editor overlay exposes editorMenuIdx and editorValue through
  // the display module's API.

  // We need to read the current editor state. Since the editor is
  // a display-layer concept on CYD, we use the menuCursor as the
  // active editor item index (set when showValueEditor was called).
  int8_t idx = menuCursor;
  if (idx < 0 || idx >= MENU_ITEM_COUNT) return;

  const MenuItem& item = MENU_ITEMS[idx];
  uint32_t val = getSettingValue(item.settingId);

  if (hitTest(evt.x, evt.y, editorMinusBtn)) {
    // Decrease value by step, clamp to minVal
    if (val > item.minVal) {
      uint32_t step = item.step > 0 ? item.step : 1;
      if (val - item.minVal >= step) {
        val -= step;
      } else {
        val = item.minVal;
      }
      setSettingValue(item.settingId, val);
    }
    markDisplayDirty();
  } else if (hitTest(evt.x, evt.y, editorPlusBtn)) {
    // Increase value by step, clamp to maxVal
    if (val < item.maxVal) {
      uint32_t step = item.step > 0 ? item.step : 1;
      if (item.maxVal - val >= step) {
        val += step;
      } else {
        val = item.maxVal;
      }
      setSettingValue(item.settingId, val);
    }
    markDisplayDirty();
  } else if (hitTest(evt.x, evt.y, editorCancelBtn)) {
    // Cancel — don't save, just close
    hideValueEditor();
    markDisplayDirty();
  } else if (hitTest(evt.x, evt.y, editorOkBtn)) {
    // OK — value already applied via setSettingValue above; save and close
    saveSettings();
    hideValueEditor();
    markDisplayDirty();
  }
}

// ============================================================================
// MODE_NORMAL TOUCH HANDLING
// ============================================================================

// Touch regions for the normal status screen
static const TouchTarget settingsBtn = { 10,  192, 100, 40, NULL };
static const TouchTarget sleepBtn    = { 210, 192, 100, 40, NULL };
static const TouchTarget toggleBtn   = { 120, 192, 80,  40, NULL };

static void handleNormalTouch(const TouchEvent& evt) {
  if (evt.gesture != GESTURE_TAP) return;

  if (hitTest(evt.x, evt.y, settingsBtn)) {
    // Switch to menu mode
    currentMode = MODE_MENU;
    // Find first visible non-heading item
    menuCursor = firstVisibleValueIdx();
    menuScrollOffset = 0;
    markDisplayDirty();
  } else if (hitTest(evt.x, evt.y, sleepBtn)) {
    sleepPending = true;
  } else if (hitTest(evt.x, evt.y, toggleBtn)) {
    // Cycle KB/MS enables: KB+MS -> KB only -> MS only -> KB+MS
    if (keyEnabled && mouseEnabled) {
      mouseEnabled = false;           // KB only
    } else if (keyEnabled && !mouseEnabled) {
      keyEnabled = false;
      mouseEnabled = true;            // MS only
    } else {
      keyEnabled = true;
      mouseEnabled = true;            // KB+MS
    }
    markDisplayDirty();
  }
}

// ============================================================================
// MODE_MENU TOUCH HANDLING
// ============================================================================

// Touch regions for menu screen
static const TouchTarget backArrowBtn = { 0, 0, 60, 32, NULL };
static const TouchTarget menuPrevBtn  = { 10,  192, 80, 40, NULL };
static const TouchTarget menuNextBtn  = { 230, 192, 80, 40, NULL };

static void handleMenuTouch(const TouchEvent& evt) {
  if (evt.gesture != GESTURE_TAP) return;

  // Back arrow — return to normal mode
  if (hitTest(evt.x, evt.y, backArrowBtn)) {
    Serial.printf("[Input] Back arrow hit at (%d,%d)\n", evt.x, evt.y);
    currentMode = MODE_NORMAL;
    markDisplayDirty();
    return;
  }

  // Pagination buttons
  int8_t visibleRows = (SCREEN_HEIGHT - CONTENT_Y - FOOTER_H) / MENU_ROW_H;
  int8_t totalVisible = countVisibleItems();

  if (hitTest(evt.x, evt.y, menuPrevBtn)) {
    menuScrollOffset -= visibleRows;
    if (menuScrollOffset < 0) menuScrollOffset = 0;
    markDisplayDirty();
    return;
  }
  if (hitTest(evt.x, evt.y, menuNextBtn)) {
    menuScrollOffset += visibleRows;
    int8_t maxScroll = totalVisible - visibleRows;
    if (maxScroll < 0) maxScroll = 0;
    if (menuScrollOffset > maxScroll) menuScrollOffset = maxScroll;
    markDisplayDirty();
    return;
  }

  // Tap on a menu row
  int8_t visIdx = touchYToVisibleIdx(evt.y);
  if (visIdx < 0) return;

  int8_t menuIdx = visibleIdxToMenuIdx(visIdx);
  if (menuIdx < 0 || menuIdx >= MENU_ITEM_COUNT) return;

  const MenuItem& item = MENU_ITEMS[menuIdx];

  if (item.type == MENU_HEADING) return;

  if (item.type == MENU_ACTION) {
    switch (item.settingId) {
      case SET_KEY_SLOTS:
      case SET_CLICK_SLOTS:
        return;  // not yet implemented for CYD touch
      case SET_RESTORE_DEFAULTS:
        loadDefaults();
        saveSettings();
        markDisplayDirty();
        return;
      case SET_REBOOT:
        ESP.restart();
        return;
      case SET_OP_MODE: {
        // Cycle between Simple (0) and Simulation (1) — skip other modes on CYD
        uint8_t newMode = (settings.operationMode == OP_SIMPLE) ? OP_SIMULATION : OP_SIMPLE;
        setSettingValue(SET_OP_MODE, newMode);
        saveSettings();
        delay(200);
        ESP.restart();
        return;
      }
      case SET_BLE_IDENTITY: {
        // Cycle through: Custom(0) -> preset 1 -> ... -> preset 10 -> back to Custom(0)
        uint8_t newIdx = (settings.decoyIndex + 1) % (DECOY_COUNT + 1);
        settings.decoyIndex = newIdx;
        saveSettings();
        delay(200);
        ESP.restart();
        return;
      }
      case SET_SCHEDULE_MODE: {
        uint8_t newMode = (settings.scheduleMode + 1) % SCHED_MODE_COUNT;
        setSettingValue(SET_SCHEDULE_MODE, newMode);
        saveSettings();
        markDisplayDirty();
        return;
      }
      case SET_SET_CLOCK:
        // On CYD, time sync happens via serial =time:SECONDS command
        // Just show a message via serial for now
        Serial.println("[Clock] Use serial: =time:SECONDS to sync");
        return;
    }
  }

  if (item.type == MENU_VALUE) {
    menuCursor = menuIdx;
    showValueEditor((uint8_t)menuIdx);
    markDisplayDirty();
  }
}

// ============================================================================
// MAIN TOUCH DISPATCH
// ============================================================================

void handleTouchInput() {
  TouchEvent evt = pollTouch();
  if (evt.gesture == GESTURE_NONE) return;

  Serial.printf("[Touch] gesture=%d x=%d y=%d dy=%d mode=%d\n",
                evt.gesture, evt.x, evt.y, evt.dy, currentMode);

  // Reset mode activity timer (prevents mode timeout)
  lastModeActivity = millis();

  // If the value editor overlay is active, route all input there
  if (isEditorVisible()) {
    handleEditorTouch(evt);
    return;
  }

  // Dispatch based on current UI mode
  switch (currentMode) {
    case MODE_NORMAL:
      handleNormalTouch(evt);
      break;

    case MODE_MENU:
      handleMenuTouch(evt);
      break;

    default:
      // Other modes (SLOTS, NAME, DECOY, SCHEDULE, etc.) not yet
      // implemented for CYD touch — Phase 3+ TODO
      break;
  }
}

#include "input.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"

// ============================================================================
// NAME EDITOR HELPERS
// ============================================================================

void initNameEditor() {
  // Snapshot current name for change detection
  strncpy(nameOriginal, settings.deviceName, NAME_MAX_LEN);
  nameOriginal[NAME_MAX_LEN] = '\0';

  // Convert each character to its index in NAME_CHARS
  int nameLen = strlen(settings.deviceName);
  for (int i = 0; i < NAME_MAX_LEN; i++) {
    if (i < nameLen) {
      // Find character in NAME_CHARS
      const char* found = strchr(NAME_CHARS, settings.deviceName[i]);
      nameCharIndex[i] = found ? (uint8_t)(found - NAME_CHARS) : 0;  // default 'A' if not found
    } else {
      nameCharIndex[i] = NAME_CHAR_END;
    }
  }
  activeNamePos = 0;
  nameConfirming = false;
  nameRebootYes = true;
}

// Save name from editor indices. Returns true if name changed.
bool saveNameEditor() {
  char newName[NAME_MAX_LEN + 1];
  int len = 0;
  for (int i = 0; i < NAME_MAX_LEN; i++) {
    if (nameCharIndex[i] >= NAME_CHAR_COUNT) break;  // END or invalid
    newName[len++] = NAME_CHARS[nameCharIndex[i]];
  }
  // Guard against empty name
  if (len == 0) {
    strncpy(newName, DEVICE_NAME, NAME_MAX_LEN);
    len = strlen(DEVICE_NAME);
    if (len > NAME_MAX_LEN) len = NAME_MAX_LEN;
  }
  newName[len] = '\0';

  bool changed = (strcmp(newName, nameOriginal) != 0);
  strncpy(settings.deviceName, newName, NAME_MAX_LEN);
  settings.deviceName[NAME_MAX_LEN] = '\0';
  saveSettings();
  return changed;
}

void returnToMenuFromName() {
  currentMode = MODE_MENU;
  menuCursor = 18;           // "Device name" item index
  menuEditing = false;
  nameConfirming = false;
  menuScrollOffset = (18 > 4) ? 18 - 4 : 0;  // ensure cursor visible in viewport
  Serial.println("Mode: MENU (from NAME)");
}

// ============================================================================
// MENU CURSOR
// ============================================================================

// Move menu cursor by direction, skipping headings
void moveCursor(int direction) {
  int8_t next = menuCursor + direction;
  // Skip headings and conditionally hidden items
  while (next >= 0 && next < MENU_ITEM_COUNT &&
         (MENU_ITEMS[next].type == MENU_HEADING ||
          (MENU_ITEMS[next].settingId == SET_MOUSE_AMP && settings.mouseStyle == 0))) {
    next += direction;
  }
  // Clamp at bounds
  if (next < 0 || next >= MENU_ITEM_COUNT) return;
  menuCursor = next;

  // Adjust scroll to keep cursor visible (5-row viewport)
  // Show the heading above the cursor when scrolling down
  int8_t viewTop = menuScrollOffset;
  int8_t viewBottom = menuScrollOffset + 4;  // 5 rows: 0..4

  if (menuCursor <= viewTop) {
    menuScrollOffset = menuCursor;
    // Show heading above if it exists
    if (menuScrollOffset > 0 && MENU_ITEMS[menuScrollOffset - 1].type == MENU_HEADING) {
      menuScrollOffset--;
    }
  } else if (menuCursor > viewBottom) {
    menuScrollOffset = menuCursor - 4;
  }
  if (menuScrollOffset < 0) menuScrollOffset = 0;

  // Reset help scroll on cursor change
  helpScrollPos = 0;
  helpScrollDir = 1;
  helpScrollTimer = millis();
}

// ============================================================================
// ENCODER INPUT
// ============================================================================

void handleEncoder() {
  int change = encoderPos - lastEncoderPos;

  if (abs(change) >= 4) {  // Full detent
    int direction = (change > 0) ? 1 : -1;
    lastEncoderPos = encoderPos;
    lastModeActivity = millis();

    // Suppress input during sleep confirm/cancel overlay
    if (sleepConfirmActive || sleepCancelActive) return;

    // Wake screensaver -- consume input
    if (screensaverActive) { screensaverActive = false; return; }

    switch (currentMode) {
      case MODE_NORMAL: {
        // Switch timing profile: LAZY <- NORMAL -> BUSY (clamped)
        int p = (int)currentProfile + direction;
        if (p < 0) p = 0;
        if (p >= PROFILE_COUNT) p = PROFILE_COUNT - 1;
        currentProfile = (Profile)p;
        profileDisplayUntil = millis() + PROFILE_DISPLAY_MS;
        scheduleNextKey();
        scheduleNextMouseState();
        break;
      }

      case MODE_MENU:
        if (defaultsConfirming) {
          defaultsConfirmYes = !defaultsConfirmYes;
        } else if (rebootConfirming) {
          rebootConfirmYes = !rebootConfirmYes;
        } else if (menuEditing && menuCursor >= 0) {
          // Adjust the selected item's value
          const MenuItem& item = MENU_ITEMS[menuCursor];
          int32_t val = (int32_t)getSettingValue(item.settingId);
          // Negative-display values: encoder CW -> toward 0%, CCW -> toward -50%
          int dir = (item.format == FMT_PERCENT_NEG) ? -direction : direction;
          val += dir * (int32_t)item.step;
          if (val < (int32_t)item.minVal) val = item.minVal;
          if (val > (int32_t)item.maxVal) val = item.maxVal;
          setSettingValue(item.settingId, (uint32_t)val);
        } else {
          // Navigate cursor
          if (menuCursor == -1 && direction > 0) {
            // Move from title to first selectable item
            menuCursor = -1;
            moveCursor(1);
          } else if (menuCursor >= 0) {
            moveCursor(direction);
          }
        }
        break;

      case MODE_SLOTS:
        // Cycle key for active slot
        settings.keySlots[activeSlot] = (settings.keySlots[activeSlot] + direction + NUM_KEYS) % NUM_KEYS;
        break;

      case MODE_NAME:
        if (nameConfirming) {
          // Toggle Yes/No selection
          nameRebootYes = !nameRebootYes;
        } else {
          // Cycle character at active position
          nameCharIndex[activeNamePos] = (nameCharIndex[activeNamePos] + direction + NAME_CHAR_TOTAL) % NAME_CHAR_TOTAL;
        }
        break;

      default:
        break;
    }
  }
}

// ============================================================================
// BUTTON INPUT
// ============================================================================

void handleButtons() {
  static bool lastEncBtn = HIGH;
  static unsigned long lastEncPress = 0;
  const unsigned long DEBOUNCE = 200;

  unsigned long now = millis();
  bool encBtn = digitalRead(PIN_ENCODER_BTN);
  bool funcBtn = digitalRead(PIN_FUNC_BTN);

  // Encoder button - mode-dependent behavior
  if (encBtn == LOW && lastEncBtn == HIGH && (now - lastEncPress > DEBOUNCE)) {
    lastEncPress = now;
    lastModeActivity = now;

    // Suppress input during sleep confirm/cancel overlay
    if (sleepConfirmActive || sleepCancelActive) { lastEncBtn = encBtn; return; }

    // Wake screensaver -- consume input
    if (screensaverActive) { screensaverActive = false; lastEncBtn = encBtn; return; }

    switch (currentMode) {
      case MODE_NORMAL: {
        // Cycle KB/MS enable combos
        bool wasKeyEnabled = keyEnabled;
        bool wasMouseEnabled = mouseEnabled;
        uint8_t state = (keyEnabled ? 2 : 0) | (mouseEnabled ? 1 : 0);
        state = (state == 0) ? 3 : state - 1;
        keyEnabled = (state & 2) != 0;
        mouseEnabled = (state & 1) != 0;
        // Reset timers when toggling back on so bars start fresh
        if (keyEnabled && !wasKeyEnabled) {
          lastKeyTime = now;
          scheduleNextKey();
        }
        if (mouseEnabled && !wasMouseEnabled) {
          lastMouseStateChange = now;
          mouseState = MOUSE_IDLE;
          mouseNetX = 0;
          mouseNetY = 0;
          mouseReturnTotal = 0;
          scheduleNextMouseState();
        }
        Serial.print("KB:"); Serial.print(keyEnabled ? "ON" : "OFF");
        Serial.print(" MS:"); Serial.println(mouseEnabled ? "ON" : "OFF");
        break;
      }

      case MODE_MENU:
        if (defaultsConfirming) {
          if (defaultsConfirmYes) {
            loadDefaults();
            saveSettings();
            currentProfile = PROFILE_NORMAL;
            pickNextKey();
            scheduleNextKey();
            scheduleNextMouseState();
            Serial.println("Settings restored to defaults");
          }
          defaultsConfirming = false;
        } else if (rebootConfirming) {
          if (rebootConfirmYes) {
            Serial.println("Rebooting...");
            NVIC_SystemReset();
          }
          rebootConfirming = false;
        } else if (menuEditing) {
          // Exit edit mode
          menuEditing = false;
          Serial.println("Menu: edit done");
        } else if (menuCursor >= 0) {
          const MenuItem& item = MENU_ITEMS[menuCursor];
          if (item.type == MENU_VALUE && item.minVal != item.maxVal) {
            // Enter edit mode (skip read-only items where min == max)
            menuEditing = true;
            Serial.print("Menu: editing "); Serial.println(item.label);
          } else if (item.type == MENU_ACTION) {
            if (item.settingId == SET_KEY_SLOTS) {
              currentMode = MODE_SLOTS;
              activeSlot = 0;
              Serial.println("Mode: SLOTS");
            } else if (item.settingId == SET_DEVICE_NAME) {
              currentMode = MODE_NAME;
              initNameEditor();
              Serial.println("Mode: NAME");
            } else if (item.settingId == SET_RESTORE_DEFAULTS) {
              defaultsConfirming = true;
              defaultsConfirmYes = false;  // default to No
              Serial.println("Menu: restore defaults?");
            } else if (item.settingId == SET_REBOOT) {
              rebootConfirming = true;
              rebootConfirmYes = false;  // default to No
              Serial.println("Menu: reboot?");
            }
          }
        }
        break;

      case MODE_SLOTS:
        // Advance active slot cursor (0->1->...->7->0)
        activeSlot = (activeSlot + 1) % NUM_SLOTS;
        saveSettings();
        Serial.print("Active slot: "); Serial.println(activeSlot);
        break;

      case MODE_NAME:
        if (nameConfirming) {
          // Confirm reboot prompt selection
          if (nameRebootYes) {
            Serial.println("Rebooting for name change...");
            NVIC_SystemReset();
          } else {
            returnToMenuFromName();
          }
        } else {
          // Advance cursor to next position (wraps)
          activeNamePos = (activeNamePos + 1) % NAME_MAX_LEN;
        }
        break;

      default: break;
    }
  }
  lastEncBtn = encBtn;

  // Function button - short = open/close menu, hold = sleep confirmation
  if (funcBtn == LOW) {
    if (!funcBtnWasPressed) {
      funcBtnPressStart = now;
      funcBtnWasPressed = true;
    } else {
      unsigned long holdTime = now - funcBtnPressStart;
      // Show confirmation overlay after threshold
      if (!sleepConfirmActive && holdTime >= SLEEP_CONFIRM_THRESHOLD_MS) {
        sleepConfirmActive = true;
        sleepConfirmStart = now;
        if (screensaverActive) screensaverActive = false;
        Serial.println("Sleep confirm: started");
      }
      // Countdown elapsed -- trigger sleep
      if (sleepConfirmActive && (now - sleepConfirmStart >= SLEEP_COUNTDOWN_MS)) {
        sleepConfirmActive = false;
        sleepPending = true;
        funcBtnWasPressed = false;
      }
    }
  } else {
    if (funcBtnWasPressed) {
      unsigned long holdTime = now - funcBtnPressStart;
      if (sleepConfirmActive) {
        // Released during countdown -- cancel
        sleepConfirmActive = false;
        sleepCancelActive = true;
        sleepCancelStart = now;
        Serial.println("Sleep confirm: cancelled");
      } else if (holdTime > 50) {
        // Short press -- mode switching
        lastModeActivity = now;

        // Wake screensaver -- consume input
        if (screensaverActive) { screensaverActive = false; funcBtnWasPressed = false; return; }

        switch (currentMode) {
          case MODE_NORMAL:
            // Open menu
            currentMode = MODE_MENU;
            menuCursor = -1;
            menuScrollOffset = 0;
            menuEditing = false;
            helpScrollPos = 0;
            helpScrollDir = 1;
            helpScrollTimer = millis();
            Serial.println("Mode: MENU");
            break;

          case MODE_MENU:
            if (defaultsConfirming) {
              defaultsConfirming = false;  // cancel confirmation
            } else if (rebootConfirming) {
              rebootConfirming = false;    // cancel confirmation
            } else {
              // Close menu -> save and return to NORMAL
              menuEditing = false;
              currentMode = MODE_NORMAL;
              saveSettings();
              Serial.println("Mode: NORMAL (menu closed)");
            }
            break;

          case MODE_SLOTS:
            // Back to menu at "Key slots" item (index 3)
            saveSettings();
            currentMode = MODE_MENU;
            menuCursor = 3;
            menuEditing = false;
            // Ensure cursor visible
            menuScrollOffset = 0;
            Serial.println("Mode: MENU (from SLOTS)");
            break;

          case MODE_NAME:
            if (!nameConfirming) {
              bool changed = saveNameEditor();
              if (changed) {
                nameConfirming = true;
                nameRebootYes = true;
              } else {
                returnToMenuFromName();
              }
            } else {
              // From reboot prompt -- func button = "No" (skip reboot)
              returnToMenuFromName();
            }
            break;

          default: break;
        }
      }
      funcBtnWasPressed = false;
    }
  }
}

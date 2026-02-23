#include "input.h"
#include "state.h"
#include "keys.h"
#include "settings.h"
#include "timing.h"
#include "hid.h"
#include "serial_cmd.h"
#include "schedule.h"
#include "orchestrator.h"
#include "sim_data.h"
#include "display.h"
#include "sound.h"

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
  menuCursor = MENU_IDX_BLE_IDENTITY;
  menuEditing = false;
  nameConfirming = false;
  menuScrollOffset = (MENU_IDX_BLE_IDENTITY > 4) ? MENU_IDX_BLE_IDENTITY - 4 : 0;
  markDisplayDirty();
  Serial.println("Mode: MENU (from NAME)");
}

// ============================================================================
// DECOY PICKER HELPERS
// ============================================================================

void returnToMenuFromDecoy() {
  currentMode = MODE_MENU;
  menuCursor = MENU_IDX_BLE_IDENTITY;
  menuEditing = false;
  decoyConfirming = false;
  menuScrollOffset = (MENU_IDX_BLE_IDENTITY > 4) ? MENU_IDX_BLE_IDENTITY - 4 : 0;
  markDisplayDirty();
  Serial.println("Mode: MENU (from DECOY)");
}

void initDecoyPicker() {
  // Position cursor on current selection
  decoyCursor = settings.decoyIndex;  // 0=Custom at bottom mapped later, 1-10=preset
  if (settings.decoyIndex == 0) {
    decoyCursor = DECOY_COUNT;  // "Custom" is last item
  } else {
    decoyCursor = settings.decoyIndex - 1;  // 0-based in list
  }
  // Ensure cursor is visible in 5-row viewport
  if (decoyCursor > 4) {
    decoyScrollOffset = decoyCursor - 4;
  } else {
    decoyScrollOffset = 0;
  }
  decoyConfirming = false;
  decoyRebootYes = true;
  decoyOriginal = settings.decoyIndex;
}

// ============================================================================
// SCHEDULE EDITOR HELPERS
// ============================================================================

void initScheduleEditor() {
  scheduleCursor = 0;
  scheduleEditing = false;
  // Snapshot for revert on timeout
  scheduleOrigMode = settings.scheduleMode;
  scheduleOrigStart = settings.scheduleStart;
  scheduleOrigEnd = settings.scheduleEnd;
}

void returnToMenuFromSchedule() {
  currentMode = MODE_MENU;
  menuCursor = MENU_IDX_SCHEDULE;
  menuEditing = false;
  menuScrollOffset = (MENU_IDX_SCHEDULE > 4) ? MENU_IDX_SCHEDULE - 4 : 0;
  markDisplayDirty();
  Serial.println("Mode: MENU (from SCHEDULE)");
}

// ============================================================================
// CLOCK EDITOR HELPERS
// ============================================================================

void initClockEditor() {
  clockCursor = 0;
  clockEditing = false;
  // Pre-fill from current time if synced, else default 12:00
  if (timeSynced) {
    uint32_t secs = currentDaySeconds();
    clockHour = (uint8_t)(secs / 3600);
    clockMinute = (uint8_t)((secs % 3600) / 60);
  } else {
    clockHour = 12;
    clockMinute = 0;
  }
}

void returnToMenuFromClock() {
  currentMode = MODE_MENU;
  menuCursor = MENU_IDX_SET_CLOCK;
  menuEditing = false;
  clockEditing = false;
  menuScrollOffset = (MENU_IDX_SET_CLOCK > 4) ? MENU_IDX_SET_CLOCK - 4 : 0;
  markDisplayDirty();
  Serial.println("Mode: MENU (from CLOCK)");
}

// ============================================================================
// MODE PICKER HELPERS
// ============================================================================

void initModePicker() {
  modeOriginalValue = settings.operationMode;
  modePickerCursor = settings.operationMode;  // 0=Simple, 1=Simulation
  modeConfirming = false;
  modeRebootYes = true;
}

void returnToMenuFromMode() {
  currentMode = MODE_MENU;
  menuCursor = MENU_IDX_OP_MODE;
  menuEditing = false;
  modeConfirming = false;
  menuScrollOffset = (MENU_IDX_OP_MODE > 4) ? MENU_IDX_OP_MODE - 4 : 0;
  markDisplayDirty();
  Serial.println("Mode: MENU (from MODE)");
}

// ============================================================================
// MENU ITEM VISIBILITY
// ============================================================================

// Returns true if a menu item should be hidden in the current mode
bool isMenuItemHidden(int8_t idx) {
  if (idx < 0 || idx >= MENU_ITEM_COUNT) return true;
  const MenuItem& item = MENU_ITEMS[idx];

  bool isSim = (settings.operationMode == 1);

  // Orphan heading auto-hide: headings with all children hidden.
  // Must run BEFORE settingId checks — headings have settingId=0 which
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

  // Conditional visibility (independent of mode)
  if (item.settingId == SET_MOUSE_AMP && settings.mouseStyle == 0) return true;
  if (item.settingId == SET_CLICK_TYPE && !settings.phantomClicks) return true;
  if (item.settingId == SET_SWITCH_KEYS && !settings.windowSwitching) return true;
  if (item.settingId == SET_SOUND_TYPE && !settings.soundEnabled) return true;

  // Simple-only items: hidden in Simulation mode
  if (isSim) {
    switch (item.settingId) {
      case SET_KEY_MIN: case SET_KEY_MAX:
      case SET_MOUSE_JIG: case SET_MOUSE_IDLE: case SET_MOUSE_STYLE:
      case SET_MOUSE_AMP: case SET_SCROLL:
      case SET_LAZY_PCT: case SET_BUSY_PCT:
        return true;
    }
  }

  // Sim-only items: hidden in Simple mode
  if (!isSim) {
    switch (item.settingId) {
      case SET_JOB_SIM: case SET_JOB_PERFORMANCE:
      case SET_PHANTOM_CLICKS: case SET_CLICK_TYPE:
      case SET_WINDOW_SWITCH: case SET_SWITCH_KEYS: case SET_HEADER_DISPLAY:
        return true;
    }
  }

  return false;
}

// ============================================================================
// MENU CURSOR
// ============================================================================

// Move menu cursor by direction, skipping headings and hidden items
void moveCursor(int direction) {
  int8_t next = menuCursor + direction;
  // Skip headings and hidden items
  while (next >= 0 && next < MENU_ITEM_COUNT &&
         (MENU_ITEMS[next].type == MENU_HEADING || isMenuItemHidden(next))) {
    next += direction;
  }
  // Clamp at bounds
  if (next < 0 || next >= MENU_ITEM_COUNT) return;
  menuCursor = next;

  // Adjust scroll to keep cursor visible in 5-row viewport
  // Count visible items between scroll offset and cursor
  int visBeforeCursor = 0;
  for (int8_t i = menuScrollOffset; i < menuCursor && i < MENU_ITEM_COUNT; i++) {
    if (!isMenuItemHidden(i)) visBeforeCursor++;
  }

  if (menuCursor < menuScrollOffset) {
    // Cursor above viewport — scroll up
    menuScrollOffset = menuCursor;
    // Show heading above if it exists and is visible
    if (menuScrollOffset > 0 && MENU_ITEMS[menuScrollOffset - 1].type == MENU_HEADING
        && !isMenuItemHidden(menuScrollOffset - 1)) {
      menuScrollOffset--;
    }
  } else if (visBeforeCursor >= 5) {
    // Cursor below viewport — scroll so cursor is the 5th visible item
    int vis = 0;
    for (int8_t i = menuCursor; i >= 0; i--) {
      if (!isMenuItemHidden(i)) {
        vis++;
        if (vis >= 5) {
          menuScrollOffset = i;
          break;
        }
      }
    }
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
    if (settings.invertDial) direction = -direction;
    lastEncoderPos = encoderPos;
    lastModeActivity = millis();
    markDisplayDirty();

    // Suppress input during sleep confirm/cancel overlay
    if (sleepConfirmActive || sleepCancelActive) return;

    // Wake from scheduled light sleep -- consume input
    if (scheduleSleeping) { exitLightSleep(); return; }

    // Wake screensaver -- consume input
    if (screensaverActive) { screensaverActive = false; return; }

    switch (currentMode) {
      case MODE_NORMAL:
        if (settings.operationMode == 1) {
          // Simulation mode: adjust job performance (0–11)
          int val = (int)settings.jobPerformance + direction;
          if (val < 0) val = 0;
          if (val > 11) val = 11;
          settings.jobPerformance = (uint8_t)val;
          orch.previewActive = true;
          orch.previewStartMs = millis();
          pushSerialStatus();
        } else {
          // Simple mode: switch timing profile
          int p = (int)currentProfile + direction;
          if (p < 0) p = 0;
          if (p >= PROFILE_COUNT) p = PROFILE_COUNT - 1;
          currentProfile = (Profile)p;
          profileDisplayStart = millis();
          scheduleNextKey();
          scheduleNextMouseState();
          pushSerialStatus();
        }
        break;

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
          if (item.settingId == SET_SOUND_TYPE) startSoundPreview((uint8_t)val);
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

      case MODE_DECOY:
        if (decoyConfirming) {
          decoyRebootYes = !decoyRebootYes;
        } else {
          // Scroll cursor (0..DECOY_COUNT where DECOY_COUNT = "Custom")
          int8_t next = decoyCursor + direction;
          if (next < 0) next = 0;
          if (next > DECOY_COUNT) next = DECOY_COUNT;
          decoyCursor = next;
          // Adjust scroll to keep cursor visible (5-row viewport)
          if (decoyCursor < decoyScrollOffset) {
            decoyScrollOffset = decoyCursor;
          } else if (decoyCursor > decoyScrollOffset + 4) {
            decoyScrollOffset = decoyCursor - 4;
          }
        }
        break;

      case MODE_SCHEDULE:
        if (scheduleEditing) {
          // Adjust value based on cursor position
          if (scheduleCursor == 0) {
            // Mode: cycle 0-2
            int8_t val = (int8_t)settings.scheduleMode + direction;
            if (val < 0) val = 0;
            if (val >= SCHED_MODE_COUNT) val = SCHED_MODE_COUNT - 1;
            settings.scheduleMode = (uint8_t)val;
          } else if (scheduleCursor == 1) {
            // Start time: 0-287
            int16_t val = (int16_t)settings.scheduleStart + direction;
            if (val < 0) val = 0;
            if (val >= SCHEDULE_SLOTS) val = SCHEDULE_SLOTS - 1;
            settings.scheduleStart = (uint16_t)val;
          } else {
            // End time: 0-287
            int16_t val = (int16_t)settings.scheduleEnd + direction;
            if (val < 0) val = 0;
            if (val >= SCHEDULE_SLOTS) val = SCHEDULE_SLOTS - 1;
            settings.scheduleEnd = (uint16_t)val;
          }
        } else {
          // Navigate cursor (0-2), skip unavailable rows
          int8_t next = scheduleCursor + direction;
          if (next < 0) next = 0;
          if (next > 2) next = 2;
          if (!timeSynced || settings.scheduleMode == SCHED_OFF) {
            // Lock to Mode only
            next = 0;
          } else if (settings.scheduleMode == SCHED_AUTO_SLEEP && next == 1) {
            // Skip Start time (irrelevant for deep sleep)
            next = (direction > 0) ? 2 : 0;
          }
          scheduleCursor = next;
        }
        break;

      case MODE_SET_CLOCK:
        if (clockEditing) {
          if (clockCursor == 0) {
            // Hour: wrap 0-23
            clockHour = (uint8_t)((clockHour + direction + 24) % 24);
          } else {
            // Minute: wrap 0-59
            clockMinute = (uint8_t)((clockMinute + direction + 60) % 60);
          }
        } else {
          // Navigate cursor 0↔1 (clamp)
          int8_t next = clockCursor + direction;
          if (next < 0) next = 0;
          if (next > 1) next = 1;
          clockCursor = next;
        }
        break;

      case MODE_MODE:
        if (modeConfirming) {
          modeRebootYes = !modeRebootYes;
        } else {
          // Toggle cursor between 0 (Simple) and 1 (Simulation)
          modePickerCursor = modePickerCursor ? 0 : 1;
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
    markDisplayDirty();

    // Suppress input during sleep confirm/cancel overlay
    if (sleepConfirmActive || sleepCancelActive) { lastEncBtn = encBtn; return; }

    // Wake from scheduled light sleep -- consume input
    if (scheduleSleeping) { exitLightSleep(); lastEncBtn = encBtn; return; }

    // Wake screensaver -- consume input
    if (screensaverActive) { screensaverActive = false; lastEncBtn = encBtn; return; }

    switch (currentMode) {
      case MODE_NORMAL:
        if (settings.operationMode == 1) {
          // Simulation mode: skip to next work mode
          skipWorkMode();
        } else {
          // Simple mode: cycle footer display mode
          uint8_t next = (uint8_t)footerMode;
          do {
            next = (next + 1) % FOOTER_MODE_COUNT;
          } while (next == (uint8_t)FOOTER_CLOCK && !timeSynced);
          footerMode = (FooterMode)next;
          profileDisplayStart = 0;  // clear profile overlay so change is visible immediately
        }
        break;

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
            pushSerialStatus();
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
          stopSoundPreview();
          Serial.println("Menu: edit done");
        } else if (menuCursor >= 0) {
          const MenuItem& item = MENU_ITEMS[menuCursor];
          if (item.type == MENU_VALUE && item.minVal != item.maxVal) {
            // Enter edit mode (skip read-only items where min == max)
            menuEditing = true;
            if (item.settingId == SET_SOUND_TYPE) startSoundPreview(settings.soundType);
            Serial.print("Menu: editing "); Serial.println(item.label);
          } else if (item.type == MENU_ACTION) {
            if (item.settingId == SET_OP_MODE) {
              currentMode = MODE_MODE;
              initModePicker();
              Serial.println("Mode: MODE");
            } else if (item.settingId == SET_SET_CLOCK) {
              currentMode = MODE_SET_CLOCK;
              initClockEditor();
              Serial.println("Mode: SET_CLOCK");
            } else if (item.settingId == SET_SCHEDULE_MODE) {
              currentMode = MODE_SCHEDULE;
              initScheduleEditor();
              Serial.println("Mode: SCHEDULE");
            } else if (item.settingId == SET_KEY_SLOTS) {
              currentMode = MODE_SLOTS;
              activeSlot = 0;
              Serial.println("Mode: SLOTS");
              pushSerialStatus();
            } else if (item.settingId == SET_BLE_IDENTITY) {
              currentMode = MODE_DECOY;
              initDecoyPicker();
              Serial.println("Mode: DECOY");
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

      case MODE_SET_CLOCK:
        // Toggle editing on/off
        clockEditing = !clockEditing;
        if (clockEditing) {
          Serial.print("Clock: editing row "); Serial.println(clockCursor);
        } else {
          Serial.println("Clock: edit done");
        }
        break;

      case MODE_SCHEDULE:
        if (scheduleEditing) {
          scheduleEditing = false;
          // If Mode just changed to Off, reset cursor to Mode row
          if (scheduleCursor > 0 && settings.scheduleMode == SCHED_OFF) {
            scheduleCursor = 0;
          }
          Serial.println("Schedule: edit done");
        } else {
          scheduleEditing = true;
          Serial.print("Schedule: editing row "); Serial.println(scheduleCursor);
        }
        break;

      case MODE_DECOY:
        if (decoyConfirming) {
          if (decoyRebootYes) {
            Serial.println("Rebooting for identity change...");
            NVIC_SystemReset();
          } else {
            returnToMenuFromDecoy();
          }
        } else {
          if (decoyCursor == DECOY_COUNT) {
            // "Custom" selected — enter name editor
            settings.decoyIndex = 0;
            currentMode = MODE_NAME;
            initNameEditor();
            markDisplayDirty();
            Serial.println("Mode: NAME (from DECOY)");
          } else {
            // Preset selected
            uint8_t newIndex = decoyCursor + 1;  // 1-based
            if (newIndex == decoyOriginal) {
              // Same as current — no change needed, return to menu
              returnToMenuFromDecoy();
            } else {
              // Apply preset name to deviceName for display consistency
              settings.decoyIndex = newIndex;
              strncpy(settings.deviceName, DECOY_NAMES[decoyCursor], NAME_MAX_LEN);
              settings.deviceName[NAME_MAX_LEN] = '\0';
              saveSettings();
              // Show reboot confirmation
              decoyConfirming = true;
              decoyRebootYes = true;
            }
          }
        }
        break;

      case MODE_MODE:
        if (modeConfirming) {
          if (modeRebootYes) {
            saveSettings();
            Serial.println("Rebooting for mode change...");
            NVIC_SystemReset();
          } else {
            settings.operationMode = modeOriginalValue;
            modeConfirming = false;
          }
        } else {
          if (modePickerCursor == modeOriginalValue) {
            // Same as current — no change, return to menu
            returnToMenuFromMode();
          } else {
            // Apply selection and show reboot confirmation
            settings.operationMode = modePickerCursor;
            modeConfirming = true;
            modeRebootYes = true;
          }
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
        markDisplayDirty();
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
        unsigned long confirmElapsed = now - sleepConfirmStart;
        sleepConfirmActive = false;
        markDisplayDirty();
        if (confirmElapsed >= SLEEP_LIGHT_THRESHOLD_MS) {
          // Held past midpoint — light sleep
          lightSleepPending = true;
          Serial.println("Sleep confirm: light sleep");
        } else {
          // Released before midpoint — cancel
          sleepCancelActive = true;
          sleepCancelStart = now;
          Serial.println("Sleep confirm: cancelled");
        }
        funcBtnWasPressed = false;
        return;  // prevent fall-through to short-press handler
      } else if (holdTime > 50) {
        // Short press -- mode switching
        lastModeActivity = now;
        markDisplayDirty();

        // Wake from scheduled light sleep -- consume input
        if (scheduleSleeping) { exitLightSleep(); funcBtnWasPressed = false; return; }

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
            pushSerialStatus();
            break;

          case MODE_MENU:
            if (defaultsConfirming) {
              defaultsConfirming = false;  // cancel confirmation
            } else if (rebootConfirming) {
              rebootConfirming = false;    // cancel confirmation
            } else {
              // Close menu -> save and return to NORMAL
              menuEditing = false;
              stopSoundPreview();
              currentMode = MODE_NORMAL;
              saveSettings();
              Serial.println("Mode: NORMAL (menu closed)");
              pushSerialStatus();
            }
            break;

          case MODE_SLOTS:
            // Back to menu at "Key slots" item
            saveSettings();
            currentMode = MODE_MENU;
            menuCursor = MENU_IDX_KEY_SLOTS;
            menuEditing = false;
            menuScrollOffset = (MENU_IDX_KEY_SLOTS > 4) ? MENU_IDX_KEY_SLOTS - 4 : 0;
            Serial.println("Mode: MENU (from SLOTS)");
            pushSerialStatus();
            break;

          case MODE_NAME:
            if (!nameConfirming) {
              settings.decoyIndex = 0;  // switching to custom (saved by saveNameEditor)
              bool changed = saveNameEditor();
              if (changed || decoyOriginal != 0) {
                // Name or identity changed — prompt reboot
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

          case MODE_SET_CLOCK:
            // Confirm: sync time and return to menu
            syncTime((uint32_t)clockHour * 3600 + (uint32_t)clockMinute * 60);
            clockEditing = false;
            returnToMenuFromClock();
            Serial.println("Clock set via manual editor");
            break;

          case MODE_SCHEDULE:
            // Save and return to menu
            saveSettings();
            scheduleEditing = false;
            returnToMenuFromSchedule();
            break;

          case MODE_DECOY:
            if (decoyConfirming) {
              decoyConfirming = false;  // cancel confirmation
              returnToMenuFromDecoy();
            } else {
              // Back to menu without changes
              returnToMenuFromDecoy();
            }
            break;

          case MODE_MODE:
            if (modeConfirming) {
              // Cancel reboot prompt — revert and return to menu
              settings.operationMode = modeOriginalValue;
              modeConfirming = false;
              returnToMenuFromMode();
            } else {
              // Back to menu without changes
              returnToMenuFromMode();
            }
            break;

          default: break;
        }
      }
      funcBtnWasPressed = false;
    }
  }

  // Mute button (D7) - cycles KB/MS enable combos in both modes
  static bool lastMuteBtn = HIGH;
  bool muteBtn = digitalRead(PIN_MUTE_BTN);
  if (muteBtn == LOW && lastMuteBtn == HIGH && (now - lastMuteBtnPress > DEBOUNCE)) {
    lastMuteBtnPress = now;
    lastModeActivity = now;
    markDisplayDirty();

    // Suppress during overlays
    if (sleepConfirmActive || sleepCancelActive) { lastMuteBtn = muteBtn; return; }
    if (scheduleSleeping) { exitLightSleep(); lastMuteBtn = muteBtn; return; }
    if (screensaverActive) { screensaverActive = false; lastMuteBtn = muteBtn; return; }

    // Cycle KB/MS enable combos (same logic as simple mode encoder button)
    bool wasKeyEnabled = keyEnabled;
    bool wasMouseEnabled = mouseEnabled;
    uint8_t mstate = (keyEnabled ? 2 : 0) | (mouseEnabled ? 1 : 0);
    mstate = (mstate == 0) ? 3 : mstate - 1;
    keyEnabled = (mstate & 2) != 0;
    mouseEnabled = (mstate & 1) != 0;
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
    Serial.print("Mute KB:"); Serial.print(keyEnabled ? "ON" : "OFF");
    Serial.print(" MS:"); Serial.println(mouseEnabled ? "ON" : "OFF");
    pushSerialStatus();
  }
  lastMuteBtn = muteBtn;
}

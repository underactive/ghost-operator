#include "display.h"
#include "state.h"
#include "keys.h"
#include "icons.h"
#include "timing.h"
#include "settings.h"

// ============================================================================
// STATIC HELPERS (file-local)
// ============================================================================

// Get short 3-char display name for a key slot
static const char* slotName(uint8_t slotIdx) {
  static const char* SHORT_NAMES[] = {
    "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20",
    "F21", "F22", "F23", "F24", "SLk", "Pau", "NLk", "LSh",
    "LCt", "LAl", "RSh", "RCt", "RAl", "Esc", "Spc", "Ent",
    " ^ ", " v ", " < ", " > ", "---"
  };
  if (slotIdx >= NUM_KEYS) return "---";
  return SHORT_NAMES[slotIdx];
}

// ============================================================================
// ANIMATIONS (footer corner, 20x10px region: x=108..127, y=54..63)
// ============================================================================

// ECG heartbeat trace (original animation)
static void drawAnimECG() {
  static uint8_t beatPhase = 0;
  beatPhase = (beatPhase + 1) % 20;

  static const int8_t ecg[] = {
    0, 0, 0, 0, 0,
    -1, -2, -1, 0,
    1, -5, 4, -1,
    0, -1, -2, -1,
    0, 0, 0
  };
  const int ECG_LEN = 20;
  int baseY = 58;
  int startX = 108;

  for (int i = 0; i < ECG_LEN - 1; i++) {
    int a = (beatPhase + i) % ECG_LEN;
    int b = (beatPhase + i + 1) % ECG_LEN;
    display.drawLine(startX + i, baseY + ecg[a],
                     startX + i + 1, baseY + ecg[b], SSD1306_WHITE);
  }
}

// Ghost sprite with gentle bob
static void drawAnimGhost() {
  static const uint8_t ghostLeft[] PROGMEM = {
    0b00111100,
    0b01111110,
    0b11111111,
    0b10110111,  // pupils left: .X.. .X..
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b10110101,
    0b01001010,
  };
  static const uint8_t ghostRight[] PROGMEM = {
    0b00111100,
    0b01111110,
    0b11111111,
    0b11101101,  // pupils right: ...X ...X
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b10110101,
    0b01001010,
  };
  static uint8_t phase = 0;
  phase = (phase + 1) % 40;

  // Vertical bob: 0 → -1 → 0 → +1 over 40 steps
  int bob = 0;
  if (phase >= 5 && phase < 15) bob = -1;
  else if (phase >= 25 && phase < 35) bob = 1;

  // Horizontal drift: sway across 108..120 (12px range for 8px-wide sprite)
  // Phase 0..19 drifts right, 20..39 drifts left
  int drift;
  if (phase < 20) drift = (int)phase * 12 / 20;
  else drift = 12 - (int)(phase - 20) * 12 / 20;

  const uint8_t* sprite = (phase < 20) ? ghostRight : ghostLeft;
  display.drawBitmap(108 + drift, 54 + bob, sprite, 8, 10, SSD1306_WHITE);
}

// Radar sweep with trailing lines
static void drawAnimRadar() {
  static uint16_t angle = 0;
  angle = (angle + 3) % 360;

  int cx = 118, cy = 58, r = 4;
  // Circle outline
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawPixel(cx, cy, SSD1306_WHITE);

  // Sweep line + 2 trailing lines
  for (int t = 0; t < 3; t++) {
    int a = ((int)angle - t * 15 + 360) % 360;
    float rad = (float)a * PI / 180.0f;
    int lineR = (t == 0) ? r : r - 1;
    int ex = cx + (int)(lineR * cos(rad));
    int ey = cy + (int)(lineR * sin(rad));
    display.drawLine(cx, cy, ex, ey, SSD1306_WHITE);
  }
}

// Equalizer bars
static void drawAnimEQ() {
  static uint8_t heights[5] = {3, 5, 2, 6, 4};
  static uint8_t targets[5] = {3, 5, 2, 6, 4};
  static uint8_t frameCount = 0;
  frameCount++;

  // Pick new random targets every 3 frames (~300ms)
  if (frameCount % 3 == 0) {
    for (int i = 0; i < 5; i++) {
      targets[i] = 1 + (random(8));  // 1-8
    }
  }

  // Animate heights toward targets
  for (int i = 0; i < 5; i++) {
    if (heights[i] < targets[i]) heights[i]++;
    else if (heights[i] > targets[i]) heights[i]--;
  }

  // Draw 5 bars: 3px wide, 1px gap, starting at x=109
  int baseY = 63;
  for (int i = 0; i < 5; i++) {
    int x = 109 + i * 4;
    display.fillRect(x, baseY - heights[i], 3, heights[i], SSD1306_WHITE);
  }
}

// Matrix rain effect
static void drawAnimMatrix() {
  static uint8_t dropY[7];
  static uint8_t dropLen[7];
  static bool initialized = false;
  static uint8_t frameCount = 0;

  if (!initialized) {
    for (int i = 0; i < 7; i++) {
      dropY[i] = random(13);         // stagger start positions
      dropLen[i] = 2 + random(3);    // trail length 2-4
    }
    initialized = true;
  }

  frameCount++;
  if (frameCount % 2 == 0) {
    for (int i = 0; i < 7; i++) {
      dropY[i]++;
      if (dropY[i] > (uint8_t)(10 + dropLen[i])) {
        dropY[i] = 0;
        dropLen[i] = 2 + random(3);
      }
    }
  }

  // Draw drops: head pixel + trailing pixels
  for (int i = 0; i < 7; i++) {
    int x = 109 + i * 3;  // 7 columns, 3px apart
    for (int t = 0; t < (int)dropLen[i]; t++) {
      int y = (int)dropY[i] - t;
      if (y >= 0 && y < 10) {
        display.drawPixel(x, 54 + y, SSD1306_WHITE);
        if (t == 0) display.drawPixel(x + 1, 54 + y, SSD1306_WHITE);  // wider head
      }
    }
  }
}

// Animation dispatcher
static void drawAnimation() {
  switch (settings.animStyle) {
    case 0: drawAnimECG();     break;
    case 1: drawAnimEQ();      break;
    case 2: drawAnimGhost();   break;
    case 3: drawAnimMatrix();  break;
    case 4: drawAnimRadar();   break;
    case 5: break;  // None
  }
}

// ============================================================================
// NORMAL MODE
// ============================================================================

static void drawNormalMode() {
  unsigned long now = millis();

  // === Header (y=0) ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("GHOST Operator");

  // Right side: BT icon + battery, right aligned
  String batStr = String(batteryPercent) + "%";
  int batWidth = batStr.length() * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;  // icon width + gap
  if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    bool btVisible = (now / 500) % 2 == 0;
    if (btVisible) {
      display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
    }
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // === Key section (y=12) ===
  display.setCursor(0, 12);
  display.print("KB [");
  display.print(AVAILABLE_KEYS[nextKeyIndex].name);
  display.print("] ");
  display.print(formatDuration(effectiveKeyMin(), false));
  display.print("-");
  display.print(formatDuration(effectiveKeyMax()));

  // ON/OFF icon right aligned
  display.drawBitmap(123, 12, keyEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Key progress bar (y=21)
  display.drawRect(0, 21, 100, 7, SSD1306_WHITE);
  if (!deviceConnected) {
    // BLE disconnected -- show empty bar and "---"
    display.setCursor(102, 21);
    display.print("---");
  } else {
    unsigned long keyElapsed = now - lastKeyTime;
    int keyRemaining = max(0L, (long)currentKeyInterval - (long)keyElapsed);
    int keyProgress = 0;
    if (currentKeyInterval > 0) {
      keyProgress = map(keyRemaining, 0, currentKeyInterval, 0, 100);
    }
    int keyBarWidth = map(keyProgress, 0, 100, 0, 98);
    if (keyBarWidth > 0) {
      display.fillRect(1, 22, keyBarWidth, 5, SSD1306_WHITE);
    }
    String keyTimeStr = formatDuration(keyRemaining);
    display.setCursor(102, 21);
    display.print(keyTimeStr);
  }

  display.drawFastHLine(0, 29, 128, SSD1306_WHITE);

  // === Mouse section (y=32) ===
  display.setCursor(0, 32);
  display.print("MS ");

  if (mouseState == MOUSE_IDLE) {
    display.print("[IDL]");
  } else if (mouseState == MOUSE_RETURNING) {
    display.print("[RTN]");
  } else {
    display.print("[MOV]");
  }

  display.print(" ");
  display.print(formatDuration(effectiveMouseJiggle()));
  display.print("/");
  display.print(formatDuration(effectiveMouseIdle()));

  // ON/OFF icon right aligned
  display.drawBitmap(123, 32, mouseEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Mouse progress bar (y=41)
  display.drawRect(0, 41, 100, 7, SSD1306_WHITE);
  if (!deviceConnected) {
    // BLE disconnected -- show empty bar and "---"
    display.setCursor(102, 41);
    display.print("---");
  } else if (mouseState == MOUSE_RETURNING) {
    // Knight Rider sweep: highlight segment bounces back and forth
    const int barInner = 98;   // usable width inside border (1..99)
    const int segW = 16;       // sweep segment width
    unsigned long phase = (now / 8) % ((barInner - segW) * 2);  // ping-pong period
    int segX;
    if (phase < (unsigned long)(barInner - segW)) {
      segX = (int)phase;
    } else {
      segX = (barInner - segW) * 2 - (int)phase;  // bounce back
    }
    display.fillRect(1 + segX, 42, segW, 5, SSD1306_WHITE);
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    int mouseRemaining = max(0L, (long)mouseDuration - (long)mouseElapsed);
    int mouseProgress = 0;
    if (mouseDuration > 0) {
      if (mouseState == MOUSE_IDLE) {
        mouseProgress = map(mouseElapsed, 0, mouseDuration, 0, 100);
        if (mouseProgress > 100) mouseProgress = 100;
      } else {
        mouseProgress = map(mouseRemaining, 0, mouseDuration, 0, 100);
      }
    }
    int mouseBarWidth = map(mouseProgress, 0, 100, 0, 98);
    if (mouseBarWidth > 0) {
      display.fillRect(1, 42, mouseBarWidth, 5, SSD1306_WHITE);
    }
    String mouseTimeStr = formatDuration(mouseRemaining);
    display.setCursor(102, 41);
    display.print(mouseTimeStr);
  }

  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  // === Footer (y=54) ===
  if (millis() < profileDisplayUntil) {
    // Show profile name left-justified
    display.setCursor(0, 54);
    display.print(PROFILE_NAMES[currentProfile]);
  } else {
    // Normal uptime display
    display.setCursor(0, 54);
    display.print("Up: ");
    display.print(formatUptime(now - startTime));
  }

  // Status animation (lower-right corner)
  if (deviceConnected) {
    drawAnimation();
  }
}

// ============================================================================
// SLEEP OVERLAYS
// ============================================================================

static void drawSleepConfirm() {
  unsigned long elapsed = millis() - sleepConfirmStart;
  unsigned long remaining = (elapsed >= SLEEP_COUNTDOWN_MS) ? 0 : SLEEP_COUNTDOWN_MS - elapsed;

  display.setTextSize(1);

  // "Hold to sleep..." centered at y=18
  const char* holdMsg = "Hold to sleep...";
  int holdWidth = strlen(holdMsg) * 6;
  display.setCursor((128 - holdWidth) / 2, 18);
  display.print(holdMsg);

  // Progress bar (same style as KB bar): border + fill, drains from full to empty
  display.drawRect(0, 28, 100, 7, SSD1306_WHITE);
  int barWidth = map(remaining, 0, SLEEP_COUNTDOWN_MS, 0, 98);
  if (barWidth > 0) {
    display.fillRect(1, 29, barWidth, 5, SSD1306_WHITE);
  }

  // Time remaining label at (102, 28)
  display.setCursor(102, 28);
  display.print(formatDuration(remaining));

  // "Release to cancel" centered at y=40
  const char* cancelMsg = "Release to cancel";
  int cancelWidth = strlen(cancelMsg) * 6;
  display.setCursor((128 - cancelWidth) / 2, 40);
  display.print(cancelMsg);
}

static void drawSleepCancelled() {
  display.setTextSize(1);
  const char* msg = "Cancelled";
  int msgWidth = strlen(msg) * 6;
  display.setCursor((128 - msgWidth) / 2, 28);
  display.print(msg);
}

// ============================================================================
// SCREENSAVER
// ============================================================================

static void drawScreensaver() {
  // Minimal display to reduce OLED burn-in
  // Vertically centered, horizontally centered labels, 1px progress bars
  unsigned long now = millis();
  display.setTextSize(1);

  // === KB label centered (y=11) ===
  String kbLabel = "[" + String(AVAILABLE_KEYS[nextKeyIndex].name) + "]";
  int kbWidth = kbLabel.length() * 6;
  display.setCursor((128 - kbWidth) / 2, 11);
  display.print(kbLabel);

  // === KB progress bar 1px high (y=21), full width ===
  if (deviceConnected) {
    unsigned long keyElapsed = now - lastKeyTime;
    int keyRemaining = max(0L, (long)currentKeyInterval - (long)keyElapsed);
    int kbBarWidth = 0;
    if (currentKeyInterval > 0) {
      kbBarWidth = map(keyRemaining, 0, currentKeyInterval, 0, 128);
    }
    if (kbBarWidth > 0) {
      display.drawFastHLine(0, 21, kbBarWidth, SSD1306_WHITE);
    }
  }
  // When disconnected: no bar drawn (empty = visual hint)

  // === MS label centered (y=32) ===
  const char* msTag = (mouseState == MOUSE_IDLE) ? "[IDL]" :
                      (mouseState == MOUSE_RETURNING) ? "[RTN]" : "[MOV]";
  int msWidth = strlen(msTag) * 6;
  display.setCursor((128 - msWidth) / 2, 32);
  display.print(msTag);

  // === MS progress bar 1px high (y=42), full width ===
  if (!deviceConnected) {
    // BLE disconnected -- no bar drawn
  } else if (mouseState == MOUSE_RETURNING) {
    // Knight Rider sweep: 1px highlight segment bounces across full width
    const int barW = 128;
    const int segW = 20;
    unsigned long phase = (now / 8) % ((barW - segW) * 2);
    int segX;
    if (phase < (unsigned long)(barW - segW)) {
      segX = (int)phase;
    } else {
      segX = (barW - segW) * 2 - (int)phase;
    }
    display.drawFastHLine(segX, 42, segW, SSD1306_WHITE);
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    int msBarWidth = 0;
    if (mouseDuration > 0) {
      if (mouseState == MOUSE_IDLE) {
        msBarWidth = map(min(mouseElapsed, mouseDuration), 0, mouseDuration, 0, 128);
      } else {
        int mouseRemaining = max(0L, (long)mouseDuration - (long)mouseElapsed);
        msBarWidth = map(mouseRemaining, 0, mouseDuration, 0, 128);
      }
    }
    if (msBarWidth > 0) {
      display.drawFastHLine(0, 42, msBarWidth, SSD1306_WHITE);
    }
  }

  // === Battery warning (y=48) -- only if <15% ===
  if (batteryPercent < 15) {
    String batStr = String(batteryPercent) + "%";
    int batWidth = batStr.length() * 6;
    display.setCursor((128 - batWidth) / 2, 48);
    display.print(batStr);
  }
}

// ============================================================================
// SLOTS MODE
// ============================================================================

static void drawSlotsMode() {
  // === Header ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MODE: SLOTS");

  // Slot indicator right aligned: [3/8]
  char slotIndicator[8];
  sprintf(slotIndicator, "[%d/%d]", activeSlot + 1, NUM_SLOTS);
  int indWidth = strlen(slotIndicator) * 6;
  display.setCursor(128 - indWidth, 0);
  display.print(slotIndicator);

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // === 2 rows x 4 slots (y=20, y=30) -- centered vertically ===
  for (int row = 0; row < 2; row++) {
    int y = 20 + row * 10;

    for (int col = 0; col < 4; col++) {
      int slot = row * 4 + col;
      int x = 14 + col * 26;

      if (slot == activeSlot) {
        // Inverted highlight: white rect, black text
        display.fillRect(x, y, 24, 9, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      // Center 3-char name in 24px cell
      display.setCursor(x + 3, y + 1);
      display.print(slotName(settings.keySlots[slot]));
    }
  }
  display.setTextColor(SSD1306_WHITE);

  display.drawFastHLine(0, 42, 128, SSD1306_WHITE);

  // === Instructions (y=48) ===
  display.setCursor(0, 48);
  display.print("Turn=key  Press=slot");

  display.setCursor(0, 57);
  display.print("Func=back");
}

// ============================================================================
// NAME MODE
// ============================================================================

static void drawNameMode() {
  display.setTextSize(1);

  if (nameConfirming) {
    // === Reboot confirmation prompt ===
    display.setCursor(0, 0);
    display.print("NAME SAVED");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Show new name in quotes, centered
    char nameBuf[NAME_MAX_LEN + 3];
    sprintf(nameBuf, "\"%s\"", settings.deviceName);
    int nameW = strlen(nameBuf) * 6;
    display.setCursor((128 - nameW) / 2, 18);
    display.print(nameBuf);

    // "Reboot to apply?"
    const char* prompt = "Reboot to apply?";
    int promptW = strlen(prompt) * 6;
    display.setCursor((128 - promptW) / 2, 30);
    display.print(prompt);

    // Yes / No options
    int optY = 42;
    int yesX = 30;
    int noX = 80;

    if (nameRebootYes) {
      // Highlight "Yes"
      display.fillRect(yesX - 2, optY - 1, 30, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(noX, optY);
      display.print("No");
    } else {
      // Highlight "No"
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.fillRect(noX - 2, optY - 1, 24, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(noX, optY);
      display.print("No");
      display.setTextColor(SSD1306_WHITE);
    }

    display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("Turn=select Press=OK");

  } else {
    // === Character editor ===
    display.setCursor(0, 0);
    display.print("DEVICE NAME");

    // Position indicator right aligned: [3/14]
    char posInd[8];
    sprintf(posInd, "[%d/%d]", activeNamePos + 1, NAME_MAX_LEN);
    int indWidth = strlen(posInd) * 6;
    display.setCursor(128 - indWidth, 0);
    display.print(posInd);

    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // === 2 rows x 7 characters (y=16, y=28) ===
    // Each cell: 16px wide, centered in 128px (7 x 16 = 112, offset 8px)
    const int cellW = 16;
    const int cellH = 10;
    const int cols = 7;
    const int offsetX = 8;

    for (int row = 0; row < 2; row++) {
      int y = 16 + row * 12;

      for (int col = 0; col < cols; col++) {
        int pos = row * cols + col;
        if (pos >= NAME_MAX_LEN) break;
        int x = offsetX + col * cellW;

        bool isActive = (pos == activeNamePos);
        if (isActive) {
          display.fillRect(x, y, cellW, cellH, SSD1306_WHITE);
          display.setTextColor(SSD1306_BLACK);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }

        // Center character in cell
        if (nameCharIndex[pos] >= NAME_CHAR_COUNT) {
          // Draw a 2x2 dot centered in cell (ASCII font lacks middle dot)
          display.fillRect(x + 7, y + 4, 2, 2, isActive ? SSD1306_BLACK : SSD1306_WHITE);
        } else {
          display.setCursor(x + 5, y + 1);
          display.print(NAME_CHARS[nameCharIndex[pos]]);
        }
      }
    }
    display.setTextColor(SSD1306_WHITE);

    display.drawFastHLine(0, 42, 128, SSD1306_WHITE);

    // === Instructions (y=48) ===
    display.setCursor(0, 48);
    display.print("Turn=char Press=next");

    display.setCursor(0, 57);
    display.print("Func=save");
  }
}

// ============================================================================
// HELP BAR
// ============================================================================

static void drawHelpBar(int y) {
  // Determine help text based on menu state
  const char* text;
  static char nameHelpBuf[48];  // buffer for dynamic device name help text
  if (menuEditing) {
    text = "Turn to adjust, Press to confirm";
  } else if (menuCursor == -1) {
    text = "Turn/Press dial to select/OK";
  } else if (menuCursor >= 0 && MENU_ITEMS[menuCursor].settingId == SET_DEVICE_NAME && MENU_ITEMS[menuCursor].type == MENU_ACTION) {
    // Dynamic help: show current saved name
    snprintf(nameHelpBuf, sizeof(nameHelpBuf), "Current: %s", settings.deviceName);
    text = nameHelpBuf;
  } else if (menuCursor >= 0 && MENU_ITEMS[menuCursor].helpText) {
    text = MENU_ITEMS[menuCursor].helpText;
  } else {
    text = "Press to select";
  }

  int textLen = strlen(text);
  int maxChars = 21;  // 128px / 6px per char

  if (textLen <= maxChars) {
    // Static: fits on screen
    display.setCursor(0, y);
    display.print(text);
  } else {
    // Scroll by character with 1.5s pause at ends, ~300ms per step
    unsigned long now = millis();
    int maxScroll = textLen - maxChars;

    if (now - helpScrollTimer >= (unsigned long)(helpScrollPos == 0 || helpScrollPos == maxScroll ? 1500 : 300)) {
      helpScrollPos += helpScrollDir;
      if (helpScrollPos >= maxScroll) { helpScrollPos = maxScroll; helpScrollDir = -1; }
      if (helpScrollPos <= 0) { helpScrollPos = 0; helpScrollDir = 1; }
      helpScrollTimer = now;
    }

    display.setCursor(0, y);
    // Print maxChars characters starting from helpScrollPos
    for (int i = 0; i < maxChars && (helpScrollPos + i) < textLen; i++) {
      display.print(text[helpScrollPos + i]);
    }
  }
}

// ============================================================================
// MENU MODE
// ============================================================================

static void drawMenuMode() {
  display.setTextSize(1);

  if (defaultsConfirming) {
    // === Reset defaults confirmation prompt ===
    display.setCursor(0, 0);
    display.print("RESET DEFAULTS");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Two centered text lines
    const char* line1 = "Restore all settings";
    const char* line2 = "to factory defaults?";
    int w1 = strlen(line1) * 6;
    int w2 = strlen(line2) * 6;
    display.setCursor((128 - w1) / 2, 18);
    display.print(line1);
    display.setCursor((128 - w2) / 2, 28);
    display.print(line2);

    // Yes / No options (No highlighted by default)
    int optY = 40;
    int yesX = 30;
    int noX = 80;

    if (defaultsConfirmYes) {
      // Highlight "Yes"
      display.fillRect(yesX - 2, optY - 1, 30, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(noX, optY);
      display.print("No");
    } else {
      // Highlight "No"
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.fillRect(noX - 2, optY - 1, 24, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(noX, optY);
      display.print("No");
      display.setTextColor(SSD1306_WHITE);
    }

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("Turn=select Press=OK");
    return;
  }

  if (rebootConfirming) {
    // === Reboot confirmation prompt ===
    display.setCursor(0, 0);
    display.print("REBOOT");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    const char* line1 = "Reboot device now?";
    int w1 = strlen(line1) * 6;
    display.setCursor((128 - w1) / 2, 22);
    display.print(line1);

    // Yes / No options (No highlighted by default)
    int optY = 40;
    int yesX = 30;
    int noX = 80;

    if (rebootConfirmYes) {
      display.fillRect(yesX - 2, optY - 1, 30, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(noX, optY);
      display.print("No");
    } else {
      display.setCursor(yesX, optY);
      display.print("Yes");
      display.fillRect(noX - 2, optY - 1, 24, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(noX, optY);
      display.print("No");
      display.setTextColor(SSD1306_WHITE);
    }

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("Turn=select Press=OK");
    return;
  }

  // === Header (y=0): "MENU" title, optionally inverted ===
  if (menuCursor == -1) {
    // Highlight title row (inverted)
    display.fillRect(0, 0, 36, 8, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(0, 0);
    display.print("MENU");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.setCursor(0, 0);
    display.print("MENU");
  }

  // BT icon + battery right-aligned in header
  String batStr = String(batteryPercent) + "%";
  int batWidth = batStr.length() * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;
  if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    if ((millis() / 500) % 2 == 0)
      display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  // === Separator ===
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Menu viewport: 5 rows x 8px each (y=10 to y=49) ===
  for (int row = 0; row < 5; row++) {
    int idx = menuScrollOffset + row;
    if (idx < 0 || idx >= MENU_ITEM_COUNT) continue;

    int y = 10 + row * 8;
    const MenuItem& item = MENU_ITEMS[idx];
    bool isSelected = (idx == menuCursor);

    if (item.type == MENU_HEADING) {
      // Heading: centered "- Label -" style
      String heading = "- " + String(item.label) + " -";
      int hw = heading.length() * 6;
      display.setCursor((128 - hw) / 2, y);
      display.print(heading);

    } else if (item.type == MENU_ACTION) {
      // Action item: label left, ">" right
      if (isSelected) {
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      }
      display.setCursor(2, y);
      display.print(item.label);
      display.setCursor(122, y);
      display.print(">");
      if (isSelected) display.setTextColor(SSD1306_WHITE);

    } else {
      // Value item
      String valStr = formatMenuValue(item.settingId, item.format);
      uint32_t curVal = getSettingValue(item.settingId);
      bool atMin = (curVal <= item.minVal);
      bool atMax = (curVal >= item.maxVal);
      // Negative-display: displayed range is inverted (raw max = displayed min)
      if (item.format == FMT_PERCENT_NEG) { bool tmp = atMin; atMin = atMax; atMax = tmp; }

      if (isSelected && menuEditing) {
        // Editing: label normal, value portion inverted with < >
        display.setCursor(2, y);
        display.print(item.label);

        // Build value display string with arrows
        String editStr = "";
        if (!atMin) editStr += "< ";
        else editStr += "  ";
        editStr += valStr;
        if (!atMax) editStr += " >";
        else editStr += "  ";

        int editW = editStr.length() * 6;
        int editX = 128 - editW - 1;
        display.fillRect(editX, y, editW + 1, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(editX, y);
        display.print(editStr);
        display.setTextColor(SSD1306_WHITE);

      } else if (isSelected) {
        // Selected but not editing: full row inverted
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(item.label);

        // Value with arrows right-aligned
        String dispStr = "";
        if (!atMin) dispStr += "< ";
        else dispStr += "  ";
        dispStr += valStr;
        if (!atMax) dispStr += " >";
        else dispStr += "  ";

        int dw = dispStr.length() * 6;
        display.setCursor(128 - dw - 1, y);
        display.print(dispStr);
        display.setTextColor(SSD1306_WHITE);

      } else {
        // Unselected: label left, value right
        display.setCursor(2, y);
        display.print(item.label);

        String dispStr = "";
        if (!atMin) dispStr += "< ";
        else dispStr += "  ";
        dispStr += valStr;
        if (!atMax) dispStr += " >";
        else dispStr += "  ";

        int dw = dispStr.length() * 6;
        display.setCursor(128 - dw - 1, y);
        display.print(dispStr);
      }
    }
  }

  // === Separator ===
  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  // === Help bar (y=52) ===
  drawHelpBar(52);
}

// ============================================================================
// PUBLIC: updateDisplay()
// ============================================================================

void updateDisplay() {
  if (!displayInitialized) return;

  // Contrast management: screensaver dims, normal uses displayBrightness
  static bool wasSaver = false;
  static uint8_t lastBrightness = 0;
  uint8_t normalContrast = (uint8_t)(0xCF * settings.displayBrightness / 100);

  if (screensaverActive != wasSaver) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(screensaverActive ? (uint8_t)(0xCF * settings.saverBrightness / 100) : normalContrast);
    wasSaver = screensaverActive;
    lastBrightness = settings.displayBrightness;
  } else if (!screensaverActive && settings.displayBrightness != lastBrightness) {
    // Live-update contrast when editing brightness in menu
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(normalContrast);
    lastBrightness = settings.displayBrightness;
  }

  display.clearDisplay();

  if (sleepCancelActive) {
    drawSleepCancelled();
  } else if (sleepConfirmActive) {
    drawSleepConfirm();
  } else if (screensaverActive) {
    drawScreensaver();
  } else if (currentMode == MODE_NORMAL) {
    drawNormalMode();
  } else if (currentMode == MODE_MENU) {
    drawMenuMode();
  } else if (currentMode == MODE_SLOTS) {
    drawSlotsMode();
  } else if (currentMode == MODE_NAME) {
    drawNameMode();
  }

  display.display();
}

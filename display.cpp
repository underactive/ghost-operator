#include "display.h"
#include "state.h"
#include "keys.h"
#include "icons.h"
#include "timing.h"
#include "settings.h"
#include "schedule.h"
#include "input.h"
#include "sim_data.h"
#include "orchestrator.h"
#include "breakout.h"

// ============================================================================
// DIRTY FLAG
// ============================================================================

void markDisplayDirty() { displayDirty = true; }

// ============================================================================
// SHADOW BUFFER — selective page redraw
// ============================================================================

static uint8_t shadowBuf[1024];
static bool shadowValid = false;

// Invalidate the shadow buffer so the next sendDirtyPages() does a full send.
// Call this after any direct display.display() that bypasses the shadow path
// (e.g. one-shot screens in schedule.cpp before light sleep).
void invalidateDisplayShadow() { shadowValid = false; }

// I2C data chunk: replicate Adafruit SSD1306 WIRE_MAX logic, minus 1 for 0x40
// prefix byte.  On Seeed nRF52 (SERIAL_BUFFER_SIZE=64) this yields 62 data
// bytes per transaction — matching Adafruit's own display() throughput.
#if defined(I2C_BUFFER_LENGTH)
#define I2C_DATA_CHUNK (min(256, I2C_BUFFER_LENGTH) - 1)
#elif defined(BUFFER_LENGTH)
#define I2C_DATA_CHUNK (min(256, BUFFER_LENGTH) - 1)
#elif defined(SERIAL_BUFFER_SIZE)
#define I2C_DATA_CHUNK (min(255, SERIAL_BUFFER_SIZE - 1) - 1)
#else
#define I2C_DATA_CHUNK 31
#endif

// Send a contiguous range of SSD1306 pages via I2C (0-indexed, inclusive)
static void sendPages(uint8_t startPage, uint8_t endPage) {
  display.ssd1306_command(SSD1306_PAGEADDR);
  display.ssd1306_command(startPage);
  display.ssd1306_command(endPage);
  display.ssd1306_command(SSD1306_COLUMNADDR);
  display.ssd1306_command(0);
  display.ssd1306_command(SCREEN_WIDTH - 1);

  uint8_t *buf = display.getBuffer() + startPage * SCREEN_WIDTH;
  uint16_t len = (uint16_t)(endPage - startPage + 1) * SCREEN_WIDTH;

  for (uint16_t i = 0; i < len; ) {
    Wire.beginTransmission(SCREEN_ADDRESS);
    Wire.write((uint8_t)0x40);  // Co=0, D/C=1 (data mode)
    uint16_t chunk = min((uint16_t)I2C_DATA_CHUNK, (uint16_t)(len - i));
    Wire.write(buf + i, chunk);
    i += chunk;
    Wire.endTransmission();
  }
}

// Compare framebuffer against shadow, send only dirty pages
static void sendDirtyPages() {
  uint8_t *buf = display.getBuffer();

  if (!shadowValid) {
    // First frame after boot or invalidation: full send
    display.display();
    memcpy(shadowBuf, buf, sizeof(shadowBuf));
    shadowValid = true;
    return;
  }

  // Find contiguous dirty page range
  int8_t firstDirty = -1, lastDirty = -1;
  for (uint8_t p = 0; p < 8; p++) {
    if (memcmp(buf + p * SCREEN_WIDTH, shadowBuf + p * SCREEN_WIDTH, SCREEN_WIDTH) != 0) {
      if (firstDirty < 0) firstDirty = p;
      lastDirty = p;
    }
  }

  if (firstDirty < 0) return;  // No pages changed — skip I2C entirely

  sendPages(firstDirty, lastDirty);
  memcpy(shadowBuf + firstDirty * SCREEN_WIDTH,
         buf + firstDirty * SCREEN_WIDTH,
         (lastDirty - firstDirty + 1) * SCREEN_WIDTH);
}

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

// Draw a PROGMEM 1-bit bitmap at integer scale (each src pixel → scale×scale block)
static void drawBitmapScaled(int16_t x, int16_t y, const uint8_t* bitmap,
                             int16_t srcW, int16_t srcH, uint8_t scale, uint16_t color) {
  int16_t byteWidth = (srcW + 7) / 8;
  for (int16_t row = 0; row < srcH; row++) {
    for (int16_t col = 0; col < srcW; col++) {
      uint8_t b = pgm_read_byte(&bitmap[row * byteWidth + col / 8]);
      if (b & (0x80 >> (col % 8))) {
        display.fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

// ============================================================================
// ANIMATIONS (footer corner, 20x10px region: x=108..127, y=54..63)
// ============================================================================

static bool animShouldAdvance = true;
static uint8_t ghostAnimPhase = 0;  // file-scope for easter egg sync

// ECG heartbeat trace (original animation)
static void drawAnimECG() {
  static uint8_t beatPhase = 0;
  if (animShouldAdvance) beatPhase = (beatPhase + 1) % 20;

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
  if (animShouldAdvance) ghostAnimPhase = (ghostAnimPhase + 1) % 40;

  // Vertical bob: 0 → -1 → 0 → +1 over 40 steps
  int bob = 0;
  if (ghostAnimPhase >= 5 && ghostAnimPhase < 15) bob = -1;
  else if (ghostAnimPhase >= 25 && ghostAnimPhase < 35) bob = 1;

  // Horizontal drift: sway across 108..120 (12px range for 8px-wide sprite)
  // Phase 0..19 drifts right, 20..39 drifts left
  int drift;
  if (ghostAnimPhase < 20) drift = (int)ghostAnimPhase * 12 / 20;
  else drift = 12 - (int)(ghostAnimPhase - 20) * 12 / 20;

  const uint8_t* sprite = (ghostAnimPhase < 20) ? ghostRight : ghostLeft;
  display.drawBitmap(108 + drift, 54 + bob, sprite, 8, 10, SSD1306_WHITE);
}

// Radar sweep with trailing lines
static void drawAnimRadar() {
  static uint16_t angle = 0;
  if (animShouldAdvance) angle = (angle + 3) % 360;

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
  if (animShouldAdvance) frameCount++;

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

  if (animShouldAdvance) frameCount++;
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
  uint8_t activeCount = (keyEnabled ? 1 : 0) + (mouseEnabled ? 1 : 0);

  // Both muted: freeze on current frame; half speed when one muted
  static uint8_t animFrameCounter = 0;
  if (activeCount == 0) {
    animShouldAdvance = false;
  } else {
    animFrameCounter++;
    // Divisors doubled vs original 10 Hz to compensate for 20 Hz display rate
    animShouldAdvance = (activeCount == 2) ? (animFrameCounter % 2 == 0) : (animFrameCounter % 4 == 0);
  }

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
// EASTER EGG: Pac-Man chases ghost across footer
// ============================================================================

static void drawEasterEgg() {
  // Sprites (all 8x10 PROGMEM)
  static const uint8_t pacOpenR[] PROGMEM = {
    0x3C, 0x7E, 0xFF, 0xFE, 0xF8, 0xF8, 0xFE, 0xFF, 0x7E, 0x3C
  };
  static const uint8_t pacOpenL[] PROGMEM = {
    0x3C, 0x7E, 0xFF, 0x7F, 0x1F, 0x1F, 0x7F, 0xFF, 0x7E, 0x3C
  };
  static const uint8_t pacClosed[] PROGMEM = {
    0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C
  };
  static const uint8_t ghostR[] PROGMEM = {
    0x3C, 0x7E, 0xFF, 0xED, 0xFF, 0xFF, 0xFF, 0xFF, 0xB5, 0x4A
  };
  static const uint8_t ghostFright[] PROGMEM = {
    0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0xFF, 0xA5, 0x5A, 0xB5, 0x4A
  };
  static const uint8_t eyesOnly[] PROGMEM = {
    0x00, 0x00, 0x00, 0x77, 0x33, 0x77, 0x00, 0x00, 0x00, 0x00
  };

  // Dot positions (y=58 for small, y=56 for energizer)
  static const int8_t dotX[] = { 10, 22, 34, 46, 78, 90, 102, 114 };
  static const uint8_t NUM_DOTS = 8;
  static const int8_t energizerX = 62;

  uint8_t f = easterEggFrame - 1;  // 0-indexed (frame 0 was sync wait)

  // --- Compute positions and state ---
  int pacX = -20, ghostX = -20;
  const uint8_t* pacSprite = pacClosed;
  const uint8_t* ghostSprite = ghostR;
  bool showPac = false, showGhost = false, showEyes = false;
  bool energizerEaten = false;
  int eyeX = -20;

  if (f <= 2) {
    // Phase 1: Dots only (f 0-2)
  } else if (f <= 21) {
    // Phase 2: Chase right
    pacX = -10 + (int)(f - 3) * 4;
    showPac = true;
    pacSprite = ((f / 2) % 2 == 0) ? pacOpenR : pacClosed;
    if (f >= 7) {
      ghostX = -10 + (int)(f - 7) * 4;
      showGhost = true;
    }
  } else if (f <= 24) {
    // Phase 3: Power-up
    pacX = 66;
    showPac = true;
    pacSprite = pacClosed;  // mouth closed = "eating"
    energizerEaten = true;
    ghostX = 50;
    showGhost = true;
    ghostSprite = (f == 22) ? ghostR : ghostFright;
  } else if (f <= 32) {
    // Phase 4: Hunt left
    energizerEaten = true;
    ghostX = 50 - (int)(f - 25) * 4;
    showGhost = true;
    ghostSprite = ghostFright;
    pacX = 66 - (int)(f - 25) * 6;
    showPac = true;
    pacSprite = ((f / 2) % 2 == 0) ? pacOpenL : pacClosed;
  } else {
    // Phase 5: Eat ghost + eyes exit + eat remaining dots (f 33-51)
    energizerEaten = true;
    if (f <= 34) {
      // Pac paused, eyes appear
      pacX = 24;
      showPac = true;
      pacSprite = pacClosed;
      eyeX = 22;
      showEyes = true;
    } else {
      // Eyes drift left at 5px/frame (unchanged)
      eyeX = 22 - (int)(f - 35) * 5;
      showEyes = (eyeX >= -8);
      // Pac-Man turns right, eats remaining dots at 7px/frame
      pacX = 24 + (int)(f - 35) * 7;
      showPac = (pacX < 136);
      pacSprite = ((f / 2) % 2 == 0) ? pacOpenR : pacClosed;
    }
  }

  // --- Render: dots → energizer → ghost/eyes → Pac-Man ---

  // Small dots (2x2) — disappear as Pac-Man passes over them
  for (uint8_t i = 0; i < NUM_DOTS; i++) {
    bool eaten;
    if (dotX[i] < energizerX) {
      // Left-side dots: eaten during Phase 2 rightward chase
      // All consumed by f=21; remain eaten for rest of animation
      eaten = (f > 21) || (showPac && pacX + 4 >= dotX[i]);
    } else {
      // Right-side dots: eaten during Phase 5 rightward pass
      eaten = (showPac && f >= 35 && pacX + 4 >= dotX[i]);
    }
    if (!eaten) {
      display.fillRect(dotX[i], 58, 2, 2, SSD1306_WHITE);
    }
  }

  // Energizer (4x4)
  if (!energizerEaten) {
    display.fillRect(energizerX, 56, 4, 4, SSD1306_WHITE);
  }

  // Ghost or eyes
  if (showGhost) {
    display.drawBitmap(ghostX, 54, ghostSprite, 8, 10, SSD1306_WHITE);
  }
  if (showEyes) {
    display.drawBitmap(eyeX, 54, eyesOnly, 8, 10, SSD1306_WHITE);
  }

  // Pac-Man (drawn last = visually on top)
  if (showPac && pacX >= -8 && pacX < 128) {
    display.drawBitmap(pacX, 54, pacSprite, 8, 10, SSD1306_WHITE);
  }

  // Advance frame (halved for 20 Hz display rate — visual speed matches original 10 Hz)
  static uint8_t easterEggDiv = 0;
  easterEggDiv++;
  if (easterEggDiv % 2 == 0) {
    easterEggFrame++;
    if (easterEggFrame >= EASTER_EGG_TOTAL_FRAMES) {
      easterEggActive = false;
    }
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
  if (strcmp(settings.deviceName, DEVICE_NAME) == 0) {
    display.print("GHOST Operator");
  } else {
    display.print(settings.deviceName);
  }

  // Right side: BT icon + battery, right aligned
  char batStr[16];
  snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
  int batWidth = strlen(batStr) * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;  // icon width + gap
  if (usbConnected) {
    display.drawBitmap(btX, 0, usbIcon, 5, 8, SSD1306_WHITE);
  } else if (deviceConnected) {
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
  char durBuf[12];
  formatDuration(effectiveKeyMin(), durBuf, sizeof(durBuf), false);
  display.print(durBuf);
  display.print("-");
  formatDuration(effectiveKeyMax(), durBuf, sizeof(durBuf));
  display.print(durBuf);

  // ON/OFF icon right aligned
  display.drawBitmap(123, 12, keyEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Key progress bar (y=21)
  display.drawRect(0, 21, 100, 7, SSD1306_WHITE);
  if (!deviceConnected && !usbConnected) {
    // BLE disconnected -- show empty bar and "---"
    display.setCursor(102, 21);
    display.print("---");
  } else {
    unsigned long keyElapsed = now - lastKeyTime;
    if (keyElapsed > currentKeyInterval) keyElapsed = currentKeyInterval;
    int keyRemaining = (int)(currentKeyInterval - keyElapsed);
    int keyProgress = 0;
    if (currentKeyInterval > 0) {
      keyProgress = map(keyRemaining, 0, currentKeyInterval, 0, 100);
    }
    int keyBarWidth = map(keyProgress, 0, 100, 0, 98);
    if (keyBarWidth > 0) {
      display.fillRect(1, 22, keyBarWidth, 5, SSD1306_WHITE);
    }
    display.setCursor(102, 21);
    if (keyEnabled) {
      formatDuration(keyRemaining, durBuf, sizeof(durBuf));
      display.print(durBuf);
    } else {
      display.print("mute");
    }
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
  formatDuration(effectiveMouseJiggle(), durBuf, sizeof(durBuf));
  display.print(durBuf);
  display.print("/");
  formatDuration(effectiveMouseIdle(), durBuf, sizeof(durBuf));
  display.print(durBuf);

  // ON/OFF icon right aligned
  display.drawBitmap(123, 32, mouseEnabled ? iconOn : iconOff, 5, 7, SSD1306_WHITE);

  // Mouse progress bar (y=41)
  display.drawRect(0, 41, 100, 7, SSD1306_WHITE);
  if (!deviceConnected && !usbConnected) {
    // Disconnected -- show empty bar and "---"
    display.setCursor(102, 41);
    display.print("---");
  } else if (mouseState == MOUSE_RETURNING) {
    // Empty bar during return (0%)
    display.setCursor(102, 41);
    if (mouseEnabled) {
      display.print("0.0s");
    } else {
      display.print("mute");
    }
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    if (mouseElapsed > mouseDuration) mouseElapsed = mouseDuration;
    int mouseRemaining = (int)(mouseDuration - mouseElapsed);
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
    display.setCursor(102, 41);
    if (mouseEnabled) {
      formatDuration(mouseRemaining, durBuf, sizeof(durBuf));
      display.print(durBuf);
    } else {
      display.print("mute");
    }
  }

  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  // === Footer (y=54) ===
  if (easterEggActive && easterEggFrame > 0) {
    drawEasterEgg();
  } else {
    if (millis() - profileDisplayStart < PROFILE_DISPLAY_MS) {
      // Show profile name left-justified
      display.setCursor(0, 54);
      display.print(PROFILE_NAMES[currentProfile]);
    } else {
      display.setCursor(0, 54);
      switch (footerMode) {
        case FOOTER_CLOCK: {
          if (timeSynced) {
            char timeBuf[12];
            formatCurrentTime(timeBuf, sizeof(timeBuf));
            display.print(timeBuf);
          } else {
            char uptimeBuf[20];
            formatUptime(now - startTime, uptimeBuf, sizeof(uptimeBuf));
            display.print(uptimeBuf);
          }
          break;
        }
        case FOOTER_UPTIME: {
          char uptimeBuf[20];
          formatUptime(now - startTime, uptimeBuf, sizeof(uptimeBuf));
          display.print(uptimeBuf);
          break;
        }
        case FOOTER_VERSION:
          display.print("v");
          display.print(VERSION);
          break;
        case FOOTER_DIETEMP: {
          int c = getDieTempCelsius();
          if (cachedDieTempRaw == INT16_MIN) {
            display.print("---");
          } else {
            int f = c * 9 / 5 + 32;
            char tempBuf[16];
            snprintf(tempBuf, sizeof(tempBuf), "%dC/%dF", c, f);
            display.print(tempBuf);
          }
          break;
        }
        default: break;
      }
    }

    // Status animation (lower-right corner)
    if (deviceConnected || usbConnected) {
      drawAnimation();
    }

    // Easter egg sync: wait for corner ghost to reach right edge before starting
    if (easterEggActive && easterEggFrame == 0) {
      static uint8_t syncWaitFrames = 0;
      bool ghostAtEdge = (ghostAnimPhase >= 18 && ghostAnimPhase <= 20);
      bool bypass = (settings.animStyle != 2) ||
                    (!keyEnabled && !mouseEnabled) ||
                    (syncWaitFrames >= 40);
      if (ghostAtEdge || bypass) {
        easterEggFrame = 1;
        syncWaitFrames = 0;
      } else {
        syncWaitFrames++;
      }
    }
  }
}

// ============================================================================
// SCROLLING NAME HELPER (simulation info rows)
// ============================================================================

static void drawScrollName(const char* name, int x, int y, int maxChars, int rowIdx) {
  int len = strlen(name);
  if (len <= maxChars) {
    display.setCursor(x, y);
    display.print(name);
    return;
  }
  unsigned long now = millis();
  int maxScroll = len - maxChars;
  if (orch.scrollDir[rowIdx] == 0) orch.scrollDir[rowIdx] = 1;

  unsigned long pause = (orch.scrollPos[rowIdx] == 0 || orch.scrollPos[rowIdx] == maxScroll) ? 1500 : 300;
  if (now - orch.scrollTimer[rowIdx] >= pause) {
    orch.scrollPos[rowIdx] += orch.scrollDir[rowIdx];
    if (orch.scrollPos[rowIdx] >= maxScroll) { orch.scrollPos[rowIdx] = maxScroll; orch.scrollDir[rowIdx] = -1; }
    if (orch.scrollPos[rowIdx] <= 0) { orch.scrollPos[rowIdx] = 0; orch.scrollDir[rowIdx] = 1; }
    orch.scrollTimer[rowIdx] = now;
  }

  display.setCursor(x, y);
  for (int i = 0; i < maxChars && (orch.scrollPos[rowIdx] + i) < len; i++) {
    display.print(name[orch.scrollPos[rowIdx] + i]);
  }
}

// ============================================================================
// SIMULATION MODE NORMAL SCREEN
// ============================================================================

static void drawSimulationNormal() {
  unsigned long now = millis();
  display.setTextSize(1);

  // === Header (y=0): Job name or device name + BT/USB + battery ===
  display.setCursor(0, 0);
  if (settings.headerDisplay == 0) {
    display.print(JOB_SIM_NAMES[settings.jobSimulation]);
  } else {
    display.print(settings.deviceName);
  }

  // Right side: BT/USB icon + battery
  char batStr[16];
  snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
  int batWidth = strlen(batStr) * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;
  if (usbConnected) {
    display.drawBitmap(btX, 0, usbIcon, 5, 8, SSD1306_WHITE);
  } else if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    if ((now / 500) % 2 == 0) display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);  // matches simple mode

  // === Block row (y=11): name left | inline bar + time right ===
  {
    drawScrollName(currentBlockName(), 0, 11, 10, 0);

    unsigned long blockElapsed = now - orch.blockStartMs;
    unsigned long blockRemain = (blockElapsed < orch.blockDurationMs) ? (orch.blockDurationMs - blockElapsed) : 0;
    char timeBuf[10];
    formatMinSec(blockRemain, timeBuf, sizeof(timeBuf));
    int timeW = strlen(timeBuf) * 6;
    int timeX = 128 - timeW;
    int barW = timeX - 2 - 64;

    if (barW > 4) {
      uint8_t progress = blockProgress(now);
      display.drawRect(64, 12, barW, 5, SSD1306_WHITE);
      int fill = map(100 - progress, 0, 100, 0, barW - 2);
      if (fill > 0) display.fillRect(65, 13, fill, 3, SSD1306_WHITE);
    }

    display.setCursor(timeX, 11);
    display.print(timeBuf);
  }

  // === Mode row (y=20): name left | inline bar + time right ===
  {
    drawScrollName(currentModeName(), 0, 20, 10, 1);

    unsigned long modeElapsed = now - orch.modeStartMs;
    unsigned long modeRemain = (modeElapsed < orch.modeDurationMs) ? (orch.modeDurationMs - modeElapsed) : 0;
    char timeBuf[10];
    formatMinSec(modeRemain, timeBuf, sizeof(timeBuf));
    int timeW = strlen(timeBuf) * 6;
    int timeX = 128 - timeW;
    int barW = timeX - 2 - 64;

    if (barW > 4) {
      uint8_t progress = modeProgress(now);
      display.drawRect(64, 21, barW, 5, SSD1306_WHITE);
      int fill = map(100 - progress, 0, 100, 0, barW - 2);
      if (fill > 0) display.fillRect(65, 22, fill, 3, SSD1306_WHITE);
    }

    display.setCursor(timeX, 20);
    display.print(timeBuf);
  }

  // === Profile stint row (y=29): name left | inline bar + time right ===
  {
    drawScrollName(PROFILE_NAMES_TITLE[orch.autoProfile], 0, 29, 10, 2);

    unsigned long stintElapsed = now - orch.profileStintStartMs;
    unsigned long stintRemain = (stintElapsed < orch.profileStintMs) ? (orch.profileStintMs - stintElapsed) : 0;
    char timeBuf[10];
    formatMinSec(stintRemain, timeBuf, sizeof(timeBuf));
    int timeW = strlen(timeBuf) * 6;
    int timeX = 128 - timeW;
    int barW = timeX - 2 - 64;

    if (barW > 4) {
      uint8_t progress = 0;
      if (orch.profileStintMs > 0) {
        progress = (uint8_t)map(stintRemain, 0, orch.profileStintMs, 0, 100);
      }
      display.drawRect(64, 30, barW, 5, SSD1306_WHITE);
      int fill = map(progress, 0, 100, 0, barW - 2);
      if (fill > 0) display.fillRect(65, 31, fill, 3, SSD1306_WHITE);
    }

    display.setCursor(timeX, 29);
    display.print(timeBuf);
  }

  display.drawFastHLine(0, 37, 128, SSD1306_WHITE);

  // === Activity row (y=38..49): label + outlined bar + time ===
  {
    // Activity label (3 chars) — state feedback now handled by footer icons
    const char* actLabel;

    switch (orch.phase) {
      case PHASE_TYPING:    actLabel = "KBD"; break;
      case PHASE_MOUSING:   actLabel = "MSE"; break;
      case PHASE_IDLE:      actLabel = "IDL"; break;
      case PHASE_KB_MOUSE:  actLabel = "K+M"; break;
      case PHASE_MOUSE_KB:  actLabel = "M+K"; break;
      default:              actLabel = "SWT"; break;
    }

    display.setCursor(0, 40);
    display.print(actLabel);

    // Progress bar (7px tall outlined, x=21..96)
    display.drawRect(21, 40, 76, 7, SSD1306_WHITE);

    unsigned long phaseElapsed = now - orch.phaseStartMs;
    unsigned long phaseRemain = (phaseElapsed < orch.phaseDurationMs) ? (orch.phaseDurationMs - phaseElapsed) : 0;

    if (orch.phaseDurationMs > 0) {
      int fill = map(phaseRemain, 0, orch.phaseDurationMs, 0, 74);
      if (fill > 0) display.fillRect(22, 41, fill, 5, SSD1306_WHITE);
    }

    // Time remaining
    char durBuf[12];
    formatDuration(phaseRemain, durBuf, sizeof(durBuf));
    display.setCursor(99, 40);
    display.print(durBuf);
  }

  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);  // matches simple mode

  // === Footer (y=54): uptime/realtime + KB/MS indicators + animation ===
  {
    display.setCursor(0, 54);
    if (timeSynced) {
      char timeBuf[12];
      formatCurrentTime(timeBuf, sizeof(timeBuf));
      display.print(timeBuf);
    } else {
      char uptimeBuf[20];
      formatUptime(now - startTime, uptimeBuf, sizeof(uptimeBuf));
      display.print(uptimeBuf);
    }

    // Keycap icon: shrinks when key held (depressed), hidden when muted
    if (keyEnabled) {
      if (orch.keyDown) {
        display.drawBitmap(60, 55, iconKeycapPressed, 9, 9, SSD1306_WHITE);
      } else {
        display.drawBitmap(60, 54, iconKeycapNormal, 10, 10, SSD1306_WHITE);
      }
    }

    // Mouse icon: click > scroll > moving(nudge) > idle; hidden when muted
    if (mouseEnabled) {
      const uint8_t* mIcon;
      int mx = 78;

      if (now - orch.lastPhantomClickMs < 200) {
        mIcon = iconMouseClick;
      } else if (settings.scrollEnabled && (now - lastScrollTime < 200)) {
        mIcon = iconMouseScroll;
      } else {
        mIcon = iconMouseNormal;
        bool mouseMoving = (orch.phase == PHASE_MOUSING && mouseState == MOUSE_JIGGLING) ||
                           (orch.phase == PHASE_KB_MOUSE && orch.kbmsSubPhase == KBMS_MOUSE_SWIPE) ||
                           (orch.phase == PHASE_MOUSE_KB && orch.mskbSubPhase == MSKB_MOUSE_DRAW);
        if (mouseMoving) {
          static const int8_t nudge[] = {0, 1, 0, -1};
          mx += nudge[(millis() / 150) % 4];
        }
      }

      display.drawBitmap(mx, 54, mIcon, 10, 10, SSD1306_WHITE);
    }

    // Animation in corner
    if (deviceConnected || usbConnected) {
      drawAnimation();
    }
  }

  // Performance level overlay (encoder changes job performance)
  if (orch.previewActive) {
    if (now - orch.previewStartMs < SIM_SCHEDULE_PREVIEW_MS) {
      display.fillRect(8, 16, 112, 32, SSD1306_BLACK);
      display.drawRect(8, 16, 112, 32, SSD1306_WHITE);

      // Title
      display.setCursor(12, 19);
      display.print("Job Performance");

      // Bar background (88px wide, inside the overlay box)
      int16_t barX = 12, barY = 31, barW = 88, barH = 8;
      display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);

      // Filled portion: level/11 of the bar width
      int16_t fillW = (int16_t)settings.jobPerformance * (barW - 2) / 11;
      if (fillW > 0) {
        display.fillRect(barX + 1, barY + 1, fillW, barH - 2, SSD1306_WHITE);
      }

      // Numeric value right of bar
      display.setCursor(barX + barW + 4, 31);
      display.print(settings.jobPerformance);
    } else {
      orch.previewActive = false;
    }
  }
}

// ============================================================================
// SIMULATION MODE SCREENSAVER
// ============================================================================

static void drawSimulationScreensaver() {
  unsigned long now = millis();
  const uint8_t scale = 3;        // 10px icons → 30px
  const uint8_t iconSz = 10 * scale;  // 30
  const uint8_t gap = 12;
  const int16_t iconY = 17;       // vertically centered in 64px

  bool showKb = keyEnabled;
  bool showMs = mouseEnabled;
  int numIcons = (showKb ? 1 : 0) + (showMs ? 1 : 0);
  if (numIcons == 0) return;      // both muted — blank screensaver

  // Compute X positions for centering
  int16_t totalW = numIcons * iconSz + (numIcons > 1 ? gap : 0);
  int16_t startX = (128 - totalW) / 2;
  int16_t kbX = startX;
  int16_t msX = showKb ? (startX + iconSz + gap) : startX;

  // Keycap: depressed when key is held, with visual hold so short presses
  // are visible at 5 Hz refresh (orch.keyDownMs is set in the main loop,
  // so it catches presses that start and end between frames)
  if (showKb) {
    bool showDepressed = orch.keyDown || (now - orch.keyDownMs < 200);
    if (showDepressed) {
      // Pressed: 9x9 bitmap → 27x27, offset +1x,+3y within 30x30 footprint
      drawBitmapScaled(kbX + scale, iconY + scale * 3, iconKeycapPressed,
                       9, 9, scale, SSD1306_WHITE);
    } else {
      drawBitmapScaled(kbX, iconY, iconKeycapNormal,
                       10, 10, scale, SSD1306_WHITE);
    }
  }

  // Mouse: click > scroll > nudge > idle
  if (showMs) {
    const uint8_t* mIcon;
    int16_t mx = msX;

    if (now - orch.lastPhantomClickMs < 200) {
      mIcon = iconMouseClick;
    } else if (settings.scrollEnabled && (now - lastScrollTime < 200)) {
      mIcon = iconMouseScroll;
    } else {
      mIcon = iconMouseNormal;
      // Nudge: ±3px, slowed to ~2Hz for screensaver
      {
        bool mouseMoving = (orch.phase == PHASE_MOUSING && mouseState == MOUSE_JIGGLING) ||
                           (orch.phase == PHASE_KB_MOUSE && orch.kbmsSubPhase == KBMS_MOUSE_SWIPE) ||
                           (orch.phase == PHASE_MOUSE_KB && orch.mskbSubPhase == MSKB_MOUSE_DRAW);
        if (mouseMoving) {
          static const int8_t nudge[] = {0, 1, 0, -1};
          mx += nudge[(now / 500) % 4] * (int8_t)scale;
        }
      }
    }

    drawBitmapScaled(mx, iconY, mIcon, 10, 10, scale, SSD1306_WHITE);
  }

  // Battery warning below icons — only if <15%
  if (batteryPercent < 15) {
    display.setTextSize(1);
    char batStr[16];
    snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
    int bw = strlen(batStr) * 6;
    display.setCursor((128 - bw) / 2, 56);
    display.print(batStr);
  }
}

// ============================================================================
// SLEEP OVERLAYS
// ============================================================================

static void drawSleepConfirm() {
  unsigned long elapsed = millis() - sleepConfirmStart;
  unsigned long remaining = (elapsed >= SLEEP_COUNTDOWN_MS) ? 0 : SLEEP_COUNTDOWN_MS - elapsed;

  display.setTextSize(1);

  // "Hold to sleep..." centered at y=10
  const char* holdMsg = "Hold to sleep...";
  int holdWidth = strlen(holdMsg) * 6;
  display.setCursor((128 - holdWidth) / 2, 10);
  display.print(holdMsg);

  // Segment labels at y=20
  // "Light" (5 chars = 30px) centered over segment 1 (0-47)
  display.setCursor((48 - 30) / 2, 20);
  display.print("Light");
  // "Deep" (4 chars = 24px) centered over segment 2 (50-97)
  display.setCursor(50 + (48 - 24) / 2, 20);
  display.print("Deep");

  // Segmented progress bar at y=28: two 48px segments with 2px gap
  // Segment 1: x=0, w=48 (fill area 1-46)
  display.drawRect(0, 28, 48, 7, SSD1306_WHITE);
  int fill1 = (elapsed >= SLEEP_LIGHT_THRESHOLD_MS) ? 46
              : map(elapsed, 0, SLEEP_LIGHT_THRESHOLD_MS, 0, 46);
  if (fill1 > 0) {
    display.fillRect(1, 29, fill1, 5, SSD1306_WHITE);
  }

  // Segment 2: x=50, w=48 (fill area 51-96)
  display.drawRect(50, 28, 48, 7, SSD1306_WHITE);
  int fill2 = (elapsed <= SLEEP_LIGHT_THRESHOLD_MS) ? 0
              : map(elapsed, SLEEP_LIGHT_THRESHOLD_MS, SLEEP_COUNTDOWN_MS, 0, 46);
  if (fill2 > 0) {
    display.fillRect(51, 29, fill2, 5, SSD1306_WHITE);
  }

  // Time remaining at x=102
  char durBuf[12];
  formatDuration(remaining, durBuf, sizeof(durBuf));
  display.setCursor(102, 28);
  display.print(durBuf);

  // Dynamic message at y=40
  const char* msg;
  if (elapsed >= SLEEP_LIGHT_THRESHOLD_MS) {
    msg = "Release = light sleep";
  } else {
    msg = "Release to cancel";
  }
  int msgWidth = strlen(msg) * 6;
  display.setCursor((128 - msgWidth) / 2, 40);
  display.print(msg);
}

static void drawSleepCancelled() {
  display.setTextSize(1);
  const char* msg = "Cancelled";
  int msgWidth = strlen(msg) * 6;
  display.setCursor((128 - msgWidth) / 2, 28);
  display.print(msg);
}

// ============================================================================
// LIGHT SLEEP BREATHING ANIMATION
// ============================================================================

void drawLightSleepBreathing() {
  // Breathing circle in lower-right corner, evokes MacBook sleep LED
  // 4-second cycle: smoothly expands and contracts
  float phase = (float)(millis() % 4000) / 4000.0f * 6.2831853f;  // 2*PI
  float breathe = (sinf(phase) + 1.0f) * 0.5f;  // 0.0 → 1.0
  int r = 2 + (int)(4.0f * breathe);             // radius 2 → 6

  display.clearDisplay();
  display.fillCircle(118, 54, r, SSD1306_WHITE);
  sendDirtyPages();
}

// ============================================================================
// SCREENSAVER
// ============================================================================

static void drawScreensaver() {
  // Minimal display to reduce OLED burn-in
  // Vertically centered, horizontally centered labels, 1px progress bars
  unsigned long now = millis();
  display.setTextSize(1);

  // Bar geometry: 65% width, centered, with 3px tall end caps
  const int barW = 83;
  const int barX = (128 - barW) / 2;  // 22
  const int barEndX = barX + barW - 1; // 104
  const int innerW = barW - 2;         // 81 (fill area between caps)

  // === KB label centered (y=11) ===
  char kbLabel[24];
  snprintf(kbLabel, sizeof(kbLabel), "[%s]", AVAILABLE_KEYS[nextKeyIndex].name);
  int kbWidth = strlen(kbLabel) * 6;
  display.setCursor((128 - kbWidth) / 2, 11);
  display.print(kbLabel);

  // === KB progress bar 1px high (y=21), 65% width with end caps ===
  display.drawFastVLine(barX, 20, 3, SSD1306_WHITE);
  display.drawFastVLine(barEndX, 20, 3, SSD1306_WHITE);
  if (deviceConnected || usbConnected) {
    unsigned long keyElapsed = now - lastKeyTime;
    if (keyElapsed > currentKeyInterval) keyElapsed = currentKeyInterval;
    int keyRemaining = (int)(currentKeyInterval - keyElapsed);
    int kbFill = 0;
    if (currentKeyInterval > 0) {
      kbFill = map(keyRemaining, 0, currentKeyInterval, 0, innerW);
    }
    if (kbFill > 0) {
      display.drawFastHLine(barX + 1, 21, kbFill, SSD1306_WHITE);
    }
  }

  // === MS label centered (y=32) ===
  const char* msTag = (mouseState == MOUSE_IDLE) ? "[IDL]" :
                      (mouseState == MOUSE_RETURNING) ? "[RTN]" : "[MOV]";
  int msWidth = strlen(msTag) * 6;
  display.setCursor((128 - msWidth) / 2, 32);
  display.print(msTag);

  // === MS progress bar 1px high (y=42), 65% width with end caps ===
  display.drawFastVLine(barX, 41, 3, SSD1306_WHITE);
  display.drawFastVLine(barEndX, 41, 3, SSD1306_WHITE);
  if (!deviceConnected && !usbConnected) {
    // Disconnected -- no fill drawn
  } else if (mouseState == MOUSE_RETURNING) {
    // Empty bar during return (0%)
  } else {
    unsigned long mouseElapsed = now - lastMouseStateChange;
    unsigned long mouseDuration = (mouseState == MOUSE_IDLE) ? currentMouseIdle : currentMouseJiggle;
    int msFill = 0;
    if (mouseDuration > 0) {
      if (mouseState == MOUSE_IDLE) {
        msFill = map(min(mouseElapsed, mouseDuration), 0, mouseDuration, 0, innerW);
      } else {
        if (mouseElapsed > mouseDuration) mouseElapsed = mouseDuration;
        int mouseRemaining = (int)(mouseDuration - mouseElapsed);
        msFill = map(mouseRemaining, 0, mouseDuration, 0, innerW);
      }
    }
    if (msFill > 0) {
      display.drawFastHLine(barX + 1, 42, msFill, SSD1306_WHITE);
    }
  }

  // === Battery warning (y=48) -- only if <15% ===
  if (batteryPercent < 15) {
    char batStr[16];
    snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
    int batWidth = strlen(batStr) * 6;
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
  snprintf(slotIndicator, sizeof(slotIndicator), "[%d/%d]", activeSlot + 1, NUM_SLOTS);
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
    snprintf(nameBuf, sizeof(nameBuf), "\"%s\"", settings.deviceName);
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
    char posInd[10];
    snprintf(posInd, sizeof(posInd), "[%d/%d]", activeNamePos + 1, NAME_MAX_LEN);
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
// DECOY MODE
// ============================================================================

static void drawDecoyMode() {
  display.setTextSize(1);

  if (decoyConfirming) {
    // === Reboot confirmation prompt ===
    display.setCursor(0, 0);
    display.print("IDENTITY SET");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Show selected preset name in quotes, centered
    const char* selectedName = (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT)
                               ? DECOY_NAMES[settings.decoyIndex - 1] : settings.deviceName;
    char nameBuf[20];
    snprintf(nameBuf, sizeof(nameBuf), "\"%s\"", selectedName);
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

    if (decoyRebootYes) {
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

    display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("Turn=select Press=OK");

  } else {
    // === Preset list picker ===
    display.setCursor(0, 0);
    display.print("BLE IDENTITY");
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // 5-row scrollable viewport (y=10..49, 8px rows)
    for (int row = 0; row < 5; row++) {
      int idx = decoyScrollOffset + row;
      if (idx < 0 || idx > DECOY_COUNT) continue;  // 0..DECOY_COUNT

      int y = 10 + row * 8;
      bool isSelected = (idx == decoyCursor);
      bool isActive;

      // Determine if this is the currently active item
      if (idx == DECOY_COUNT) {
        // "Custom" row — active when decoyIndex == 0
        isActive = (settings.decoyIndex == 0);
      } else {
        // Preset row — active when decoyIndex == idx+1
        isActive = (settings.decoyIndex == idx + 1);
      }

      if (isSelected) {
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      // Active marker
      display.setCursor(2, y);
      if (isActive) {
        display.print("*");
      } else {
        display.print(" ");
      }

      // Item label
      display.setCursor(8, y);
      if (idx == DECOY_COUNT) {
        display.print("Custom");
      } else {
        display.print(DECOY_NAMES[idx]);
      }

      if (isSelected) display.setTextColor(SSD1306_WHITE);
    }

    // Footer
    display.drawFastHLine(0, 50, 128, SSD1306_WHITE);
    display.setCursor(0, 52);
    display.print("Func=back");
  }
}

// ============================================================================
// SCHEDULE MODE
// ============================================================================

static void drawScheduleMode() {
  display.setTextSize(1);

  // === Header (y=0) ===
  display.setCursor(0, 0);
  display.print("SCHEDULE");

  // Separator
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Not synced: show only sync message, nothing interactive ===
  if (!timeSynced) {
    const char* lines[] = { "Sync clock via USB", "dashboard to enable", "scheduling." };
    for (int i = 0; i < 3; i++) {
      int w = strlen(lines[i]) * 6;
      display.setCursor((128 - w) / 2, 20 + i * 10);
      display.print(lines[i]);
    }
    return;
  }

  // === Synced: show settings rows ===
  const char* rowLabels[] = { "Mode", "Start time", "End time" };
  const int rowY[] = { 10, 18, 26 };

  // Mode row
  {
    char valStr[20];
    formatMenuValue(SET_SCHEDULE_MODE, FMT_SCHEDULE_MODE, valStr, sizeof(valStr));
    bool atMin = (settings.scheduleMode == 0);
    bool atMax = (settings.scheduleMode >= SCHED_MODE_COUNT - 1);
    bool isSelected = (scheduleCursor == 0);

    char arrowStr[32];
    snprintf(arrowStr, sizeof(arrowStr), "%s%s%s",
             !atMin ? "< " : "  ", valStr, !atMax ? " >" : "  ");
    int arrowW = strlen(arrowStr) * 6;

    if (isSelected && scheduleEditing) {
      display.setCursor(2, rowY[0]);
      display.print(rowLabels[0]);

      int editX = 128 - arrowW - 1;
      display.fillRect(editX, rowY[0], arrowW + 1, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(editX, rowY[0]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else if (isSelected) {
      display.fillRect(0, rowY[0], 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, rowY[0]);
      display.print(rowLabels[0]);

      display.setCursor(128 - arrowW - 1, rowY[0]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(2, rowY[0]);
      display.print(rowLabels[0]);
      int vw = strlen(valStr) * 6;
      display.setCursor(128 - vw - 1, rowY[0]);
      display.print(valStr);
    }
  }

  // Start/End rows
  for (int row = 1; row <= 2; row++) {
    uint8_t settingId = (row == 1) ? SET_SCHEDULE_START : SET_SCHEDULE_END;
    bool isOff = (settings.scheduleMode == SCHED_OFF);
    bool isHidden = isOff || (row == 1 && settings.scheduleMode == SCHED_AUTO_SLEEP);
    char valStr[20];
    if (isHidden) {
      snprintf(valStr, sizeof(valStr), "---");
    } else {
      formatMenuValue(settingId, FMT_TIME_5MIN, valStr, sizeof(valStr));
    }
    uint16_t rawVal = (row == 1) ? settings.scheduleStart : settings.scheduleEnd;
    bool atMin = isHidden || (rawVal == 0);
    bool atMax = isHidden || (rawVal >= SCHEDULE_SLOTS - 1);
    bool isSelected = (scheduleCursor == row);

    char arrowStr[32];
    snprintf(arrowStr, sizeof(arrowStr), "%s%s%s",
             !atMin ? "< " : "  ", valStr, !atMax ? " >" : "  ");
    int arrowW = strlen(arrowStr) * 6;

    if (isSelected && scheduleEditing) {
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);

      int editX = 128 - arrowW - 1;
      display.fillRect(editX, rowY[row], arrowW + 1, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(editX, rowY[row]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else if (isSelected) {
      display.fillRect(0, rowY[row], 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);

      display.setCursor(128 - arrowW - 1, rowY[row]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);
      int vw = strlen(valStr) * 6;
      display.setCursor(128 - vw - 1, rowY[row]);
      display.print(valStr);
    }
  }

  // === Separator ===
  display.drawFastHLine(0, 42, 128, SSD1306_WHITE);

  // === Help text (2 lines at y=44, y=52) ===
  const char* help1;
  const char* help2;
  if (scheduleCursor == 0) {
    switch (settings.scheduleMode) {
      case SCHED_OFF:
        help1 = "Schedule disabled.";
        help2 = "Manual Power On/Off.";
        break;
      case SCHED_AUTO_SLEEP:
        help1 = "Power off at end.";
        help2 = "Button wakes device.";
        break;
      case SCHED_FULL_AUTO:
      default:
        help1 = "Light sleep at end.";
        help2 = "Auto-wakes at start.";
        break;
    }
  } else if (scheduleCursor == 1) {
    help1 = "Schedule start time.";
    help2 = "5-min increments.";
  } else {
    help1 = "Schedule end time.";
    help2 = "5-min increments.";
  }
  display.setCursor(0, 44);
  display.print(help1);
  display.setCursor(0, 52);
  display.print(help2);
}

// ============================================================================
// GENERIC CAROUSEL (MODE_CAROUSEL)
// ============================================================================

static void drawCarouselPage() {
  if (!carouselConfig) return;
  display.setTextSize(1);

  // === Header ===
  display.setCursor(0, 0);
  display.print(carouselConfig->title);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Horizontal carousel strip (same animation style as MODE_MODE) ===
  {
    static float crslScrollX = 0.0f;
    static const CarouselConfig* crslLastConfig = NULL;
    static bool crslSnap = true;

    // Reset scroll position when config changes
    if (carouselConfig != crslLastConfig) {
      crslSnap = true;
      crslLastConfig = carouselConfig;
    }

    // Compute cell layout
    int cellCenterX[16];  // max 16 options
    int cellWidth[16];
    int count = carouselConfig->count;
    if (count > 16) count = 16;
    int runX = 0;
    for (int i = 0; i < count; i++) {
      const char* nm = carouselConfig->names[i];
      cellWidth[i] = (int)strlen(nm) * 6 + 16;
      cellCenterX[i] = runX + cellWidth[i] / 2;
      runX += cellWidth[i];
    }

    // Animate scroll toward selected item (snap on first frame)
    float target = (float)cellCenterX[carouselCursor] - 64.0f;
    if (crslSnap) {
      crslScrollX = target;
      crslSnap = false;
    } else {
      float delta = target - crslScrollX;
      if (delta > -0.5f && delta < 0.5f) {
        crslScrollX = target;
      } else {
        crslScrollX += delta * 0.25f;
        markDisplayDirty();  // keep animating until converged
      }
    }
    int scrollI = (int)crslScrollX;

    // Render strip at y=24 (centered in content area)
    const int stripY = 24;
    const int stripH = 11;
    display.setTextWrap(false);
    for (int i = 0; i < count; i++) {
      const char* nm = carouselConfig->names[i];
      int tw = (int)strlen(nm) * 6;
      int tx = cellCenterX[i] - scrollI - tw / 2;

      if (tx >= 128 || tx + tw <= 0) continue;

      if (i == carouselCursor) {
        int rx = cellCenterX[i] - scrollI - cellWidth[i] / 2;
        display.fillRect(rx, stripY - 1, cellWidth[i], stripH, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(tx, stripY);
        display.print(nm);
        display.setTextColor(SSD1306_WHITE);
      } else {
        display.setCursor(tx, stripY);
        display.print(nm);
      }
    }
    display.setTextWrap(true);
  }

  // === Help text (scrolls if overflow) ===
  const char* desc = (carouselCursor < carouselConfig->count) ? carouselConfig->descs[carouselCursor] : "???";
  int descLen = strlen(desc);
  const int maxChars = 21;  // 128px / 6px per char

  static int crslHelpScroll = 0;
  static int crslHelpDir = 1;
  static unsigned long crslHelpTimer = 0;
  static uint8_t crslHelpLastCursor = 0xFF;
  static const CarouselConfig* crslHelpLastConfig = NULL;
  unsigned long now = millis();

  // Reset scroll on cursor or config change
  if (carouselCursor != crslHelpLastCursor || carouselConfig != crslHelpLastConfig) {
    crslHelpScroll = 0;
    crslHelpDir = 1;
    crslHelpTimer = now;
    crslHelpLastCursor = carouselCursor;
    crslHelpLastConfig = carouselConfig;
  }

  if (descLen <= maxChars) {
    int descW = descLen * 6;
    display.setCursor((128 - descW) / 2, 40);
    display.print(desc);
  } else {
    int maxScroll = descLen - maxChars;
    if (now - crslHelpTimer >= (unsigned long)(crslHelpScroll == 0 || crslHelpScroll == maxScroll ? 1500 : 300)) {
      crslHelpScroll += crslHelpDir;
      if (crslHelpScroll >= maxScroll) { crslHelpScroll = maxScroll; crslHelpDir = -1; }
      if (crslHelpScroll <= 0) { crslHelpScroll = 0; crslHelpDir = 1; }
      crslHelpTimer = now;
    }
    display.setCursor(0, 40);
    for (int i = 0; i < maxChars && (crslHelpScroll + i) < descLen; i++) {
      display.print(desc[crslHelpScroll + i]);
    }
  }

  // === Footer ===
  display.drawFastHLine(0, 53, 128, SSD1306_WHITE);
  const char* footerTxt = "Turn to browse, press to set";
  int footerLen = strlen(footerTxt);

  static int crslFootScroll = 0;
  static int crslFootDir = 1;
  static unsigned long crslFootTimer = 0;

  if (footerLen <= maxChars) {
    int fw = footerLen * 6;
    display.setCursor((128 - fw) / 2, 56);
    display.print(footerTxt);
  } else {
    int maxScroll = footerLen - maxChars;
    if (now - crslFootTimer >= (unsigned long)(crslFootScroll == 0 || crslFootScroll == maxScroll ? 1500 : 300)) {
      crslFootScroll += crslFootDir;
      if (crslFootScroll >= maxScroll) { crslFootScroll = maxScroll; crslFootDir = -1; }
      if (crslFootScroll <= 0) { crslFootScroll = 0; crslFootDir = 1; }
      crslFootTimer = now;
    }
    display.setCursor(0, 56);
    for (int i = 0; i < maxChars && (crslFootScroll + i) < footerLen; i++) {
      display.print(footerTxt[crslFootScroll + i]);
    }
  }
}

// ============================================================================
// BREAKOUT NORMAL MODE
// ============================================================================

static void drawBreakoutNormal() {
  display.setTextSize(1);

  // --- Header: Score, Level, Lives, Battery ---
  {
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "S:%u Lv:%u", brk.score, brk.level);
    display.setCursor(0, 0);
    display.print(hdr);

    // Lives as hearts (right-aligned before battery)
    // Drawn manually — 5x6px heart via two circles + triangle (font-independent)
    int heartsX = 80;
    for (uint8_t i = 0; i < brk.lives && i < 5; i++) {
      int hx = heartsX + i * 7;
      display.fillCircle(hx + 1, 1, 1, SSD1306_WHITE);
      display.fillCircle(hx + 3, 1, 1, SSD1306_WHITE);
      display.fillTriangle(hx, 2, hx + 4, 2, hx + 2, 5, SSD1306_WHITE);
    }

    // Battery (far right)
    char batBuf[8];
    snprintf(batBuf, sizeof(batBuf), "%d%%", batteryPercent);
    int batW = strlen(batBuf) * 6;
    display.setCursor(128 - batW, 0);
    display.print(batBuf);
  }

  // --- Bricks ---
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    int16_t by = 10 + r * (BREAKOUT_BRICK_H + 1);  // 1px gap
    for (uint8_t c = 0; c < BREAKOUT_BRICK_COLS; c++) {
      if (!((brk.brickRows[r] >> c) & 1)) continue;
      int16_t bx = c * (BREAKOUT_BRICK_W + 1);

      // Reinforced bricks: outline only; normal: filled
      if ((brk.brickHits[r] >> c) & 1) {
        display.drawRect(bx, by, BREAKOUT_BRICK_W, BREAKOUT_BRICK_H, SSD1306_WHITE);
      } else {
        display.fillRect(bx, by, BREAKOUT_BRICK_W, BREAKOUT_BRICK_H, SSD1306_WHITE);
      }
    }
  }

  // --- Ball ---
  {
    int16_t bx = brk.ballX >> 8;
    int16_t by = brk.ballY >> 8;
    display.fillRect(bx, by, BREAKOUT_BALL_SIZE, BREAKOUT_BALL_SIZE, SSD1306_WHITE);
  }

  // --- Paddle ---
  display.fillRect(brk.paddleX, BREAKOUT_PADDLE_Y, brk.paddleW, 2, SSD1306_WHITE);

  // --- Footer / overlays ---
  display.drawFastHLine(0, 56, 128, SSD1306_WHITE);

  if (brk.state == BRK_IDLE) {
    const char* hint = "D7=Launch  Turn=Move";
    display.setCursor(0, 57);
    display.print(hint);
  } else if (brk.state == BRK_PAUSED) {
    // Paused overlay
    const char* msg = "PAUSED";
    int w = strlen(msg) * 6;
    display.fillRect((128 - w) / 2 - 4, 28, w + 8, 12, SSD1306_BLACK);
    display.drawRect((128 - w) / 2 - 4, 28, w + 8, 12, SSD1306_WHITE);
    display.setCursor((128 - w) / 2, 30);
    display.print(msg);

    display.setCursor(0, 57);
    display.print("D7=Resume");
  } else if (brk.state == BRK_LEVEL_CLEAR) {
    const char* msg = "LEVEL CLEAR!";
    int w = strlen(msg) * 6;
    display.fillRect((128 - w) / 2 - 4, 28, w + 8, 12, SSD1306_BLACK);
    display.drawRect((128 - w) / 2 - 4, 28, w + 8, 12, SSD1306_WHITE);
    display.setCursor((128 - w) / 2, 30);
    display.print(msg);

    display.setCursor(0, 57);
    display.print("D7=Next level");
  } else if (brk.state == BRK_GAME_OVER) {
    const char* msg = "GAME OVER";
    int w = strlen(msg) * 6;
    display.fillRect((128 - w) / 2 - 6, 24, w + 12, 20, SSD1306_BLACK);
    display.drawRect((128 - w) / 2 - 6, 24, w + 12, 20, SSD1306_WHITE);
    display.setCursor((128 - w) / 2, 26);
    display.print(msg);

    char scoreBuf[20];
    snprintf(scoreBuf, sizeof(scoreBuf), "Score: %u", brk.score);
    int sw = strlen(scoreBuf) * 6;
    display.setCursor((128 - sw) / 2, 36);
    display.print(scoreBuf);

    display.setCursor(0, 57);
    display.print("D7=New game");
  } else {
    // Playing — show score + controls hint
    display.setCursor(0, 57);
    display.print("D7=Pause  Turn=Move");
  }
}

// ============================================================================
// MODE PICKER (MODE_MODE sub-page)
// ============================================================================

static void drawModePickerPage() {
  display.setTextSize(1);

  if (modeConfirming) {
    // === Mode change confirmation prompt ===
    display.setCursor(0, 0);
    display.print("MODE CHANGE");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Show new mode name in quotes, centered
    const char* modeName = (settings.operationMode < 4) ? OP_MODE_NAMES[settings.operationMode] : "???";
    char nameBuf[20];
    snprintf(nameBuf, sizeof(nameBuf), "\"%s\"", modeName);
    int nameW = strlen(nameBuf) * 6;
    display.setCursor((128 - nameW) / 2, 18);
    display.print(nameBuf);

    // "Reboot to apply?"
    const char* prompt = "Reboot to apply?";
    int promptW = strlen(prompt) * 6;
    display.setCursor((128 - promptW) / 2, 30);
    display.print(prompt);

    // Yes / No options (Yes highlighted by default)
    int optY = 42;
    int yesX = 30;
    int noX = 80;

    if (modeRebootYes) {
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

    display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("Turn=select Press=OK");
    return;
  }

  // === Header ===
  display.setCursor(0, 0);
  display.print("DEVICE MODE SELECT");
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Horizontal carousel strip ===
  {
    static float modeScrollX = 0.0f;

    // Compute cell layout (snap handled after target computation)
    int cellWidth[4];
    int cellCenterX[4];
    int runX = 0;
    for (int i = 0; i < 4; i++) {
      const char* nm = (i < 4) ? OP_MODE_NAMES[i] : "???";
      cellWidth[i] = (int)strlen(nm) * 6 + 16;
      cellCenterX[i] = runX + cellWidth[i] / 2;
      runX += cellWidth[i];
    }

    // Animate scroll toward selected item (snap on first frame)
    float target = (float)cellCenterX[modePickerCursor] - 64.0f;
    if (modePickerSnap) {
      modeScrollX = target;
      modePickerSnap = false;
    } else {
      float delta = target - modeScrollX;
      if (delta > -0.5f && delta < 0.5f) {
        modeScrollX = target;
      } else {
        modeScrollX += delta * 0.25f;
        markDisplayDirty();  // keep animating until converged
      }
    }
    int scrollI = (int)modeScrollX;

    // Render strip at y=24 (centered in content area)
    const int stripY = 24;
    const int stripH = 11;
    display.setTextWrap(false);
    for (int i = 0; i < 4; i++) {
      const char* nm = OP_MODE_NAMES[i];
      int tw = (int)strlen(nm) * 6;
      int tx = cellCenterX[i] - scrollI - tw / 2;

      if (tx >= 128 || tx + tw <= 0) continue;

      if (i == modePickerCursor) {
        int rx = cellCenterX[i] - scrollI - cellWidth[i] / 2;
        display.fillRect(rx, stripY - 1, cellWidth[i], stripH, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(tx, stripY);
        display.print(nm);
        display.setTextColor(SSD1306_WHITE);
      } else {
        display.setCursor(tx, stripY);
        display.print(nm);
      }
    }
    display.setTextWrap(true);
  }

  // === Help text (scrolls if overflow) ===
  static const char* MODE_DESCS[] = { "Direct timing control", "Human work patterns", "Media controller", "Brick-breaking arcade" };
  const char* desc = (modePickerCursor < 4) ? MODE_DESCS[modePickerCursor] : "???";
  int descLen = strlen(desc);
  const int maxChars = 21;  // 128px / 6px per char

  static int modeHelpScroll = 0;
  static int modeHelpDir = 1;
  static unsigned long modeHelpTimer = 0;
  static uint8_t modeHelpLastCursor = 0xFF;
  unsigned long now = millis();

  // Reset scroll on cursor change
  if (modePickerCursor != modeHelpLastCursor) {
    modeHelpScroll = 0;
    modeHelpDir = 1;
    modeHelpTimer = now;
    modeHelpLastCursor = modePickerCursor;
  }

  display.setCursor(0, 40);
  if (descLen <= maxChars) {
    // Center short text
    int descW = descLen * 6;
    display.setCursor((128 - descW) / 2, 40);
    display.print(desc);
  } else {
    int maxScroll = descLen - maxChars;
    if (now - modeHelpTimer >= (unsigned long)(modeHelpScroll == 0 || modeHelpScroll == maxScroll ? 1500 : 300)) {
      modeHelpScroll += modeHelpDir;
      if (modeHelpScroll >= maxScroll) { modeHelpScroll = maxScroll; modeHelpDir = -1; }
      if (modeHelpScroll <= 0) { modeHelpScroll = 0; modeHelpDir = 1; }
      modeHelpTimer = now;
    }
    display.setCursor(0, 40);
    for (int i = 0; i < maxChars && (modeHelpScroll + i) < descLen; i++) {
      display.print(desc[modeHelpScroll + i]);
    }
  }

  // === Footer (scrolls if overflow) ===
  display.drawFastHLine(0, 53, 128, SSD1306_WHITE);
  const char* footerTxt = "Turn dial to adjust, press to select";
  int footerLen = strlen(footerTxt);

  static int modeFootScroll = 0;
  static int modeFootDir = 1;
  static unsigned long modeFootTimer = 0;

  if (footerLen <= maxChars) {
    int fw = footerLen * 6;
    display.setCursor((128 - fw) / 2, 56);
    display.print(footerTxt);
  } else {
    int maxScroll = footerLen - maxChars;
    if (now - modeFootTimer >= (unsigned long)(modeFootScroll == 0 || modeFootScroll == maxScroll ? 1500 : 300)) {
      modeFootScroll += modeFootDir;
      if (modeFootScroll >= maxScroll) { modeFootScroll = maxScroll; modeFootDir = -1; }
      if (modeFootScroll <= 0) { modeFootScroll = 0; modeFootDir = 1; }
      modeFootTimer = now;
    }
    display.setCursor(0, 56);
    for (int i = 0; i < maxChars && (modeFootScroll + i) < footerLen; i++) {
      display.print(footerTxt[modeFootScroll + i]);
    }
  }
}

// ============================================================================
// SET CLOCK MODE
// ============================================================================

static void drawSetClockMode() {
  display.setTextSize(1);

  // === Header (y=0) ===
  display.setCursor(0, 0);
  display.print("SET CLOCK");
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Large HH:MM preview at size 2, centered (y=12..27) ===
  {
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", clockHour, clockMinute);
    display.setTextSize(2);
    int tw = strlen(timeBuf) * 12;  // size 2 = 12px per char
    display.setCursor((128 - tw) / 2, 14);
    display.print(timeBuf);
    display.setTextSize(1);
  }

  // === Editable rows (y=32, y=40) ===
  const char* rowLabels[] = { "Hour", "Minute" };
  const int rowY[] = { 32, 40 };

  for (int row = 0; row < 2; row++) {
    bool isSelected = (clockCursor == row);
    uint8_t val = (row == 0) ? clockHour : clockMinute;

    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%d", val);

    // Always show arrows (values wrap)
    char arrowStr[16];
    snprintf(arrowStr, sizeof(arrowStr), "< %s >", valStr);
    int arrowW = strlen(arrowStr) * 6;

    if (isSelected && clockEditing) {
      // Editing: label normal, value inverted with arrows
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);

      int editX = 128 - arrowW - 1;
      display.fillRect(editX, rowY[row], arrowW + 1, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(editX, rowY[row]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else if (isSelected) {
      // Selected but not editing: full row inverted
      display.fillRect(0, rowY[row], 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);

      display.setCursor(128 - arrowW - 1, rowY[row]);
      display.print(arrowStr);
      display.setTextColor(SSD1306_WHITE);
    } else {
      // Unselected: label left, value right
      display.setCursor(2, rowY[row]);
      display.print(rowLabels[row]);
      int vw = strlen(valStr) * 6;
      display.setCursor(128 - vw - 1, rowY[row]);
      display.print(valStr);
    }
  }

  // === Separator + help text ===
  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);

  const char* help;
  if (clockCursor == 0) {
    help = "Set hour (0-23)";
  } else {
    help = "Set minute (0-59)";
  }
  display.setCursor(0, 52);
  display.print(help);
  display.setCursor(0, 60);
  display.print("Func=confirm");
}

// ============================================================================
// VOLUME CONTROL DISPLAY THEMES
// ============================================================================

static void drawVolumeHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Volume");

  // Right side: BT/USB icon + battery
  char batStr[8];
  snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
  int batWidth = strlen(batStr) * 6;
  int batX = 128 - batWidth;
  int btX = batX - 8;
  unsigned long now = millis();
  if (usbConnected) {
    display.drawBitmap(btX, 0, usbIcon, 5, 8, SSD1306_WHITE);
  } else if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    if ((now / 500) % 2 == 0) display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  }
  display.setCursor(batX, 0);
  display.print(batStr);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
}

static void drawVolumeFooter() {
  display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
  // Play/Pause icon at x=2, y=56
  if (volPlaying) {
    display.drawBitmap(2, 56, iconPlay8, 8, 8, SSD1306_WHITE);
  } else {
    display.drawBitmap(2, 56, iconPause8, 8, 8, SSD1306_WHITE);
  }
  // Mute state text at x=14
  display.setCursor(14, 56);
  if (volMuted) {
    display.print("MUTED");
  }
}

static void drawVolumeBasic() {
  display.clearDisplay();
  drawVolumeHeader();

  // Sony 90s-style segmented volume bar, centered
  // 21 segments total: center = index 10, left 0-9, right 11-20
  // Each segment: 4px wide, 2px gap, total = 21 * 6 - 2 = 124px, start at x=2
  const int numSegs = 21;
  const int centerSeg = 10;
  const int segW = 4;
  const int segGap = 2;
  const int segH = 20;
  const int startX = 2;
  const int segY = 14;

  // Determine how many segments to fill based on feedback
  int fillCount = 0;  // segments from center (positive = right, negative = left)
  unsigned long now = millis();
  if (volFeedbackDir != 0 && (now - volFeedbackStart < VOL_FEEDBACK_DISPLAY_MS)) {
    float progress = 1.0f - (float)(now - volFeedbackStart) / (float)VOL_FEEDBACK_DISPLAY_MS;
    int maxFill = centerSeg;  // 10 segments max in either direction
    fillCount = (int)(maxFill * progress + 0.5f);
    if (fillCount < 1) fillCount = 1;
    if (volFeedbackDir < 0) fillCount = -fillCount;
  }

  for (int i = 0; i < numSegs; i++) {
    int x = startX + i * (segW + segGap);

    if (i == centerSeg) {
      // Center reference bar — always filled
      display.fillRect(x, segY, segW, segH, SSD1306_WHITE);
    } else if (fillCount > 0 && i > centerSeg && i <= centerSeg + fillCount) {
      // Right fill (volume up)
      display.fillRect(x, segY, segW, segH, SSD1306_WHITE);
    } else if (fillCount < 0 && i < centerSeg && i >= centerSeg + fillCount) {
      // Left fill (volume down)
      display.fillRect(x, segY, segW, segH, SSD1306_WHITE);
    } else {
      // Empty segment — small dot in center
      display.fillRect(x + 1, segY + segH / 2 - 1, 2, 2, SSD1306_WHITE);
    }
  }

  // Direction label below bar
  if (fillCount != 0) {
    const char* txt = (fillCount > 0) ? "VOL+" : "VOL-";
    int w = strlen(txt) * 6;
    display.setCursor((128 - w) / 2, 37);
    display.print(txt);
  }

  // Muted overlay
  if (volMuted) {
    display.setCursor(46, 46);
    display.print("MUTED");
  }

  drawVolumeFooter();
}

static void drawVolumeRetro() {
  display.clearDisplay();
  drawVolumeHeader();

  // VU meter housing
  int hx = 14, hy = 13, hw = 100, hh = 34;
  display.drawRoundRect(hx, hy, hw, hh, 3, SSD1306_WHITE);

  // Scale labels inside housing
  display.setCursor(hx + 10, hy + 3);
  display.print("-");
  display.setCursor(hx + hw/2 - 3, hy + 3);
  display.print("0");
  display.setCursor(hx + hw - 16, hy + 3);
  display.print("+");

  // Scale tick marks
  int pivotX = hx + hw / 2;
  int pivotY = hy + hh - 6;
  for (int a = -40; a <= 40; a += 20) {
    float rad = (float)a * 3.14159f / 180.0f;
    int tx = pivotX + (int)(20.0f * sinf(rad));
    int ty = pivotY - (int)(20.0f * cosf(rad));
    display.drawPixel(tx, ty, SSD1306_WHITE);
  }

  // Needle angle: center = 0, swing on feedback
  float angle = 0;
  unsigned long now = millis();
  if (volFeedbackDir != 0 && (now - volFeedbackStart < 800)) {
    float progress = (float)(now - volFeedbackStart) / 800.0f;
    float ease = 1.0f - progress * progress;  // ease-out
    angle = (volFeedbackDir > 0 ? 35.0f : -35.0f) * ease;
  }

  float rad = angle * 3.14159f / 180.0f;
  int needleLen = 22;
  int nx = pivotX + (int)((float)needleLen * sinf(rad));
  int ny = pivotY - (int)((float)needleLen * cosf(rad));
  display.drawLine(pivotX, pivotY, nx, ny, SSD1306_WHITE);

  // Pivot dot
  display.fillCircle(pivotX, pivotY, 2, SSD1306_WHITE);

  drawVolumeFooter();

  // Retro label
  display.setCursor(86, 56);
  display.print("RETRO");
}

static void drawVolumeFuturistic() {
  display.clearDisplay();
  drawVolumeHeader();

  // Slider control — horizontal track with tick marks and draggable thumb
  const int trackY = 30;       // vertical center of track
  const int trackX0 = 4;      // left edge
  const int trackX1 = 123;    // right edge
  const int trackW = trackX1 - trackX0;
  const int centerX = trackX0 + trackW / 2;
  const int numTicks = 21;    // tick marks including endpoints

  // Horizontal track line
  display.drawFastHLine(trackX0, trackY, trackW, SSD1306_WHITE);

  // Tick marks above and below track
  for (int i = 0; i < numTicks; i++) {
    int tx = trackX0 + (i * trackW) / (numTicks - 1);
    int tickH = (i == numTicks / 2) ? 6 : 4;  // center tick taller
    display.drawFastVLine(tx, trackY - tickH, tickH, SSD1306_WHITE);
    display.drawFastVLine(tx, trackY + 1, tickH, SSD1306_WHITE);
  }

  // Thumb position: center at rest, moves left/right on feedback
  int thumbX = centerX;
  unsigned long now = millis();
  if (volFeedbackDir != 0 && (now - volFeedbackStart < VOL_FEEDBACK_DISPLAY_MS)) {
    float progress = 1.0f - (float)(now - volFeedbackStart) / (float)VOL_FEEDBACK_DISPLAY_MS;
    int maxTravel = trackW / 2 - 4;  // max offset from center
    int offset = (int)(maxTravel * progress + 0.5f);
    if (offset < 1) offset = 1;
    thumbX = centerX + (volFeedbackDir > 0 ? offset : -offset);
  }

  // Thumb: filled rectangle with border (5px wide, 14px tall)
  const int thumbW = 5;
  const int thumbH = 14;
  int tx = thumbX - thumbW / 2;
  int ty = trackY - thumbH / 2;
  display.fillRect(tx, ty, thumbW, thumbH, SSD1306_WHITE);
  // Inner cutout for visual depth (1px inset on sides)
  display.drawFastVLine(tx + 1, ty + 2, thumbH - 4, SSD1306_BLACK);
  display.drawFastVLine(tx + 3, ty + 2, thumbH - 4, SSD1306_BLACK);

  // Direction labels
  display.setCursor(trackX0, trackY + 12);
  display.print("-");
  display.setCursor(trackX1 - 5, trackY + 12);
  display.print("+");

  // Muted overlay
  if (volMuted) {
    display.setCursor(46, 46);
    display.print("MUTED");
  }

  drawVolumeFooter();
}

static void drawVolumeNormal() {
  switch (settings.volumeTheme) {
    case 1:  drawVolumeRetro(); break;
    case 2:  drawVolumeFuturistic(); break;
    default: drawVolumeBasic(); break;
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
  } else if (menuCursor >= 0 && MENU_ITEMS[menuCursor].settingId == SET_BLE_IDENTITY && MENU_ITEMS[menuCursor].type == MENU_ACTION) {
    // Dynamic help: show current identity
    if (settings.decoyIndex > 0 && settings.decoyIndex <= DECOY_COUNT) {
      snprintf(nameHelpBuf, sizeof(nameHelpBuf), "Current: %s", DECOY_NAMES[settings.decoyIndex - 1]);
    } else {
      snprintf(nameHelpBuf, sizeof(nameHelpBuf), "Current: %s", settings.deviceName);
    }
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
    // === Restore defaults confirmation prompt ===
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

  // BT/USB icon + battery right-aligned in header
  char batStr[16];
  snprintf(batStr, sizeof(batStr), "%d%%", batteryPercent);
  int batWidth = strlen(batStr) * 6;
  int batX = 128 - batWidth;
  int btX = batX - 5 - 3;
  if (usbConnected) {
    display.drawBitmap(btX, 0, usbIcon, 5, 8, SSD1306_WHITE);
  } else if (deviceConnected) {
    display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  } else {
    if ((millis() / 500) % 2 == 0)
      display.drawBitmap(btX, 0, btIcon, 5, 8, SSD1306_WHITE);
  }
  display.setCursor(batX, 0);
  display.print(batStr);

  // === Separator ===
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // === Menu viewport: 5 visible rows x 8px each (y=10 to y=49) ===
  // Skip hidden items — only render visible ones into row slots
  int row = 0;
  for (int idx = menuScrollOffset; idx < MENU_ITEM_COUNT && row < 5; idx++) {
    if (isMenuItemHidden(idx)) continue;

    int y = 10 + row * 8;
    const MenuItem& item = MENU_ITEMS[idx];
    bool isSelected = (idx == menuCursor);

    if (item.type == MENU_HEADING) {
      // Heading: centered "- Label -" style
      char heading[32];
      snprintf(heading, sizeof(heading), "- %s -", item.label);
      int hw = strlen(heading) * 6;
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
      char valStr[20];
      formatMenuValue(item.settingId, item.format, valStr, sizeof(valStr));
      uint32_t curVal = getSettingValue(item.settingId);
      bool atMin = (curVal <= item.minVal);
      bool atMax = (curVal >= item.maxVal);
      // Hide Move size value when Bezier mode is active (radius is auto-randomized)
      if (item.settingId == SET_MOUSE_AMP && settings.mouseStyle == 0) {
        snprintf(valStr, sizeof(valStr), "---");
        atMin = true;
        atMax = true;
      }
      // Negative-display: displayed range is inverted (raw max = displayed min)
      if (item.format == FMT_PERCENT_NEG) { bool tmp = atMin; atMin = atMax; atMax = tmp; }

      // Build arrow-wrapped value string once for all three branches
      char arrowStr[32];
      snprintf(arrowStr, sizeof(arrowStr), "%s%s%s",
               !atMin ? "< " : "  ", valStr, !atMax ? " >" : "  ");
      int arrowW = strlen(arrowStr) * 6;

      if (isSelected && menuEditing) {
        // Editing: label normal, value portion inverted with < >
        display.setCursor(2, y);
        display.print(item.label);

        int editX = 128 - arrowW - 1;
        display.fillRect(editX, y, arrowW + 1, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(editX, y);
        display.print(arrowStr);
        display.setTextColor(SSD1306_WHITE);

      } else if (isSelected) {
        // Selected but not editing: full row inverted
        display.fillRect(0, y, 128, 8, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(item.label);

        display.setCursor(128 - arrowW - 1, y);
        display.print(arrowStr);
        display.setTextColor(SSD1306_WHITE);

      } else {
        // Unselected: label left, value right
        display.setCursor(2, y);
        display.print(item.label);

        display.setCursor(128 - arrowW - 1, y);
        display.print(arrowStr);
      }
    }
    row++;
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
  if (scheduleSleeping) return;  // light sleep — display managed by schedule module

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
    if (settings.operationMode == 3) drawBreakoutNormal();  // dim the game display
    else if (settings.operationMode == 2) drawVolumeNormal();
    else if (settings.operationMode == 1) drawSimulationScreensaver();
    else drawScreensaver();
  } else if (currentMode == MODE_NORMAL) {
    if (settings.operationMode == 3) drawBreakoutNormal();
    else if (settings.operationMode == 2) drawVolumeNormal();
    else if (settings.operationMode == 1) drawSimulationNormal();
    else drawNormalMode();
  } else if (currentMode == MODE_MENU) {
    drawMenuMode();
  } else if (currentMode == MODE_SLOTS) {
    drawSlotsMode();
  } else if (currentMode == MODE_NAME) {
    drawNameMode();
  } else if (currentMode == MODE_DECOY) {
    drawDecoyMode();
  } else if (currentMode == MODE_SCHEDULE) {
    drawScheduleMode();
  } else if (currentMode == MODE_SET_CLOCK) {
    drawSetClockMode();
  } else if (currentMode == MODE_MODE) {
    drawModePickerPage();
  } else if (currentMode == MODE_CAROUSEL) {
    drawCarouselPage();
  }

  sendDirtyPages();
}

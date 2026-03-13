#include "snake.h"
#include "state.h"
#include "settings.h"
#include "display.h"
#include "sound.h"

// ============================================================================
// CONSTANTS
// ============================================================================

static const uint16_t SNAKE_TICK_MS[] = { SNAKE_TICK_SLOW_MS, SNAKE_TICK_NORMAL_MS, SNAKE_TICK_FAST_MS };

// ============================================================================
// SOUND EFFECTS (piezo, gated by soundEnabled || systemSoundEnabled)
// ============================================================================


// --- Short blocking sounds (< 10ms, safe to inline) ---

static void playEatSound() {
  if (!canPlayGameSound()) return;
  // Short rising chirp (~5ms)
  for (int f = 2000; f < 4000; f += 400) {
    int halfPeriod = 500000 / f;
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_SOUND, HIGH);
      delayMicroseconds(halfPeriod);
      digitalWrite(PIN_SOUND, LOW);
      delayMicroseconds(halfPeriod);
    }
  }
}

static void playDeathSound() {
  if (!canPlayGameSound()) return;
  // Short low buzz (~8ms)
  for (int i = 0; i < 30; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(130);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(130);
  }
}

// --- Non-blocking game-over melody (descending, ~450ms) ---

enum SnakeSoundId { SS_NONE = 0, SS_GAME_OVER };

static const int SS_GAME_OVER_FREQ[] = { 523, 440, 349, 262 };
#define SS_GAME_OVER_NOTE_COUNT 4
#define SS_GAME_OVER_CYCLES     30
#define SS_GAME_OVER_GAP_US     3000
#define SS_MAX_BLOCK_US         5000

static struct {
  uint8_t type;
  uint8_t noteIdx;
  uint8_t cycleIdx;
  bool gapPending;
  unsigned long gapStartUs;
  uint16_t gapUs;
} snakeSound;

static void stopSnakeSound() {
  snakeSound.type = SS_NONE;
  digitalWrite(PIN_SOUND, LOW);
}

static void startGameOverSound() {
  if (!canPlayGameSound()) return;
  snakeSound.type = SS_GAME_OVER;
  snakeSound.noteIdx = 0;
  snakeSound.cycleIdx = 0;
  snakeSound.gapPending = false;
}

void updateSnakeSound() {
  if (snakeSound.type == SS_NONE) return;

  if (snakeSound.gapPending) {
    if ((micros() - snakeSound.gapStartUs) < snakeSound.gapUs) return;
    snakeSound.gapPending = false;
    snakeSound.noteIdx++;
    snakeSound.cycleIdx = 0;
  }

  if (snakeSound.noteIdx >= SS_GAME_OVER_NOTE_COUNT) {
    stopSnakeSound();
    return;
  }

  int freq = SS_GAME_OVER_FREQ[snakeSound.noteIdx];
  int halfPeriod = 500000 / freq;
  unsigned long startUs = micros();

  while (snakeSound.cycleIdx < SS_GAME_OVER_CYCLES) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(halfPeriod);
    snakeSound.cycleIdx++;
    if ((micros() - startUs) >= SS_MAX_BLOCK_US) return;
  }

  // Note complete
  if (snakeSound.noteIdx < SS_GAME_OVER_NOTE_COUNT - 1) {
    snakeSound.gapPending = true;
    snakeSound.gapStartUs = micros();
    snakeSound.gapUs = SS_GAME_OVER_GAP_US;
  } else {
    snakeSound.noteIdx++;
    if (snakeSound.noteIdx >= SS_GAME_OVER_NOTE_COUNT) {
      stopSnakeSound();
    }
  }
}

// ============================================================================
// FOOD PLACEMENT
// ============================================================================

static void placeFood() {
  // Try random placement first (fast path)
  for (int attempt = 0; attempt < 50; attempt++) {
    uint8_t fx = (uint8_t)random(SNAKE_GRID_W);
    uint8_t fy = (uint8_t)random(SNAKE_GRID_H);
    bool onSnake = false;
    for (uint8_t i = 0; i < gSnk.length; i++) {
      uint8_t idx = (gSnk.headIdx - i) & 0xFF;
      if (gSnk.bodyX[idx] == fx && gSnk.bodyY[idx] == fy) {
        onSnake = true;
        break;
      }
    }
    if (!onSnake) {
      gSnk.foodX = fx;
      gSnk.foodY = fy;
      return;
    }
  }

  // Linear scan fallback (guaranteed placement)
  for (uint8_t y = 0; y < SNAKE_GRID_H; y++) {
    for (uint8_t x = 0; x < SNAKE_GRID_W; x++) {
      bool onSnake = false;
      for (uint8_t i = 0; i < gSnk.length; i++) {
        uint8_t idx = (gSnk.headIdx - i) & 0xFF;
        if (gSnk.bodyX[idx] == x && gSnk.bodyY[idx] == y) {
          onSnake = true;
          break;
        }
      }
      if (!onSnake) {
        gSnk.foodX = x;
        gSnk.foodY = y;
        return;
      }
    }
  }
}

// ============================================================================
// INIT
// ============================================================================

void initSnake() {
  stopSnakeSound();
  memset(&gSnk, 0, sizeof(gSnk));

  // Place 3-segment snake in center, facing right
  uint8_t cx = SNAKE_GRID_W / 2;
  uint8_t cy = SNAKE_GRID_H / 2;
  gSnk.length = 3;
  // Tail at index 0, head at index 2
  for (uint8_t i = 0; i < 3; i++) {
    gSnk.bodyX[i] = cx - 2 + i;
    gSnk.bodyY[i] = cy;
  }
  gSnk.headIdx = 2;
  gSnk.dirX = 1;
  gSnk.dirY = 0;
  gSnk.nextDirX = 1;
  gSnk.nextDirY = 0;
  gSnk.score = 0;
  gSnk.state = SNAKE_IDLE;
  gSnk.lastTickMs = millis();

  placeFood();
  markDisplayDirty();
}

// ============================================================================
// ENCODER INPUT (relative turning)
// ============================================================================

void snakeEncoderInput(int direction) {
  if (gSnk.state != SNAKE_PLAYING && gSnk.state != SNAKE_IDLE) return;

  // Current effective direction (use committed dir, not queued)
  int8_t dx = gSnk.dirX;
  int8_t dy = gSnk.dirY;

  if (direction > 0) {
    // Turn right: (dx,dy) -> (-dy, dx)
    gSnk.nextDirX = -dy;
    gSnk.nextDirY = dx;
  } else {
    // Turn left: (dx,dy) -> (dy, -dx)
    gSnk.nextDirX = dy;
    gSnk.nextDirY = -dx;
  }

  // Auto-start on first input when idle
  if (gSnk.state == SNAKE_IDLE) {
    gSnk.state = SNAKE_PLAYING;
    gSnk.lastTickMs = millis();
  }

  markDisplayDirty();
}

// ============================================================================
// BUTTON PRESS (D7)
// ============================================================================

void snakeButtonPress() {
  switch (gSnk.state) {
    case SNAKE_IDLE:
      gSnk.state = SNAKE_PLAYING;
      gSnk.lastTickMs = millis();
      break;
    case SNAKE_PLAYING:
      gSnk.state = SNAKE_PAUSED;
      break;
    case SNAKE_PAUSED:
      gSnk.state = SNAKE_PLAYING;
      gSnk.lastTickMs = millis();
      break;
    case SNAKE_GAME_OVER:
      stopSnakeSound();
      initSnake();
      break;
  }
  markDisplayDirty();
}

// ============================================================================
// GAME TICK
// ============================================================================

void tickSnake() {
  if (screensaverActive) return;
  if (gSnk.state != SNAKE_PLAYING) return;

  unsigned long now = millis();
  uint8_t speedIdx = (settings.snakeSpeed < SNAKE_SPEED_COUNT) ? settings.snakeSpeed : 1;
  if (now - gSnk.lastTickMs < SNAKE_TICK_MS[speedIdx]) return;
  gSnk.lastTickMs = now;

  // Apply queued direction
  gSnk.dirX = gSnk.nextDirX;
  gSnk.dirY = gSnk.nextDirY;

  // Compute new head position
  uint8_t hx = gSnk.bodyX[gSnk.headIdx];
  uint8_t hy = gSnk.bodyY[gSnk.headIdx];
  int16_t nx = (int16_t)hx + gSnk.dirX;
  int16_t ny = (int16_t)hy + gSnk.dirY;

  // Wall handling
  if (settings.snakeWalls == 0) {
    // Solid walls — collision
    if (nx < 0 || nx >= SNAKE_GRID_W || ny < 0 || ny >= SNAKE_GRID_H) {
      playDeathSound();
      gSnk.state = SNAKE_GAME_OVER;
      if (gSnk.score > settings.snakeHighScore) {
        settings.snakeHighScore = gSnk.score;
        settingsDirty = true;
        settingsDirtyMs = millis();
      }
      startGameOverSound();
      markDisplayDirty();
      return;
    }
  } else {
    // Wrap walls
    if (nx < 0) nx = SNAKE_GRID_W - 1;
    else if (nx >= SNAKE_GRID_W) nx = 0;
    if (ny < 0) ny = SNAKE_GRID_H - 1;
    else if (ny >= SNAKE_GRID_H) ny = 0;
  }

  // Self-collision check (exclude tail segment — it will vacate this tick)
  for (uint8_t i = 0; i < gSnk.length - 1; i++) {
    uint8_t idx = (gSnk.headIdx - i) & 0xFF;
    if (gSnk.bodyX[idx] == (uint8_t)nx && gSnk.bodyY[idx] == (uint8_t)ny) {
      playDeathSound();
      gSnk.state = SNAKE_GAME_OVER;
      if (gSnk.score > settings.snakeHighScore) {
        settings.snakeHighScore = gSnk.score;
        settingsDirty = true;
        settingsDirtyMs = millis();
      }
      startGameOverSound();
      markDisplayDirty();
      return;
    }
  }

  // Move head forward
  uint8_t newHeadIdx = (gSnk.headIdx + 1) & 0xFF;
  gSnk.bodyX[newHeadIdx] = (uint8_t)nx;
  gSnk.bodyY[newHeadIdx] = (uint8_t)ny;
  gSnk.headIdx = newHeadIdx;

  // Check food
  if ((uint8_t)nx == gSnk.foodX && (uint8_t)ny == gSnk.foodY) {
    // Grow (don't remove tail)
    if (gSnk.length < SNAKE_MAX_LENGTH) gSnk.length++;
    gSnk.score++;
    playEatSound();
    placeFood();
  }
  // No food eaten — length stays the same (tail naturally advances since headIdx moved)

  markDisplayDirty();
}

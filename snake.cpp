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

static bool canPlaySound() {
  return settings.soundEnabled || settings.systemSoundEnabled;
}

// --- Short blocking sounds (< 10ms, safe to inline) ---

static void playEatSound() {
  if (!canPlaySound()) return;
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
  if (!canPlaySound()) return;
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
  if (!canPlaySound()) return;
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
    for (uint8_t i = 0; i < snk.length; i++) {
      uint8_t idx = (snk.headIdx - i) & 0xFF;
      if (snk.bodyX[idx] == fx && snk.bodyY[idx] == fy) {
        onSnake = true;
        break;
      }
    }
    if (!onSnake) {
      snk.foodX = fx;
      snk.foodY = fy;
      return;
    }
  }

  // Linear scan fallback (guaranteed placement)
  for (uint8_t y = 0; y < SNAKE_GRID_H; y++) {
    for (uint8_t x = 0; x < SNAKE_GRID_W; x++) {
      bool onSnake = false;
      for (uint8_t i = 0; i < snk.length; i++) {
        uint8_t idx = (snk.headIdx - i) & 0xFF;
        if (snk.bodyX[idx] == x && snk.bodyY[idx] == y) {
          onSnake = true;
          break;
        }
      }
      if (!onSnake) {
        snk.foodX = x;
        snk.foodY = y;
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
  memset(&snk, 0, sizeof(snk));

  // Place 3-segment snake in center, facing right
  uint8_t cx = SNAKE_GRID_W / 2;
  uint8_t cy = SNAKE_GRID_H / 2;
  snk.length = 3;
  // Tail at index 0, head at index 2
  for (uint8_t i = 0; i < 3; i++) {
    snk.bodyX[i] = cx - 2 + i;
    snk.bodyY[i] = cy;
  }
  snk.headIdx = 2;
  snk.dirX = 1;
  snk.dirY = 0;
  snk.nextDirX = 1;
  snk.nextDirY = 0;
  snk.score = 0;
  snk.state = SNAKE_IDLE;
  snk.lastTickMs = millis();

  placeFood();
  markDisplayDirty();
}

// ============================================================================
// ENCODER INPUT (relative turning)
// ============================================================================

void snakeEncoderInput(int direction) {
  if (snk.state != SNAKE_PLAYING && snk.state != SNAKE_IDLE) return;

  // Current effective direction (use committed dir, not queued)
  int8_t dx = snk.dirX;
  int8_t dy = snk.dirY;

  if (direction > 0) {
    // Turn right: (dx,dy) -> (-dy, dx)
    snk.nextDirX = -dy;
    snk.nextDirY = dx;
  } else {
    // Turn left: (dx,dy) -> (dy, -dx)
    snk.nextDirX = dy;
    snk.nextDirY = -dx;
  }

  // Auto-start on first input when idle
  if (snk.state == SNAKE_IDLE) {
    snk.state = SNAKE_PLAYING;
    snk.lastTickMs = millis();
  }

  markDisplayDirty();
}

// ============================================================================
// BUTTON PRESS (D7)
// ============================================================================

void snakeButtonPress() {
  switch (snk.state) {
    case SNAKE_IDLE:
      snk.state = SNAKE_PLAYING;
      snk.lastTickMs = millis();
      break;
    case SNAKE_PLAYING:
      snk.state = SNAKE_PAUSED;
      break;
    case SNAKE_PAUSED:
      snk.state = SNAKE_PLAYING;
      snk.lastTickMs = millis();
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
  if (snk.state != SNAKE_PLAYING) return;

  unsigned long now = millis();
  uint8_t speedIdx = (settings.snakeSpeed < SNAKE_SPEED_COUNT) ? settings.snakeSpeed : 1;
  if (now - snk.lastTickMs < SNAKE_TICK_MS[speedIdx]) return;
  snk.lastTickMs = now;

  // Apply queued direction
  snk.dirX = snk.nextDirX;
  snk.dirY = snk.nextDirY;

  // Compute new head position
  uint8_t hx = snk.bodyX[snk.headIdx];
  uint8_t hy = snk.bodyY[snk.headIdx];
  int16_t nx = (int16_t)hx + snk.dirX;
  int16_t ny = (int16_t)hy + snk.dirY;

  // Wall handling
  if (settings.snakeWalls == 0) {
    // Solid walls — collision
    if (nx < 0 || nx >= SNAKE_GRID_W || ny < 0 || ny >= SNAKE_GRID_H) {
      playDeathSound();
      snk.state = SNAKE_GAME_OVER;
      if (snk.score > settings.snakeHighScore) {
        settings.snakeHighScore = snk.score;
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
  for (uint8_t i = 0; i < snk.length - 1; i++) {
    uint8_t idx = (snk.headIdx - i) & 0xFF;
    if (snk.bodyX[idx] == (uint8_t)nx && snk.bodyY[idx] == (uint8_t)ny) {
      playDeathSound();
      snk.state = SNAKE_GAME_OVER;
      if (snk.score > settings.snakeHighScore) {
        settings.snakeHighScore = snk.score;
        settingsDirty = true;
        settingsDirtyMs = millis();
      }
      startGameOverSound();
      markDisplayDirty();
      return;
    }
  }

  // Move head forward
  uint8_t newHeadIdx = (snk.headIdx + 1) & 0xFF;
  snk.bodyX[newHeadIdx] = (uint8_t)nx;
  snk.bodyY[newHeadIdx] = (uint8_t)ny;
  snk.headIdx = newHeadIdx;

  // Check food
  if ((uint8_t)nx == snk.foodX && (uint8_t)ny == snk.foodY) {
    // Grow (don't remove tail)
    if (snk.length < SNAKE_MAX_LENGTH) snk.length++;
    snk.score++;
    playEatSound();
    placeFood();
  }
  // No food eaten — length stays the same (tail naturally advances since headIdx moved)

  markDisplayDirty();
}

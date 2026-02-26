#include "breakout.h"
#include "state.h"
#include "settings.h"
#include "display.h"
#include "sound.h"

// ============================================================================
// CONSTANTS
// ============================================================================

// Fixed-point scale: 1 pixel = 256 units
#define FP_SHIFT 8
#define FP_ONE   (1 << FP_SHIFT)

// Paddle widths per paddleSize setting: Small=16, Normal=22, Large=28, XL=34
static const uint8_t PADDLE_WIDTHS[] = { 16, 22, 28, 34 };

// Ball base speeds (fixed-point dx magnitude) per ballSpeed: Slow, Normal, Fast
static const int16_t BALL_BASE_SPEEDS[] = { 180, 256, 360 };

// Brick layout geometry
#define BRICK_AREA_Y    10   // top of brick area
#define BRICK_GAP       1    // gap between bricks (included in 8px cell)

// Ball starting Y position (on paddle)
#define BALL_START_Y    (BREAKOUT_PADDLE_Y - BREAKOUT_BALL_SIZE)

// Level clear / game over display duration
#define STATE_DISPLAY_MS 2000

// ============================================================================
// SOUND EFFECTS (piezo, gated by soundEnabled || systemSoundEnabled)
// ============================================================================

static bool canPlaySound() {
  return settings.soundEnabled || settings.systemSoundEnabled;
}

// --- Short blocking sounds (< 10ms, safe to inline) ---

static void playPaddleHit() {
  if (!canPlaySound()) return;
  // Short tick (~4ms)
  for (int i = 0; i < 20; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(100);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(100);
  }
}

static void playBrickBreak() {
  if (!canPlaySound()) return;
  // Rising chirp (~6ms)
  for (int f = 3000; f < 5000; f += 200) {
    int halfPeriod = 500000 / f;
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_SOUND, HIGH);
      delayMicroseconds(halfPeriod);
      digitalWrite(PIN_SOUND, LOW);
      delayMicroseconds(halfPeriod);
    }
  }
}

static void playWallBounce() {
  if (!canPlaySound()) return;
  // Quiet tick (~3ms)
  for (int i = 0; i < 10; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(150);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(150);
  }
}

// --- Non-blocking sound state machine for long sounds ---
// playLifeLost (~150ms), playLevelClear (~88ms), playGameOver (~450ms)
// are converted to yield after ~5ms per updateGameSound() call.

enum GameSoundId { GS_NONE = 0, GS_LIFE_LOST, GS_LEVEL_CLEAR, GS_GAME_OVER };

// Life lost: descending 800→250 Hz in 50 Hz steps, 4 cycles each
#define GS_LIFE_LOST_NOTE_COUNT 12

// Level clear: ascending arpeggio, 20 cycles each, 2ms gap
static const int GS_LEVEL_CLEAR_FREQ[] = { 523, 659, 784, 1047 };
#define GS_LEVEL_CLEAR_NOTE_COUNT 4
#define GS_LEVEL_CLEAR_CYCLES     20
#define GS_LEVEL_CLEAR_GAP_US     2000

// Game over: sad descend, 30 cycles each, 3ms gap
static const int GS_GAME_OVER_FREQ[] = { 392, 330, 262, 196 };
#define GS_GAME_OVER_NOTE_COUNT   4
#define GS_GAME_OVER_CYCLES       30
#define GS_GAME_OVER_GAP_US       3000

// Max microseconds to block per updateGameSound() call
#define GS_MAX_BLOCK_US 5000

static struct {
  uint8_t type;              // GameSoundId
  uint8_t noteIdx;           // current note/step index
  uint8_t cycleIdx;          // current cycle within note
  bool gapPending;           // waiting for inter-note gap
  unsigned long gapStartUs;  // micros() when gap started
  uint16_t gapUs;            // gap duration in microseconds
} gameSound;

static void stopGameSound() {
  gameSound.type = GS_NONE;
  digitalWrite(PIN_SOUND, LOW);
}

static void startLifeLostSound() {
  if (!canPlaySound()) return;
  gameSound.type = GS_LIFE_LOST;
  gameSound.noteIdx = 0;
  gameSound.cycleIdx = 0;
  gameSound.gapPending = false;
}

static void startLevelClearSound() {
  if (!canPlaySound()) return;
  gameSound.type = GS_LEVEL_CLEAR;
  gameSound.noteIdx = 0;
  gameSound.cycleIdx = 0;
  gameSound.gapPending = false;
}

static void startGameOverSound() {
  if (!canPlaySound()) return;
  gameSound.type = GS_GAME_OVER;
  gameSound.noteIdx = 0;
  gameSound.cycleIdx = 0;
  gameSound.gapPending = false;
}

void updateGameSound() {
  if (gameSound.type == GS_NONE) return;

  // Handle inter-note gap
  if (gameSound.gapPending) {
    if ((micros() - gameSound.gapStartUs) < gameSound.gapUs) return;
    gameSound.gapPending = false;
    gameSound.noteIdx++;
    gameSound.cycleIdx = 0;
  }

  // Get note parameters based on sound type
  int freq = 0;
  uint8_t totalCycles = 0;
  uint16_t gapUs = 0;
  uint8_t noteCount = 0;

  switch (gameSound.type) {
    case GS_LIFE_LOST:
      noteCount = GS_LIFE_LOST_NOTE_COUNT;
      freq = 800 - gameSound.noteIdx * 50;
      totalCycles = 4;
      gapUs = 0;
      break;
    case GS_LEVEL_CLEAR:
      noteCount = GS_LEVEL_CLEAR_NOTE_COUNT;
      if (gameSound.noteIdx < noteCount) freq = GS_LEVEL_CLEAR_FREQ[gameSound.noteIdx];
      totalCycles = GS_LEVEL_CLEAR_CYCLES;
      gapUs = GS_LEVEL_CLEAR_GAP_US;
      break;
    case GS_GAME_OVER:
      noteCount = GS_GAME_OVER_NOTE_COUNT;
      if (gameSound.noteIdx < noteCount) freq = GS_GAME_OVER_FREQ[gameSound.noteIdx];
      totalCycles = GS_GAME_OVER_CYCLES;
      gapUs = GS_GAME_OVER_GAP_US;
      break;
    default:
      stopGameSound();
      return;
  }

  // Check if sound sequence is complete
  if (gameSound.noteIdx >= noteCount || freq <= 0) {
    stopGameSound();
    return;
  }

  int halfPeriod = 500000 / freq;
  unsigned long startUs = micros();

  // Play cycles until note done or time budget exceeded
  while (gameSound.cycleIdx < totalCycles) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(halfPeriod);
    gameSound.cycleIdx++;
    if ((micros() - startUs) >= GS_MAX_BLOCK_US) return;  // yield to main loop
  }

  // Note complete — start gap or advance
  if (gapUs > 0 && gameSound.noteIdx < noteCount - 1) {
    gameSound.gapPending = true;
    gameSound.gapStartUs = micros();
    gameSound.gapUs = gapUs;
  } else {
    gameSound.noteIdx++;
    gameSound.cycleIdx = 0;
    if (gameSound.noteIdx >= noteCount) {
      stopGameSound();
    }
  }
}

// ============================================================================
// BRICK HELPERS
// ============================================================================

static bool getBrick(uint8_t row, uint8_t col) {
  if (row >= BREAKOUT_BRICK_ROWS || col >= BREAKOUT_BRICK_COLS) return false;
  return (brk.brickRows[row] >> col) & 1;
}

static void clearBrick(uint8_t row, uint8_t col) {
  brk.brickRows[row] &= ~(1 << col);
}

static bool isReinforced(uint8_t row, uint8_t col) {
  if (row >= BREAKOUT_BRICK_ROWS) return false;
  return (brk.brickHits[row] >> col) & 1;
}

static void clearReinforced(uint8_t row, uint8_t col) {
  brk.brickHits[row] &= ~(1 << col);
}

static uint8_t countBricks() {
  uint8_t count = 0;
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    // Count set bits
    uint16_t v = brk.brickRows[r];
    while (v) { count++; v &= v - 1; }
  }
  return count;
}

// ============================================================================
// LEVEL SETUP
// ============================================================================

static void setupLevel() {
  uint8_t level = brk.level;

  // Reset bricks
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    brk.brickRows[r] = 0xFFFF;  // all 16 bricks present
    brk.brickHits[r] = 0;
  }

  // Level 4-6: checkerboard pattern (remove alternating bricks)
  if (level >= 4 && level <= 6) {
    for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
      uint16_t mask = (r % 2 == 0) ? 0xAAAA : 0x5555;
      brk.brickRows[r] = mask;
    }
  }

  // Level 7+: reinforced bricks (2-hit, drawn as outline vs filled)
  if (level >= 7) {
    for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
      // Top two rows are reinforced
      if (r < 2) {
        brk.brickHits[r] = brk.brickRows[r];  // all present bricks are reinforced
      }
    }
  }

  brk.bricksRemaining = countBricks();

  // Position ball on paddle
  brk.paddleW = PADDLE_WIDTHS[settings.paddleSize < PADDLE_SIZE_COUNT ? settings.paddleSize : 1];
  if (brk.paddleW == 0) brk.paddleW = PADDLE_WIDTHS[1];  // safety net
  brk.paddleX = (SCREEN_WIDTH - brk.paddleW) / 2;
  brk.ballX = (int16_t)((brk.paddleX + brk.paddleW / 2 - 1) * FP_ONE);
  brk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
  brk.ballDx = 0;
  brk.ballDy = 0;

  brk.state = BRK_IDLE;
}

// ============================================================================
// INIT
// ============================================================================

void initBreakout() {
  stopGameSound();
  memset(&brk, 0, sizeof(brk));
  brk.level = 1;
  brk.lives = settings.startLives;
  brk.score = 0;
  brk.lastTickMs = millis();
  setupLevel();
  markDisplayDirty();
}

// ============================================================================
// LAUNCH BALL
// ============================================================================

static void launchBall() {
  int16_t speed = BALL_BASE_SPEEDS[settings.ballSpeed < BALL_SPEED_COUNT ? settings.ballSpeed : 1];

  // Speed scales ~5% per level (capped at ~2.5x base); cap level for arithmetic safety
  uint8_t effectiveLevel = min(brk.level, (uint8_t)30);
  int16_t levelBonus = (int16_t)((int32_t)speed * (effectiveLevel - 1) * 5 / 100);
  int16_t maxSpeed = (int16_t)(speed * 5 / 2);
  speed = speed + levelBonus;
  if (speed > maxSpeed) speed = maxSpeed;

  // Random angle: slight random X direction
  int16_t dx = (random(2) == 0) ? speed : -speed;
  // Reduce dx slightly, ensure dy is dominant for playability
  brk.ballDx = (int16_t)(dx * 3 / 4);
  brk.ballDy = (int16_t)(-speed);  // always up

  brk.state = BRK_PLAYING;
}

// ============================================================================
// ENCODER INPUT
// ============================================================================

void breakoutEncoderInput(int direction) {
  // Move paddle (4px per encoder detent)
  int16_t step = 4;
  brk.paddleX += (int16_t)(direction * step);

  // Clamp to screen bounds
  if (brk.paddleX < 0) brk.paddleX = 0;
  if (brk.paddleX + brk.paddleW > SCREEN_WIDTH) brk.paddleX = SCREEN_WIDTH - brk.paddleW;

  // If idle, ball follows paddle
  if (brk.state == BRK_IDLE) {
    brk.ballX = (int16_t)((brk.paddleX + brk.paddleW / 2 - 1) * FP_ONE);
    brk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
  }

  markDisplayDirty();
}

// ============================================================================
// BUTTON PRESS (D7)
// ============================================================================

void breakoutButtonPress() {
  switch (brk.state) {
    case BRK_IDLE:
      launchBall();
      break;
    case BRK_PLAYING:
      brk.state = BRK_PAUSED;
      brk.stateTimer = millis();
      break;
    case BRK_PAUSED:
      brk.state = BRK_PLAYING;
      brk.lastTickMs = millis();  // reset tick timer to avoid catch-up
      break;
    case BRK_LEVEL_CLEAR:
      stopGameSound();
      brk.level++;
      setupLevel();
      break;
    case BRK_GAME_OVER:
      stopGameSound();
      initBreakout();
      break;
  }
  markDisplayDirty();
}

// ============================================================================
// GAME TICK (physics + collision at 20 Hz)
// ============================================================================

void tickBreakout() {
  if (screensaverActive) return;  // pause game during screensaver
  if (brk.state != BRK_PLAYING) return;

  unsigned long now = millis();
  if (now - brk.lastTickMs < BREAKOUT_TICK_MS) return;
  brk.lastTickMs = now;

  // Move ball
  brk.ballX += brk.ballDx;
  brk.ballY += brk.ballDy;

  // Convert to pixel coordinates for collision checks
  int16_t bx = brk.ballX >> FP_SHIFT;
  int16_t by = brk.ballY >> FP_SHIFT;

  // --- Wall collisions ---
  // Left wall
  if (bx <= 0) {
    brk.ballX = 0;
    brk.ballDx = -brk.ballDx;
    bx = 0;
    playWallBounce();
  }
  // Right wall
  if (bx + BREAKOUT_BALL_SIZE >= SCREEN_WIDTH) {
    brk.ballX = (int16_t)((SCREEN_WIDTH - BREAKOUT_BALL_SIZE) * FP_ONE);
    brk.ballDx = -brk.ballDx;
    bx = SCREEN_WIDTH - BREAKOUT_BALL_SIZE;
    playWallBounce();
  }
  // Top wall
  if (by <= 0) {
    brk.ballY = 0;
    brk.ballDy = -brk.ballDy;
    by = 0;
    playWallBounce();
  }

  // --- Bottom (ball lost) ---
  if (by + BREAKOUT_BALL_SIZE > SCREEN_HEIGHT) {
    brk.lives--;
    if (brk.lives == 0) {
      brk.state = BRK_GAME_OVER;
      brk.stateTimer = now;
      // Update high score
      if (brk.score > settings.highScore) {
        settings.highScore = brk.score;
        saveSettings();
      }
      startGameOverSound();  // non-blocking (includes descending tone)
    } else {
      startLifeLostSound();  // non-blocking
      // Reset ball to paddle
      brk.ballX = (int16_t)((brk.paddleX + brk.paddleW / 2 - 1) * FP_ONE);
      brk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
      brk.ballDx = 0;
      brk.ballDy = 0;
      brk.state = BRK_IDLE;
    }
    markDisplayDirty();
    return;
  }

  // --- Paddle collision ---
  if (brk.ballDy > 0 &&  // moving down
      by + BREAKOUT_BALL_SIZE >= BREAKOUT_PADDLE_Y &&
      by + BREAKOUT_BALL_SIZE <= BREAKOUT_PADDLE_Y + 4 &&  // within paddle zone
      bx + BREAKOUT_BALL_SIZE > brk.paddleX &&
      bx < brk.paddleX + brk.paddleW) {
    // Reflect upward
    brk.ballDy = -brk.ballDy;
    brk.ballY = (int16_t)((BREAKOUT_PADDLE_Y - BREAKOUT_BALL_SIZE) * FP_ONE);

    // Adjust X velocity based on where ball hits paddle (5 zones)
    int16_t hitPos = bx - brk.paddleX + BREAKOUT_BALL_SIZE / 2;
    int16_t zone = (int16_t)((int32_t)hitPos * 5 / brk.paddleW);
    if (zone < 0) zone = 0;
    if (zone > 4) zone = 4;

    // Speed magnitude (cap level for arithmetic safety)
    int16_t speed = BALL_BASE_SPEEDS[settings.ballSpeed < BALL_SPEED_COUNT ? settings.ballSpeed : 1];
    uint8_t effectiveLevel = min(brk.level, (uint8_t)30);
    int16_t levelBonus = (int16_t)((int32_t)speed * (effectiveLevel - 1) * 5 / 100);
    int16_t maxSpeed = (int16_t)(speed * 5 / 2);
    speed = speed + levelBonus;
    if (speed > maxSpeed) speed = maxSpeed;

    // Map zone to dx: -1.0, -0.5, 0, +0.5, +1.0 (relative to speed)
    static const int8_t ZONE_DX[] = { -4, -2, 0, 2, 4 };
    brk.ballDx = (int16_t)((int32_t)speed * ZONE_DX[zone] / 4);

    // Nudge if dead center hit produces zero dx (prevent vertical lock)
    if (brk.ballDx == 0) {
      brk.ballDx = (random(2) == 0) ? (speed / 8) : -(speed / 8);
    }

    // Clamp minimum Y velocity (prevent too-horizontal ball)
    if (brk.ballDy > -speed / 3) brk.ballDy = -speed / 3;

    playPaddleHit();
  }

  // --- Brick collision ---
  // Check which brick cell the ball overlaps
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    int16_t brickTopY = BRICK_AREA_Y + r * (BREAKOUT_BRICK_H + BRICK_GAP);
    int16_t brickBotY = brickTopY + BREAKOUT_BRICK_H;

    // Quick Y range check
    if (by + BREAKOUT_BALL_SIZE <= brickTopY || by >= brickBotY) continue;

    for (uint8_t c = 0; c < BREAKOUT_BRICK_COLS; c++) {
      if (!getBrick(r, c)) continue;

      int16_t brickLeftX = c * (BREAKOUT_BRICK_W + BRICK_GAP);
      int16_t brickRightX = brickLeftX + BREAKOUT_BRICK_W;

      // AABB overlap check
      if (bx + BREAKOUT_BALL_SIZE <= brickLeftX || bx >= brickRightX) continue;

      // Hit!
      if (isReinforced(r, c)) {
        // First hit on reinforced brick: weaken it
        clearReinforced(r, c);
        playWallBounce();
      } else {
        clearBrick(r, c);
        brk.bricksRemaining--;
        brk.score += (r < 2) ? 3 : 1;  // top rows worth more
        playBrickBreak();
      }

      // Determine reflection axis
      // Calculate overlap on each side
      int16_t overlapLeft = bx + BREAKOUT_BALL_SIZE - brickLeftX;
      int16_t overlapRight = brickRightX - bx;
      int16_t overlapTop = by + BREAKOUT_BALL_SIZE - brickTopY;
      int16_t overlapBottom = brickBotY - by;

      int16_t minOverlapX = (overlapLeft < overlapRight) ? overlapLeft : overlapRight;
      int16_t minOverlapY = (overlapTop < overlapBottom) ? overlapTop : overlapBottom;

      if (minOverlapX < minOverlapY) {
        brk.ballDx = -brk.ballDx;
      } else {
        brk.ballDy = -brk.ballDy;
      }

      // Only process one brick per tick
      goto brickDone;
    }
  }
  brickDone:

  // Check level clear
  if (brk.bricksRemaining == 0) {
    brk.state = BRK_LEVEL_CLEAR;
    brk.stateTimer = now;
    startLevelClearSound();  // non-blocking
  }

  markDisplayDirty();
}

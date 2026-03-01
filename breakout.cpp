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


// --- Short blocking sounds (< 10ms, safe to inline) ---

static void playPaddleHit() {
  if (!canPlayGameSound()) return;
  // Short tick (~4ms)
  for (int i = 0; i < 20; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(100);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(100);
  }
}

static void playBrickBreak() {
  if (!canPlayGameSound()) return;
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
  if (!canPlayGameSound()) return;
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
  if (!canPlayGameSound()) return;
  gameSound.type = GS_LIFE_LOST;
  gameSound.noteIdx = 0;
  gameSound.cycleIdx = 0;
  gameSound.gapPending = false;
}

static void startLevelClearSound() {
  if (!canPlayGameSound()) return;
  gameSound.type = GS_LEVEL_CLEAR;
  gameSound.noteIdx = 0;
  gameSound.cycleIdx = 0;
  gameSound.gapPending = false;
}

static void startGameOverSound() {
  if (!canPlayGameSound()) return;
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
  return (gBrk.brickRows[row] >> col) & 1;
}

static void clearBrick(uint8_t row, uint8_t col) {
  gBrk.brickRows[row] &= ~(1 << col);
}

static bool isReinforced(uint8_t row, uint8_t col) {
  if (row >= BREAKOUT_BRICK_ROWS) return false;
  return (gBrk.brickHits[row] >> col) & 1;
}

static void clearReinforced(uint8_t row, uint8_t col) {
  gBrk.brickHits[row] &= ~(1 << col);
}

static uint8_t countBricks() {
  uint8_t count = 0;
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    // Count set bits
    uint16_t v = gBrk.brickRows[r];
    while (v) { count++; v &= v - 1; }
  }
  return count;
}

// ============================================================================
// LEVEL SETUP
// ============================================================================

static void setupLevel() {
  uint8_t level = gBrk.level;

  // Reset bricks
  for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
    gBrk.brickRows[r] = 0xFFFF;  // all 16 bricks present
    gBrk.brickHits[r] = 0;
  }

  // Level 4-6: checkerboard pattern (remove alternating bricks)
  if (level >= 4 && level <= 6) {
    for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
      uint16_t mask = (r % 2 == 0) ? 0xAAAA : 0x5555;
      gBrk.brickRows[r] = mask;
    }
  }

  // Level 7+: reinforced bricks (2-hit, drawn as outline vs filled)
  if (level >= 7) {
    for (uint8_t r = 0; r < BREAKOUT_BRICK_ROWS; r++) {
      // Top two rows are reinforced
      if (r < 2) {
        gBrk.brickHits[r] = gBrk.brickRows[r];  // all present bricks are reinforced
      }
    }
  }

  gBrk.bricksRemaining = countBricks();

  // Position ball on paddle
  gBrk.paddleW = PADDLE_WIDTHS[settings.paddleSize < PADDLE_SIZE_COUNT ? settings.paddleSize : 1];
  if (gBrk.paddleW == 0) gBrk.paddleW = PADDLE_WIDTHS[1];  // safety net
  gBrk.paddleX = (SCREEN_WIDTH - gBrk.paddleW) / 2;
  gBrk.ballX = (int16_t)((gBrk.paddleX + gBrk.paddleW / 2 - 1) * FP_ONE);
  gBrk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
  gBrk.ballDx = 0;
  gBrk.ballDy = 0;

  gBrk.state = BRK_IDLE;
}

// ============================================================================
// INIT
// ============================================================================

void initBreakout() {
  stopGameSound();
  memset(&gBrk, 0, sizeof(gBrk));
  gBrk.level = 1;
  gBrk.lives = settings.startLives;
  gBrk.score = 0;
  gBrk.lastTickMs = millis();
  setupLevel();
  markDisplayDirty();
}

// ============================================================================
// LAUNCH BALL
// ============================================================================

static void launchBall() {
  int16_t speed = BALL_BASE_SPEEDS[settings.ballSpeed < BALL_SPEED_COUNT ? settings.ballSpeed : 1];

  // Speed scales ~5% per level (capped at ~2.5x base); cap level for arithmetic safety
  uint8_t effectiveLevel = min(gBrk.level, (uint8_t)30);
  int16_t levelBonus = (int16_t)((int32_t)speed * (effectiveLevel - 1) * 5 / 100);
  int16_t maxSpeed = (int16_t)(speed * 5 / 2);
  speed = speed + levelBonus;
  if (speed > maxSpeed) speed = maxSpeed;

  // Random angle: slight random X direction
  int16_t dx = (random(2) == 0) ? speed : -speed;
  // Reduce dx slightly, ensure dy is dominant for playability
  gBrk.ballDx = (int16_t)(dx * 3 / 4);
  gBrk.ballDy = (int16_t)(-speed);  // always up

  gBrk.state = BRK_PLAYING;
}

// ============================================================================
// ENCODER INPUT
// ============================================================================

void breakoutEncoderInput(int direction) {
  // Move paddle (4px per encoder detent)
  int16_t step = 4;
  gBrk.paddleX += (int16_t)(direction * step);

  // Clamp to screen bounds
  if (gBrk.paddleX < 0) gBrk.paddleX = 0;
  if (gBrk.paddleX + gBrk.paddleW > SCREEN_WIDTH) gBrk.paddleX = SCREEN_WIDTH - gBrk.paddleW;

  // If idle, ball follows paddle
  if (gBrk.state == BRK_IDLE) {
    gBrk.ballX = (int16_t)((gBrk.paddleX + gBrk.paddleW / 2 - 1) * FP_ONE);
    gBrk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
  }

  markDisplayDirty();
}

// ============================================================================
// BUTTON PRESS (D7)
// ============================================================================

void breakoutButtonPress() {
  switch (gBrk.state) {
    case BRK_IDLE:
      launchBall();
      break;
    case BRK_PLAYING:
      gBrk.state = BRK_PAUSED;
      gBrk.stateTimer = millis();
      break;
    case BRK_PAUSED:
      gBrk.state = BRK_PLAYING;
      gBrk.lastTickMs = millis();  // reset tick timer to avoid catch-up
      break;
    case BRK_LEVEL_CLEAR:
      stopGameSound();
      gBrk.level++;
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
  if (gBrk.state != BRK_PLAYING) return;

  unsigned long now = millis();
  if (now - gBrk.lastTickMs < BREAKOUT_TICK_MS) return;
  gBrk.lastTickMs = now;

  // Move ball
  gBrk.ballX += gBrk.ballDx;
  gBrk.ballY += gBrk.ballDy;

  // Convert to pixel coordinates for collision checks
  int16_t bx = gBrk.ballX >> FP_SHIFT;
  int16_t by = gBrk.ballY >> FP_SHIFT;

  // --- Wall collisions ---
  // Left wall
  if (bx <= 0) {
    gBrk.ballX = 0;
    gBrk.ballDx = -gBrk.ballDx;
    bx = 0;
    playWallBounce();
  }
  // Right wall
  if (bx + BREAKOUT_BALL_SIZE >= SCREEN_WIDTH) {
    gBrk.ballX = (int16_t)((SCREEN_WIDTH - BREAKOUT_BALL_SIZE) * FP_ONE);
    gBrk.ballDx = -gBrk.ballDx;
    bx = SCREEN_WIDTH - BREAKOUT_BALL_SIZE;
    playWallBounce();
  }
  // Top wall
  if (by <= 0) {
    gBrk.ballY = 0;
    gBrk.ballDy = -gBrk.ballDy;
    by = 0;
    playWallBounce();
  }

  // --- Bottom (ball lost) ---
  if (by + BREAKOUT_BALL_SIZE > SCREEN_HEIGHT) {
    gBrk.lives--;
    if (gBrk.lives == 0) {
      gBrk.state = BRK_GAME_OVER;
      gBrk.stateTimer = now;
      // Update high score
      if (gBrk.score > settings.highScore) {
        settings.highScore = gBrk.score;
        settingsDirty = true;
        settingsDirtyMs = millis();
      }
      startGameOverSound();  // non-blocking (includes descending tone)
    } else {
      startLifeLostSound();  // non-blocking
      // Reset ball to paddle
      gBrk.ballX = (int16_t)((gBrk.paddleX + gBrk.paddleW / 2 - 1) * FP_ONE);
      gBrk.ballY = (int16_t)(BALL_START_Y * FP_ONE);
      gBrk.ballDx = 0;
      gBrk.ballDy = 0;
      gBrk.state = BRK_IDLE;
    }
    markDisplayDirty();
    return;
  }

  // --- Paddle collision ---
  if (gBrk.ballDy > 0 &&  // moving down
      by + BREAKOUT_BALL_SIZE >= BREAKOUT_PADDLE_Y &&
      by + BREAKOUT_BALL_SIZE <= BREAKOUT_PADDLE_Y + 4 &&  // within paddle zone
      bx + BREAKOUT_BALL_SIZE > gBrk.paddleX &&
      bx < gBrk.paddleX + gBrk.paddleW) {
    // Reflect upward
    gBrk.ballDy = -gBrk.ballDy;
    gBrk.ballY = (int16_t)((BREAKOUT_PADDLE_Y - BREAKOUT_BALL_SIZE) * FP_ONE);

    // Adjust X velocity based on where ball hits paddle (5 zones)
    int16_t hitPos = bx - gBrk.paddleX + BREAKOUT_BALL_SIZE / 2;
    int16_t zone = (int16_t)((int32_t)hitPos * 5 / gBrk.paddleW);
    if (zone < 0) zone = 0;
    if (zone > 4) zone = 4;

    // Speed magnitude (cap level for arithmetic safety)
    int16_t speed = BALL_BASE_SPEEDS[settings.ballSpeed < BALL_SPEED_COUNT ? settings.ballSpeed : 1];
    uint8_t effectiveLevel = min(gBrk.level, (uint8_t)30);
    int16_t levelBonus = (int16_t)((int32_t)speed * (effectiveLevel - 1) * 5 / 100);
    int16_t maxSpeed = (int16_t)(speed * 5 / 2);
    speed = speed + levelBonus;
    if (speed > maxSpeed) speed = maxSpeed;

    // Map zone to dx: -1.0, -0.5, 0, +0.5, +1.0 (relative to speed)
    static const int8_t ZONE_DX[] = { -4, -2, 0, 2, 4 };
    gBrk.ballDx = (int16_t)((int32_t)speed * ZONE_DX[zone] / 4);

    // Nudge if dead center hit produces zero dx (prevent vertical lock)
    if (gBrk.ballDx == 0) {
      gBrk.ballDx = (random(2) == 0) ? (speed / 8) : -(speed / 8);
    }

    // Clamp minimum Y velocity (prevent too-horizontal ball)
    if (gBrk.ballDy > -speed / 3) gBrk.ballDy = -speed / 3;

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
        gBrk.bricksRemaining--;
        gBrk.score += (r < 2) ? 3 : 1;  // top rows worth more
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
        gBrk.ballDx = -gBrk.ballDx;
      } else {
        gBrk.ballDy = -gBrk.ballDy;
      }

      // Only process one brick per tick
      goto brickDone;
    }
  }
  brickDone:

  // Check level clear
  if (gBrk.bricksRemaining == 0) {
    gBrk.state = BRK_LEVEL_CLEAR;
    gBrk.stateTimer = now;
    startLevelClearSound();  // non-blocking
  }

  markDisplayDirty();
}

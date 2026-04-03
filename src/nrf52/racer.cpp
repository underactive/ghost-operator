#include "racer.h"
#include "state.h"
#include "settings.h"
#include "display.h"
#include "sound.h"

// ============================================================================
// CONSTANTS
// ============================================================================

// Tick intervals per speed setting (ms)
static const uint16_t RACER_TICK_SPEEDS[] = { 50, 33, 22 };

// Pixels the road scrolls per tick (visual speed)
static const uint8_t RACER_SCROLL_SPEEDS[] = { 2, 3, 4 };

// Enemy spawn interval range (ms) — decreases with score
#define RACER_SPAWN_INITIAL_MS  1200
#define RACER_SPAWN_MIN_MS      400
#define RACER_SPAWN_SCORE_SCALE 3   // ms reduction per point

// Road curve parameters
#define RACER_CURVE_STEP       1    // center shift per tick toward target
#define RACER_CURVE_CHANGE_MS  2000 // ms between new curve targets

// Portrait screen dimensions (after rotation)
#define PORT_W 64
#define PORT_H 128

// ============================================================================
// SOUND EFFECTS (piezo, gated by soundEnabled || systemSoundEnabled)
// ============================================================================

static void playCrashSound() {
  if (!canPlayGameSound()) return;
  // Short low buzz (~8ms)
  for (int i = 0; i < 30; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(130);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(130);
  }
}

static void playScoreSound() {
  if (!canPlayGameSound()) return;
  // Very short blip (~2ms) — plays when passing an enemy
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(100);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(100);
  }
}

// --- Non-blocking game-over melody ---
enum RacerSoundId { RS_NONE = 0, RS_GAME_OVER };

static const int RS_GAME_OVER_FREQ[] = { 523, 440, 349, 262 };
#define RS_GAME_OVER_NOTE_COUNT 4
#define RS_GAME_OVER_CYCLES     30
#define RS_GAME_OVER_GAP_US     3000
#define RS_MAX_BLOCK_US         5000

static struct {
  uint8_t type;
  uint8_t noteIdx;
  uint8_t cycleIdx;
  bool gapPending;
  unsigned long gapStartUs;
  uint16_t gapUs;
} racerSound;

static void stopRacerSound() {
  racerSound.type = RS_NONE;
  digitalWrite(PIN_SOUND, LOW);
}

static void startGameOverSound() {
  if (!canPlayGameSound()) return;
  racerSound.type = RS_GAME_OVER;
  racerSound.noteIdx = 0;
  racerSound.cycleIdx = 0;
  racerSound.gapPending = false;
}

void updateRacerSound() {
  if (racerSound.type != RS_GAME_OVER) return;

  unsigned long startUs = micros();

  while (racerSound.noteIdx < RS_GAME_OVER_NOTE_COUNT) {
    if (racerSound.gapPending) {
      if ((micros() - racerSound.gapStartUs) < racerSound.gapUs) return;
      racerSound.gapPending = false;
      racerSound.noteIdx++;
      racerSound.cycleIdx = 0;
      continue;
    }

    int freq = RS_GAME_OVER_FREQ[racerSound.noteIdx];
    int halfPeriod = 500000 / freq;

    while (racerSound.cycleIdx < RS_GAME_OVER_CYCLES) {
      if ((micros() - startUs) > RS_MAX_BLOCK_US) return;  // yield
      digitalWrite(PIN_SOUND, HIGH);
      delayMicroseconds(halfPeriod);
      digitalWrite(PIN_SOUND, LOW);
      delayMicroseconds(halfPeriod);
      racerSound.cycleIdx++;
    }

    racerSound.gapPending = true;
    racerSound.gapStartUs = micros();
    racerSound.gapUs = RS_GAME_OVER_GAP_US;
    return;
  }

  stopRacerSound();
}

// ============================================================================
// GAME LOGIC
// ============================================================================

static void resetEnemies() {
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    gRcr.enemies[i].active = false;
  }
}

static void updateSpawnInterval() {
  int interval = RACER_SPAWN_INITIAL_MS - (int)gRcr.score * RACER_SPAWN_SCORE_SCALE;
  if (interval < RACER_SPAWN_MIN_MS) interval = RACER_SPAWN_MIN_MS;
  gRcr.spawnIntervalMs = (uint16_t)interval;
}

void initRacer() {
  memset(&gRcr, 0, sizeof(RacerGameState));
  gRcr.playerX = (PORT_W - RACER_CAR_W) / 2;
  gRcr.roadCenterX = PORT_W / 2;
  gRcr.roadTargetX = PORT_W / 2;
  gRcr.score = 0;
  gRcr.scrollOffset = 0;
  gRcr.state = RACER_IDLE;
  gRcr.lastTickMs = millis();
  gRcr.lastEnemySpawnMs = millis();
  gRcr.spawnIntervalMs = RACER_SPAWN_INITIAL_MS;
  resetEnemies();
  gRcr.lastCurveChangeMs = millis();
  stopRacerSound();
  markDisplayDirty();
}

static void spawnEnemy() {
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!gRcr.enemies[i].active) {
      int roadLeft = gRcr.roadCenterX - RACER_ROAD_W / 2;
      int spawnRange = RACER_ROAD_W - RACER_ENEMY_W;
      if (spawnRange < 1) spawnRange = 1;
      gRcr.enemies[i].x = (int8_t)(roadLeft + random(0, spawnRange));
      gRcr.enemies[i].y = -RACER_ENEMY_H;  // spawn above screen
      gRcr.enemies[i].active = true;
      return;
    }
  }
}

static bool checkCollision() {
  // Road boundary check
  int roadLeft = gRcr.roadCenterX - RACER_ROAD_W / 2;
  int roadRight = roadLeft + RACER_ROAD_W;
  if (gRcr.playerX < roadLeft || (gRcr.playerX + RACER_CAR_W) > roadRight) {
    return true;
  }

  // Enemy collision (AABB)
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!gRcr.enemies[i].active) continue;
    RacerEnemy& e = gRcr.enemies[i];
    if (gRcr.playerX < e.x + RACER_ENEMY_W &&
        gRcr.playerX + RACER_CAR_W > e.x &&
        RACER_PLAYER_Y < e.y + RACER_ENEMY_H &&
        RACER_PLAYER_Y + RACER_CAR_H > e.y) {
      return true;
    }
  }
  return false;
}

static void gameOver() {
  gRcr.state = RACER_GAME_OVER;
  playCrashSound();
  startGameOverSound();

  // Update high score
  if (gRcr.score > settings.racerHighScore) {
    settings.racerHighScore = gRcr.score;
    settingsDirty = true;
    settingsDirtyMs = millis();
  }
  markDisplayDirty();
}

void tickRacer() {
  if (gRcr.state != RACER_PLAYING) return;

  unsigned long now = millis();
  uint8_t speedIdx = settings.racerSpeed;
  if (speedIdx >= RACER_SPEED_COUNT) speedIdx = 1;
  uint16_t tickMs = RACER_TICK_SPEEDS[speedIdx];

  if (now - gRcr.lastTickMs < tickMs) return;
  gRcr.lastTickMs = now;

  uint8_t scrollSpeed = RACER_SCROLL_SPEEDS[speedIdx];

  // Animate road scroll
  gRcr.scrollOffset = (gRcr.scrollOffset + scrollSpeed) % 8;

  // Update road curves
  if (now - gRcr.lastCurveChangeMs > RACER_CURVE_CHANGE_MS) {
    gRcr.lastCurveChangeMs = now;
    // Pick new random target center, constrained so road stays on screen
    int margin = RACER_ROAD_W / 2 + 2;
    gRcr.roadTargetX = (int16_t)random(margin, PORT_W - margin);
  }

  // Smoothly move road center toward target
  if (gRcr.roadCenterX < gRcr.roadTargetX) {
    gRcr.roadCenterX += RACER_CURVE_STEP;
    if (gRcr.roadCenterX > gRcr.roadTargetX) gRcr.roadCenterX = gRcr.roadTargetX;
  } else if (gRcr.roadCenterX > gRcr.roadTargetX) {
    gRcr.roadCenterX -= RACER_CURVE_STEP;
    if (gRcr.roadCenterX < gRcr.roadTargetX) gRcr.roadCenterX = gRcr.roadTargetX;
  }

  // Move enemies downward
  bool scoredThisTick = false;
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!gRcr.enemies[i].active) continue;
    gRcr.enemies[i].y += scrollSpeed;
    // Deactivate if off-screen bottom, award point
    if (gRcr.enemies[i].y > PORT_H) {
      gRcr.enemies[i].active = false;
      gRcr.score++;
      updateSpawnInterval();
      scoredThisTick = true;
    }
  }
  // Play score sound once per tick (avoid stacking 4× ~2ms blocking sounds)
  if (scoredThisTick) playScoreSound();

  // Spawn new enemies
  if (now - gRcr.lastEnemySpawnMs >= gRcr.spawnIntervalMs) {
    gRcr.lastEnemySpawnMs = now;
    spawnEnemy();
  }

  // Check collision
  if (checkCollision()) {
    gameOver();
    return;
  }

  // Score +1 per tick (distance-based)
  // Already scoring per enemy passed above, but let's also add distance points
  // Actually, let's keep it simple — score only from passing enemies.

  markDisplayDirty();
}

void racerEncoderInput(int direction) {
  if (gRcr.state == RACER_PLAYING) {
    // Steer left/right
    gRcr.playerX += direction * RACER_STEER_STEP;
    // Clamp to screen bounds
    if (gRcr.playerX < 0) gRcr.playerX = 0;
    if (gRcr.playerX > PORT_W - RACER_CAR_W) gRcr.playerX = PORT_W - RACER_CAR_W;
    markDisplayDirty();
  } else if (gRcr.state == RACER_GAME_OVER) {
    // No action on encoder during game over
  }
}

void racerButtonPress() {
  switch (gRcr.state) {
    case RACER_IDLE:
      // Start game
      initRacer();
      gRcr.state = RACER_PLAYING;
      gRcr.lastTickMs = millis();
      gRcr.lastEnemySpawnMs = millis();
      gRcr.lastCurveChangeMs = millis();
      break;
    case RACER_PLAYING:
      // Pause
      gRcr.state = RACER_PAUSED;
      break;
    case RACER_PAUSED:
      // Resume
      gRcr.state = RACER_PLAYING;
      gRcr.lastTickMs = millis();
      gRcr.lastEnemySpawnMs = millis();
      break;
    case RACER_GAME_OVER:
      // New game
      stopRacerSound();
      initRacer();
      gRcr.state = RACER_PLAYING;
      gRcr.lastTickMs = millis();
      gRcr.lastEnemySpawnMs = millis();
      gRcr.lastCurveChangeMs = millis();
      break;
  }
  markDisplayDirty();
}

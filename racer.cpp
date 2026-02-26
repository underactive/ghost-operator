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
static unsigned long lastCurveChangeMs = 0;

// Portrait screen dimensions (after rotation)
#define PORT_W 64
#define PORT_H 128

// ============================================================================
// SOUND EFFECTS (piezo, gated by soundEnabled || systemSoundEnabled)
// ============================================================================

static bool canPlaySound() {
  return settings.soundEnabled || settings.systemSoundEnabled;
}

static void playCrashSound() {
  if (!canPlaySound()) return;
  // Short low buzz (~8ms)
  for (int i = 0; i < 30; i++) {
    digitalWrite(PIN_SOUND, HIGH);
    delayMicroseconds(130);
    digitalWrite(PIN_SOUND, LOW);
    delayMicroseconds(130);
  }
}

static void playScoreSound() {
  if (!canPlaySound()) return;
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
  if (!canPlaySound()) return;
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
    rcr.enemies[i].active = false;
  }
}

static void updateSpawnInterval() {
  int interval = RACER_SPAWN_INITIAL_MS - (int)rcr.score * RACER_SPAWN_SCORE_SCALE;
  if (interval < RACER_SPAWN_MIN_MS) interval = RACER_SPAWN_MIN_MS;
  rcr.spawnIntervalMs = (uint16_t)interval;
}

void initRacer() {
  memset(&rcr, 0, sizeof(RacerGameState));
  rcr.playerX = (PORT_W - RACER_CAR_W) / 2;
  rcr.roadCenterX = PORT_W / 2;
  rcr.roadTargetX = PORT_W / 2;
  rcr.score = 0;
  rcr.scrollOffset = 0;
  rcr.state = RACER_IDLE;
  rcr.lastTickMs = millis();
  rcr.lastEnemySpawnMs = millis();
  rcr.spawnIntervalMs = RACER_SPAWN_INITIAL_MS;
  resetEnemies();
  lastCurveChangeMs = millis();
  stopRacerSound();
  markDisplayDirty();
}

static void spawnEnemy() {
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!rcr.enemies[i].active) {
      int roadLeft = rcr.roadCenterX - RACER_ROAD_W / 2;
      int spawnRange = RACER_ROAD_W - RACER_ENEMY_W;
      if (spawnRange < 1) spawnRange = 1;
      rcr.enemies[i].x = (int8_t)(roadLeft + random(0, spawnRange));
      rcr.enemies[i].y = -RACER_ENEMY_H;  // spawn above screen
      rcr.enemies[i].active = true;
      return;
    }
  }
}

static bool checkCollision() {
  // Road boundary check
  int roadLeft = rcr.roadCenterX - RACER_ROAD_W / 2;
  int roadRight = roadLeft + RACER_ROAD_W;
  if (rcr.playerX < roadLeft || (rcr.playerX + RACER_CAR_W) > roadRight) {
    return true;
  }

  // Enemy collision (AABB)
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!rcr.enemies[i].active) continue;
    RacerEnemy& e = rcr.enemies[i];
    if (rcr.playerX < e.x + RACER_ENEMY_W &&
        rcr.playerX + RACER_CAR_W > e.x &&
        RACER_PLAYER_Y < e.y + RACER_ENEMY_H &&
        RACER_PLAYER_Y + RACER_CAR_H > e.y) {
      return true;
    }
  }
  return false;
}

static void gameOver() {
  rcr.state = RACER_GAME_OVER;
  playCrashSound();
  startGameOverSound();

  // Update high score
  if (rcr.score > settings.racerHighScore) {
    settings.racerHighScore = rcr.score;
    settingsDirty = true;
    settingsDirtyMs = millis();
  }
  markDisplayDirty();
}

void tickRacer() {
  if (rcr.state != RACER_PLAYING) return;

  unsigned long now = millis();
  uint8_t speedIdx = settings.racerSpeed;
  if (speedIdx >= RACER_SPEED_COUNT) speedIdx = 1;
  uint16_t tickMs = RACER_TICK_SPEEDS[speedIdx];

  if (now - rcr.lastTickMs < tickMs) return;
  rcr.lastTickMs = now;

  uint8_t scrollSpeed = RACER_SCROLL_SPEEDS[speedIdx];

  // Animate road scroll
  rcr.scrollOffset = (rcr.scrollOffset + scrollSpeed) % 8;

  // Update road curves
  if (now - lastCurveChangeMs > RACER_CURVE_CHANGE_MS) {
    lastCurveChangeMs = now;
    // Pick new random target center, constrained so road stays on screen
    int margin = RACER_ROAD_W / 2 + 2;
    rcr.roadTargetX = (int16_t)random(margin, PORT_W - margin);
  }

  // Smoothly move road center toward target
  if (rcr.roadCenterX < rcr.roadTargetX) {
    rcr.roadCenterX += RACER_CURVE_STEP;
    if (rcr.roadCenterX > rcr.roadTargetX) rcr.roadCenterX = rcr.roadTargetX;
  } else if (rcr.roadCenterX > rcr.roadTargetX) {
    rcr.roadCenterX -= RACER_CURVE_STEP;
    if (rcr.roadCenterX < rcr.roadTargetX) rcr.roadCenterX = rcr.roadTargetX;
  }

  // Move enemies downward
  bool scoredThisTick = false;
  for (int i = 0; i < RACER_MAX_ENEMIES; i++) {
    if (!rcr.enemies[i].active) continue;
    rcr.enemies[i].y += scrollSpeed;
    // Deactivate if off-screen bottom, award point
    if (rcr.enemies[i].y > PORT_H) {
      rcr.enemies[i].active = false;
      rcr.score++;
      updateSpawnInterval();
      scoredThisTick = true;
    }
  }
  // Play score sound once per tick (avoid stacking 4× ~2ms blocking sounds)
  if (scoredThisTick) playScoreSound();

  // Spawn new enemies
  if (now - rcr.lastEnemySpawnMs >= rcr.spawnIntervalMs) {
    rcr.lastEnemySpawnMs = now;
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
  if (rcr.state == RACER_PLAYING) {
    // Steer left/right
    rcr.playerX += direction * RACER_STEER_STEP;
    // Clamp to screen bounds
    if (rcr.playerX < 0) rcr.playerX = 0;
    if (rcr.playerX > PORT_W - RACER_CAR_W) rcr.playerX = PORT_W - RACER_CAR_W;
    markDisplayDirty();
  } else if (rcr.state == RACER_GAME_OVER) {
    // No action on encoder during game over
  }
}

void racerButtonPress() {
  switch (rcr.state) {
    case RACER_IDLE:
      // Start game
      initRacer();
      rcr.state = RACER_PLAYING;
      rcr.lastTickMs = millis();
      rcr.lastEnemySpawnMs = millis();
      lastCurveChangeMs = millis();
      break;
    case RACER_PLAYING:
      // Pause
      rcr.state = RACER_PAUSED;
      break;
    case RACER_PAUSED:
      // Resume
      rcr.state = RACER_PLAYING;
      rcr.lastTickMs = millis();
      rcr.lastEnemySpawnMs = millis();
      break;
    case RACER_GAME_OVER:
      // New game
      stopRacerSound();
      initRacer();
      rcr.state = RACER_PLAYING;
      rcr.lastTickMs = millis();
      rcr.lastEnemySpawnMs = millis();
      lastCurveChangeMs = millis();
      break;
  }
  markDisplayDirty();
}

#include "timing.h"
#include "state.h"
#include "keys.h"

unsigned long applyRandomness(unsigned long baseValue) {
  long variation = (long)(baseValue * RANDOMNESS_PERCENT / 100);
  long result = (long)baseValue + random(-variation, variation + 1);
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

String formatDuration(unsigned long ms, bool withUnit) {
  float sec = ms / 1000.0;
  String suffix = withUnit ? "s" : "";
  if (sec < 10) {
    return String(sec, 1) + suffix;
  } else {
    return String((int)sec) + suffix;
  }
}

unsigned long applyProfile(unsigned long baseValue, int direction) {
  // direction: +1 = increase value, -1 = decrease value
  uint8_t pct = 0;
  switch (currentProfile) {
    case PROFILE_LAZY:  pct = settings.lazyPercent; break;
    case PROFILE_BUSY:  pct = settings.busyPercent; break;
    default: return baseValue;  // NORMAL = no change
  }
  long delta = (long)baseValue * pct / 100;
  long result = (long)baseValue + direction * delta;
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

// Profile-adjusted effective values
// BUSY: shorter KB intervals (-%), longer mouse jiggle (+%), shorter mouse idle (-%)
// LAZY: longer KB intervals (+%), shorter mouse jiggle (-%), longer mouse idle (+%)
unsigned long effectiveKeyMin()      { return applyProfile(settings.keyIntervalMin,      currentProfile == PROFILE_BUSY ? -1 : 1); }
unsigned long effectiveKeyMax()      { return applyProfile(settings.keyIntervalMax,      currentProfile == PROFILE_BUSY ? -1 : 1); }
unsigned long effectiveMouseJiggle() { return applyProfile(settings.mouseJiggleDuration, currentProfile == PROFILE_BUSY ? 1 : -1); }
unsigned long effectiveMouseIdle()   { return applyProfile(settings.mouseIdleDuration,   currentProfile == PROFILE_BUSY ? -1 : 1); }

void scheduleNextKey() {
  // Random between effective min and max (profile-adjusted)
  unsigned long eMin = effectiveKeyMin();
  unsigned long eMax = effectiveKeyMax();
  if (eMax > eMin) {
    currentKeyInterval = eMin + random(eMax - eMin + 1);
  } else {
    currentKeyInterval = eMin;
  }
}

void scheduleNextMouseState() {
  if (mouseState == MOUSE_IDLE) {
    currentMouseJiggle = applyRandomness(effectiveMouseJiggle());
  } else {
    currentMouseIdle = applyRandomness(effectiveMouseIdle());
  }
}

unsigned long saverTimeoutMs() {
  if (settings.saverTimeout == 0) return 0;  // Never
  return (unsigned long)SAVER_MINUTES[settings.saverTimeout] * 60000UL;
}

String formatUptime(unsigned long ms) {
  unsigned long totalSecs = ms / 1000;
  unsigned long d = totalSecs / 86400;
  unsigned long h = (totalSecs % 86400) / 3600;
  unsigned long m = (totalSecs % 3600) / 60;
  unsigned long s = totalSecs % 60;

  String result;
  if (d > 0) result += String(d) + "d ";
  if (h > 0) result += String(h) + "h ";
  if (m > 0) result += String(m) + "m ";
  if (d == 0 && s > 0) result += String(s) + "s ";
  if (result.length() == 0) result = "0s ";
  result.trim();
  return result;
}

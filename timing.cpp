#include "timing.h"
#include "state.h"
#include "keys.h"

unsigned long applyRandomness(unsigned long baseValue) {
  long variation = (long)(baseValue * RANDOMNESS_PERCENT / 100);
  long result = (long)baseValue + random(-variation, variation + 1);
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

void formatDuration(unsigned long ms, char* buf, size_t bufSize, bool withUnit) {
  const char* suffix = withUnit ? "s" : "";
  unsigned long tenths = ms / 100;  // tenths of seconds
  if (tenths < 100) {
    // < 10.0s: show one decimal place (e.g. "3.2s")
    snprintf(buf, bufSize, "%lu.%lu%s", tenths / 10, tenths % 10, suffix);
  } else {
    // >= 10s: whole seconds only (e.g. "15s")
    snprintf(buf, bufSize, "%lu%s", ms / 1000, suffix);
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

void formatUptime(unsigned long ms, char* buf, size_t bufSize) {
  unsigned long totalSecs = ms / 1000;
  unsigned long d = totalSecs / 86400;
  unsigned long h = (totalSecs % 86400) / 3600;
  unsigned long m = (totalSecs % 3600) / 60;
  unsigned long s = totalSecs % 60;

  int pos = 0;
  if (d > 0) pos += snprintf(buf + pos, bufSize - pos, "%lud ", d);
  if (h > 0) pos += snprintf(buf + pos, bufSize - pos, "%luh ", h);
  if (m > 0) pos += snprintf(buf + pos, bufSize - pos, "%lum ", m);
  if (d == 0 && s > 0) pos += snprintf(buf + pos, bufSize - pos, "%lus", s);
  if (pos == 0) { snprintf(buf, bufSize, "0s"); return; }
  // Trim trailing space
  if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';
}

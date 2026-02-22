#include "battery.h"
#include "state.h"

// LiPo discharge curve lookup table (11 inflection points)
// Matches real LiPo characteristics: plateau at 3.7-3.8V, steep dropoff below 3.5V
static const uint16_t LUT_MV[]  = { 3200, 3300, 3400, 3500, 3600, 3700, 3750, 3800, 3900, 4050, 4200 };
static const uint8_t  LUT_PCT[] = {    0,    2,    5,   10,   20,   40,   50,   60,   75,   90,  100 };
#define LUT_SIZE 11

static int mvToPercent(float mv) {
  if (mv <= LUT_MV[0]) return 0;
  if (mv >= LUT_MV[LUT_SIZE - 1]) return 100;
  for (int i = 1; i < LUT_SIZE; i++) {
    if (mv <= LUT_MV[i]) {
      float frac = (mv - LUT_MV[i - 1]) / (float)(LUT_MV[i] - LUT_MV[i - 1]);
      return LUT_PCT[i - 1] + (int)(frac * (LUT_PCT[i] - LUT_PCT[i - 1]));
    }
  }
  return 100;
}

void readBattery() {
  digitalWrite(PIN_VBAT_ENABLE, HIGH);
  delay(1);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);

  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_VBAT);
    delay(1);
  }

  digitalWrite(PIN_VBAT_ENABLE, LOW);
  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  float mv = (sum / 8) * VBAT_MV_PER_LSB * VBAT_DIVIDER;
  batteryVoltage = mv / 1000.0;
  batteryPercent = mvToPercent(mv);
}

#include "battery.h"
#include "state.h"

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
  batteryPercent = constrain((int)(((mv - VBAT_MIN_MV) / (VBAT_MAX_MV - VBAT_MIN_MV)) * 100), 0, 100);
}

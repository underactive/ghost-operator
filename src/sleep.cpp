#include "sleep.h"
#include "state.h"
#include "settings.h"
#include <nrf_soc.h>
#include <nrf_power.h>

void enterDeepSleep() {
  Serial.println("\n*** ENTERING DEEP SLEEP ***");
  Serial.flush();

  // Time is lost after system-off wake
  timeSynced = false;
  scheduleSleeping = false;
  scheduleManualWake = false;

  // Save settings and stats before sleep
  settingsDirty = false;
  saveSettings();
  if (statsDirty) {
    saveStats();
    statsDirty = false;
  }

  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.setCursor(30, 20);
    display.println("SLEEPING...");
    display.setCursor(20, 38);
    display.println("Press btn to wake");
    display.display();
    delay(SLEEP_DISPLAY_MS);
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  // Clean BLE shutdown — must disable restartOnDisconnect BEFORE stopping,
  // otherwise a pending disconnect event (e.g. from trackBleNotify) will
  // restart advertising during delay() calls, leaving SoftDevice events
  // pending that prevent sd_power_system_off() from succeeding.
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.stop();
  if (deviceConnected && bleConnHandle != BLE_CONN_HANDLE_INVALID) {
    Bluefruit.disconnect(bleConnHandle);
  }
  // Peripheral registers, not SoftDevice-owned — safe for direct access
  NRF_UARTE0->ENABLE = 0;
  NRF_TWIM0->ENABLE = 0;

  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A));
  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B));

  nrf_gpio_cfg_input(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_sense_set(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_SENSE_LOW);

  delay(100);
  unsigned long waitStart = millis();
  while (digitalRead(PIN_FUNC_BTN) == LOW) {
    NRF_WDT->RR[0] = 0x6E524635;  // Feed WDT while waiting for button release
    if (millis() - waitStart >= 30000UL) break;  // 30s timeout — don't drain battery on stuck GPIO
    delay(10);
  }
  delay(100);

  sd_power_system_off();
  while(1) { }
}

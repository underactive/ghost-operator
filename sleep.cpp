#include "sleep.h"
#include "state.h"
#include "settings.h"
#include <nrf_soc.h>
#include <nrf_power.h>

void enterDeepSleep() {
  Serial.println("\n*** ENTERING DEEP SLEEP ***");
  Serial.flush();

  // Save settings before sleep
  saveSettings();

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

  digitalWrite(PIN_LED, LOW);
  Bluefruit.Advertising.stop();
  NRF_UARTE0->ENABLE = 0;
  NRF_TWIM0->ENABLE = 0;

  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A));
  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B));

  nrf_gpio_cfg_input(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_sense_set(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_SENSE_LOW);

  delay(100);
  while (digitalRead(PIN_FUNC_BTN) == LOW) delay(10);
  delay(100);

  sd_power_system_off();
  while(1) { }
}

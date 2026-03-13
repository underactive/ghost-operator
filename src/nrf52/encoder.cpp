#include "encoder.h"
#include "state.h"
#include <nrf_soc.h>

static void processEncoderState() {
  uint32_t port = NRF_P0->IN;
  uint8_t state = ((port >> PIN_ENC_A_NRF) & 1) << 1 | ((port >> PIN_ENC_B_NRF) & 1);
  if (state == encoderPrevState) return;
  static const int8_t transitions[] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  int8_t delta = transitions[(encoderPrevState << 2) | state];
  if (delta == 0 && lastEncoderDir != 0) {
    // Missed an intermediate state (2-step jump) -- infer direction from last
    // known movement.  Common during fast rotation when polling is blocked by
    // I2C display updates or BLE radio events.
    delta = lastEncoderDir * 2;
  } else if (delta != 0) {
    lastEncoderDir = delta;
  }
  encoderPos += delta;
  encoderPrevState = state;
}

void encoderISR() {
  processEncoderState();
}

void pollEncoder() {
  noInterrupts();
  processEncoderState();
  interrupts();
}

#ifndef GHOST_SETTINGS_PURE_H
#define GHOST_SETTINGS_PURE_H

#include <stddef.h>
#include <stdint.h>

inline uint8_t ghost_xor_checksum_bytes(const uint8_t* p, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum ^= p[i];
  }
  return sum;
}

inline uint32_t ghost_clamp_u32(uint32_t value, uint32_t lo, uint32_t hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

#endif

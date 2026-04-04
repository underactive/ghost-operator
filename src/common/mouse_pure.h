#ifndef GHOST_MOUSE_PURE_H
#define GHOST_MOUSE_PURE_H

#include <stdint.h>
#include <math.h>

#ifndef GHOST_M_PI
#define GHOST_M_PI 3.14159265358979323846f
#endif

// Fixed-point 8-bit fractional to integer — round half away from zero.
// Fixes the systematic positive drift of the naive (fp + 128) >> 8 formula:
// that formula maps fp=-128 (exactly -0.5px) to 0 instead of -1.
inline int8_t mouse_fp8_round(int32_t fp) {
  if (fp >= 0) return (int8_t)((fp + 128) >> 8);
  return -(int8_t)((-fp + 128) >> 8);
}

// Single return-phase step: moves net displacement toward zero by at most 5 units.
// Guaranteed to converge: each call strictly reduces |net| until net == 0.
inline int8_t mouse_return_step(int32_t net) {
  if (net > 0) return -(int8_t)(net < 5 ? net : 5);
  if (net < 0) return  (int8_t)(-net < 5 ? -net : 5);
  return 0;
}

// Brownian sine-easing amplitude: ramps 0→peak→0 over progress [0,1].
inline float mouse_brownian_amp(float amplitude, float progress) {
  return amplitude * sinf(GHOST_M_PI * progress);
}

// Quadratic Bezier position in fixed-point space.
// P0/P1/P2 are fixed-point values; t is in [0,1].
inline int32_t mouse_bezier_eval(int32_t p0, int32_t p1, int32_t p2, float t) {
  float omt = 1.0f - t;
  return (int32_t)(omt * omt * (float)p0 + 2.0f * omt * t * (float)p1 + t * t * (float)p2);
}

#endif // GHOST_MOUSE_PURE_H

#include <unity.h>
#include "mouse_pure.h"

// ============================================================================
// mouse_fp8_round — fixed-point to int8, round half away from zero
// ============================================================================

void test_fp8_round_zero() {
  TEST_ASSERT_EQUAL_INT8(0, mouse_fp8_round(0));
}

void test_fp8_round_positive_full_pixel() {
  TEST_ASSERT_EQUAL_INT8(1, mouse_fp8_round(256));
  TEST_ASSERT_EQUAL_INT8(2, mouse_fp8_round(512));
}

void test_fp8_round_negative_full_pixel() {
  TEST_ASSERT_EQUAL_INT8(-1, mouse_fp8_round(-256));
  TEST_ASSERT_EQUAL_INT8(-2, mouse_fp8_round(-512));
}

void test_fp8_round_positive_half_rounds_up() {
  // Exactly 0.5px (128 in fixed-point) → rounds to 1
  TEST_ASSERT_EQUAL_INT8(1, mouse_fp8_round(128));
}

void test_fp8_round_negative_half_rounds_away_from_zero() {
  // Exactly -0.5px (-128 in fixed-point) → rounds to -1.
  // The old formula (fp + 128) >> 8 gave 0 here, causing systematic
  // positive drift whenever the path had negative sub-pixel deltas.
  TEST_ASSERT_EQUAL_INT8(-1, mouse_fp8_round(-128));
}

void test_fp8_round_just_below_half_truncates() {
  // 0.499px (127 in fixed-point) → rounds to 0
  TEST_ASSERT_EQUAL_INT8(0, mouse_fp8_round(127));
  // -0.499px → rounds to 0
  TEST_ASSERT_EQUAL_INT8(0, mouse_fp8_round(-127));
}

void test_fp8_round_one_and_half() {
  // 1.5px (384 in fixed-point) rounds to 2 in both directions
  TEST_ASSERT_EQUAL_INT8(2, mouse_fp8_round(384));
  TEST_ASSERT_EQUAL_INT8(-2, mouse_fp8_round(-384));
}

// ============================================================================
// mouse_bezier_eval — quadratic Bezier in fixed-point
// ============================================================================

void test_bezier_eval_at_t0_returns_p0() {
  // At t=0, B(0) = P0 regardless of other control points
  TEST_ASSERT_EQUAL_INT32(0,    mouse_bezier_eval(0, 1000, 2000, 0.0f));
  TEST_ASSERT_EQUAL_INT32(500,  mouse_bezier_eval(500, 1000, 2000, 0.0f));
  TEST_ASSERT_EQUAL_INT32(-300, mouse_bezier_eval(-300, 0, 300, 0.0f));
}

void test_bezier_eval_at_t1_returns_p2() {
  // At t=1, B(1) = P2 regardless of control point
  TEST_ASSERT_EQUAL_INT32(2000, mouse_bezier_eval(0, 1000, 2000, 1.0f));
  TEST_ASSERT_EQUAL_INT32(300,  mouse_bezier_eval(-300, 0, 300, 1.0f));
}

void test_bezier_straight_line_cumulative_sum() {
  // For P0=0, P1=1280, P2=2560 (dx=10px, midpoint control = straight line):
  // B(t) simplifies to 2560*t — uniform motion of exactly 256 fp-units per step.
  // Verifies that cumulative fp8_round calls sum to the intended pixel displacement.
  const int32_t p0 = 0, p1 = 1280, p2 = 2560;
  const int N = 10;
  int32_t prev = p0;
  int32_t total = 0;
  for (int i = 1; i <= N; i++) {
    float t = (float)i / (float)N;
    int32_t cur = mouse_bezier_eval(p0, p1, p2, t);
    total += mouse_fp8_round(cur - prev);
    prev = cur;
  }
  TEST_ASSERT_EQUAL_INT32(10, total);  // 10px endpoint
}

void test_bezier_negative_displacement_cumulative_sum() {
  // Same path mirrored: P2=-2560, expects sum=-10px
  const int32_t p0 = 0, p1 = -1280, p2 = -2560;
  const int N = 10;
  int32_t prev = p0;
  int32_t total = 0;
  for (int i = 1; i <= N; i++) {
    float t = (float)i / (float)N;
    int32_t cur = mouse_bezier_eval(p0, p1, p2, t);
    total += mouse_fp8_round(cur - prev);
    prev = cur;
  }
  TEST_ASSERT_EQUAL_INT32(-10, total);
}

// ============================================================================
// mouse_return_step — return phase convergence
// ============================================================================

void test_return_step_zero_net() {
  TEST_ASSERT_EQUAL_INT8(0, mouse_return_step(0));
}

void test_return_step_small_positive() {
  // net=1: step=-1, net reaches 0 in one step
  int8_t step = mouse_return_step(1);
  TEST_ASSERT_EQUAL_INT8(-1, step);
}

void test_return_step_small_negative() {
  int8_t step = mouse_return_step(-1);
  TEST_ASSERT_EQUAL_INT8(1, step);
}

void test_return_step_clamps_at_five() {
  // Any net >= 5 takes exactly 5 px toward zero
  TEST_ASSERT_EQUAL_INT8(-5, mouse_return_step(5));
  TEST_ASSERT_EQUAL_INT8(-5, mouse_return_step(100));
  TEST_ASSERT_EQUAL_INT8(5,  mouse_return_step(-5));
  TEST_ASSERT_EQUAL_INT8(5,  mouse_return_step(-100));
}

void test_return_step_converges_to_zero() {
  // Starting from net=500 must reach 0 in exactly 100 steps (500/5)
  int32_t net = 500;
  int steps = 0;
  while (net != 0) {
    net += mouse_return_step(net);
    steps++;
    TEST_ASSERT_TRUE_MESSAGE(steps <= 200, "return phase did not converge");
  }
  TEST_ASSERT_EQUAL_INT32(100, steps);
}

void test_return_step_converges_negative() {
  int32_t net = -500;
  int steps = 0;
  while (net != 0) {
    net += mouse_return_step(net);
    steps++;
    TEST_ASSERT_TRUE_MESSAGE(steps <= 200, "return phase did not converge");
  }
  TEST_ASSERT_EQUAL_INT32(100, steps);
}

void test_return_step_never_overshoots() {
  // Verify the step never carries net past zero
  for (int32_t start = -20; start <= 20; start++) {
    int32_t net = start;
    int guard = 0;
    while (net != 0 && guard++ < 100) {
      int8_t step = mouse_return_step(net);
      // Overshoot: sign of (net + step) matches sign of net when it shouldn't
      int32_t next = net + step;
      if (net > 0) TEST_ASSERT_TRUE(next >= 0);
      if (net < 0) TEST_ASSERT_TRUE(next <= 0);
      net = next;
    }
  }
}

// ============================================================================
// mouse_brownian_amp — sine easing amplitude profile
// ============================================================================

void test_brownian_amp_zero_at_start() {
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, mouse_brownian_amp(5.0f, 0.0f));
}

void test_brownian_amp_peak_at_midpoint() {
  // sin(π/2) = 1.0 exactly, so amp at progress=0.5 should equal amplitude
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 5.0f, mouse_brownian_amp(5.0f, 0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, mouse_brownian_amp(1.0f, 0.5f));
}

void test_brownian_amp_zero_at_end() {
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, mouse_brownian_amp(5.0f, 1.0f));
}

void test_brownian_amp_always_non_negative() {
  // Progress is always 0..1 in practice; allow tiny negative values from
  // floating-point rounding at the endpoint where sin(pi) is not exact.
  for (int i = 0; i <= 20; i++) {
    float progress = (float)i / 20.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f,
      0.0f,
      fminf(0.0f, mouse_brownian_amp(5.0f, progress)));
  }
}

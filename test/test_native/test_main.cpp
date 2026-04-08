#include <unity.h>

void setUp() {}
void tearDown() {}

void test_fp8_round_zero();
void test_fp8_round_positive_full_pixel();
void test_fp8_round_negative_full_pixel();
void test_fp8_round_positive_half_rounds_up();
void test_fp8_round_negative_half_rounds_away_from_zero();
void test_fp8_round_just_below_half_truncates();
void test_fp8_round_one_and_half();
void test_bezier_eval_at_t0_returns_p0();
void test_bezier_eval_at_t1_returns_p2();
void test_bezier_straight_line_cumulative_sum();
void test_bezier_negative_displacement_cumulative_sum();
void test_return_step_zero_net();
void test_return_step_small_positive();
void test_return_step_small_negative();
void test_return_step_clamps_at_five();
void test_return_step_converges_to_zero();
void test_return_step_converges_negative();
void test_return_step_never_overshoots();
void test_brownian_amp_zero_at_start();
void test_brownian_amp_peak_at_midpoint();
void test_brownian_amp_zero_at_end();
void test_brownian_amp_always_non_negative();
void test_ghost_clamp_u32();
void test_ghost_xor_checksum_bytes();

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_fp8_round_zero);
  RUN_TEST(test_fp8_round_positive_full_pixel);
  RUN_TEST(test_fp8_round_negative_full_pixel);
  RUN_TEST(test_fp8_round_positive_half_rounds_up);
  RUN_TEST(test_fp8_round_negative_half_rounds_away_from_zero);
  RUN_TEST(test_fp8_round_just_below_half_truncates);
  RUN_TEST(test_fp8_round_one_and_half);
  RUN_TEST(test_bezier_eval_at_t0_returns_p0);
  RUN_TEST(test_bezier_eval_at_t1_returns_p2);
  RUN_TEST(test_bezier_straight_line_cumulative_sum);
  RUN_TEST(test_bezier_negative_displacement_cumulative_sum);
  RUN_TEST(test_return_step_zero_net);
  RUN_TEST(test_return_step_small_positive);
  RUN_TEST(test_return_step_small_negative);
  RUN_TEST(test_return_step_clamps_at_five);
  RUN_TEST(test_return_step_converges_to_zero);
  RUN_TEST(test_return_step_converges_negative);
  RUN_TEST(test_return_step_never_overshoots);
  RUN_TEST(test_brownian_amp_zero_at_start);
  RUN_TEST(test_brownian_amp_peak_at_midpoint);
  RUN_TEST(test_brownian_amp_zero_at_end);
  RUN_TEST(test_brownian_amp_always_non_negative);
  RUN_TEST(test_ghost_clamp_u32);
  RUN_TEST(test_ghost_xor_checksum_bytes);

  return UNITY_END();
}

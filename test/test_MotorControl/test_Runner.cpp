#include <unity.h>

// Declarations for all tests across grouped files
// ProtocolBasicsAndMotion
void test_bad_cmd();
void test_bad_id();
void test_bad_param();
void test_move_out_of_range();
void test_all_addressing_and_status_awake();
void test_busy_rule_and_completion();
void test_home_defaults();
void test_help_format();
void test_status_format_lines();
void test_home_parsing_comma_skips();
void test_home_busy_rule_reject_when_moving();
void test_home_all_concurrency_and_post_state();
void test_help_includes_home_line_again();
void test_sleep_busy_then_ok();
void test_move_all_out_of_range();
void test_wake_sleep_single_and_status();
void test_home_with_params_acceptance();
void test_move_sets_speed_accel_in_status();

// StatusAndBudget
void test_status_includes_new_keys();
void test_budget_spend_and_refill_clamp();
void test_home_and_steps_since_home();
void test_budget_clamps_and_ttfc_non_negative();
void test_ttfc_clamp_and_recovery();
void test_homed_resets_on_reboot();
void test_steps_since_home_resets_after_second_home();

// Thermal
void test_help_includes_thermal_get_set();
void test_get_thermal_runtime_limiting_default_on_and_max_budget();

// KinematicsAndStub
void test_estimator_trapezoidal_matches_simple_formula();
void test_estimator_triangular_above_naive_bound();
void test_stub_move_uses_estimator_duration();
void test_stub_home_uses_estimator_duration();

// Hardware
void test_bitpack_dir_sleep_basic();
void test_backend_latch_before_start();
void test_backend_dir_bits_per_target();
void test_backend_wake_sleep_overrides();
void test_backend_busy_rule_overlapping_move();
void test_backend_dir_latched_once_per_move();
void test_backend_speed_accel_passed_to_adapter();

extern void setUp();
extern void tearDown();

int main(int, char**) {
  UNITY_BEGIN();

  // Protocol basics + motion/wake/sleep
  setUp(); RUN_TEST(test_bad_cmd);
  setUp(); RUN_TEST(test_bad_id);
  setUp(); RUN_TEST(test_bad_param);
  setUp(); RUN_TEST(test_move_out_of_range);
  setUp(); RUN_TEST(test_all_addressing_and_status_awake);
  setUp(); RUN_TEST(test_busy_rule_and_completion);
  setUp(); RUN_TEST(test_home_defaults);
  setUp(); RUN_TEST(test_help_format);
  setUp(); RUN_TEST(test_status_format_lines);
  setUp(); RUN_TEST(test_home_parsing_comma_skips);
  setUp(); RUN_TEST(test_home_busy_rule_reject_when_moving);
  setUp(); RUN_TEST(test_home_all_concurrency_and_post_state);
  setUp(); RUN_TEST(test_help_includes_home_line_again);
  setUp(); RUN_TEST(test_sleep_busy_then_ok);
  setUp(); RUN_TEST(test_move_all_out_of_range);
  setUp(); RUN_TEST(test_wake_sleep_single_and_status);
  setUp(); RUN_TEST(test_home_with_params_acceptance);
  setUp(); RUN_TEST(test_move_sets_speed_accel_in_status);

  // Status + budget
  setUp(); RUN_TEST(test_status_includes_new_keys);
  setUp(); RUN_TEST(test_budget_spend_and_refill_clamp);
  setUp(); RUN_TEST(test_home_and_steps_since_home);
  setUp(); RUN_TEST(test_budget_clamps_and_ttfc_non_negative);
  setUp(); RUN_TEST(test_homed_resets_on_reboot);
  setUp(); RUN_TEST(test_steps_since_home_resets_after_second_home);

  // Thermal flag GET/SET (and later: preflight/enforcement)
  setUp(); RUN_TEST(test_help_includes_thermal_get_set);
  setUp(); RUN_TEST(test_get_thermal_runtime_limiting_default_on_and_max_budget);

  // Kinematics/stub integration
  setUp(); RUN_TEST(test_estimator_trapezoidal_matches_simple_formula);
  setUp(); RUN_TEST(test_estimator_triangular_above_naive_bound);
  setUp(); RUN_TEST(test_stub_move_uses_estimator_duration);
  setUp(); RUN_TEST(test_stub_home_uses_estimator_duration);

  // Hardware + bitpack
  setUp(); RUN_TEST(test_bitpack_dir_sleep_basic);
  setUp(); RUN_TEST(test_backend_latch_before_start);
  setUp(); RUN_TEST(test_backend_dir_bits_per_target);
  setUp(); RUN_TEST(test_backend_wake_sleep_overrides);
  setUp(); RUN_TEST(test_backend_busy_rule_overlapping_move);
  setUp(); RUN_TEST(test_backend_dir_latched_once_per_move);
  setUp(); RUN_TEST(test_backend_speed_accel_passed_to_adapter);

  return UNITY_END();
}


#include <unity.h>
#include <PIDKilnLogic.h>

void test_eta_remaining(void) {
  PIDKiln_ProgramStep steps[3] = {
      {100, 10, 5},
      {200, 20, 0},
      {300, 1, 1},
  };
  /* from step 0, not dwell: 10+5 + 20+0 + 1+1 minutes */
  TEST_ASSERT_EQUAL_UINT32((10 + 5 + 20 + 0 + 1 + 1) * 60u,
                           PIDKiln_eta_remaining_seconds(steps, 0, 3, 0));
  /* dwell_only on step 0: skip first togo */
  TEST_ASSERT_EQUAL_UINT32((5 + 20 + 0 + 1 + 1) * 60u,
                           PIDKiln_eta_remaining_seconds(steps, 0, 3, 1));
  TEST_ASSERT_EQUAL_UINT32(0, PIDKiln_eta_remaining_seconds(steps, 3, 3, 0));
}

void test_step_capacity(void) {
  TEST_ASSERT_TRUE(PIDKiln_can_add_program_step(0, 40));
  TEST_ASSERT_FALSE(PIDKiln_can_add_program_step(40, 40));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_eta_remaining);
  RUN_TEST(test_step_capacity);
  return UNITY_END();
}

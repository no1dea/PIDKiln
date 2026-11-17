#include <unity.h>
#include <PIDKilnLogic.h>

void test_temp_limits(void) {
  TEST_ASSERT_TRUE(PIDKiln_temp_below_min(10.0, 20));
  TEST_ASSERT_FALSE(PIDKiln_temp_below_min(20.0, 20));
  TEST_ASSERT_TRUE(PIDKiln_temp_above_max(1351.0, 1350));
  TEST_ASSERT_FALSE(PIDKiln_temp_above_max(1350.0, 1350));
}

void test_housing_limit(void) {
  TEST_ASSERT_TRUE(PIDKiln_housing_above_max(101.0, 100));
  TEST_ASSERT_FALSE(PIDKiln_housing_above_max(100.0, 100));
}

void test_thermal_runaway_heating(void) {
  TEST_ASSERT_TRUE(PIDKiln_thermal_runaway_heating(0.1f, 200.0, 100.0));
  TEST_ASSERT_FALSE(PIDKiln_thermal_runaway_heating(0.0f, 200.0, 100.0));
  TEST_ASSERT_FALSE(PIDKiln_thermal_runaway_heating(0.1f, 104.0, 100.0));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_temp_limits);
  RUN_TEST(test_housing_limit);
  RUN_TEST(test_thermal_runaway_heating);
  return UNITY_END();
}

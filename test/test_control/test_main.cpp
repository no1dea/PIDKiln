#include <unity.h>
#include <PIDKilnLogic.h>
#include <string.h>

/* --- Pure helpers --- */

void test_ramp_temp_incr(void) {
  /* 100 → 200 over 100 min → 100°C / 6000 s = 1/60 °C/s */
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / 60.0f, PIDKiln_ramp_temp_incr(100.0, 200.0, 100));
  /* Cooling */
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f / 60.0f, PIDKiln_ramp_temp_incr(200.0, 100.0, 100));
  /* Instant jump */
  TEST_ASSERT_EQUAL_FLOAT(0.0f, PIDKiln_ramp_temp_incr(25.0, 900.0, 0));
  /* 25 → 85 over 1 min → 1 °C/s */
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, PIDKiln_ramp_temp_incr(25.0, 85.0, 1));
}

void test_threshold_band(void) {
  TEST_ASSERT_FALSE(PIDKiln_threshold_out_of_band(100.0, 105.0, -1));
  TEST_ASSERT_FALSE(PIDKiln_threshold_out_of_band(100.0, 0.0, 10));
  TEST_ASSERT_FALSE(PIDKiln_threshold_out_of_band(100.0, 105.0, 10)); /* in band */
  TEST_ASSERT_TRUE(PIDKiln_threshold_out_of_band(100.0, 120.0, 10));  /* kiln low */
  TEST_ASSERT_TRUE(PIDKiln_threshold_out_of_band(100.0, 80.0, 10));   /* kiln high */
  TEST_ASSERT_FALSE(PIDKiln_threshold_out_of_band(100.0, 110.0, 10)); /* edge: kiln+T == set */
}

void test_ssr_window(void) {
  const uint16_t win = 5000;
  /* 0% duty */
  TEST_ASSERT_FALSE(PIDKiln_ssr_on(0.0, win, 0, 0, 1));
  TEST_ASSERT_FALSE(PIDKiln_ssr_on(0.0, win, 0, 100, 1));
  /* 50%: on in first half of window */
  TEST_ASSERT_TRUE(PIDKiln_ssr_on(2500.0, win, 0, 0, 1));
  TEST_ASSERT_TRUE(PIDKiln_ssr_on(2500.0, win, 0, 2499, 1));
  TEST_ASSERT_FALSE(PIDKiln_ssr_on(2500.0, win, 0, 2500, 1)); /* boundary: > not >= */
  /* 100% */
  TEST_ASSERT_TRUE(PIDKiln_ssr_on(5000.0, win, 0, 4999, 1));
  TEST_ASSERT_FALSE(PIDKiln_ssr_on(5000.0, win, 0, 5000, 1));
  /* window == 0 → off */
  TEST_ASSERT_FALSE(PIDKiln_ssr_on(100.0, 0, 0, 0, 1));
}

void test_window_advance_catchup(void) {
  const uint16_t win = 5000;
  TEST_ASSERT_EQUAL_UINT32(0, PIDKiln_window_advance(0, 5000, win)); /* not > win */
  TEST_ASSERT_EQUAL_UINT32(5000, PIDKiln_window_advance(0, 5001, win));
  /* Large lag: catch up fully */
  TEST_ASSERT_EQUAL_UINT32(15000, PIDKiln_window_advance(0, 15001, win));
  TEST_ASSERT_EQUAL_UINT32(0, PIDKiln_window_advance(0, 100, 0));
}

void test_heat_percent(void) {
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, (float)PIDKiln_heat_percent(0.0, 5000, 1));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50.0f, (float)PIDKiln_heat_percent(2500.0, 5000, 1));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 100.0f, (float)PIDKiln_heat_percent(5000.0, 5000, 1));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, (float)PIDKiln_heat_percent(100.0, 0, 1));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 100.0f, (float)PIDKiln_heat_percent(50.0, 100, 2));
}

/* --- FSM helpers --- */

static void fsm_init(PIDKiln_ProgramFsm *f, const PIDKiln_ProgramStep *steps, unsigned n,
                     double kiln, int16_t threshold) {
  memset(f, 0, sizeof(*f));
  f->steps = steps;
  f->size = n;
  f->threshold = threshold;
  PIDKiln_program_fsm_start(f, kiln, 0);
}

static int run_one(PIDKiln_ProgramFsm *f, double kiln, uint32_t *t) {
  int rc = PIDKiln_program_fsm_tick(f, kiln, *t);
  (*t)++;
  return rc;
}

static void run_secs(PIDKiln_ProgramFsm *f, double kiln, uint32_t *t, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    run_one(f, kiln, t);
}

/* Run until the current ramp phase ends and dwell begins (snaps set_temp). */
static void run_until_dwell(PIDKiln_ProgramFsm *f, double kiln, uint32_t *t, unsigned max_s) {
  unsigned guard = 0;
  if (f->step < 0) {
    run_one(f, kiln, t);
    TEST_ASSERT_TRUE(f->next_is_dwell);
  }
  while (f->next_is_dwell && guard < max_s) {
    run_one(f, kiln, t);
    guard++;
  }
  TEST_ASSERT_TRUE(f->step >= 0);
  TEST_ASSERT_FALSE(f->next_is_dwell);
}

void test_fsm_single_ramp_then_dwell(void) {
  /* 25 → 85 in 1 min, dwell 1 min */
  PIDKiln_ProgramStep steps[] = {{85, 1, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 25.0, -1);

  int rc = run_one(&f, 25.0, &t);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_TICK_ETA_RAMP, rc);
  TEST_ASSERT_EQUAL(0, f.step);
  TEST_ASSERT_TRUE(f.next_is_dwell);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, f.temp_incr);
  /* First tick: set_temp = kiln + incr */
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 26.0f, (float)f.set_temp);

  /* Run through ramp; dwell snaps set_temp to target */
  run_until_dwell(&f, 25.0, &t, 120);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_RUNNING, f.state);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.temp_incr);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 85.0f, (float)f.set_temp);

  /* During dwell set_temp stays at target */
  float frozen = (float)f.set_temp;
  run_secs(&f, 85.0, &t, 20);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, frozen, (float)f.set_temp);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.temp_incr);
}

void test_fsm_togo_zero_instant_jump(void) {
  PIDKiln_ProgramStep steps[] = {{900, 0, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 25.0, -1);

  int rc = run_one(&f, 25.0, &t);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_TICK_ETA_RAMP, rc);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.temp_incr);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 900.0f, (float)f.set_temp);

  /* Still at target after more seconds (until dwell cadence) */
  run_secs(&f, 25.0, &t, 9);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 900.0f, (float)f.set_temp);

  run_until_dwell(&f, 900.0, &t, 30);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 900.0f, (float)f.set_temp);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.temp_incr);
}

void test_fsm_multi_step_no_overshoot_at_dwell(void) {
  /* Step0: 25→100 in 1 min; Step1: 100→200 in 1 min */
  PIDKiln_ProgramStep steps[] = {{100, 1, 1}, {200, 1, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 2, 25.0, -1);

  run_until_dwell(&f, 25.0, &t, 120);
  TEST_ASSERT_EQUAL(0, f.step);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 100.0f, (float)f.set_temp);

  /* Finish dwell → next ramp → next dwell */
  unsigned guard = 0;
  while (f.step == 0 && f.state != PIDKILN_FSM_ENDED && guard < 200) {
    run_one(&f, 100.0, &t);
    guard++;
  }
  TEST_ASSERT_EQUAL(1, f.step);
  TEST_ASSERT_TRUE(f.next_is_dwell);

  run_until_dwell(&f, 100.0, &t, 120);
  TEST_ASSERT_EQUAL(1, f.step);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 200.0f, (float)f.set_temp);
}

void test_fsm_cooling_ramp(void) {
  PIDKiln_ProgramStep steps[] = {{50, 1, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 110.0, -1);

  run_one(&f, 110.0, &t);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, f.temp_incr);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 109.0f, (float)f.set_temp);

  run_until_dwell(&f, 50.0, &t, 120);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 50.0f, (float)f.set_temp);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.temp_incr);
}

void test_fsm_program_end(void) {
  PIDKiln_ProgramStep steps[] = {{80, 0, 0}}; /* instant + zero dwell */
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 25.0, -1);

  run_one(&f, 25.0, &t); /* ramp togo=0 */
  /* Next cadence: dwell with dwell=0 → next_end = now */
  run_secs(&f, 80.0, &t, 9);
  int rc = run_one(&f, 80.0, &t); /* cadence → dwell */
  TEST_ASSERT_EQUAL(PIDKILN_FSM_TICK_ETA_DWELL, rc);
  TEST_ASSERT_FALSE(f.next_is_dwell);

  /* Next cadence: dwell done → step++ → ended */
  run_secs(&f, 80.0, &t, 9);
  rc = run_one(&f, 80.0, &t);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_TICK_ENDED, rc);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_ENDED, f.state);
}

void test_fsm_threshold_freeze_and_resume(void) {
  PIDKiln_ProgramStep steps[] = {{85, 1, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 25.0, 5);

  run_one(&f, 25.0, &t); /* start ramp; set≈26, kiln=25 → still in band (±5) */
  TEST_ASSERT_EQUAL(PIDKILN_FSM_RUNNING, f.state);

  /* Advance setpoint far ahead of kiln by running with kiln stuck low, but
   * first leave band: jump kiln view by waiting until set is high, then check
   * on cadence with kiln still at 25. */
  run_secs(&f, 25.0, &t, 40); /* set_temp climbs; kiln fixed at 25 */
  /* Force cadence evaluation: align to next multiple of 10 */
  while (t % 10 != 0)
    run_one(&f, 25.0, &t);

  /* If not yet threshold (band still ok), keep climbing until out of band */
  unsigned guard = 0;
  while (f.state != PIDKILN_FSM_THRESHOLD && guard < 100) {
    run_one(&f, 25.0, &t);
    guard++;
  }
  TEST_ASSERT_EQUAL(PIDKILN_FSM_THRESHOLD, f.state);

  uint32_t end_before = f.next_step_end_s;
  float frozen = (float)f.set_temp;
  /* 20 more seconds while out of band — set_temp frozen, schedule stretches */
  run_secs(&f, 25.0, &t, 20);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_THRESHOLD, f.state);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, frozen, (float)f.set_temp);
  TEST_ASSERT_TRUE(f.next_step_end_s > end_before);

  /* Bring kiln into band; next cadence should clear threshold */
  guard = 0;
  while (f.state == PIDKILN_FSM_THRESHOLD && guard < 20) {
    run_one(&f, (double)frozen, &t);
    guard++;
  }
  TEST_ASSERT_EQUAL(PIDKILN_FSM_RUNNING, f.state);

  /* Resume climbing without snapping backwards */
  float after = (float)f.set_temp;
  run_secs(&f, (double)frozen, &t, 5);
  TEST_ASSERT_TRUE((float)f.set_temp >= after - 1e-3f);
}

void test_fsm_pause_freeze_and_resume(void) {
  PIDKiln_ProgramStep steps[] = {{85, 1, 1}};
  PIDKiln_ProgramFsm f;
  uint32_t t = 0;
  fsm_init(&f, steps, 1, 25.0, -1);

  run_secs(&f, 25.0, &t, 15);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_RUNNING, f.state);
  float frozen = (float)f.set_temp;
  uint32_t end_before = f.next_step_end_s;

  f.state = PIDKILN_FSM_PAUSED;
  run_secs(&f, 25.0, &t, 20);
  TEST_ASSERT_EQUAL(PIDKILN_FSM_PAUSED, f.state);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, frozen, (float)f.set_temp);
  TEST_ASSERT_TRUE(f.next_step_end_s >= end_before);

  f.state = PIDKILN_FSM_RUNNING;
  run_secs(&f, 25.0, &t, 5);
  TEST_ASSERT_TRUE((float)f.set_temp >= frozen - 1e-3f);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_ramp_temp_incr);
  RUN_TEST(test_threshold_band);
  RUN_TEST(test_ssr_window);
  RUN_TEST(test_window_advance_catchup);
  RUN_TEST(test_heat_percent);
  RUN_TEST(test_fsm_single_ramp_then_dwell);
  RUN_TEST(test_fsm_togo_zero_instant_jump);
  RUN_TEST(test_fsm_multi_step_no_overshoot_at_dwell);
  RUN_TEST(test_fsm_cooling_ramp);
  RUN_TEST(test_fsm_program_end);
  RUN_TEST(test_fsm_threshold_freeze_and_resume);
  RUN_TEST(test_fsm_pause_freeze_and_resume);
  return UNITY_END();
}

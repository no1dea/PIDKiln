#include <unity.h>
#include <stdio.h>
#include <string.h>

#include <PIDKilnLogic.h>

static void test_valid_filename_accepts_safe_names(void) {
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("program1.txt"));
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("test_up_down.txt"));
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("A.B_c123"));
}

static void test_valid_filename_rejects_unsafe_names(void) {
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("bad name.txt"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("../etc/passwd"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("prog;rm.txt"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename(""));
}

static void test_check_valid_chars(void) {
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('A'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('z'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('0'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars(':'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('\n'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('_'));
  TEST_ASSERT_FALSE(PIDKiln_check_valid_chars((unsigned char)7));
  TEST_ASSERT_FALSE(PIDKiln_check_valid_chars((unsigned char)200));
}

static void test_parse_program_line_ok(void) {
  PIDKiln_ProgramStep step;
  TEST_ASSERT_EQUAL_INT(0, PIDKiln_parse_program_line("95:30:20", 1350, &step));
  TEST_ASSERT_EQUAL_UINT16(95, step.temp);
  TEST_ASSERT_EQUAL_UINT16(30, step.togo);
  TEST_ASSERT_EQUAL_UINT16(20, step.dwell);

  TEST_ASSERT_EQUAL_INT(0, PIDKiln_parse_program_line("970:120:50", 1350, &step));
  TEST_ASSERT_EQUAL_UINT16(970, step.temp);
  TEST_ASSERT_EQUAL_UINT16(120, step.togo);
  TEST_ASSERT_EQUAL_UINT16(50, step.dwell);

  TEST_ASSERT_EQUAL_INT(0, PIDKiln_parse_program_line("600:90:0", 1350, &step));
  TEST_ASSERT_EQUAL_UINT16(600, step.temp);
  TEST_ASSERT_EQUAL_UINT16(90, step.togo);
  TEST_ASSERT_EQUAL_UINT16(0, step.dwell);
}

static void test_parse_program_line_errors(void) {
  PIDKiln_ProgramStep step;
  TEST_ASSERT_EQUAL_INT(3, PIDKiln_parse_program_line("95:30:x", 1350, &step));
  TEST_ASSERT_EQUAL_INT(3, PIDKiln_parse_program_line("95-30-20", 1350, &step));
  TEST_ASSERT_EQUAL_INT(4, PIDKiln_parse_program_line("1400:10:0", 1350, &step));
}

static void test_inline_comment_pos(void) {
  TEST_ASSERT_EQUAL_INT(0, PIDKiln_inline_comment_pos("# full line comment"));
  TEST_ASSERT_EQUAL_INT(9, PIDKiln_inline_comment_pos("95:30:20 # dwell"));
  TEST_ASSERT_EQUAL_INT(-1, PIDKiln_inline_comment_pos("95:30:20"));
}

static void test_can_add_program_step(void) {
  TEST_ASSERT_TRUE(PIDKiln_can_add_program_step(0, 40));
  TEST_ASSERT_TRUE(PIDKiln_can_add_program_step(39, 40));
  TEST_ASSERT_FALSE(PIDKiln_can_add_program_step(40, 40));
  TEST_ASSERT_FALSE(PIDKiln_can_add_program_step(41, 40));
}

static void test_parse_sample_program1_steps(void) {
  /* Steps from data/programs/program1.txt (comment lines ignored by loader). */
  const char *steps[] = {
      "95:30:20",  "134:20:30", "97:80:30", "360:180:20",
      "600:100:30", "970:120:50", "600:90:0",
  };
  PIDKiln_ProgramStep step;
  for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
    char msg[64];
    snprintf(msg, sizeof(msg), "step %u: %s", (unsigned)i, steps[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, PIDKiln_parse_program_line(steps[i], 1350, &step), msg);
  }
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_filename_accepts_safe_names);
  RUN_TEST(test_valid_filename_rejects_unsafe_names);
  RUN_TEST(test_check_valid_chars);
  RUN_TEST(test_parse_program_line_ok);
  RUN_TEST(test_parse_program_line_errors);
  RUN_TEST(test_inline_comment_pos);
  RUN_TEST(test_can_add_program_step);
  RUN_TEST(test_parse_sample_program1_steps);
  return UNITY_END();
}

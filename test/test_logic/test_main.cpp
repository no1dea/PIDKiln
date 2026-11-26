#include <unity.h>
#include <stdio.h>
#include <string.h>

#include <PIDKilnLogic.h>

static void test_valid_filename_accepts_safe_names(void) {
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("program1.json"));
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("test_up_down.json"));
  TEST_ASSERT_TRUE(PIDKiln_valid_filename("A.B_c123"));
}

static void test_valid_filename_rejects_unsafe_names(void) {
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("bad name.json"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("../etc/passwd"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename("prog;rm.json"));
  TEST_ASSERT_FALSE(PIDKiln_valid_filename(""));
}

static void test_check_valid_chars(void) {
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('A'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('z'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('0'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('{'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('}'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('['));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars(']'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('"'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('\n'));
  TEST_ASSERT_TRUE(PIDKiln_check_valid_chars('_'));
  TEST_ASSERT_FALSE(PIDKiln_check_valid_chars((unsigned char)7));
  TEST_ASSERT_FALSE(PIDKiln_check_valid_chars((unsigned char)200));
}

static void test_rate_to_togo(void) {
  TEST_ASSERT_EQUAL_UINT16(30, PIDKiln_rate_to_togo_min(20, 95, 150));
  TEST_ASSERT_EQUAL_UINT16(1, PIDKiln_rate_to_togo_min(20, 95, 9999));
  TEST_ASSERT_EQUAL_UINT16(1, PIDKiln_rate_to_togo_min(100, 100, 120));
  TEST_ASSERT_EQUAL_UINT16(0, PIDKiln_rate_to_togo_min(20, 95, 0));
}

static void test_parse_json_ramp_min(void) {
  const char *json = "{"
                     "\"schema\":\"pidkiln.program/v1\","
                     "\"description\":\"unit test\","
                     "\"steps\":[{\"target_c\":95,\"ramp_min\":30,\"hold_min\":20},"
                     "{\"target_c\":134,\"ramp_min\":20,\"hold_min\":30}]"
                     "}";
  PIDKiln_ProgramParsed out;
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_OK,
                        PIDKiln_parse_program_json(json, strlen(json), 1350, NULL, &out));
  TEST_ASSERT_EQUAL_UINT(2, out.size);
  TEST_ASSERT_EQUAL_UINT16(95, out.steps[0].temp);
  TEST_ASSERT_EQUAL_UINT16(30, out.steps[0].togo);
  TEST_ASSERT_EQUAL_UINT16(20, out.steps[0].dwell);
  TEST_ASSERT_EQUAL_UINT16(0, out.rate_c_per_h[0]);
  TEST_ASSERT_EQUAL_UINT16(134, out.steps[1].temp);
  TEST_ASSERT_EQUAL_UINT16(20, out.steps[1].togo);
  TEST_ASSERT_EQUAL_UINT16(0, out.rate_c_per_h[1]);
}

static void test_parse_json_rate_with_assume_start(void) {
  const char *json = "{"
                     "\"schema\":\"pidkiln.program/v1\","
                     "\"assume_start_c\":25,"
                     "\"steps\":[{\"target_c\":45,\"rate_c_per_h\":240,\"hold_min\":5},"
                     "{\"target_c\":30,\"rate_c_per_h\":180,\"hold_min\":5}]"
                     "}";
  PIDKiln_ProgramParsed out;
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_OK,
                        PIDKiln_parse_program_json(json, strlen(json), 1350, NULL, &out));
  TEST_ASSERT_EQUAL_UINT(2, out.size);
  TEST_ASSERT_TRUE(out.has_assume_start);
  TEST_ASSERT_EQUAL_INT16(25, out.assume_start_c);
  /* |45-25|/240*60 = 5 min */
  TEST_ASSERT_EQUAL_UINT16(5, out.steps[0].togo);
  TEST_ASSERT_EQUAL_UINT16(240, out.rate_c_per_h[0]);
  /* |30-45|/180*60 = 5 min */
  TEST_ASSERT_EQUAL_UINT16(5, out.steps[1].togo);
  TEST_ASSERT_EQUAL_UINT16(180, out.rate_c_per_h[1]);
}

static void test_parse_json_rate_step0_unresolved_without_start(void) {
  const char *json = "{"
                     "\"schema\":\"pidkiln.program/v1\","
                     "\"steps\":[{\"target_c\":45,\"rate_c_per_h\":240,\"hold_min\":5}]"
                     "}";
  PIDKiln_ProgramParsed out;
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_OK,
                        PIDKiln_parse_program_json(json, strlen(json), 1350, NULL, &out));
  TEST_ASSERT_EQUAL_UINT16(0, out.steps[0].togo);
  TEST_ASSERT_EQUAL_UINT16(240, out.rate_c_per_h[0]);

  PIDKiln_resolve_rate_step_togo(&out.steps[0], out.rate_c_per_h[0], 25);
  TEST_ASSERT_EQUAL_UINT16(5, out.steps[0].togo);
}

static void test_parse_json_rate_with_measured_start(void) {
  const char *json = "{"
                     "\"schema\":\"pidkiln.program/v1\","
                     "\"assume_start_c\":99,"
                     "\"steps\":[{\"target_c\":45,\"rate_c_per_h\":240,\"hold_min\":5}]"
                     "}";
  int measured = 25;
  PIDKiln_ProgramParsed out;
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_OK,
                        PIDKiln_parse_program_json(json, strlen(json), 1350, &measured, &out));
  /* measured wins over assume_start_c */
  TEST_ASSERT_EQUAL_UINT16(5, out.steps[0].togo);
}

static void test_parse_json_rejects_both_or_neither_ramp(void) {
  PIDKiln_ProgramParsed out;
  const char *both = "{\"schema\":\"pidkiln.program/v1\",\"steps\":["
                     "{\"target_c\":95,\"ramp_min\":30,\"rate_c_per_h\":100,\"hold_min\":20}]}";
  const char *neither = "{\"schema\":\"pidkiln.program/v1\",\"steps\":["
                        "{\"target_c\":95,\"hold_min\":20}]}";
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_ERR_STEP,
                        PIDKiln_parse_program_json(both, strlen(both), 1350, NULL, &out));
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_ERR_STEP,
                        PIDKiln_parse_program_json(neither, strlen(neither), 1350, NULL, &out));
}

static void test_parse_json_rejects_hot_and_bad_schema(void) {
  PIDKiln_ProgramParsed out;
  const char *hot = "{\"schema\":\"pidkiln.program/v1\",\"steps\":["
                    "{\"target_c\":1360,\"ramp_min\":10,\"hold_min\":0}]}";
  const char *bad =
      "{\"schema\":\"other\",\"steps\":[{\"target_c\":10,\"ramp_min\":1,\"hold_min\":0}]}";
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_ERR_TEMP,
                        PIDKiln_parse_program_json(hot, strlen(hot), 1350, NULL, &out));
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_ERR_JSON,
                        PIDKiln_parse_program_json(bad, strlen(bad), 1350, NULL, &out));
}

static void test_parse_sample_program1_json(void) {
  const char *json = "{"
                     "\"schema\":\"pidkiln.program/v1\","
                     "\"name\":\"program1\","
                     "\"description\":\"Short, clear description of the program.\","
                     "\"steps\":["
                     "{\"target_c\":95,\"ramp_min\":30,\"hold_min\":20},"
                     "{\"target_c\":134,\"rate_c_per_h\":117,\"hold_min\":30},"
                     "{\"target_c\":97,\"ramp_min\":80,\"hold_min\":30},"
                     "{\"target_c\":360,\"ramp_min\":180,\"hold_min\":20},"
                     "{\"target_c\":600,\"ramp_min\":100,\"hold_min\":30},"
                     "{\"target_c\":970,\"ramp_min\":120,\"hold_min\":50},"
                     "{\"target_c\":600,\"ramp_min\":90,\"hold_min\":0}"
                     "]}";
  PIDKiln_ProgramParsed out;
  int start = 20;
  TEST_ASSERT_EQUAL_INT(PIDKILN_PRG_OK,
                        PIDKiln_parse_program_json(json, strlen(json), 1350, &start, &out));
  TEST_ASSERT_EQUAL_UINT(7, out.size);
  TEST_ASSERT_EQUAL_UINT16(0, out.rate_c_per_h[0]);
  TEST_ASSERT_EQUAL_UINT16(117, out.rate_c_per_h[1]);
  /* |134-95|/117*60 ≈ 20 min */
  TEST_ASSERT_EQUAL_UINT16(20, out.steps[1].togo);
}

static void test_inline_comment_pos(void) {
  TEST_ASSERT_EQUAL_INT(0, PIDKiln_inline_comment_pos("# full line comment"));
  TEST_ASSERT_EQUAL_INT(9, PIDKiln_inline_comment_pos("95:30:20 # dwell"));
  TEST_ASSERT_EQUAL_INT(-1, PIDKiln_inline_comment_pos("95:30:20"));
}

static void test_prefs_split_kv(void) {
  char key[64], val[64];
  TEST_ASSERT_TRUE(
      PIDKiln_prefs_split_kv("Auth_Username = admin", key, sizeof(key), val, sizeof(val)));
  TEST_ASSERT_EQUAL_STRING("Auth_Username", key);
  TEST_ASSERT_EQUAL_STRING("admin", val);
  TEST_ASSERT_TRUE(PIDKiln_prefs_split_kv("Auth_Password=", key, sizeof(key), val, sizeof(val)));
  TEST_ASSERT_EQUAL_STRING("Auth_Password", key);
  TEST_ASSERT_EQUAL_STRING("", val);
  TEST_ASSERT_TRUE(PIDKiln_prefs_split_kv("WiFi_Mode=1", key, sizeof(key), val, sizeof(val)));
  TEST_ASSERT_EQUAL_STRING("WiFi_Mode", key);
  TEST_ASSERT_EQUAL_STRING("1", val);
  TEST_ASSERT_FALSE(PIDKiln_prefs_split_kv("noequals", key, sizeof(key), val, sizeof(val)));
  TEST_ASSERT_FALSE(PIDKiln_prefs_split_kv("=novaluekey", key, sizeof(key), val, sizeof(val)));
}

static void test_can_add_program_step(void) {
  TEST_ASSERT_TRUE(PIDKiln_can_add_program_step(0, 40));
  TEST_ASSERT_TRUE(PIDKiln_can_add_program_step(39, 40));
  TEST_ASSERT_FALSE(PIDKiln_can_add_program_step(40, 40));
  TEST_ASSERT_FALSE(PIDKiln_can_add_program_step(41, 40));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_valid_filename_accepts_safe_names);
  RUN_TEST(test_valid_filename_rejects_unsafe_names);
  RUN_TEST(test_check_valid_chars);
  RUN_TEST(test_rate_to_togo);
  RUN_TEST(test_parse_json_ramp_min);
  RUN_TEST(test_parse_json_rate_with_assume_start);
  RUN_TEST(test_parse_json_rate_step0_unresolved_without_start);
  RUN_TEST(test_parse_json_rate_with_measured_start);
  RUN_TEST(test_parse_json_rejects_both_or_neither_ramp);
  RUN_TEST(test_parse_json_rejects_hot_and_bad_schema);
  RUN_TEST(test_parse_sample_program1_json);
  RUN_TEST(test_inline_comment_pos);
  RUN_TEST(test_prefs_split_kv);
  RUN_TEST(test_can_add_program_step);
  return UNITY_END();
}

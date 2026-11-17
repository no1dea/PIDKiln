#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
} PIDKiln_ProgramStep;

/* Same charset as firmware MAX_FILENAME / upload rules. */
extern const char PIDKILN_ALLOWED_FILENAME_CHARS[];

/* 1 = ok, 0 = reject */
int PIDKiln_check_valid_chars(unsigned char a);
int PIDKiln_valid_filename(const char *file);

/*
 * Parse "temp:togo:dwell" (digits and ':' only).
 * Returns 0 on success, 3 = bad chars, 4 = temp > max_temp.
 */
int PIDKiln_parse_program_line(const char *line, uint16_t max_temp, PIDKiln_ProgramStep *out);

/* Index of first '#' for inline comments, or -1 if none. Leading '#' is index 0. */
int PIDKiln_inline_comment_pos(const char *line);

/* 1 if another program step may be stored, 0 if at capacity. */
int PIDKiln_can_add_program_step(unsigned size, unsigned max_steps);

/* Safety predicates (pure; used by firmware SAFETY_Check and host tests). */
int PIDKiln_temp_below_min(double kiln_temp, uint8_t min_temp);
int PIDKiln_temp_above_max(double kiln_temp, uint16_t max_temp);
int PIDKiln_housing_above_max(double case_temp, uint16_t max_housing);
/* 1 when ramping up with setpoint at least 5C above kiln temp. */
int PIDKiln_thermal_runaway_heating(float temp_incr, double set_temp, double kiln_temp);

/* ETA: seconds to add for remaining steps from run_step (dwell_only skips first togo). */
uint32_t PIDKiln_eta_remaining_seconds(const PIDKiln_ProgramStep *steps, unsigned run_step,
                                       unsigned run_size, int dwell_only);

#ifdef __cplusplus
}
#endif

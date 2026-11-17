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

#ifdef __cplusplus
}
#endif

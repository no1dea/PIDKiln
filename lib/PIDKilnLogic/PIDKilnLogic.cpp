#include "PIDKilnLogic.h"

#include <string.h>

const char PIDKILN_ALLOWED_FILENAME_CHARS[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890._";

int PIDKiln_check_valid_chars(unsigned char a) {
  if (a == 0 || a == 9 || a == 95) return 1; // EOF, tab, _
  if (a == 10 || a == 13) return 1;          // LF, CR
  if (a >= 32 && a <= 90) return 1;
  if (a >= 97 && a <= 122) return 1;
  return 0;
}

int PIDKiln_valid_filename(const char *file) {
  if (!file || !*file) return 0;
  char c;
  while ((c = *file++)) {
    if (strchr(PIDKILN_ALLOWED_FILENAME_CHARS, c)) continue;
    return 0;
  }
  return 1;
}

int PIDKiln_parse_program_line(const char *line, uint16_t max_temp, PIDKiln_ProgramStep *out) {
  uint16_t prg[3];
  uint8_t pos = 2;
  int multi = 1;
  size_t a;

  if (!line || !out) return 3;

  a = strlen(line);
  prg[0] = prg[1] = prg[2] = 0;

  while (a--) {
    unsigned char ch = (unsigned char)line[a];
    if (ch == '\0') continue;
    if (ch < 48 || ch > 58) return 3; // not digit or ':'
    if (ch == 58) {                   // ':'
      multi = 1;
      pos--;
    } else {
      prg[pos] = (uint16_t)(prg[pos] + multi * (ch - 48));
      multi *= 10;
    }
  }

  if (prg[0] > max_temp) return 4;

  out->temp = prg[0];
  out->togo = prg[1];
  out->dwell = prg[2];
  return 0;
}

int PIDKiln_inline_comment_pos(const char *line) {
  if (!line) return -1;
  const char *p = strchr(line, '#');
  if (!p) return -1;
  return (int)(p - line);
}

int PIDKiln_can_add_program_step(unsigned size, unsigned max_steps) {
  return (size < max_steps) ? 1 : 0;
}

int PIDKiln_temp_below_min(double kiln_temp, uint8_t min_temp) {
  return (kiln_temp < (double)min_temp) ? 1 : 0;
}

int PIDKiln_temp_above_max(double kiln_temp, uint16_t max_temp) {
  return (kiln_temp > (double)max_temp) ? 1 : 0;
}

int PIDKiln_housing_above_max(double case_temp, uint16_t max_housing) {
  return (case_temp > (double)max_housing) ? 1 : 0;
}

int PIDKiln_thermal_runaway_heating(float temp_incr, double set_temp, double kiln_temp) {
  return (temp_incr > 0.0f && set_temp >= (kiln_temp + 5.0)) ? 1 : 0;
}

uint32_t PIDKiln_eta_remaining_seconds(const PIDKiln_ProgramStep *steps, unsigned run_step,
                                       unsigned run_size, int dwell_only) {
  uint32_t sec = 0;
  int skip_togo = dwell_only ? 1 : 0;
  if (!steps || run_step >= run_size)
    return 0;
  for (unsigned a = run_step; a < run_size; a++) {
    if (!skip_togo)
      sec += (uint32_t)steps[a].togo * 60u;
    else
      skip_togo = 0;
    sec += (uint32_t)steps[a].dwell * 60u;
  }
  return sec;
}

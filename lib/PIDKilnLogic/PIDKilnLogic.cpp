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

/*
** Wall-clock bootstrap without requiring WiFi
*/
#include "PIDKiln.h"
#include <sys/time.h>

void Setup_start_date() {
  struct timeval tv;
  struct tm mytm;
  char *tmp, msg[20];

  strcpy(msg, Prefs[PRF_INIT_DATE].value.str);
  tmp = strtok(msg, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] Y:%s ", tmp);
  mytm.tm_year = atoi(tmp) - 1900;

  tmp = strtok(NULL, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] M:%s ", tmp);
  mytm.tm_mon = atoi(tmp) - 1;

  tmp = strtok(NULL, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] D:%s ", tmp);
  mytm.tm_mday = atoi(tmp);

  strcpy(msg, Prefs[PRF_INIT_TIME].value.str);
  tmp = strtok(msg, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] H:%s ", tmp);
  mytm.tm_hour = atoi(tmp);

  tmp = strtok(NULL, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] m:%s ", tmp);
  mytm.tm_min = atoi(tmp);

  tmp = strtok(NULL, ".-:");
  DBG dbgLog(LOG_INFO, "[TIME] s:%s\n", tmp);
  mytm.tm_sec = atoi(tmp);

  time_t t = mktime(&mytm);
  tv.tv_sec = t;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

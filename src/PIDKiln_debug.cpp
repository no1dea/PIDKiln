#include "PIDKiln.h"
#include <stdarg.h>

void dbgLog(uint16_t pri, const char *fmt, ...) {
  if (!Prefs[PRF_DBG_SERIAL].value.uint8 && !Prefs[PRF_DBG_SYSLOG].value.uint8)
    return;

  char *message;
  va_list args;
  size_t initialLen;
  size_t len;

  initialLen = strlen(fmt);
  message = new char[initialLen + 1];

  va_start(args, fmt);
  len = vsnprintf(message, initialLen + 1, fmt, args);
  if (len > initialLen) {
    delete[] message;
    message = new char[len + 1];
    vsnprintf(message, len + 1, fmt, args);
  }
  va_end(args);

  if (Prefs[PRF_DBG_SERIAL].value.uint8)
    Serial.print(message);
#if PK_FEATURE_SYSLOG
  if (Prefs[PRF_DBG_SYSLOG].value.uint8)
    syslog.log(pri, message);
#else
  (void)pri;
#endif

  delete[] message;
}

void initSerial() {
  if (Prefs[PRF_DBG_SERIAL].value.uint8)
    Serial.begin(115200);
}

void Restart_ESP() { ESP.restart(); }

#if PK_FEATURE_SYSLOG
void initSysLog() {
  if (Prefs[PRF_DBG_SYSLOG].value.uint8) {
    DBG dbgLog(LOG_DEBUG, "[LOG] Trying to enable Syslog\n");
    if (!strlen(Prefs[PRF_SYSLOG_SRV].value.str) ||
        !Prefs[PRF_SYSLOG_PORT].value.uint16) {
      Prefs[PRF_DBG_SYSLOG].value.uint8 = 0;
      DBG dbgLog(LOG_ERR,
                 "[LOG] Syslog enabled but not configured - disabling\n");
      return;
    }
    syslog.server(Prefs[PRF_SYSLOG_SRV].value.str,
                  Prefs[PRF_SYSLOG_PORT].value.uint16);
    syslog.deviceHostname("PIDKiln-ESP32");
    syslog.appName("PIDKiln");
    syslog.defaultPriority(LOG_KERN);
    syslog.log(LOG_INFO, "Begin syslog");
    DBG dbgLog(LOG_DEBUG, "[LOG] Syslog enabled\n");
  } else {
    DBG dbgLog(LOG_DEBUG, "[LOG] Syslog disabled\n");
  }
}
#endif

#include "PIDKiln.h"

void STOP_Alarm() {
  ALARM_countdown = 0;
  digitalWrite(ALARM_PIN, LOW);
}

void START_Alarm() {
  if (!Prefs[PRF_ALARM_TIMEOUT].value.uint16)
    return;
  ALARM_countdown = Prefs[PRF_ALARM_TIMEOUT].value.uint16;
  digitalWrite(ALARM_PIN, HIGH);
}

void Setup_Alarm() { pinMode(ALARM_PIN, OUTPUT); }

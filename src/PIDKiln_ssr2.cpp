#include "PIDKiln.h"

void Kiln_SSR2_Set(boolean on) {
  digitalWrite(SSR2_RELAY_PIN, on ? HIGH : LOW);
}

void Setup_SSR2() { pinMode(SSR2_RELAY_PIN, OUTPUT); }

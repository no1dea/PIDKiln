#include "PIDKiln.h"

void Enable_EMR() { digitalWrite(EMR_RELAY_PIN, HIGH); }

void Disable_EMR() { digitalWrite(EMR_RELAY_PIN, LOW); }

void Setup_EMR() { pinMode(EMR_RELAY_PIN, OUTPUT); }

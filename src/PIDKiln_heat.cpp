/*
** Core heater path: SSR1 + primary MAX31855
*/
#include <Adafruit_MAX31855.h>
#include <math.h>
#include "PIDKiln.h"

SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 ThermocoupleA(MAXCS1, ESP32_SPI);

boolean SSR_On;

uint16_t Energy_Wattage = 0;
double Energy_Usage = 0;

void Enable_SSR() {
  if (!SSR_On) {
    digitalWrite(SSR1_RELAY_PIN, HIGH);
    Kiln_SSR2_Set(true);
    SSR_On = true;
  }
}

void Disable_SSR() {
  if (SSR_On) {
    digitalWrite(SSR1_RELAY_PIN, LOW);
    Kiln_SSR2_Set(false);
    SSR_On = false;
  }
}

void Kiln_SetSSR(boolean on) {
  if (on)
    Enable_SSR();
  else
    Disable_SSR();
}

static void Handle_ThermocoupleA_Fault(uint8_t err) {
  if (err & MAX31855_FAULT_SHORT_VCC) {
    DBG dbgLog(LOG_ERR, "[HEAT] ThermocoupleA short to VCC\n");
  } else if (err & MAX31855_FAULT_SHORT_GND) {
    DBG dbgLog(LOG_ERR, "[HEAT] ThermocoupleA short to GND\n");
  } else if (err & MAX31855_FAULT_OPEN) {
    DBG dbgLog(LOG_ERR, "[HEAT] ThermocoupleA not connected\n");
  } else {
    DBG dbgLog(LOG_ERR, "[HEAT] ThermocoupleA unknown error, check spi cable\n");
  }

  if (TempA_errors < Prefs[PRF_ERROR_GRACE_COUNT].value.uint8) {
    TempA_errors++;
    DBG dbgLog(LOG_ERR, "[HEAT] ThermocoupleA error below grace threshold %d of %d\n",
               TempA_errors, Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
  } else {
    ABORT_Program(PR_ERR_MAX31A_INT_ERR);
  }
}

void Update_TemperatureA() {
  double kiln_tmp1;
  double int_tmp1;
  uint8_t err;

  int_tmp1 = ThermocoupleA.readInternal();
  kiln_tmp1 = ThermocoupleA.readCelsius();

  if (isnan(kiln_tmp1)) {
    err = ThermocoupleA.readError();
    if (err == 0 || err == MAX31855_FAULT_ALL) {
      DBG dbgLog(LOG_ERR, "[HEAT] MAX31855 ThermocoupleA did not respond\n");
      ABORT_Program(PR_ERR_MAX31A_NC);
      return;
    }
    Handle_ThermocoupleA_Fault(err);
    return;
  }

  int_temp = (int_temp + int_tmp1) / 2;
  kiln_temp = (kiln_temp * 0.9 + kiln_tmp1 * 0.1);

  if (TempA_errors > 0)
    TempA_errors--;

  DBG dbgLog(LOG_DEBUG,
             "[HEAT] Sensor A: Internal=%.1f Last=%.1f Avg kiln=%.1f\n", int_temp,
             kiln_tmp1, kiln_temp);
}

void Setup_Heat() {
  pinMode(SSR1_RELAY_PIN, OUTPUT);
  SSR_On = false;
  ESP32_SPI->begin(14, 12, 13);
  if (!ThermocoupleA.begin()) {
    DBG dbgLog(LOG_ERR, "[HEAT] MAX31855 ThermocoupleA begin() failed\n");
  }
}

void Setup_Addons() {
  Setup_Heat();
  Setup_EMR();
  Setup_Alarm();
  Setup_Housing_TC();
  Setup_SSR2();
  Setup_Energy();
}

/* Shared SPI for optional housing TC */
SPIClass *Kiln_Get_MAX_SPI() { return ESP32_SPI; }

#include <Adafruit_MAX31855.h>
#include <math.h>
#include "PIDKiln.h"

extern SPIClass *Kiln_Get_MAX_SPI();

static Adafruit_MAX31855 *ThermocoupleB = nullptr;

static void Handle_ThermocoupleB_Fault(uint8_t err) {
  if (err & MAX31855_FAULT_SHORT_VCC) {
    DBG dbgLog(LOG_ERR, "[HOUSING] ThermocoupleB short to VCC\n");
  } else if (err & MAX31855_FAULT_SHORT_GND) {
    DBG dbgLog(LOG_ERR, "[HOUSING] ThermocoupleB short to GND\n");
  } else if (err & MAX31855_FAULT_OPEN) {
    DBG dbgLog(LOG_ERR, "[HOUSING] ThermocoupleB not connected\n");
  } else {
    DBG dbgLog(LOG_ERR, "[HOUSING] ThermocoupleB unknown error\n");
  }

  if (TempB_errors < Prefs[PRF_ERROR_GRACE_COUNT].value.uint8) {
    TempB_errors++;
  } else {
    ABORT_Program(PR_ERR_MAX31B_INT_ERR);
  }
}

void Update_TemperatureB() {
  if (!ThermocoupleB)
    return;

  double case_tmp1;
  double int_tmp1;
  uint8_t err;

  int_tmp1 = ThermocoupleB->readInternal();
  case_tmp1 = ThermocoupleB->readCelsius();

  if (isnan(case_tmp1)) {
    err = ThermocoupleB->readError();
    if (err == 0 || err == MAX31855_FAULT_ALL) {
      DBG dbgLog(LOG_ERR, "[HOUSING] MAX31855 ThermocoupleB did not respond\n");
      ABORT_Program(PR_ERR_MAX31B_NC);
      return;
    }
    Handle_ThermocoupleB_Fault(err);
    return;
  }

  int_temp = (int_temp + int_tmp1) / 2;
  case_temp = (case_temp * 0.8 + case_tmp1 * 0.2);

  if (TempB_errors > 0)
    TempB_errors--;

  DBG dbgLog(LOG_DEBUG,
             "[HOUSING] Sensor B: Internal=%.1f Last=%.1f Avg case=%.1f\n",
             int_temp, case_tmp1, case_temp);
}

void Setup_Housing_TC() {
  ThermocoupleB = new Adafruit_MAX31855(MAXCS2, Kiln_Get_MAX_SPI());
  if (!ThermocoupleB->begin()) {
    DBG dbgLog(LOG_ERR, "[HOUSING] MAX31855 ThermocoupleB begin() failed\n");
  }
}

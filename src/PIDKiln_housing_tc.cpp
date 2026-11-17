#include <Adafruit_MAX31855.h>
#include <math.h>
#include "PIDKiln.h"
#include "PIDKiln_max31855_decode.h"

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

  uint32_t raw = Max31855_SpiRead32(Kiln_Get_MAX_SPI(), MAXCS2);
  Max31855Sample sample = Max31855_Decode(raw, MAX31855_FAULT_ALL);

  if (!sample.ok) {
    if (sample.fault == 0 || sample.fault == MAX31855_FAULT_ALL) {
      DBG dbgLog(LOG_ERR, "[HOUSING] MAX31855 ThermocoupleB did not respond\n");
      ABORT_Program(PR_ERR_MAX31B_NC);
      return;
    }
    Handle_ThermocoupleB_Fault(sample.fault);
    return;
  }

  int_temp = (int_temp + sample.internal_c) / 2;
  case_temp = (case_temp * 0.8 + sample.tc_c * 0.2);

  if (TempB_errors > 0)
    TempB_errors--;
}

void Setup_Housing_TC() {
  pinMode(MAXCS2, OUTPUT);
  digitalWrite(MAXCS2, HIGH);
  ThermocoupleB = new Adafruit_MAX31855(MAXCS2, Kiln_Get_MAX_SPI());
  if (!ThermocoupleB->begin()) {
    DBG dbgLog(LOG_ERR, "[HOUSING] MAX31855 ThermocoupleB begin() failed\n");
  }
}

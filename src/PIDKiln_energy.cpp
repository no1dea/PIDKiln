#include <EmonLib.h>
#include "PIDKiln.h"

#define ENERGY_MON_AMPS 30
#define EMERGY_MON_VOLTAGE 230
#define ENERGY_IGNORE_VALUE 0.4

static EnergyMonitor emon1;

void Read_Energy_INPUT() {
  double Irms;
  static uint8_t cnt = 0;
  static uint32_t last = 0;

  Irms = emon1.calcIrms(512);
  if (Irms < ENERGY_IGNORE_VALUE) {
    Energy_Wattage = 0;
    return;
  }
  Energy_Wattage = (uint16_t)(Energy_Wattage + Irms * EMERGY_MON_VOLTAGE) / 2;
  if (last) {
    uint16_t ttime = millis() - last;
    Energy_Usage += (double)(Energy_Wattage * ttime) / 3600000;
  }
  last = millis();

  if (cnt++ > 20) {
    DBG dbgLog(LOG_DEBUG,
               "[ENERGY] VCC:%d RAW Power:%.1fW Current:%.2fA Global:%d W/h:%.6f\n",
               emon1.readVcc(), Irms * EMERGY_MON_VOLTAGE, Irms, Energy_Wattage,
               Energy_Usage);
    cnt = 0;
  }
}

static void Power_Loop(void *parameter) {
  (void)parameter;
  for (;;) {
    Read_Energy_INPUT();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void Setup_Energy() {
  emon1.current(ENERGY_MON_PIN, ENERGY_MON_AMPS);
  xTaskCreatePinnedToCore(Power_Loop, "Power_metter", 8192, NULL, 2, NULL, 1);
}

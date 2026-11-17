/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>
#include <math.h>

// Dedicated HSPI bus for MAX31855 (LCD uses VSPI)
SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 ThermocoupleA(MAXCS1, ESP32_SPI);

// If we have defines power meter pins
#ifdef ENERGY_MON_PIN
#include <EmonLib.h>
#define ENERGY_MON_AMPS 30        // how many amps produces 1V on your meter (usualy with voltage output meters it's their max value).
#define EMERGY_MON_VOLTAGE 230    // what is your mains voltage
#define ENERGY_IGNORE_VALUE 0.4   // if measured current is below this - ignore it (it's just noise)
EnergyMonitor emon1;
#endif
uint16_t Energy_Wattage=0;        // keeping present power consumtion in Watts
double Energy_Usage=0;            // total energy used (Watt/time)

// If you have second thermoucouple
#ifdef MAXCS2
Adafruit_MAX31855 ThermocoupleB(MAXCS2, ESP32_SPI);
#endif

boolean SSR_On; // just to narrow down state changes.. I don't know if this is needed/faster

// Sole heater SSR writers — call only from Program_Loop / END_Program (single owner).
//
void Enable_SSR(){
  if(!SSR_On){
    digitalWrite(SSR1_RELAY_PIN, HIGH);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, HIGH);
#endif
    SSR_On=true;
  }
}

void Disable_SSR(){
  if(SSR_On){
    digitalWrite(SSR1_RELAY_PIN, LOW);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, LOW);
#endif
    SSR_On=false;
  }
}

// Explicit ownership API used by the control task
void Kiln_SetSSR(boolean on){
  if(on) Enable_SSR();
  else Disable_SSR();
}

void Enable_EMR(){
  digitalWrite(EMR_RELAY_PIN, HIGH);
}

void Disable_EMR(){
  digitalWrite(EMR_RELAY_PIN, LOW);
}

// Log Adafruit MAX31855 fault bits and apply grace-count abort for sensor A
//
static void Handle_ThermocoupleA_Fault(uint8_t err){
  if(err & MAX31855_FAULT_SHORT_VCC){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to VCC\n");
  }else if(err & MAX31855_FAULT_SHORT_GND){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to GND\n");
  }else if(err & MAX31855_FAULT_OPEN){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA not connected\n");
  }else{
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA unknown error, check spi cable\n");
  }

  if(TempA_errors<Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
    TempA_errors++;
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA had an error but we are still below grace threshold - continue. Error %d of %d\n",TempA_errors,Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
  }else{
    ABORT_Program(PR_ERR_MAX31A_INT_ERR);
  }
}

#ifdef MAXCS2
// Log Adafruit MAX31855 fault bits and apply grace-count abort for sensor B
//
static void Handle_ThermocoupleB_Fault(uint8_t err){
  if(err & MAX31855_FAULT_SHORT_VCC){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB short to VCC\n");
  }else if(err & MAX31855_FAULT_SHORT_GND){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB short to GND\n");
  }else if(err & MAX31855_FAULT_OPEN){
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB not connected\n");
  }else{
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB unknown error, check spi cable\n");
  }

  if(TempB_errors<Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
    TempB_errors++;
    DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleB had an error but we are still below grace threshold - continue. Error %d of %d\n",TempB_errors,Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
  }else{
    ABORT_Program(PR_ERR_MAX31B_INT_ERR);
  }
}
#endif


// ThermocoupleA temperature readout
//
void Update_TemperatureA(){
  double kiln_tmp1;
  double int_tmp1;
  uint8_t err;

  int_tmp1 = ThermocoupleA.readInternal();
  kiln_tmp1 = ThermocoupleA.readCelsius();

  if(isnan(kiln_tmp1)){
    err = ThermocoupleA.readError();
    // All fault bits set usually means floating SPI / chip not responding
    if(err == 0 || err == MAX31855_FAULT_ALL){
      DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 for ThermocoupleA did not respond\n");
      ABORT_Program(PR_ERR_MAX31A_NC);
      return;
    }
    Handle_ThermocoupleA_Fault(err);
    return;
  }

  int_temp = (int_temp+int_tmp1)/2;
  kiln_temp=(kiln_temp*0.9+kiln_tmp1*0.1);    // We try to make bigger hysteresis

  if(TempA_errors>0) TempA_errors--;  // Lower errors count after proper readout

  DBG dbgLog(LOG_DEBUG, "[ADDONS] Temperature sensor A readout: Internal temp = %.1f \t Last temp = %.1f \t Average kiln temp = %.1f\n", int_temp, kiln_tmp1, kiln_temp);
}


#ifdef MAXCS2
// ThermocoupleB temperature readout
//
void Update_TemperatureB(){
  double case_tmp1;
  double int_tmp1;
  uint8_t err;

  int_tmp1 = ThermocoupleB.readInternal();
  case_tmp1 = ThermocoupleB.readCelsius();

  if(isnan(case_tmp1)){
    err = ThermocoupleB.readError();
    if(err == 0 || err == MAX31855_FAULT_ALL){
      DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 for ThermocoupleB did not respond\n");
      ABORT_Program(PR_ERR_MAX31B_NC);
      return;
    }
    Handle_ThermocoupleB_Fault(err);
    return;
  }

  int_temp = (int_temp+int_tmp1)/2;
  case_temp=(case_temp*0.8+case_tmp1*0.2);    // We try to make bigger hysteresis

  if(TempB_errors>0) TempB_errors--;  // Lower errors count after proper readout

  DBG dbgLog(LOG_DEBUG,"[ADDONS] Temperature sensor B readout: Internal temp = %.1f \t Last temp = %.1f \t Average case temp = %.1f\n", int_temp, case_tmp1, case_temp);
}
#endif


// Measure current power usage - to be expanded
//
void Read_Energy_INPUT(){
double Irms;
static uint8_t cnt=0;
static uint32_t last=0;

#ifdef ENERGY_MON_PIN
  Irms = emon1.calcIrms(512);  // Calculate Irms only; 512 = number of samples (internaly ESP does 8 samples per measurement)
  if(Irms<ENERGY_IGNORE_VALUE){
    Energy_Wattage=0;
    return;   // In my case everything below 0,3A is just noise. Comparing to 10-30A we are going to use we can ignore it. Final readout is correct.
  }
  Energy_Wattage=(uint16_t)(Energy_Wattage+Irms*EMERGY_MON_VOLTAGE)/2;  // just some small hysteresis
  if(last){
    uint16_t ttime;
    ttime=millis()-last;
    Energy_Usage+=(double)(Energy_Wattage*ttime)/3600000;  // W/h - 60*60*1000 (miliseconds)
  }
  last=millis();

  if(cnt++>20){
    DBG dbgLog(LOG_DEBUG,"[ADDONS] VCC is set:%d ; RAW Power: %.1fW, Raw current: %.2fA, Power global:%d W/h:%.6f\n",emon1.readVcc(),Irms*EMERGY_MON_VOLTAGE,Irms,Energy_Wattage,Energy_Usage);
    cnt=0;
  }

#else
  return;
#endif

}


// Power metter loop - read energy consumption
//
void Power_Loop(void * parameter){
  for(;;){
    Read_Energy_INPUT();  // current redout takes around 3-5ms - so we will do it 10 times a second.
    vTaskDelay( 100 / portTICK_PERIOD_MS );
  }
}


// Stops Alarm
//
void STOP_Alarm(){
  ALARM_countdown=0;
  digitalWrite(ALARM_PIN, LOW);
}
// Start Alarm
//
void START_Alarm(){
  if(!Prefs[PRF_ALARM_TIMEOUT].value.uint16) return;
  ALARM_countdown=Prefs[PRF_ALARM_TIMEOUT].value.uint16;
  digitalWrite(ALARM_PIN, HIGH);
}


void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR1_RELAY_PIN, OUTPUT);
#ifdef SSR2_RELAY_PIN
    pinMode(SSR2_RELAY_PIN, OUTPUT);
#endif

  pinMode(ALARM_PIN, OUTPUT);

  SSR_On=false;

  // HSPI: SCLK=14, MISO=12, MOSI=13 (matches MAXCS* comments / README)
  ESP32_SPI->begin(14, 12, 13);

  if(!ThermocoupleA.begin()){
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 ThermocoupleA begin() failed\n");
  }
#ifdef MAXCS2
  if(!ThermocoupleB.begin()){
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 ThermocoupleB begin() failed\n");
  }
#endif
#ifdef ENERGY_MON_PIN
  emon1.current(ENERGY_MON_PIN, ENERGY_MON_AMPS);
  xTaskCreatePinnedToCore(
              Power_Loop,      /* Task function. */
              "Power_metter",  /* String with name of task. */
              8192,            /* Stack size in bytes. */
              NULL,            /* Parameter passed as input of the task */
              2,               /* Priority of the task. */
              NULL,1);         /* Task handle. */

#endif
}

#include <LittleFS.h>
#include <time.h>
#include "PIDKiln.h"
/*
** Logging functionality + live chart ring (RAM)
**
*/

double EnW_last=0;    // last energy consumption for csv report

/* ~24h at 30s LOG_Window; PSRAM preferred via MALLOC */
#define CHART_RING_CAP 2880

struct ChartSample {
  uint32_t t;
  int16_t k;
  int16_t c;
  int16_t e;
};

static ChartSample *chart_ring = NULL;
static uint16_t chart_head = 0;
static uint16_t chart_count = 0;
static uint32_t chart_gen = 0;

static bool chart_ring_ensure(void) {
  if (chart_ring)
    return true;
  chart_ring = (ChartSample *)MALLOC(sizeof(ChartSample) * CHART_RING_CAP);
  if (!chart_ring)
    chart_ring = (ChartSample *)malloc(sizeof(ChartSample) * CHART_RING_CAP);
  return chart_ring != NULL;
}

void ChartRing_Clear() {
  chart_head = 0;
  chart_count = 0;
  chart_gen++;
  if (!chart_ring)
    chart_ring_ensure();
}

void ChartRing_Push(uint32_t ts, int16_t kiln_c, int16_t case_c, int16_t energy) {
  if (!chart_ring_ensure())
    return;
  chart_ring[chart_head].t = ts;
  chart_ring[chart_head].k = kiln_c;
  chart_ring[chart_head].c = case_c;
  chart_ring[chart_head].e = energy;
  chart_head = (uint16_t)((chart_head + 1) % CHART_RING_CAP);
  if (chart_count < CHART_RING_CAP)
    chart_count++;
}

uint32_t ChartRing_Gen() { return chart_gen; }

size_t ChartRing_Count() { return chart_count; }

void ChartRing_PrintPointsJson(Print &out, uint32_t since_ts) {
  out.print('[');
  if (!chart_ring || !chart_count) {
    out.print(']');
    return;
  }
  uint16_t start = (uint16_t)((chart_head + CHART_RING_CAP - chart_count) % CHART_RING_CAP);
  bool first = true;
  for (uint16_t i = 0; i < chart_count; i++) {
    const ChartSample &s = chart_ring[(start + i) % CHART_RING_CAP];
    if (s.t <= since_ts)
      continue;
    if (!first)
      out.print(',');
    first = false;
#if PK_FEATURE_ENERGY
    out.printf("{\"t\":%lu,\"k\":%d,\"c\":%d,\"e\":%d}", (unsigned long)s.t, (int)s.k,
               (int)s.c, (int)s.e);
#else
    out.printf("{\"t\":%lu,\"k\":%d,\"c\":%d}", (unsigned long)s.t, (int)s.k, (int)s.c);
#endif
  }
  out.print(']');
}


// Starts a new log file
//
void Init_log_file(){
char str[33];
struct tm timeinfo, *tmm;

  ChartRing_Clear();

  if(CSVFile) CSVFile.close();
  if(getLocalTime(&timeinfo)) strftime(str, 32, "/logs/%y%m%d_%H%M%S.csv", &timeinfo); //YYMMDD_HHMMSS.log
  else sprintf(str,"/logs/%d.csv",millis());  // if we don't have a clock - use millis - this should NOT happend
  DBG dbgLog(LOG_INFO,"[LOG] Trying to create log file:%s\n",str);
  
  if(CSVFile=LittleFS.open(str, "w")){
    DBG dbgLog(LOG_INFO,"[LOG] Created new log file %s\n",str);
#if PK_FEATURE_ENERGY
    CSVFile.print(String("Date,Temperature,Housing,Energy"));
#else
    CSVFile.print(String("Date,Temperature,Housing"));
#endif
  }

  // Try to create additional info-log file if not.. well
  char *tmpstr;
  tmpstr=strstr(str,".csv");
  if(tmpstr){
    strncpy(tmpstr,".log",4);
    if(LOGFile=LittleFS.open(str, "w")){
      LOGFile.printf("Program name: %s\n",Program_run_name);
      LOGFile.printf("Program desc: %s\n",Program_run_desc);
      if(tmm = localtime(&Program_run_start)){
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Started at: %s\n", str);
      }
      if(tmm = localtime(&Program_run_end)){
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Possible end at: %s\n", str);
      }
      LOGFile.printf("PID values. Kp:%.2f Ki:%.2f Kd:%.2f\n",Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat);
      LOGFile.printf("Start temperature: %.1fC\n",kiln_temp);
      LOGFile.printf("CSV filename: %s\n-=-=-= Starting program =-=-=-=-\n",str);
      LOGFile.flush();
//      LOGFile.close();
    }else{
      DBG dbgLog(LOG_ERR,"[LOG] Failed to create .log file: %s\n",str);
    }
  }
  
  EnW_last=0;   // reset last energy consumption with new program
  
  Generate_LOGS_INDEX();
}


// Add single log line with current date and temperature
//
void Add_log_line(){
String tmp;
char str[33];
struct tm timeinfo;
int energy_delta = 0;
uint32_t ts;

  if(!CSVFile) return;

  ts = (uint32_t)time(NULL);
  if (getLocalTime(&timeinfo))
    strftime(str, 29, "%F %T", &timeinfo);
  else
    sprintf(str, "%d", (int)millis());

#if PK_FEATURE_ENERGY
  energy_delta = (int)(Energy_Usage - EnW_last);
  EnW_last = Energy_Usage;
  tmp = String(str) + "," + String(kiln_temp, 0) + "," + String(case_temp, 0) + "," +
        String(energy_delta);
#else
  tmp = String(str) + "," + String(kiln_temp, 0) + "," + String(case_temp, 0);
#endif

  ChartRing_Push(ts, (int16_t)kiln_temp, (int16_t)case_temp, (int16_t)energy_delta);

  DBG dbgLog(LOG_INFO,"[LOG] Writing to log file:%s\n",tmp.c_str());
  CSVFile.println();
  CSVFile.print(tmp);
  CSVFile.flush();
}


// Closes cleanly log file
//
void Close_log_file(){
struct tm timeinfo, *tmm;
char str[33];

  if(CSVFile){
    CSVFile.flush();
    CSVFile.close();
  }
  if(LOGFile){
    if(tmm = localtime(&Program_run_end)){
      strftime(str, 29, "%F %T", tmm);
      LOGFile.printf("Program ended at: %s\n", str);
    }
    LOGFile.printf("End temperature: %.1fC\n",kiln_temp);
    if(Energy_Wattage) LOGFile.printf("Used power: %.1f W/h\n",Energy_Wattage);
    if(Program_error){
      LOGFile.printf("Program aborted with error: %d\n",Program_error);
    }
    LOGFile.flush();
    LOGFile.close();
  }
  Clean_LOGS();
  Generate_LOGS_INDEX();
}


// Function leans logs on LittleFS depending on preferences value "LOG_Files_Limit"
//
void Clean_LOGS(){
char fname[MAX_FILENAME];

  if(Logs_DIR_size<=Prefs[PRF_LOG_LIMIT].value.uint16) return;
  DBG dbgLog(LOG_INFO,"[LOG] Cleaning logs...\n");
  for(uint16_t a=Prefs[PRF_LOG_LIMIT].value.uint16; a<Logs_DIR_size; a++){
    sprintf(fname,"%s/%s",LOG_Directory,Logs_DIR[a].filename);
    DBG dbgLog(LOG_INFO,"[LOG] Deleting file:%s\n",fname);
    LittleFS.remove(fname);
  }
}


// Load logs directory into memory - to sort it etc for easier processing PSMmem is plenty
//
uint8_t Load_LOGS_Dir(){
uint16_t count=0;
File dir,file;

  dir = LittleFS.open(LOG_Directory);
  if(!dir) return 1;  // directory open failed
  DBG dbgLog(LOG_INFO,"[LOG] Loading dir: Loading logs directory...\n");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  
  DBG dbgLog(LOG_INFO,"[LOG] Loading dir:\tcounted %d files\n",count);
  if(Logs_DIR){
    free(Logs_DIR);
    Logs_DIR=NULL;
  }
  Logs_DIR=(DIRECTORY*)MALLOC(sizeof(DIRECTORY)*count);
  Logs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Logs_DIR_size<=count){    // now we do acctual loading into memory
    char tmp[32];
    uint8_t len2;

    vTaskDelay(20);
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    
    // LEGACY: skip on-disk index.html left from when indexes were written to the FS
    if(!strcmp(tmp,"index.html")) continue;
    
    strcpy(Logs_DIR[Logs_DIR_size].filename,tmp);
    Logs_DIR[Logs_DIR_size].filesize=file.size();
    Logs_DIR[Logs_DIR_size].sel=0;
    
    DBG dbgLog(LOG_DEBUG,"[LOG] FName: %s\t FSize:%d\tSel:%d\n",Logs_DIR[Logs_DIR_size].filename,Logs_DIR[Logs_DIR_size].filesize,Logs_DIR[Logs_DIR_size].sel);
    
    Logs_DIR_size++;
  }
  dir.close();

  // sorting... slooow but easy
  bool nok=true;
  while(nok){
    nok=false;
    if(Logs_DIR_size>1) // if we have at least 2 logs
      for(int a=0; a<Logs_DIR_size-1; a++){
        DIRECTORY tmp;
        if(strcmp(Logs_DIR[a].filename,Logs_DIR[a+1].filename)<0){
          tmp=Logs_DIR[a];
          Logs_DIR[a]=Logs_DIR[a+1];
          Logs_DIR[a+1]=tmp;
          nok=true;
        }
      }    
   }

  //if(Logs_DIR_size) Logs_DIR[0].sel=1; // make first log seleted if we have at least one
  return 0;
}



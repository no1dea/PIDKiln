#include <Arduino.h>
#include <PID_v1.h>
#include <SPI.h>
#include <LittleFS.h>
#include <string.h>
#include <PIDKilnLogic.h>
#include "PIDKiln.h"

// Other variables
//
hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


/*
** Core/main program functions
**
*/

// Setup timer function
//
void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  //portENTER_CRITICAL_ISR(&timerMux);
  //portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}


// Load program from file to memory - return 0 if success
//
uint8_t Load_program(char *file){
char file_path[32];
int sel;
File prg;

  if(file){  // if function got an argument - this can happen if you want to validate new program uploaded by http
    sprintf(file_path,"%s/%s",PRG_Directory,file);
    DBG dbgLog(LOG_DEBUG,"[PRG] Got pointer to load:'%s'\n",file);
    Program_name=String(file);
  }else{
    if((sel=Find_selected_program())<0) return Cleanup_program(PIDKILN_PRG_ERR_IO);
    sprintf(file_path,"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
    Program_name=String(Programs_DIR[sel].filename);
  }
  DBG dbgLog(LOG_INFO,"[PRG] Load program name: '%s'\n",file_path);

  prg = LittleFS.open(file_path,"r");
  if(!prg) return Cleanup_program(PIDKILN_PRG_ERR_IO);

  size_t fsize = prg.size();
  if(fsize == 0 || fsize > (size_t)MAX_Prog_File_Size){
    prg.close();
    return Cleanup_program(PIDKILN_PRG_ERR_IO);
  }

  char *buf = (char *)MALLOC(fsize + 1);
  if(!buf){
    prg.close();
    return Cleanup_program(PIDKILN_PRG_ERR_IO);
  }
  size_t got = prg.readBytes(buf, fsize);
  prg.close();
  buf[got] = '\0';

  PIDKiln_ProgramParsed parsed;
  int start_c = (int)kiln_temp;
  int err = PIDKiln_parse_program_json(buf, got, Prefs[PRF_MAX_TEMP].value.uint16, &start_c, &parsed);
  free(buf);
  if(err) return Cleanup_program((uint8_t)err);

  Program_size = 0;
  Program_desc = String(parsed.description);
  for(unsigned a = 0; a < parsed.size; a++){
    Program[a].temp = parsed.steps[a].temp;
    Program[a].togo = parsed.steps[a].togo;
    Program[a].dwell = parsed.steps[a].dwell;
    Program_rate_c_per_h[a] = parsed.rate_c_per_h[a];
    DBG dbgLog(LOG_DEBUG,"[PRG] Step %u: Temp:%uC togo:%u dwell:%u rate:%u\n",
               a, Program[a].temp, Program[a].togo, Program[a].dwell, Program_rate_c_per_h[a]);
  }
  Program_size = (uint8_t)parsed.size;
  for(unsigned a = parsed.size; a < MAX_PRG_LENGTH; a++)
    Program_rate_c_per_h[a] = 0;

  DBG dbgLog(LOG_DEBUG,"[PRG] Found description: %s\n",Program_desc.c_str());
  return PIDKILN_PRG_OK;
}


// Load programs directory into memory - to sort it etc for easier processing
//
uint8_t Load_programs_dir(){
uint16_t count=0;
File dir,file;

  dir = LittleFS.open(PRG_Directory);
  if(!dir) return 1;  // directory open failed
  DBG dbgLog(LOG_INFO,"[PRG] Loading directory...\n");
  while(dir.openNextFile()) count++;  // not the prettiest - but we count files first to do proper malloc without fragmenting memory
  DBG dbgLog(LOG_DEBUG,"[PRG]\tcounted %d files\n",count);
  if(Programs_DIR){
    free(Programs_DIR);
    Programs_DIR=NULL;
  }
  Programs_DIR=(DIRECTORY*)MALLOC(sizeof(DIRECTORY)*count);
  Programs_DIR_size=0;
  dir.rewindDirectory();
  while((file=dir.openNextFile()) && Programs_DIR_size<=count){    // now we do actual loading into memory
    char tmp[32];
    uint8_t len2;
    
    strcpy(tmp,file.name());
    len2=strlen(tmp);
    if(len2>31 || len2<2) return 2; // file name with dir too long or just /
    DBG dbgLog(LOG_DEBUG,"[PRG] Processing filename: %s\n",tmp);
/* Outdated with ESP32 IC 2.0+
    fname=strchr(tmp+1,'/');        // seek for the NEXT directory separator...
    fname++;                        //  ..and skip it
*/
    // Only .json program files (skip index.html and any other non-program assets)
    if(!strcmp(tmp,"index.html")) continue;
    if(len2 < 5 || strcmp(tmp + len2 - 5, ".json") != 0) continue;
    strcpy(Programs_DIR[Programs_DIR_size].filename,tmp);
    Programs_DIR[Programs_DIR_size].filesize=file.size();
    Programs_DIR[Programs_DIR_size].sel=0;
    
    DBG dbgLog(LOG_DEBUG,"[PRG] FName: %s\t FSize:%d\tSel:%d\n",Programs_DIR[Programs_DIR_size].filename,Programs_DIR[Programs_DIR_size].filesize,Programs_DIR[Programs_DIR_size].sel);
    
    Programs_DIR_size++;
  }
  dir.close();

  // sorting... slooow but easy
  bool nok=true;
  while(nok){
    nok=false;
    if(Programs_DIR_size>1) // if we have at least 2 progs
      for(int a=0; a<Programs_DIR_size-1; a++){
        DIRECTORY tmp;
        if(strcmp(Programs_DIR[a].filename,Programs_DIR[a+1].filename)>0){
          tmp=Programs_DIR[a];
          Programs_DIR[a]=Programs_DIR[a+1];
          Programs_DIR[a+1]=tmp;
          nok=true;
        }
      }    
   }

  if(Programs_DIR_size) Programs_DIR[0].sel=1; // make first program selected if we have at least one
  return 0;
}


// Edit program step
//
void Update_program_step(uint8_t sstep, uint16_t stemp, uint16_t stime, uint16_t sdwell){
  if(Program_run_size<=sstep)
    if(Program_run_size==sstep){ // we are out of the program - but this is just NEXT step, we can add
      Program_run_size++;
      Program_run=(PROGRAM *)REALLOC(Program_run,sizeof(PROGRAM)*Program_run_size);
      Program_run_rate_c_per_h=(uint16_t *)REALLOC(Program_run_rate_c_per_h,sizeof(uint16_t)*Program_run_size);
      if(Program_run_rate_c_per_h) Program_run_rate_c_per_h[sstep]=0;
    }else return;   // we are out of the program - we can edit it

  Program_run[sstep].temp=stemp;
  Program_run[sstep].togo=stime;
  Program_run[sstep].dwell=sdwell;
  if(Program_run_rate_c_per_h && sstep<Program_run_size)
    Program_run_rate_c_per_h[sstep]=0; /* LCD/quick edits are duration-based */
}


// Initizalize program - clear memory
//
void Initialize_program_to_run(){
  if(!Program_run_size && !Program_run && !Program_run_rate_c_per_h) return;
  if(Program_run!=NULL){
    free(Program_run);
    Program_run=NULL;
  }
  if(Program_run_rate_c_per_h!=NULL){
    free(Program_run_rate_c_per_h);
    Program_run_rate_c_per_h=NULL;
  }
  if(Program_run_desc!=NULL){
    free(Program_run_desc);
    Program_run_desc=NULL;
  }
  if(Program_run_name!=NULL){
    free(Program_run_name);
    Program_run_name=NULL;
  }
  Program_run_size=0;
  DBG dbgLog(LOG_INFO,"[PRG] Initialized new in-memory program\n");
}


// Copy selected/loaded program to RUN program memory
//
void Load_program_to_run(){
  
  Initialize_program_to_run();
  Program_run=(PROGRAM *)MALLOC(sizeof(PROGRAM)*Program_size);
  Program_run_rate_c_per_h=(uint16_t *)MALLOC(sizeof(uint16_t)*Program_size);
  for(uint8_t a=0;a<Program_size;a++){
    Program_run[a]=Program[a];
    Program_run_rate_c_per_h[a]=Program_rate_c_per_h[a];
  }
  
  Program_run_desc=(char *)MALLOC((Program_desc.length()+1)*sizeof(char));
  strcpy(Program_run_desc,Program_desc.c_str());
  Program_run_name=(char *)MALLOC((Program_name.length()+1)*sizeof(char));
  strcpy(Program_run_name,Program_name.c_str());
  Program_run_size=Program_size;
  Program_run_state=PR_READY;
}


/*
** Helping functions
**
*/

// Find selected program
//
int Find_selected_program(){
  for(uint16_t a=0; a<Programs_DIR_size; a++) if(Programs_DIR[a].sel>0) return a;
  return -1;  // in case there is NO selected program or there can be no programs at all
}


// Move program selection up/down
//
void rotate_selected_program(int dir){
int a = Find_selected_program();

  DBG dbgLog(LOG_INFO,"[PRG] Rotating programs. For a:%d, dir: %d, selected?:%d, dir_size:%d\n",a,dir,Programs_DIR[a].sel,Programs_DIR_size);
  if(dir<0 && a>0){   // if we are DOWN down and we can a>0 - do it, if we can't - do nothing
    Programs_DIR[a].sel=0;  // delete old selection
    Programs_DIR[a-1].sel=1;
  }else if(dir>0 && a<(Programs_DIR_size-1)){   // if we are going UP and there is more programs - do it, if it's the end of program list, do nothing
    Programs_DIR[a].sel=0;  // delete old selection
    Programs_DIR[a+1].sel=1;
  }
}


// Short function to cleanup program def. after failed load
//
byte Cleanup_program(byte err){
  Program_size=0;
  Program_desc="";
  Program_name="";
  for(byte a=0;a<MAX_PRG_LENGTH;a++){
    Program[a].temp=Program[a].togo=Program[a].dwell=0;
    Program_rate_c_per_h[a]=0;
  }
  DBG dbgLog(LOG_INFO,"[PRG] Cleaning up program with error %d\n",err);
  return err;
}


// Erase program from disk
//
boolean Erase_program_file(){
char file[32];

  sprintf(file,"%s/%s",PRG_Directory,Programs_DIR[Find_selected_program()].filename);
  DBG dbgLog(LOG_INFO,"[PRG] !!! Erasing file from disk: %s",file);
  return LittleFS.remove(file);
}


// Cleanly end program
//
void END_Program(){

  DBG dbgLog(LOG_INFO,"[PRG] Ending program cleanly\n");
  Program_run_state=PR_ENDED;
  KilnPID.SetMode(MANUAL);
  Kiln_SetSSR(false);
  Disable_EMR();
  Program_run_end=time(NULL);
  Close_log_file();
  set_temp=0;
  pid_out=0;
  Program_run_step=-1;
}


// Abort program if something goes wrong - this has no real meaning now - perhaps later...
//
void ABORT_Program(uint8_t error){

  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED || Program_run_state==PR_THRESHOLD){
    Program_error=error;
    DBG dbgLog(LOG_INFO,"[PRG] Aborting program with error: %d\n",Program_error);
    END_Program();
    Program_run_state=PR_ABORTED;
    //Program_run_start=0;
    START_Alarm();
  }
}


// Pause program. We are keeping current temperature and wait until it's resumed
//
void PAUSE_Program(){
  if(Program_run_state==PR_RUNNING) Program_run_state=PR_PAUSED;
}


// Resume pasued program.
//
void RESUME_Program(){
  if(Program_run_state==PR_PAUSED) Program_run_state=PR_RUNNING;
}


// Recalculate ETA based on current step
//
void Program_recalculate_ETA(boolean dwell){
  Program_run_end = time(NULL);
  if (Program_run && Program_run_step >= 0) {
    Program_run_end += PIDKiln_eta_remaining_seconds(
        Program_run, (unsigned)Program_run_step,
        Program_run_size, dwell ? 1 : 0);
  }
}


// Called every second while a program is active. Setpoint ramp / step FSM lives in PIDKilnLogic.
//
void Program_calculate_steps(boolean prg_start){
static PIDKiln_ProgramFsm fsm;
static uint16_t cnt1=1;

  if(prg_start){
    cnt1=0;
    memset(&fsm, 0, sizeof(fsm));
    fsm.steps = Program_run;
    fsm.size = Program_run_size;
    fsm.threshold = Prefs[PRF_PID_TEMP_THRESHOLD].value.int16;
    PIDKiln_program_fsm_start(&fsm, kiln_temp, (uint32_t)time(NULL));
    Program_run_step = fsm.step;
  }

  // Logging — Prefs[PRF_LOG_WINDOW] == 0 disables
  cnt1++;
  if(Prefs[PRF_LOG_WINDOW].value.uint16 && cnt1>Prefs[PRF_LOG_WINDOW].value.uint16){
    cnt1=1;
    Add_log_line();
  }

  fsm.steps = Program_run;
  fsm.size = Program_run_size;
  fsm.threshold = Prefs[PRF_PID_TEMP_THRESHOLD].value.int16;

  /* Sync user pause/resume into the FSM; leave THRESHOLD to the FSM. */
  if(Program_run_state==PR_PAUSED)
    fsm.state = PIDKILN_FSM_PAUSED;
  else if(fsm.state==PIDKILN_FSM_PAUSED && Program_run_state==PR_RUNNING)
    fsm.state = PIDKILN_FSM_RUNNING;

  int rc = PIDKiln_program_fsm_tick(&fsm, kiln_temp, (uint32_t)time(NULL));

  set_temp = fsm.set_temp;
  temp_incr = fsm.temp_incr;
  Program_run_step = fsm.step;

  if(rc==PIDKILN_FSM_TICK_ENDED){
    END_Program();
    return;
  }

  if(fsm.state==PIDKILN_FSM_THRESHOLD){
    if(Program_run_state!=PR_THRESHOLD){
      DBG dbgLog(LOG_INFO,"[PRG] Temperature in TEMP_THRESHOLD. Kiln_temp:%.1f Set_temp:%.1f Window:%d\n",
                 kiln_temp,set_temp,(int)Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
    }
    Program_run_state=PR_THRESHOLD;
  }else if(fsm.state==PIDKILN_FSM_RUNNING && Program_run_state!=PR_PAUSED){
    Program_run_state=PR_RUNNING;
  }

  if(rc==PIDKILN_FSM_TICK_ETA || rc==PIDKILN_FSM_TICK_ETA_RAMP)
    Program_recalculate_ETA(false);
  else if(rc==PIDKILN_FSM_TICK_ETA_DWELL)
    Program_recalculate_ETA(true);

  DBG dbgLog(LOG_DEBUG,"[PRG] Curr temp: %.0f, Set_temp: %.0f, Incr: %.2f Pid_out:%.2f Step: %d\n",
             kiln_temp, set_temp, temp_incr, pid_out*PID_WINDOW_DIVIDER, Program_run_step);
}


// Start running the in memory program
//
void START_Program(){

  if(Program_run_state==PR_NONE || Program_run_state==PR_PAUSED) return; // check if program is ready (we can start also from ended,failed etc,) and not paused
  
  DBG dbgLog(LOG_INFO,"[PRG] Starting new program!");
  Program_run_state=PR_RUNNING;
  Program_start_temp=kiln_temp;
  Energy_Usage=0;
  Program_error=0;
  TempA_errors=TempB_errors=0;  // Reset temperature errors

  /* Rate-authored step 0: resolve togo from live kiln temperature. */
  if(Program_run && Program_run_size && Program_run_rate_c_per_h && Program_run_rate_c_per_h[0]){
    PIDKiln_ProgramStep s0 = {Program_run[0].temp, Program_run[0].togo, Program_run[0].dwell};
    PIDKiln_resolve_rate_step_togo(&s0, Program_run_rate_c_per_h[0], (int)kiln_temp);
    Program_run[0].togo = s0.togo;
    DBG dbgLog(LOG_INFO,"[PRG] Step0 rate %u C/h → togo %u min (from %.1fC)\n",
               Program_run_rate_c_per_h[0], Program_run[0].togo, kiln_temp);
  }
  
  Enable_EMR();

  KilnPID.SetTunings(Prefs[PRF_PID_KP].value.vfloat,Prefs[PRF_PID_KI].value.vfloat,Prefs[PRF_PID_KD].value.vfloat,Prefs[PRF_PID_POE].value.uint8); // set actual PID parameters
  Program_run_start=time(NULL);
  Program_calculate_steps(true);
  windowStartTime=millis();

  // PID output range is 0..PID_Window/PID_WINDOW_DIVIDER (divider defaults to 1) for time-proportioned SSR
  KilnPID.SetOutputLimits(0, Prefs[PRF_PID_WINDOW].value.uint16/PID_WINDOW_DIVIDER);
  KilnPID.SetSampleTime(1000); /* match 1 Hz TC update */
  KilnPID.SetMode(AUTOMATIC);

  DBG dbgLog(LOG_INFO,"[PRG] Trying to start log - window size:%d\n",Prefs[PRF_LOG_WINDOW].value.uint16);
  if(Prefs[PRF_LOG_WINDOW].value.uint16){ // if we should create log file
    DBG dbgLog(LOG_INFO,"[PRG] Trying to create logs");
    Init_log_file();
    Add_log_line();
  }
}


// Check all possible safety measures - if something is wrong - abort
//
void SAFETY_Check(){
  static boolean runaway_watch=false;
  static time_t runaway_start=0;
  static double runaway_temp0=0;

  if(PIDKiln_temp_below_min(kiln_temp, Prefs[PRF_MIN_TEMP].value.uint8)){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - MIN temperature < %d\n",Prefs[PRF_MIN_TEMP].value.uint8);
    ABORT_Program(PR_ERR_TOO_COLD);
  }else if(PIDKiln_temp_above_max(kiln_temp, Prefs[PRF_MAX_TEMP].value.uint16)){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - MAX temperature > %d\n",Prefs[PRF_MAX_TEMP].value.uint16);
    ABORT_Program(PR_ERR_TOO_HOT);  
  }
#if PK_FEATURE_HOUSING_TC
  if(PIDKiln_housing_above_max(case_temp, Prefs[PRF_MAX_HOUSING_TEMP].value.uint16)){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - MAX housing temperature > %d\n",Prefs[PRF_MAX_HOUSING_TEMP].value.uint16);
    ABORT_Program(PR_ERR_TOO_HOT_HOUSING);
  }
#endif

  // Thermal runaway: while ramping up and setpoint is 5C+ above kiln temp, require +5C rise within N minutes.
  // Pref units are minutes; 0 disables. Matches data/etc/pidkiln.conf comments.
  uint16_t run_win=Prefs[PRF_THERMAL_RUN].value.uint16;
  boolean heating_up=PIDKiln_thermal_runaway_heating(temp_incr, set_temp, kiln_temp);
  if(!run_win || Program_run_state!=PR_RUNNING || !heating_up){
    runaway_watch=false;
  }else if(!runaway_watch){
    runaway_watch=true;
    runaway_start=time(NULL);
    runaway_temp0=kiln_temp;
    DBG dbgLog(LOG_INFO,"[PRG] Thermal runaway watch start at %.1fC window=%umin\n",runaway_temp0,run_win);
  }else if(kiln_temp>=runaway_temp0+5.0){
    runaway_start=time(NULL);
    runaway_temp0=kiln_temp;
  }else if((time(NULL)-runaway_start)>=(time_t)run_win*60){
    DBG dbgLog(LOG_ERR,"[PRG] Safety check failed - thermal runaway (no +5C in %umin from %.1fC)\n",run_win,runaway_temp0);
    runaway_watch=false;
    ABORT_Program(PR_ERR_THERMAL_RUNAWAY);
  }
}


// Function that create task to handle program running
//
void Program_Loop(void * parameter){
static uint16_t cnt1=0;
uint32_t now;

 for(;;){

    now = millis();
 
    // Interrupts triggered ones per second
    // 
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){

      // Update temperature readout
      Update_TemperatureA();
      Kiln_PublishTelemetry();

      // Check if there is Alarm ON - if so, lower time and call STOP
      if(ALARM_countdown>0){
        if(ALARM_countdown<=1) STOP_Alarm();
        ALARM_countdown--;
      }

      // Do slow stuff every 10th second - cnt1=[0..9]
      //
      if(cnt1>9) cnt1=0;
      else cnt1++;
#if PK_FEATURE_HOUSING_TC
      if(cnt1==3){  // just to make it in other time then next if cnt1
        Update_TemperatureB();      // this does not have to be updated so often as kiln temp
        Kiln_PublishTelemetry();
      }
#endif
      /* Energy metering runs only on Power_Loop when PK_FEATURE_ENERGY=1 */
      // Ask Input_Loop to redraw main views — keep LCD off the control path
#if PK_FEATURE_LOCAL_UI
      if(LCD_State==SCR_MAIN_VIEW && Program_run_size && LCD_Main==MAIN_VIEW1)
        Kiln_RequestLcdRefresh(LCD_REFRESH_MAIN_V1);
#endif

      if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED || Program_run_state==PR_THRESHOLD){
        // Do all the program recalc
        Program_calculate_steps();

        if(cnt1==6){
          SAFETY_Check();
        }else if(cnt1==9){
          cnt1=0;
#if PK_FEATURE_LOCAL_UI
          if(LCD_Main==MAIN_VIEW2 && (Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED))
            Kiln_RequestLcdRefresh(LCD_REFRESH_MAIN_V2);
#endif
          DBG dbgLog(LOG_DEBUG,"[PRG] Pid_out RAW:%.2f Pid_out:%.2f Now-window:%d WindowSize:%d Prg_state:%d\n",pid_out,pid_out*PID_WINDOW_DIVIDER,(now - windowStartTime), Prefs[PRF_PID_WINDOW].value.uint16, (byte)Program_run_state);
        }
       }
    }

    // Do the PID stuff
    if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED || Program_run_state==PR_THRESHOLD){
      KilnPID.Compute();

      uint16_t win = Prefs[PRF_PID_WINDOW].value.uint16;
      windowStartTime = PIDKiln_window_advance(windowStartTime, now, win);
      Kiln_SetSSR(PIDKiln_ssr_on(pid_out, win, windowStartTime, now, PID_WINDOW_DIVIDER) ? true : false);
    }
    //yield();
    vTaskDelay(10); // This should enable to run other tasks on this core
  }
}


/*
** Main setup function for programs module
*/
void Program_Setup(){
 
  // Start interupt timer handler - 1s
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // has now only 1 parameter (frequency). There is an automatic calculation of the divider using different clock sources to achieve the selected frequency.
  timer = timerBegin(1000000);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarm(timer, 1000000, true, 0);


  xTaskCreatePinnedToCore(
              Program_Loop,    /* Task function. */
              "Program_loop",  /* String with name of task. */
              8192,            /* Stack size in bytes. */
              NULL,            /* Parameter passed as input of the task */
              1,               /* Priority of the task. */
              NULL,0);         /* Task handle. */
                    
}

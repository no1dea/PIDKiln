#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <PID_v1.h>
#include "PIDKiln_features.h"
#include <PIDKilnLogic.h>

#if PK_FEATURE_SYSLOG
#include <Syslog.h>
#include <WiFiUdp.h>
#else
/* Priority constants normally from Syslog.h — needed by dbgLog callers */
#ifndef LOG_EMERG
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#endif
#endif

extern const uint16_t Long_Press;
extern const int MAX_Prog_File_Size;

extern uint16_t ALARM_countdown;

extern double int_temp, kiln_temp, case_temp;
extern double set_temp, pid_out;
extern float temp_incr;
extern uint32_t windowStartTime;
#define PID_WINDOW_DIVIDER 1

extern PID KilnPID;

extern uint16_t Energy_Wattage;
extern double Energy_Usage;

/*
** LCD / local UI types (always declared; unused when LOCAL_UI=0)
*/
typedef enum {
  SCR_MAIN_VIEW,
  SCR_MENU,
  SCR_PROGRAM_LIST,
  SCR_PROGRAM_SHOW,
  SCR_PROGRAM_DELETE,
  SCR_PROGRAM_FULL,
  SCR_QUICK_PROGRAM,
  SCR_ABOUT,
  SCR_PREFERENCES,
  SCR_OTHER
} LCD_State_enum;

typedef enum {
  MAIN_VIEW1,
  MAIN_VIEW2,
  MAIN_VIEW3,
  MAIN_end
} LCD_MAIN_View_enum;

typedef enum {
  M_SCR_MAIN_VIEW,
  M_LIST_PROGRAMS,
  M_QUICK_PROGRAM,
  M_INFORMATIONS,
  M_PREFERENCES,
  M_CONNECT_WIFI,
  M_ABOUT,
  M_RESTART,
  M_END
} LCD_SCR_MENU_Item_enum;

extern LCD_State_enum LCD_State;
extern LCD_MAIN_View_enum LCD_Main;
extern LCD_SCR_MENU_Item_enum LCD_Menu;
extern const char *Menu_Names[];

typedef enum {
  P_EXIT,
  P_SHOW,
  P_LOAD,
  P_DELETE,
  P_end
} LCD_PSCR_MENU_Item_enum;

extern const char *Prog_Menu_Names[];
extern const uint8_t Prog_Menu_Size;

extern const uint8_t SCR_MENU_LINES;
extern const uint8_t SCR_MENU_SPACE;
extern const uint8_t SCR_MENU_MIDDLE;

struct PROGRAM {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
};
#define MAX_PRG_LENGTH 40

extern PROGRAM Program[MAX_PRG_LENGTH];
extern uint8_t Program_size;
extern String Program_desc, Program_name;

extern PROGRAM *Program_run;
extern uint8_t Program_run_size;
extern char *Program_run_desc, *Program_run_name;
extern time_t Program_run_start;
extern time_t Program_run_end;
extern int Program_run_step;
extern uint16_t Program_start_temp;
extern uint8_t Program_error;
extern byte TempA_errors, TempB_errors;

typedef enum {
  PR_NONE,
  PR_READY,
  PR_RUNNING,
  PR_PAUSED,
  PR_ABORTED,
  PR_ENDED,
  PR_THRESHOLD,
  PR_end
} PROGRAM_RUN_STATE;
extern PROGRAM_RUN_STATE Program_run_state;
extern const char *Prog_Run_Names[];

typedef enum {
  PR_ERR_FILE_LOAD,
  PR_ERR_TOO_LONG_LINE,
  PR_ERR_BAD_CHAR,
  PR_ERR_TOO_HOT,
  PR_ERR_TOO_COLD,
  PR_ERR_TOO_HOT_HOUSING,
  PR_ERR_MAX31A_NC,
  PR_ERR_MAX31A_INT_ERR,
  PR_ERR_MAX31A_KPROBE,
  PR_ERR_MAX31B_NC,
  PR_ERR_MAX31B_INT_ERR,
  PR_ERR_MAX31B_KPROBE,
  PR_ERR_USER_ABORT,
  PR_ERR_THERMAL_RUNAWAY,
  PR_ERR_end
} PROGRAM_ERROR_STATE;

#define MAX_FILENAME 30
#define MAX_PROGNAME 20
#define allowed_chars_in_filename PIDKILN_ALLOWED_FILENAME_CHARS

struct DIRECTORY {
  char filename[MAX_PROGNAME + 1];
  uint16_t filesize;
  uint8_t sel;
};

extern DIRECTORY *Programs_DIR;
extern uint16_t Programs_DIR_size;
extern DIRECTORY *Logs_DIR;
extern uint16_t Logs_DIR_size;

#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
extern const char *PRG_Directory;
extern const char *LOG_Directory;

#define FORMAT_LITTLEFS_IF_FAILED true

#define PREFS_FILE "/etc/pidkiln.conf"

typedef enum {
  PRF_NONE,
  PRF_WIFI_SSID,
  PRF_WIFI_PASS,
  PRF_WIFI_MODE,
  PRF_WIFI_RETRY_CNT,
  PRF_WIFI_AP_NAME,
  PRF_WIFI_AP_PASS,
  PRF_HTTP_JS_LOCAL,
  PRF_AUTH_USER,
  PRF_AUTH_PASS,
  PRF_NTPSERVER1,
  PRF_NTPSERVER2,
  PRF_NTPSERVER3,
  PRF_GMT_OFFSET,
  PRF_DAYLIGHT_OFFSET,
  PRF_INIT_DATE,
  PRF_INIT_TIME,
  PRF_PID_WINDOW,
  PRF_PID_KP,
  PRF_PID_KI,
  PRF_PID_KD,
  PRF_PID_POE,
  PRF_PID_TEMP_THRESHOLD,
  PRF_LOG_WINDOW,
  PRF_LOG_LIMIT,
  PRF_MIN_TEMP,
  PRF_MAX_TEMP,
  PRF_MAX_HOUSING_TEMP,
  PRF_THERMAL_RUN,
  PRF_ALARM_TIMEOUT,
  PRF_ERROR_GRACE_COUNT,
  PRF_DBG_SERIAL,
  PRF_DBG_SYSLOG,
  PRF_SYSLOG_SRV,
  PRF_SYSLOG_PORT,
  PRF_end
} PREFERENCES;

extern const char *PrefsName[];

typedef enum {
  NONE,
  UINT8,
  UINT16,
  INT16,
  INT32,
  STRING,
  VFLOAT,
} TYPE;

struct PrefsStruct {
  TYPE type;
  union {
    uint8_t uint8;
    uint16_t uint16;
    int16_t int16;
    int32_t int32;
    char *str;
    double vfloat;
  } value;
};

extern struct PrefsStruct Prefs[PRF_end];
extern File CSVFile, LOGFile;
extern const char *PVer;
extern const char *PDate;

#ifndef DEBUG
#define DEBUG 1
#endif
#ifndef MALLOC
#define MALLOC ps_malloc
#endif
#ifndef REALLOC
#define REALLOC ps_realloc
#endif
#define DBG if (DEBUG)

#if PK_FEATURE_SYSLOG
extern WiFiUDP udpClient;
extern Syslog syslog;
#endif

#define JS_UPLOT "https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.iife.min.js"
#define CSS_UPLOT "https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.min.css"

/* Local UI */
void load_msg(char msg[MAX_CHARS_PL]);
boolean return_LCD_string(char *msg, char *rest, int mod, uint16_t screen_w = SCREEN_W);
void LCD_Display_program_summary(int dir = 0, byte load_prg = 0);
void LCD_Display_quick_program(int dir = 0, byte pos = 0);
void LCD_display_mainv1();
void LCD_display_mainv2();
void LCD_display_mainv3(int dir = 0, byte ctrl = 0);
void LCD_display_main_view();
void LCD_display_menu();
void LCD_display_programs();
void LCD_Display_program_delete(int dir = 0, boolean pressed = 0);
void LCD_Display_program_full(int dir = 0);
void LCD_Display_info();
void LCD_Display_prefs(int dir = 0);
void LCD_Display_about();
void Setup_LCD(void);
void Setup_Input(void);
void LCD_Reconect_WiFi();

/* Program */
uint8_t Cleanup_program(uint8_t err = 0);
uint8_t Load_program(char *file = 0);
uint8_t Load_programs_dir();
void ABORT_Program(uint8_t error = 0);
void END_Program();
void PAUSE_Program();
void RESUME_Program();
void START_Program();
void Program_Setup();
void Load_program_to_run();
void Initialize_program_to_run();
void Update_program_step(uint8_t sstep, uint16_t stemp, uint16_t stime, uint16_t sdwell);
void rotate_selected_program(int dir);
int Find_selected_program();
boolean Erase_program_file();
void Program_recalculate_ETA(boolean dwell = false);
void Program_calculate_steps(boolean prg_start = false);
void SAFETY_Check();

/* Heat / SSR */
void Kiln_SetSSR(boolean on);
void Enable_SSR();
void Disable_SSR();
void Kiln_SSR2_Set(boolean on);
void Update_TemperatureA();
void Update_TemperatureB();
void Read_Energy_INPUT();
void Setup_Heat();
void Setup_Addons();
SPIClass *Kiln_Get_MAX_SPI();

/* EMR / alarm */
void Enable_EMR();
void Disable_EMR();
void START_Alarm();
void STOP_Alarm();
void Setup_EMR();
void Setup_Alarm();
void Setup_Housing_TC();
void Setup_SSR2();
void Setup_Energy();

/* Prefs / time / net / http */
void Setup_prefs(void);
void Load_prefs();
void Save_prefs();
boolean Change_prefs_value(String item, String value);
void Prefs_updated_hook();
void Setup_start_date();
void Return_Current_IP(IPAddress &lip);
void Disable_WiFi();
boolean Setup_WiFi();
boolean Restart_WiFi();
void SETUP_WebServer(void);
void STOP_WebServer();
void SETUP_Api(void);
void WebHost_Begin();
void WebHost_End();
void Generate_INDEX();
void Generate_LOGS_INDEX();

/* Logs / debug */
void Init_log_file();
void Add_log_line();
void Close_log_file();
void Clean_LOGS();
uint8_t Load_LOGS_Dir();

/* Live chart ring (RAM); CSV on LittleFS remains the durable archive */
void ChartRing_Clear();
void ChartRing_Push(uint32_t ts, int16_t kiln_c, int16_t case_c, int16_t energy);
uint32_t ChartRing_Gen();
size_t ChartRing_Count();
void ChartRing_PrintPointsJson(Print &out, uint32_t since_ts);
void dbgLog(uint16_t pri, const char *fmt, ...);
void initSysLog();
void initSerial();
void Restart_ESP();

/* Telemetry snapshot — Program_Loop publishes; HTTP (and others) read under spinlock */
struct KilnTelemetry {
  double kiln_temp;
  double case_temp;
  double int_temp;
  double set_temp;
  double pid_out;
  float temp_incr;
  uint16_t pid_window;
  uint16_t program_start_temp;
  int program_run_step;
  uint8_t program_run_size;
  uint8_t program_run_state;
  uint8_t program_error;
  time_t program_run_start;
  time_t program_run_end;
};
void Kiln_PublishTelemetry();
void Kiln_ReadTelemetry(KilnTelemetry *out);

/* LCD refresh requests — Program_Loop signals; Input_Loop draws (keeps UI off control path) */
#define LCD_REFRESH_NONE 0
#define LCD_REFRESH_MAIN_V1 1
#define LCD_REFRESH_MAIN_V2 2
void Kiln_RequestLcdRefresh(uint8_t which);
uint8_t Kiln_TakeLcdRefresh(void);

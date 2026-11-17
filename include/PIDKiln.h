#include <PID_v1.h>
#include <Syslog.h>
#include <WiFiUdp.h>
#include <FS.h>

/* 
** Some definitions - usually you should not edit this, but you may want to
*/
#define ENCODER0_PINA    34
#define ENCODER0_PINB    35
#define ENCODER0_BUTTON  32
#define ENCODER_BUTTON_DELAY 150  // 150ms between button press readout
#define ENCODER_ROTATE_DELAY 120  // 120ms between rotate readout
extern const uint16_t Long_Press; // long press button takes about 0,9 second

extern const int MAX_Prog_File_Size;  // maximum file size (bytes) that can be uploaded as program

/*
** Relays and thermocouple defs. and other addons
**
*/

#define EMR_RELAY_PIN 21
#define SSR1_RELAY_PIN 25   // was 19 — that pin is VSPI MISO used by the LCD
//#define SSR2_RELAY_PIN 22   // if you want to use additional SSR for second heater, uncoment this

// MAX31855 variables/defs
#define MAXCS1  27    // for hardware SPI - HSPI (MOSI-13, MISO-12, CLK-14) - 1st device CS-27
#define MAXCS2  15    // same SPI - 2nd device CS-15 (comment out if no second thermocouple)

// If you have power meter - uncoment this
//#define ENERGY_MON_PIN 33       // if you don't use - comment out

#define ALARM_PIN 26        // Pin goes high on abort
extern uint16_t ALARM_countdown; // countdown in seconds to stop alarm

/*
** Temperature, PID and probes variables/definitions
*/
extern double int_temp, kiln_temp, case_temp;
extern double set_temp, pid_out;
extern float temp_incr;
extern uint32_t windowStartTime;
#define PID_WINDOW_DIVIDER 1

extern PID KilnPID;

/*
** Global value of LCD screen/menu and menu position
**
*/
typedef enum {
  SCR_MAIN_VIEW,      // group of main screens showing running program
  SCR_MENU,           // menu
  SCR_PROGRAM_LIST,   // list of all programs
  SCR_PROGRAM_SHOW,   // showing program content
  SCR_PROGRAM_DELETE, // deleting program
  SCR_PROGRAM_FULL,   // step by step program display
  SCR_QUICK_PROGRAM,  // set manually desire, single program step
  SCR_ABOUT,          // short info screen
  SCR_PREFERENCES,    // show current preferences
  SCR_OTHER           // some other screens like about that are stateless
} LCD_State_enum;

typedef enum { // different main screens
  MAIN_VIEW1,
  MAIN_VIEW2,
  MAIN_VIEW3,
  MAIN_end
} LCD_MAIN_View_enum;

typedef enum { // menu positions
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

typedef enum { // program menu positions
  P_EXIT,
  P_SHOW,
  P_LOAD,
  P_DELETE,
  P_end
} LCD_PSCR_MENU_Item_enum;

extern const char *Prog_Menu_Names[];
extern const uint8_t Prog_Menu_Size;

#define SCREEN_W 128   // LCD screen width and height
#define SCREEN_H 64
#define MAX_CHARS_PL SCREEN_W/3  // char can have min. 3 points on screen

extern const uint8_t SCR_MENU_LINES;
extern const uint8_t SCR_MENU_SPACE;
extern const uint8_t SCR_MENU_MIDDLE;

/*
** Kiln program variables
**
*/
struct PROGRAM {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
};
// maxinum number of program lines (this goes to memory - so be careful)
#define MAX_PRG_LENGTH 40

extern PROGRAM Program[MAX_PRG_LENGTH];
extern uint8_t Program_size;
extern String Program_desc,Program_name;

extern PROGRAM* Program_run;
extern uint8_t Program_run_size;
extern char *Program_run_desc,*Program_run_name;
extern time_t Program_run_start;
extern time_t Program_run_end;
extern int Program_run_step;
extern uint16_t Program_start_temp;
extern uint8_t Program_error;
extern byte TempA_errors,TempB_errors;

typedef enum { // program menu positions
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

/* 
**  Program errors:
*/
typedef enum {
  PR_ERR_FILE_LOAD,       // failed to load file
  PR_ERR_TOO_LONG_LINE,   // program line too long
  PR_ERR_BAD_CHAR,        // not allowed character in program
  PR_ERR_TOO_HOT,         // exceeded max temperature defined in MAX_Temp
  PR_ERR_TOO_COLD,        // temperature redout below MIN_Temp
  PR_ERR_TOO_HOT_HOUSING, // housing temperature exceeded
  PR_ERR_MAX31A_NC,       // MAX31855 A not connected
  PR_ERR_MAX31A_INT_ERR,  // failed to read MAX31855 internal temperature on kiln
  PR_ERR_MAX31A_KPROBE,   // failed to read K-probe temperature on kiln
  PR_ERR_MAX31B_NC,       // MAX31855 B not connected
  PR_ERR_MAX31B_INT_ERR,  // failed to read MAX31855 internal temperature on case
  PR_ERR_MAX31B_KPROBE,   // failed to read K-probe temperature on case
  PR_ERR_USER_ABORT,      // user aborted
  PR_ERR_THERMAL_RUNAWAY, // kiln failed to heat within Thermal_Runaway window
  PR_ERR_end
} PROGRAM_ERROR_STATE;

/*
** Filesystem definintions
**
*/
#define MAX_FILENAME 30
#define MAX_PROGNAME 20

#include <PIDKilnLogic.h>
#define allowed_chars_in_filename PIDKILN_ALLOWED_FILENAME_CHARS

struct DIRECTORY {
  char filename[MAX_PROGNAME+1];
  uint16_t filesize;
  uint8_t sel;
};

extern DIRECTORY* Programs_DIR;
extern uint16_t Programs_DIR_size;

extern DIRECTORY* Logs_DIR;
extern uint16_t Logs_DIR_size;

#define PRG_DIRECTORY "/programs"
#define PRG_DIRECTORY_X(x) PRG_DIRECTORY x
extern const char *PRG_Directory;

extern const char *LOG_Directory;

#define FORMAT_LITTLEFS_IF_FAILED true

/*
**  Preference definitions
*/
#define PREFS_FILE "/etc/pidkiln.conf"

typedef enum {
  PRF_NONE,
  PRF_WIFI_SSID,
  PRF_WIFI_PASS,
  PRF_WIFI_MODE,
  PRF_WIFI_RETRY_CNT,
  PRF_WIFI_AP_NAME,
  PRF_WIFI_AP_USERNAME,
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

extern File CSVFile,LOGFile;

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
#define DBG if(DEBUG)

extern WiFiUDP udpClient;
extern Syslog syslog;

#define JS_JQUERY "https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js"
#define JS_CHART "https://cdn.jsdelivr.net/npm/chart.js@2.9.4/dist/Chart.bundle.min.js"
#define JS_CHART_DS "https://cdn.jsdelivr.net/npm/chartjs-plugin-datasource@0.1.0/dist/chartjs-plugin-datasource.min.js"

void load_msg(char msg[MAX_CHARS_PL]);
boolean return_LCD_string(char* msg,char* rest, int mod, uint16_t screen_w=SCREEN_W);
void LCD_Display_program_summary(int dir=0,byte load_prg=0);
void LCD_Display_quick_program(int dir=0,byte pos=0);

uint8_t Cleanup_program(uint8_t err=0);
uint8_t Load_program(char *file=0);
void ABORT_Program(uint8_t error=0);
void Kiln_SetSSR(boolean on);
void Enable_SSR();
void Disable_SSR();
void Enable_EMR();
void Disable_EMR();

struct KilnTelemetry {
  double kiln_temp;
  double case_temp;
  double int_temp;
  double set_temp;
  uint8_t program_run_state;
  uint8_t program_error;
};
void Kiln_PublishTelemetry();
void Kiln_ReadTelemetry(KilnTelemetry *out);

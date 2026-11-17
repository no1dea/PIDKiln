/*
** Single definition site for PIDKiln globals (header keeps extern + types).
*/
#include <Arduino.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <PID_v1.h>
#include <Syslog.h>
#include "PIDKiln.h"

const uint16_t Long_Press = 400;
const int MAX_Prog_File_Size = 10240;

uint16_t ALARM_countdown = 0;

double int_temp = 20, kiln_temp = 20, case_temp = 20;
double set_temp, pid_out;
float temp_incr = 0;
uint32_t windowStartTime;

PID KilnPID(&kiln_temp, &pid_out, &set_temp, 0, 0, 0, P_ON_E, DIRECT);

LCD_State_enum LCD_State = SCR_MAIN_VIEW;
LCD_MAIN_View_enum LCD_Main = MAIN_VIEW1;
LCD_SCR_MENU_Item_enum LCD_Menu = M_SCR_MAIN_VIEW;

const char *Menu_Names[] = {
  "1) Main view", "2) List programs", "3) Quick program", "4) Information",
  "5) Preferences", "6) Reconnect WiFi", "7) About", "8) Restart"
};

const char *Prog_Menu_Names[] = {"Exit", "Show", "Load", "Del."};
const uint8_t Prog_Menu_Size = 4;

const uint8_t SCR_MENU_LINES = 5;
const uint8_t SCR_MENU_SPACE = 2;
const uint8_t SCR_MENU_MIDDLE = 3;

PROGRAM Program[MAX_PRG_LENGTH];
uint8_t Program_size = 0;
String Program_desc, Program_name;

PROGRAM *Program_run = nullptr;
uint8_t Program_run_size = 0;
char *Program_run_desc = NULL, *Program_run_name = NULL;
time_t Program_run_start = 0;
time_t Program_run_end = 0;
int Program_run_step = -1;
uint16_t Program_start_temp = 0;
uint8_t Program_error = 0;
byte TempA_errors = 0, TempB_errors = 0;

PROGRAM_RUN_STATE Program_run_state = PR_NONE;
const char *Prog_Run_Names[] = {
  "unknown", "Ready", "Running", "Paused", "Aborted", "Ended", "Waiting"
};

DIRECTORY *Programs_DIR = NULL;
uint16_t Programs_DIR_size = 0;
DIRECTORY *Logs_DIR = NULL;
uint16_t Logs_DIR_size = 0;

const char *PRG_Directory = PRG_DIRECTORY;
const char *LOG_Directory = "/logs";

const char *PrefsName[] = {
  "None", "WiFi_SSID", "WiFi_Password", "WiFi_Mode", "WiFi_Retry_cnt", "WiFi_AP_Name",
  "WiFi_AP_Username", "WiFi_AP_Pass", "HTTP_Local_JS", "Auth_Username", "Auth_Password",
  "NTP_Server1", "NTP_Server2", "NTP_Server3", "GMT_Offset_sec", "Daylight_Offset_sec",
  "Initial_Date", "Initial_Time", "PID_Window", "PID_Kp", "PID_Ki", "PID_Kd", "PID_POE",
  "PID_Temp_Threshold", "LOG_Window", "LOG_Files_Limit", "MIN_Temperature", "MAX_Temperature",
  "MAX_Housing_Temperature", "Thermal_Runaway", "Alarm_Timeout", "MAX31855_Error_Grace_Count",
  "DBG_Serial", "DBG_Syslog", "DBG_Syslog_Srv", "DBG_Syslog_Port",
};

struct PrefsStruct Prefs[PRF_end];

File CSVFile, LOGFile;

const char *PVer = "PIDKiln v1.6";
const char *PDate = "2026.11.16";

WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

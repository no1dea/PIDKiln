/*
** No-op implementations when optional features are compiled out (src_filter).
*/
#include "PIDKiln.h"

#if !PK_FEATURE_LOCAL_UI
void load_msg(char msg[MAX_CHARS_PL]) { (void)msg; }
boolean return_LCD_string(char *msg, char *rest, int mod, uint16_t screen_w) {
  (void)msg;
  (void)rest;
  (void)mod;
  (void)screen_w;
  return false;
}
void LCD_Display_program_summary(int dir, byte load_prg) {
  (void)dir;
  (void)load_prg;
}
void LCD_Display_quick_program(int dir, byte pos) {
  (void)dir;
  (void)pos;
}
void LCD_display_mainv1() {}
void LCD_display_mainv2() {}
void LCD_display_mainv3(int dir, byte ctrl) {
  (void)dir;
  (void)ctrl;
}
void LCD_display_main_view() {}
void LCD_display_menu() {}
void LCD_display_programs() {}
void LCD_Display_program_delete(int dir, boolean pressed) {
  (void)dir;
  (void)pressed;
}
void LCD_Display_program_full(int dir) { (void)dir; }
void LCD_Display_info() {}
void LCD_Display_prefs(int dir) { (void)dir; }
void LCD_Display_about() {}
void Setup_LCD(void) {}
void Setup_Input(void) {}
#endif

#if !PK_FEATURE_EMR
void Enable_EMR() {}
void Disable_EMR() {}
void Setup_EMR() {}
#endif

#if !PK_FEATURE_ALARM
void START_Alarm() {}
void STOP_Alarm() {}
void Setup_Alarm() {}
#endif

#if !PK_FEATURE_SSR2
void Kiln_SSR2_Set(boolean on) { (void)on; }
void Setup_SSR2() {}
#endif

#if !PK_FEATURE_HOUSING_TC
void Update_TemperatureB() {}
void Setup_Housing_TC() {}
#endif

#if !PK_FEATURE_ENERGY
void Read_Energy_INPUT() {}
void Setup_Energy() {}
#endif

#if !PK_FEATURE_WIFI
void Disable_WiFi() {}
boolean Setup_WiFi() { return true; }
boolean Restart_WiFi() { return true; }
void Return_Current_IP(IPAddress &lip) { lip = IPAddress(0, 0, 0, 0); }
#endif

#if !PK_FEATURE_HTTP
void SETUP_WebServer(void) {}
void STOP_WebServer() {}
#endif

#if !PK_FEATURE_API
void SETUP_Api(void) {}
#endif

#if !(PK_FEATURE_HTTP || PK_FEATURE_API)
void WebHost_Begin() {}
void WebHost_End() {}
#endif

#if !PK_FEATURE_LOGS
void Init_log_file() {}
void Add_log_line() {}
void Close_log_file() {}
void Clean_LOGS() {}
uint8_t Load_LOGS_Dir() { return 0; }
#endif

#if !PK_FEATURE_SYSLOG
void initSysLog() {}
#endif

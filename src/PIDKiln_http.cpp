#include <WiFi.h>
#include <LittleFS.h>
#include <soc/efuse_reg.h>
#include <Esp.h>
#include <Update.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "PIDKiln.h"
#include "PIDKiln_webhost.h"
#if PK_FEATURE_LOCAL_UI
#include <U8g2lib.h>
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
#endif

boolean delete_file(File &newFile);
boolean check_valid_chars(byte a);
boolean valid_filename(char *file);

#define server WebHost_Server()
#define _webAuth WebHost_Auth

// Other variables
//
char *Errors;         // pointer to string if there are some errors during POST

/* Async template callbacks must return String — build once from a stack buffer. */
static String http_buf(const char *s) {
  return s ? String(s) : String();
}

static String http_fmt(const char *fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return String(buf);
}

static void read_first_line(File &file, char *out, size_t out_len) {
  if (!out || out_len == 0)
    return;
  out[0] = '\0';
  size_t i = 0;
  while (file.available() && i + 1 < out_len) {
    char c = (char)file.read();
    if (c == '\n' || c == '\r')
      break;
    out[i++] = c;
  }
  out[i] = '\0';
}


/*
** Core/main HTTP functions
**
*/

// Template preprocessor for preferences - preferences.html
//
String Preferences_parser(const String& var){

      if(var=="WiFi_SSID") return String(Prefs[PRF_WIFI_SSID].value.str);
 else if(var=="WiFi_Password") return String(Prefs[PRF_WIFI_PASS].value.str);
 else if(var=="WiFi_Mode0" && Prefs[PRF_WIFI_MODE].value.uint8==0) return "checked";
 else if(var=="WiFi_Mode1" && Prefs[PRF_WIFI_MODE].value.uint8==1) return "checked";
 else if(var=="WiFi_Mode2" && Prefs[PRF_WIFI_MODE].value.uint8==2) return "checked";
 else if(var=="WiFi_Mode3" && Prefs[PRF_WIFI_MODE].value.uint8==3) return "checked";
 else if(var=="WiFi_Retry_cnt") return String(Prefs[PRF_WIFI_RETRY_CNT].value.uint8);
 else if(var=="WiFi_AP_Name") return String(Prefs[PRF_WIFI_AP_NAME].value.str);
 else if(var=="WiFi_AP_Pass") return String(Prefs[PRF_WIFI_AP_PASS].value.str);

 else if(var=="HTTP_Local_JS0" && Prefs[PRF_HTTP_JS_LOCAL].value.uint8==0) return "checked";
 else if(var=="HTTP_Local_JS1" && Prefs[PRF_HTTP_JS_LOCAL].value.uint8==1) return "checked";
 
 else if(var=="Auth_Username") return String(Prefs[PRF_AUTH_USER].value.str);
 else if(var=="Auth_Password") return String(Prefs[PRF_AUTH_PASS].value.str);
 
 else if(var=="NTP_Server1") return String(Prefs[PRF_NTPSERVER1].value.str);
 else if(var=="NTP_Server2") return String(Prefs[PRF_NTPSERVER2].value.str);
 else if(var=="NTP_Server3") return String(Prefs[PRF_NTPSERVER3].value.str);
 else if(var=="GMT_Offset_sec") return String(Prefs[PRF_GMT_OFFSET].value.int16);
 else if(var=="Daylight_Offset_sec") return String(Prefs[PRF_DAYLIGHT_OFFSET].value.int16);
 else if(var=="Initial_Date") return String(Prefs[PRF_INIT_DATE].value.str);
 else if(var=="Initial_Time") return String(Prefs[PRF_INIT_TIME].value.str);
 
 else if(var=="MIN_Temperature") return String(Prefs[PRF_MIN_TEMP].value.uint8);
 else if(var=="MAX_Temperature") return String(Prefs[PRF_MAX_TEMP].value.uint16);
 else if(var=="MAX_Housing_Temperature") return String(Prefs[PRF_MAX_HOUSING_TEMP].value.uint16);
 else if(var=="Alarm_Timeout") return String(Prefs[PRF_ALARM_TIMEOUT].value.uint16);
 else if(var=="MAX31855_Error_Grace_Count") return String(Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);

 else if(var=="PID_Window") return String(Prefs[PRF_PID_WINDOW].value.uint16);
 else if(var=="PID_Kp") return String(Prefs[PRF_PID_KP].value.vfloat);
 else if(var=="PID_Ki") return String(Prefs[PRF_PID_KI].value.vfloat);
 else if(var=="PID_Kd") return String(Prefs[PRF_PID_KD].value.vfloat);
 else if(var=="PID_POE0" && Prefs[PRF_PID_POE].value.uint8==0) return "checked";
 else if(var=="PID_POE1" && Prefs[PRF_PID_POE].value.uint8==1) return "checked";
 else if(var=="PID_Temp_Threshold") return String(Prefs[PRF_PID_TEMP_THRESHOLD].value.int16);
 
 else if(var=="LOG_Window") return String(Prefs[PRF_LOG_WINDOW].value.uint16);
 else if(var=="LOG_Files_Limit") return String(Prefs[PRF_LOG_LIMIT].value.uint16);

 else if(var=="DBG_Serial0" && Prefs[PRF_DBG_SERIAL].value.uint8==0) return "selected";
 else if(var=="DBG_Serial1" && Prefs[PRF_DBG_SERIAL].value.uint8==1) return "selected";
 else if(var=="DBG_Syslog0" && Prefs[PRF_DBG_SYSLOG].value.uint8==0) return "selected";
 else if(var=="DBG_Syslog1" && Prefs[PRF_DBG_SYSLOG].value.uint8==1) return "selected";
 else if(var=="DBG_Syslog_Port") return String(Prefs[PRF_SYSLOG_PORT].value.uint16);
 else if(var=="DBG_Syslog_Srv") return String(Prefs[PRF_SYSLOG_SRV].value.str);

 else if(var=="ERRORS" && Errors){
  String out=http_fmt("<div class=error> There where errors: %s</div>", Errors);
  DBG dbgLog(LOG_ERR,"[HTTP] Errors pointer1:%p\n",Errors);
  free(Errors);Errors=NULL;
  return out;
 }
 return String();
}


// Template preprocessor for debug screen - debug_board.html
//
String Debug_ESP32(const String& var){

 //return String();

 // Main chip parameters
 //
 if(var=="CHIP_ID"){
   uint64_t chipid;
   char tmp[14];
   chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
   sprintf(tmp,"%04X%08X",(uint16_t)(chipid>>32),(uint32_t)chipid);
   DBG dbgLog(LOG_INFO,"[HTTP] Chip id: %s\n",tmp);
   return String(tmp);
 }else if (var=="SDK_VERSION") return String(ESP.getSdkVersion());
 else if (var=="CPU_FREQ") return String(ESP.getCpuFreqMHz());
 else if (var=="CHIP_REV") return String(ESP.getChipRevision());
 else if (var=="CHIP_MODEL") return String(ESP.getChipModel());
 else if (var=="CHIP_CORES") return String(ESP.getChipCores());
 else if (var=="CHIP_REVF") return String(REG_READ(EFUSE_BLK0_RDATA3_REG) >> 15, BIN);
 else if (var=="MAC_ADDRESS") return String(WiFi.macAddress());
 // SPI Flash RAM parameters
 //
 else if (var=="SFLASH_RAM"){
   float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
   DBG dbgLog(LOG_INFO,"[HTTP] flashChipSize: %f\n",flashChipSize);
   return String(flashChipSize);
 }else if (var=="FLASH_FREQ"){
   float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
   DBG dbgLog(LOG_INFO,"[HTTP] flashFreq: %f\n",flashFreq);
   return String(flashFreq);
 }else if (var=="SKETCH_SIZE"){
   float sketchSize = (float)ESP.getSketchSize() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] sketchSize: %f\n",sketchSize);
   return String(sketchSize);
 }else if (var=="SKETCH_TOTAL"){ // There is an error in ESP framework that shows Total space as freespace - wrong name for the function
   float freeSketchSpace = (float)ESP.getFreeSketchSpace() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] freeSketchSpace: %f\n",freeSketchSpace);
   return String(freeSketchSpace);
 }else if (var=="FLASH_MODE"){
   String mode;
   FlashMode_t fMode = (FlashMode_t)ESP.getFlashChipMode();
   if(fMode==FM_QIO) mode="QIO (0) Quad I/O - Fastest";
   else if(fMode==FM_QOUT) mode="QOUT (1) Quad Output - 15% slower than QIO";
   else if(fMode==FM_DIO) mode="DIO (2) Dual I/O - 45% slower than QIO";
   else if(fMode==FM_DOUT) mode="DOUT (3) Dual Output - 50% slower than QIO";
   else if(fMode==FM_FAST_READ) mode="FAST_READ (4)";
   else if(fMode==FM_SLOW_READ) mode="SLOW_READ (5)";
   else mode="Unknown";
   DBG dbgLog(LOG_DEBUG,"flashChipMode: %s\n",mode.c_str());
   DBG dbgLog(LOG_DEBUG,"flashChipMode: %d\n",(byte)fMode);
   return String(mode);
 }
 // PSRAM parameters
 //
 else if (var=="TOTAL_PSRAM"){
   float psramSize = (float)ESP.getPsramSize() / 1024;
   DBG dbgLog(LOG_INFO,"[HTTP] psramSize: %f\n",psramSize);
   return String(psramSize);
 }else if (var=="FREE_PSRAM"){
   float freePsram = (float)ESP.getFreePsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] freePsram: %f\n",freePsram);
   return String(freePsram);
 }else if (var=="SMALEST_PSRAM"){
   float minFreePsram = (float)ESP.getMinFreePsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] minFreePsram: %f\n",minFreePsram);
   return String(minFreePsram);
 }else if (var=="LARGEST_PSRAM"){
   float maxAllocPsram = (float)ESP.getMaxAllocPsram() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] maxAllocPsram: %f\n",maxAllocPsram);
   return String(maxAllocPsram);
 }
 // RAM parameters
 //
 else if (var=="TOTAL_HEAP"){
   float heapSize = (float)ESP.getHeapSize() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] heapSize: %f\n",heapSize);
   return String(heapSize);
 }else if (var=="FREE_HEAP"){
   float freeHeap = (float)ESP.getFreeHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] freeHeap: %f\n",freeHeap);
   return String(freeHeap);
 }else if (var=="SMALEST_HEAP"){
   float minFreeHeap = (float)ESP.getMinFreeHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] minFreeHeap: %f\n",minFreeHeap);
   return String(minFreeHeap);
 }else if (var=="LARGEST_HEAP"){
   float maxAllocHeap = (float)ESP.getMaxAllocHeap() / 1024;
   DBG dbgLog(LOG_DEBUG,"[HTTP] maxAllocHeap: %f\n",maxAllocHeap);
   return String(maxAllocHeap);
 }
 // LittleFS parameters
 //
 else if (var=="TOTAL_KB"){
   float fileTotalKB = (float)LittleFS.totalBytes() / 1024.0;
   return String(fileTotalKB);
 }
 else if (var=="USED_KB"){
   float fileUsedKB = (float)LittleFS.usedBytes() / 1024.0;
   return String(fileUsedKB);
 }
 else return String();
 
 DBG return String();
}


/* Single LittleFS HTML templates with <!--PIDKILN_ROWS--> injection point. */
static const char *ROWS_MARKER = "<!--PIDKILN_ROWS-->";

static void html_escape_write(AsyncResponseStream *response, const char *s) {
  if (!s)
    return;
  for (; *s; ++s) {
    switch (*s) {
    case '&':
      response->print("&amp;");
      break;
    case '<':
      response->print("&lt;");
      break;
    case '>':
      response->print("&gt;");
      break;
    case '"':
      response->print("&quot;");
      break;
    case '\'':
      response->print("&#39;");
      break;
    default:
      response->print(*s);
      break;
    }
  }
}

static String html_escape_string(const char *s) {
  String out;
  if (!s)
    return out;
  for (; *s; ++s) {
    switch (*s) {
    case '&':
      out += F("&amp;");
      break;
    case '<':
      out += F("&lt;");
      break;
    case '>':
      out += F("&gt;");
      break;
    case '"':
      out += F("&quot;");
      break;
    case '\'':
      out += F("&#39;");
      break;
    default:
      out += *s;
      break;
    }
  }
  return out;
}

/* Marker has no self-overlap; restart on mismatch is safe. */
static bool find_marker_offset(const char *path, const char *marker, size_t *at) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return false;
  size_t mlen = strlen(marker);
  size_t match = 0;
  size_t idx = 0;
  while (f.available()) {
    char c = (char)f.read();
    if (c == marker[match]) {
      match++;
      if (match == mlen) {
        *at = idx + 1 - mlen;
        f.close();
        return true;
      }
    } else if (c == marker[0]) {
      match = 1;
    } else {
      match = 0;
    }
    idx++;
  }
  f.close();
  return false;
}

static bool stream_fs_range(AsyncResponseStream *response, const char *path,
                            size_t start, size_t length) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return false;
  if (start && !f.seek(start)) {
    f.close();
    return false;
  }
  uint8_t buf[256];
  while (length) {
    size_t chunk = length > sizeof(buf) ? sizeof(buf) : length;
    size_t n = f.read(buf, chunk);
    if (!n)
      break;
    response->write(buf, n);
    length -= n;
  }
  f.close();
  return true;
}

static bool stream_fs_after(AsyncResponseStream *response, const char *path,
                            size_t start) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return false;
  size_t sz = f.size();
  f.close();
  if (start > sz)
    return false;
  return stream_fs_range(response, path, start, sz - start);
}

typedef void (*listing_rows_fn)(AsyncResponseStream *response);

static bool stream_listing_template(AsyncResponseStream *response, const char *path,
                                    listing_rows_fn emit_rows) {
  size_t at = 0;
  size_t mlen = strlen(ROWS_MARKER);
  if (!find_marker_offset(path, ROWS_MARKER, &at))
    return false;
  if (!stream_fs_range(response, path, 0, at))
    return false;
  if (emit_rows)
    emit_rows(response);
  return stream_fs_after(response, path, at + mlen);
}

static void write_program_rows(AsyncResponseStream *response) {
  for (uint16_t a = 0; a < Programs_DIR_size; a++) {
    char path[64];
    char desc[128];
    snprintf(path, sizeof(path), "%s/%s", PRG_Directory, Programs_DIR[a].filename);
    response->print(F("<tr><td><img src=\"/icons/heat.png\" alt=\"\"></td><td><a href=\""));
    html_escape_write(response, path);
    response->print(F("\" target=\"_blank\">"));
    html_escape_write(response, Programs_DIR[a].filename);
    response->print(F("</a></td><td>"));
    response->printf("%u", Programs_DIR[a].filesize);
    response->print(F("</td><td>"));
    File file = LittleFS.open(path, "r");
    if (file) {
      read_first_line(file, desc, sizeof(desc));
      file.close();
      html_escape_write(response, desc);
    } else {
      response->print(F("Error opening file to read description"));
    }
    response->print(F("</td><td><a href=\"/program_edit.html?prog="));
    html_escape_write(response, Programs_DIR[a].filename);
    response->print(F("\">edit</a></td><td><a href=\"/load.html?prog_name="));
    html_escape_write(response, Programs_DIR[a].filename);
    response->print(F("\">load</a></td><td><a href=\"/delete.html?prog_name="));
    html_escape_write(response, Programs_DIR[a].filename);
    response->print(F("\">delete</a></td></tr>\n"));
  }
}

static void write_log_rows(AsyncResponseStream *response) {
  for (uint16_t a = 0; a < Logs_DIR_size; a++) {
    char path[64];
    char desc[128];
    snprintf(path, sizeof(path), "%s/%s", LOG_Directory, Logs_DIR[a].filename);
    response->print(F("<tr><td><a href=\""));
    html_escape_write(response, path);
    response->print(F("\" target=\"_blank\">"));
    html_escape_write(response, Logs_DIR[a].filename);
    response->print(F("</a></td><td>"));
    response->printf("%u", Logs_DIR[a].filesize);
    response->print(F("</td><td>"));
    if (strstr(Logs_DIR[a].filename, ".log")) {
      File file = LittleFS.open(path, "r");
      if (file) {
        read_first_line(file, desc, sizeof(desc));
        file.close();
        html_escape_write(response, desc);
      }
    }
    response->print(F("</td></tr>\n"));
  }
}

// Dynamic /programs/index.html — stream template + inject rows (no FS rewrite).
//
void handleProgramsIndex(AsyncWebServerRequest *request) {
  if (!_webAuth(request))
    return;
  if (Load_programs_dir()) {
    request->send(500, "text/plain", "Cannot load programs directory");
    return;
  }
  size_t marker_at = 0;
  if (!find_marker_offset("/programs/index.html", ROWS_MARKER, &marker_at)) {
    request->send(500, "text/plain",
                  "Missing or invalid /programs/index.html (need <!--PIDKILN_ROWS-->)");
    return;
  }
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  stream_listing_template(response, "/programs/index.html", write_program_rows);
  request->send(response);
}

// Dynamic /logs/index.html
//
void handleLogsIndex(AsyncWebServerRequest *request) {
  if (!_webAuth(request))
    return;
  if (Load_LOGS_Dir()) {
    request->send(500, "text/plain", "Cannot load logs directory");
    return;
  }
  size_t marker_at = 0;
  if (!find_marker_offset("/logs/index.html", ROWS_MARKER, &marker_at)) {
    request->send(500, "text/plain",
                  "Missing or invalid /logs/index.html (need <!--PIDKILN_ROWS-->)");
    return;
  }
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  stream_listing_template(response, "/logs/index.html", write_log_rows);
  request->send(response);
}


/* HTTP UI live chart feed — RAM ring + program profile (not /api/v1). */
static void print_program_profile_json(AsyncResponseStream *response) {
  KilnTelemetry telem;
  Kiln_ReadTelemetry(&telem);
  response->print('[');
  if (!telem.program_run_size || !Program_run) {
    response->print(']');
    return;
  }
  time_t current_time = telem.program_run_start ? telem.program_run_start : time(NULL);
  double y0 = telem.program_start_temp ? (double)telem.program_start_temp : telem.kiln_temp;
  response->printf("{\"t\":%ld,\"y\":%.1f}", (long)current_time, y0);
  for (uint16_t a = 0; a < telem.program_run_size && a < Program_run_size; a++) {
    current_time += Program_run[a].togo * 60;
    response->printf(",{\"t\":%ld,\"y\":%u}", (long)current_time, Program_run[a].temp);
    current_time += Program_run[a].dwell * 60;
    response->printf(",{\"t\":%ld,\"y\":%u}", (long)current_time, Program_run[a].temp);
  }
  response->print(']');
}

static void handleChartJson(AsyncWebServerRequest *request) {
  if (!_webAuth(request))
    return;

  uint32_t since = 0;
  if (request->hasParam("since")) {
    since = (uint32_t)strtoul(request->getParam("since")->value().c_str(), NULL, 10);
  }
  /* Incremental polls keep program polyline client-side — omit to cut payload */
  const bool include_program = (since == 0);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->print('{');
  response->printf("\"gen\":%lu,", (unsigned long)ChartRing_Gen());
  response->print("\"log_path\":");
  if (CSVFile) {
    const char *path = CSVFile.path();
    response->print('"');
    if (path) {
      for (const char *p = path; *p; ++p) {
        if (*p == '"' || *p == '\\')
          response->print('\\');
        response->print(*p);
      }
    }
    response->print('"');
  } else {
    response->print("null");
  }
  response->print(",\"program\":");
  if (include_program)
    print_program_profile_json(response);
  else
    response->print("null");
  response->print(",\"points\":");
  ChartRing_PrintPointsJson(*response, since);
  response->print('}');
  request->send(response);
}


// Template preprocessor for main view - index.html, about and perhaps others
//
String About_parser(const String& var) {
  DBG dbgLog(LOG_DEBUG,var.c_str());
  if (var == "VERSION")
    return http_fmt("%s %s", PVer, PDate);
  return String();
}


// Manages new program upload
// Abort if file too big or contains not allowed characters
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
static File newFile;
static boolean abort=false;
static char upload_path[MAX_FILENAME + 1];

  // Abort file upload if file is too large or there are some not allowed characters in program
  if(abort){
    abort=false;
    if(newFile) delete_file(newFile);
    request->client()->abort();
    return;
  }


  // Checking how much has been uploaded - if more then MAX_Prog_File_Size - abort
  if(len+index>MAX_Prog_File_Size){
     DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded file too large! Aborting\n");
     request->send(406, "text/html", http_fmt(
       "<html><body><h1>File is too large!</h1> Current limit is %d<br><br><a href=/>Return to main view</a></body></html",
       MAX_Prog_File_Size));
     abort=true;
     return;
  }
    
  if(!index){
    snprintf(upload_path, sizeof(upload_path), "%s/%s", PRG_Directory, filename.c_str());
    DBG dbgLog(LOG_INFO,"UploadStart: %s\n", upload_path);
    
    // Check if declared file size in header is not too large
    if(request->hasHeader("Content-Length")){
      const AsyncWebHeader* h = request->getHeader("Content-Length");
      if(h->value().toInt()>MAX_Prog_File_Size){
        DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded file too large! Aborting\n");
        request->send(406, "text/html", http_fmt(
          "<html><body><h1>File is too large!</h1> Current limit is %d<br><br><a href=/programs/>Return to programs view</a></body></html",
          MAX_Prog_File_Size));
        abort=true;
        return;
      }
    }

    // Abort if filename is too long (otherwise esp will not write file to LittleFS silently!)
    if(strlen(upload_path)>MAX_FILENAME){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename is too large! Aborting\n");
      request->send(406, "text/html", http_fmt(
        "<html><body><h1>Filename is too long!</h1> Current limit is %d letters for directory and filename, so program name can be only %d <br><br><a href=/programs/>Return to programs view</a></body></html",
        MAX_FILENAME, MAX_PROGNAME));
      abort=true;
      return;
    }

    char tmp_filename[MAX_PROGNAME];
    strncpy(tmp_filename, filename.c_str(), sizeof(tmp_filename) - 1);
    tmp_filename[sizeof(tmp_filename) - 1] = '\0';
    // Abort if filename contains not allowed characters or trys to overwrite index.html
    if(!valid_filename(tmp_filename) || filename.compareTo("index.html")==0){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename containg bad characters! Aborting\n");
      request->send(406, "text/html",
        "<html><body><h1>Filename is bad!</h1> Filename contains not allowed characters - use letters, numbers and . _ signs <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }
    
    if (newFile) newFile.close();
    newFile = LittleFS.open(upload_path, "w");
  }
  DBG dbgLog(LOG_DEBUG,"[HTTP] Next iteration of file upload...\n");
  for(size_t i=0; i<len; i++){
    if(!check_valid_chars(data[i])){ // Basic sanitization - check for allowed characters
      request->send(200, "text/html",
        "<html><body><h1>File contains not allowed character(s)!</h1> You can use all letters, numbers and basic symbols in ASCII code.<br><br><a href=/>Return to main view</a></body></html");
      delete_file(newFile);
      DBG dbgLog(LOG_ERR,"[HTTP] Basic program check failed!\n");
      abort=true;
      return;
    }else newFile.write(data[i]);
  }
  if(final){
    newFile.flush();
    DBG dbgLog(LOG_DEBUG,"[HTTP] UploadEnd: %s, %d B\n", newFile.name(), newFile.size());
    newFile.close();

    char fname[22];
    strncpy(fname, filename.c_str(), sizeof(fname) - 1);
    fname[sizeof(fname) - 1] = '\0';
    DBG dbgLog(LOG_INFO,"[HTTP] Checking uploaded program structure: '%s'\n",fname); 
    uint8_t err=Load_program(fname);
    
    if(err){  // program did not validate correctly
      request->send(200, "text/html", http_fmt(
        "<html><body><h1>Program stucture is incorrect!</h1> Error code %u.<br><br><a href=/programs/>Return to programs</a></body></html",
        err));
      delete_file(newFile=LittleFS.open(upload_path, "r"));  // open again to close via existing helper
      DBG dbgLog(LOG_ERR,"[HTTP] Detailed program check failed!\n");
      abort=true; // this will never happend...
      request->redirect("/programs");
    }else{ // Everything went fine - commit file
      Generate_INDEX();
      request->redirect("/programs");
    }
  }
}


/* Filled by GET_Handle_Delete for ~PROGRAM_NAME~ substitution. */
static char g_delete_prog_name[MAX_PROGNAME + 1];

String Delete_parser(const String &var) {
  if (var == "PROGRAM_NAME")
    return html_escape_string(g_delete_prog_name);
  return String();
}

// Handle delete - second run, post - actual deletion
//
void POST_Handle_Delete(AsyncWebServerRequest *request) {
  if (!_webAuth(request))
    return;

  /* Rely on submit button name="yes" (not the display value). */
  if (request->hasParam("prog_name", true) && request->hasParam("yes", true)) {
    const AsyncWebParameter *p = request->getParam("prog_name", true);
    char path[32];
    sprintf(path, "%s/%.*s", PRG_Directory, MAX_PROGNAME, p->value().c_str());
    DBG dbgLog(LOG_DEBUG, "[HTTP] Removing program: %s with fpath:%s\n",
               p->value().c_str(), path);
    if (LittleFS.exists(path)) {
      LittleFS.remove(path);
      Generate_INDEX();
    }
  }
  request->redirect("/programs");
}

// Handle delete - first run, get - are you sure question
//
void GET_Handle_Delete(AsyncWebServerRequest *request) {
  DBG dbgLog(LOG_DEBUG, "[HTTP]  Request type: %d\n", request->method());
  DBG dbgLog(LOG_DEBUG, "[HTTP]  Request url: %s\n", request->url().c_str());

  if (!request->hasParam("prog_name") || !LittleFS.exists("/delete.html")) {
    DBG dbgLog(LOG_ERR, "[HTTP]  Failed to find GET prog_name or delete.html");
    request->redirect("/programs");
    return;
  }

  const AsyncWebParameter *p = request->getParam("prog_name");
  strncpy(g_delete_prog_name, p->value().c_str(), MAX_PROGNAME);
  g_delete_prog_name[MAX_PROGNAME] = '\0';

  request->send(LittleFS, "/delete.html", String(), false, Delete_parser);
}


// Load program from file to memory
//
void GET_Handle_Load(AsyncWebServerRequest *request){
char prname[MAX_FILENAME];

  if(!request->hasParam("prog_name")){  // if no program to load - skip
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to load program - no program name");
    request->redirect("/programs");
    return;
  }

  const AsyncWebParameter* p = request->getParam("prog_name");
  strcpy(prname,p->value().c_str());
  if(!Load_program(prname)){
    request->redirect("/index.html");
    Load_program_to_run();
    return;
  }else{
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to load program - Load_program() failed");
    request->redirect("/programs");
    return;
  }
}


// Handle prefs upload
//
void handlePrefs(AsyncWebServerRequest *request){
boolean save=false;

  if(!_webAuth(request)) return;

  int params = request->params();
  for(int i=0;i<params;i++){
    const AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Prefs parser POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("save")){
        save=true;
        continue;
      }else if(p->name().equalsIgnoreCase("update")){
        continue;
      }else if(!Change_prefs_value(p->name(),p->value())){
        DBG dbgLog(LOG_DEBUG,"[HTTP]!!! We have post error for %s with '%s'\n",p->name().c_str(),p->value().c_str());
        // we have some errors add new field to error list
        if(Errors!=NULL){
          DBG dbgLog(LOG_DEBUG,"[HTTP] Realloc call of size %d\n",(strlen(Errors)+p->name().length()+3)*sizeof(char));
          Errors=(char *)REALLOC(Errors,(strlen(Errors)+p->name().length()+3)*sizeof(char));
          strcat(Errors," ");
          strcat(Errors,p->name().c_str());
          DBG dbgLog(LOG_DEBUG,"[HTTP] Errors now:%s\n",Errors);
        }else{
          DBG dbgLog(LOG_DEBUG,"[HTTP] Malloc call of size %d\n",(p->name().length()+3)*sizeof(char));
          Errors=strdup(p->name().c_str());
          DBG dbgLog(LOG_DEBUG,"[HTTP] Errors now:%s\n",Errors);
        }
      }
    }
  }
  Prefs_updated_hook();
  if(save){
    Save_prefs();
  }
  request->send(LittleFS, "/preferences.html", String(), false, Preferences_parser);
}


// Handler for index.html with POST - program control
//
void handleIndexPost(AsyncWebServerRequest *request){
int params = request->params();

  if(!_webAuth(request)) return;

  for(int i=0;i<params;i++){
    const AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Index post parser: POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      if(p->name().equalsIgnoreCase("prog_start")){ // start program
        if(Program_run_state==PR_PAUSED) RESUME_Program();
        else START_Program();
      }else if(p->name().equalsIgnoreCase("prog_pause") && Program_run_state==PR_RUNNING){
        PAUSE_Program();
      }else if(p->name().equalsIgnoreCase("prog_end") && (Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED)){
        END_Program();
      }else if(p->name().equalsIgnoreCase("prog_abort")){
        ABORT_Program(PR_ERR_USER_ABORT);
      }
    }
  }
  request->send(LittleFS, "/index.html");
}


static void json_copy_escaped(char *dst, size_t dst_len, const char *src) {
  size_t j = 0;
  if (!dst || dst_len == 0)
    return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  for (size_t i = 0; src[i] && j + 2 < dst_len; i++) {
    char c = src[i];
    if (c == '"' || c == '\\')
      dst[j++] = '\\';
    dst[j++] = c;
  }
  dst[j] = '\0';
}

/* One-shot JSON for the dashboard poll — no LittleFS template / per-~VAR~ Strings */
static void handleVarsJson(AsyncWebServerRequest *request) {
  if (!_webAuth(request))
    return;

  KilnTelemetry telem;
  Kiln_ReadTelemetry(&telem);

  char curr_time[32] = "";
  char prog_start[32] = "";
  char prog_end[32] = "";
  char step_raw[48];
  char step_buf[64];
  char log_raw[64] = "/logs/test.csv";
  char log_path[72];
  struct tm timeinfo, *tmm;

  if (getLocalTime(&timeinfo))
    strftime(curr_time, sizeof(curr_time), "%F %T", &timeinfo);
  if (telem.program_run_start && (tmm = localtime(&telem.program_run_start)))
    strftime(prog_start, sizeof(prog_start), "%F %T", tmm);
  if (telem.program_run_end && (tmm = localtime(&telem.program_run_end)))
    strftime(prog_end, sizeof(prog_end), "%F %T", tmm);

  const char *label;
  if (telem.program_run_state == PR_RUNNING)
    label = (telem.temp_incr == 0) ? "Dwell" : "Running";
  else if (telem.program_run_state < PR_end)
    label = Prog_Run_Names[telem.program_run_state];
  else
    label = "unknown";
  snprintf(step_raw, sizeof(step_raw), "%d of %u - %s", telem.program_run_step + 1,
           telem.program_run_size, label);
  json_copy_escaped(step_buf, sizeof(step_buf), step_raw);

  if (CSVFile && CSVFile.path()) {
    strncpy(log_raw, CSVFile.path(), sizeof(log_raw) - 1);
    log_raw[sizeof(log_raw) - 1] = '\0';
  }
  json_copy_escaped(log_path, sizeof(log_path), log_raw);

  double heat_pct = PIDKiln_heat_percent(telem.pid_out, telem.pid_window, PID_WINDOW_DIVIDER);

  char body[768];
  snprintf(
      body, sizeof(body),
      "{\"program_status\":%u,\"log_file\":\"%s\",\"pidkiln\":["
      "{\"html_id\":\"#kiln_temp\",\"value\":\"%.2f\"},"
      "{\"html_id\":\"#set_temp\",\"value\":\"%.2f\"},"
      "{\"html_id\":\"#env_temp\",\"value\":\"%.2f\"},"
      "{\"html_id\":\"#case_temp\",\"value\":\"%.2f\"},"
      "{\"html_id\":\"#prog_start\",\"value\":\"%s\"},"
      "{\"html_id\":\"#prog_end\",\"value\":\"%s\"},"
      "{\"html_id\":\"#curr_time\",\"value\":\"%s\"},"
      "{\"html_id\":\"#heat_time\",\"value\":\"%.2f\"},"
      "{\"html_id\":\"#temp_change\",\"value\":\"%.3f\"},"
      "{\"html_id\":\"#step\",\"value\":\"%s\"}"
      "]}",
      (unsigned)telem.program_run_state, log_path, telem.kiln_temp, telem.set_temp,
      telem.int_temp, telem.case_temp, prog_start, prog_end, curr_time, heat_pct,
      (double)telem.temp_incr, step_buf);

  AsyncWebServerResponse *r =
      request->beginResponse(200, "application/json; charset=utf-8", body);
  request->send(r);
}


// Function(s) writing LCD screenshot over WWW - not perfect - but working
//
char *screenshot;
#if PK_FEATURE_LOCAL_UI
void out(const char *s){strcat(screenshot,s);}
void do_screenshot(AsyncWebServerRequest *request){

  screenshot=(char *)MALLOC(SCREEN_W*SCREEN_H*2*sizeof(char)+1);
  if(screenshot==NULL){
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to allocate memory for screenshot");
    request->send(500);
    return;
  }
  *screenshot='\0';

  AsyncResponseStream *response = request->beginResponseStream("image/x-portable-bitmap");
  response->addHeader("Server","ESP Async Web Server");
  response->addHeader("Content-Disposition","attachment; filename=\"PIDKiln_screenshot.pbm\"");

  u8g2_WriteBufferPBM2(u8g2.getU8g2(),out);
  response->println(screenshot);
  free(screenshot); screenshot=NULL;
  
  request->send(response);
}
#endif


// Firmware-only upload/update (from https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino).
// LittleFS images are not updated over HTTP — use PlatformIO uploadfs.
//
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  (void)filename;
  if (!index){
    DBG dbgLog(LOG_INFO,"[HTTP] Beginning firmware update\n");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Please wait while the device reboots...");
    response->addHeader("Refresh", "20; url=/about.html");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(1000);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      DBG Serial.println("[HTTP] Update complete");
      DBG Serial.flush();
      Restart_ESP();
    }
  }
}


/* 
** Setup Webserver UI routes (server begin is WebHost_Begin from net).
**
*/
void SETUP_WebServer(void) {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });

  server.serveStatic("/index.html", LittleFS, "/index.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);

  server.on("/index.html", HTTP_POST, handleIndexPost);
  
  // Local JS mode: large libs ship as precompressed *.gz only under data/js/.
  // send(LittleFS, logicalPath) falls back to path.gz and sets Content-Encoding: gzip.
  // Regenerate with scripts/compress_webassets.ps1 after bumping uPlot assets.
  if (Prefs[PRF_HTTP_JS_LOCAL].value.uint8) {
    server.on("/js/uPlot.iife.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!_webAuth(request))
        return;
      request->send(LittleFS, "/js/uPlot.iife.min.js", "text/javascript");
    });
    server.on("/css/uPlot.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!_webAuth(request))
        return;
      request->send(LittleFS, "/css/uPlot.min.css", "text/css");
    });
  } else {
    server.on("/js/uPlot.iife.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect(JS_UPLOT);
    });
    server.on("/css/uPlot.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect(CSS_UPLOT);
    });
  }

  server.on("/PIDKiln_vars.json", HTTP_GET, handleVarsJson);

  server.on("/PIDKiln_chart.json", HTTP_GET, handleChartJson);

  // Dynamic directory indexes (no rewritten index.html on LittleFS)
  server.on("/programs/", HTTP_GET, handleProgramsIndex);
  server.on("/programs/index.html", HTTP_GET, handleProgramsIndex);
  server.on("/logs/", HTTP_GET, handleLogsIndex);
  server.on("/logs/index.html", HTTP_GET, handleLogsIndex);
  server.serveStatic("/programs/", LittleFS, "/programs/").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  server.serveStatic("/logs/", LittleFS, "/logs/").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);

  // Upload new programs
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(200);
  }, handleUpload);

  server.on("/debug_board.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/debug_board.html", String(), false, Debug_ESP32);
  });

  server.on("/preferences.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/preferences.html", String(), false, Preferences_parser);
  });

  server.on("/preferences.html", HTTP_POST, handlePrefs);
  
  server.on("/delete.html", HTTP_GET, GET_Handle_Delete);
  
  server.on("/delete.html", HTTP_POST, POST_Handle_Delete);

  server.on("/load.html", HTTP_GET, GET_Handle_Load);

#if PK_FEATURE_LOCAL_UI
  server.on("/screenshot.html", HTTP_GET, do_screenshot);
#endif

  server.on("/program_edit.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/program_edit.html");
  });

  server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/about.html", String(), false, About_parser);
  });

  server.on("/flash_firmware.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/flash_firmware.html", String(), false, Preferences_parser);
  });
  
  server.on("/flash_firmware.html", HTTP_POST,
    [](AsyncWebServerRequest *request) { if(!_webAuth(request)) return; },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  
  // Serve some static data (serveStatic prefers *.gz when present)
  server.serveStatic("/icons/", LittleFS, "/icons/").setCacheControl("max-age=86400");
  server.serveStatic("/js/", LittleFS, "/js/").setCacheControl("max-age=86400");
  server.serveStatic("/css/", LittleFS, "/css/").setCacheControl("max-age=86400");
  // Do not serve PREFS_FILE over HTTP — it contains WiFi/auth secrets.
  server.serveStatic("/favicon.ico", LittleFS, "/icons/heat.png");
}


void STOP_WebServer() { WebHost_End(); }

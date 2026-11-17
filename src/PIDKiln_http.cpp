#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <soc/efuse_reg.h>
#include <Esp.h>
#include <Update.h>
#include "PIDKiln.h"
#if PK_FEATURE_LOCAL_UI
#include <U8g2lib.h>
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
#endif

boolean delete_file(File &newFile);
boolean check_valid_chars(byte a);
boolean valid_filename(char *file);
static bool _webAuth(AsyncWebServerRequest *request);

// Other variables
//
String template_str;  // Stores template pareser output
char *Errors;         // pointer to string if there are some errors during POST

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


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
  String out="<div class=error> There where errors: "+String(Errors)+"</div>";
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


static void stream_fs_text(AsyncResponseStream *response, const char *path){
  File f=LittleFS.open(path,"r");
  if(!f) return;
  while(f.available()){
    uint8_t buf[256];
    size_t n=f.read(buf, sizeof(buf));
    if(n) response->write(buf, n);
  }
  f.close();
}


// Dynamic /programs/index.html — no LittleFS rewrite of directory listings.
//
void handleProgramsIndex(AsyncWebServerRequest *request){
  if(!_webAuth(request)) return;
  if(Load_programs_dir()){
    request->send(500, "text/plain", "Cannot load programs directory");
    return;
  }
  AsyncResponseStream *response=request->beginResponseStream("text/html");
  stream_fs_text(response, "/prog_beg.txt");
  for(uint16_t a=0; a<Programs_DIR_size; a++){
    String tmp=String(PRG_Directory)+"/"+String(Programs_DIR[a].filename);
    response->printf("<tr><td><img src=\"/icons/heat.png\" alt=\"[ICO]\"></td>");
    response->printf("<td><a href=\"%s\" target=\"_blank\">%s</a></td>", tmp.c_str(), Programs_DIR[a].filename);
    response->printf("<td>%u</td>", Programs_DIR[a].filesize);
    File file=LittleFS.open(tmp.c_str(),"r");
    if(file){
      String desc=file.readStringUntil('\n');
      file.close();
      response->printf("<td>%s</td>", desc.c_str());
    }else{
      response->print("<td> Error opening file to read description </td>");
    }
    response->printf("<td><a href=/load.html?prog_name=%s>load</a></td>", Programs_DIR[a].filename);
    response->printf("<td><a href=/delete.html?prog_name=%s>delete</a></td>", Programs_DIR[a].filename);
    response->print("</tr>\n");
  }
  stream_fs_text(response, "/prog_end.txt");
  request->send(response);
}


// Dynamic /logs/index.html
//
void handleLogsIndex(AsyncWebServerRequest *request){
  if(!_webAuth(request)) return;
  if(Load_LOGS_Dir()){
    request->send(500, "text/plain", "Cannot load logs directory");
    return;
  }
  AsyncResponseStream *response=request->beginResponseStream("text/html");
  stream_fs_text(response, "/logs_beg.txt");
  for(uint16_t a=0; a<Logs_DIR_size; a++){
    String tmp=String(LOG_Directory)+"/"+String(Logs_DIR[a].filename);
    response->printf("<tr>");
    response->printf("<td><a href=\"%s\" target=\"_blank\">%s</a></td>", tmp.c_str(), Logs_DIR[a].filename);
    response->printf("<td>%u</td>", Logs_DIR[a].filesize);
    if(tmp.lastIndexOf(".log")>0){
      File file=LittleFS.open(tmp.c_str(),"r");
      if(file){
        String desc=file.readStringUntil('\n');
        file.close();
        response->printf("<td>%s</td>", desc.c_str());
      }else response->print("<td>  </td>");
    }else response->print("<td>  </td>");
    response->print("</tr>\n");
  }
  stream_fs_text(response, "/logs_end.txt");
  request->send(response);
}


// Template preprocessor for chart.js
//
String Chart_parser(const String& var) {
String tmp;
time_t current_time;
char *str;

  tmp=String();
  if(Program_run_start) current_time=Program_run_start;
  else current_time=time(NULL);

  if(var == "CHART_DATA" && Program_run_size>0){
    str=ctime(&current_time);str[strlen(str)-1]='\0';  // Dont know why - probably error, but ctime returns string with new line char and tab - so we cut it off
    tmp+="{x:'"+String(str)+"'";
    if(Program_start_temp) tmp+=",y:"+String(Program_start_temp)+"},";
    else tmp+=",y:"+String(kiln_temp)+"},";
    for(uint16_t a=0;a<Program_run_size;a++){
       if(a>0) tmp+=",";
       current_time+=Program_run[a].togo*60;
       str=ctime(&current_time);str[strlen(str)-1]='\0';  // Dont know why - probably error, but ctime returns string with new line char and tab - so we cut the tab
       //DBG Serial.printf("Seconds:%d \t Parsed:'%s'\n",current_time,str);
       tmp+="{x:'"+String(str)+"'";
       tmp+=",y:"+String(Program_run[a].temp)+"},";
       current_time+=Program_run[a].dwell*60;
       str=ctime(&current_time);str[strlen(str)-1]='\0';
       tmp+="{x:'"+String(str)+"'";
       tmp+=",y:"+String(Program_run[a].temp)+"}";
    }
    return tmp;
  }else if(var == "LOG_FILE"){
    if(CSVFile) return CSVFile.path();
    else return String("/logs/test.csv");
  }else if(var == "PROGRAM_NAME"){
    return String(Program_run_name);
  }else if(var == "CONFIG"){        // if we have log fie to show - show it on graph, otherwise show just program graph
    if(CSVFile) return String("config_with");
    else return String("config_without");
  }
  return String();
}


// Template preprocessor for main view - index.html, about and perhaps others
//
String About_parser(const String& var) {
String tmp;

  template_str=String();
  DBG dbgLog(LOG_DEBUG,var.c_str());
  if (var == "VERSION"){
    template_str+=PVer;
    template_str+=" ";
    template_str+=PDate;
  }
  
  DBG dbgLog(LOG_DEBUG,template_str.c_str());
  return template_str;
}


// Manages new program upload
// Abort if file too big or contains not allowed characters
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
static File newFile;
static boolean abort=false;
String tmp=String(PRG_Directory);


  tmp.concat("/");
  tmp.concat(filename.c_str());

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
     request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/>Return to main view</a></body></html");
     abort=true;
     return;
  }
    
  if(!index){
    DBG dbgLog(LOG_INFO,"UploadStart: %s\n", tmp.c_str() );
    
    // Check if declared file size in header is not too large
    if(request->hasHeader("Content-Length")){
      const AsyncWebHeader* h = request->getHeader("Content-Length");
      if(h->value().toInt()>MAX_Prog_File_Size){
        DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded file too large! Aborting\n");
        request->send(406, "text/html", "<html><body><h1>File is too large!</h1> Current limit is "+String(MAX_Prog_File_Size)+"<br><br><a href=/programs/>Return to programs view</a></body></html");
        abort=true;
        return;
      }
    }

    // Abort if filename is too long (otherwise esp will not write file to LittleFS silently!)
    if(tmp.length()>MAX_FILENAME){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename is too large! Aborting\n");
      request->send(406, "text/html", "<html><body><h1>Filename is too long!</h1> Current limit is "+String(MAX_FILENAME)+"letters for directory and filename, so program name can be only "+String(MAX_PROGNAME)+" <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }

    char tmp_filename[MAX_PROGNAME];
    strcpy(tmp_filename,filename.c_str());
    // Abort if filename contains not allowed characters or trys to overwrite index.html
    if(!valid_filename(tmp_filename) || filename.compareTo("index.html")==0){
      DBG dbgLog(LOG_DEBUG,"[HTTP] Uploaded filename containg bad characters! Aborting\n");
      request->send(406, "text/html", "<html><body><h1>Filename is bad!</h1> Filename contains not allowed characters - use letters, numbers and . _ signs <br><br><a href=/programs/>Return to programs view</a></body></html");
      abort=true;
      return;
    }
    
    if (newFile) newFile.close();
    newFile = LittleFS.open( tmp.c_str(), "w");
  }
  DBG dbgLog(LOG_DEBUG,"[HTTP] Next iteration of file upload...\n");
  for(size_t i=0; i<len; i++){
    if(!check_valid_chars(data[i])){ // Basic sanitization - check for allowed characters
      request->send(200, "text/html", "<html><body><h1>File contains not allowed character(s)!</h1> You can use all letters, numbers and basic symbols in ASCII code.<br><br><a href=/>Return to main view</a></body></html");
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
    strcpy(fname,filename.c_str());
    DBG dbgLog(LOG_INFO,"[HTTP] Checking uploaded program structure: '%s'\n",fname); 
    uint8_t err=Load_program(fname);
    
    if(err){  // program did not validate correctly
      request->send(200, "text/html", "<html><body><h1>Program stucture is incorrect!</h1> Error code "+String(err)+".<br><br><a href=/programs/>Return to programs</a></body></html");
      delete_file(newFile=LittleFS.open( tmp.c_str(), "r"));  // we need to open file again - to close it with already existing function
      DBG dbgLog(LOG_ERR,"[HTTP] Detailed program check failed!\n");
      abort=true; // this will never happend...
      request->redirect("/programs");
    }else{ // Everything went fine - commit file
      Generate_INDEX();
      request->redirect("/programs");
    }
  }
}


// Handle delete - second run, post - actual deletion
//
void POST_Handle_Delete(AsyncWebServerRequest *request){

  if(!_webAuth(request)) return;

  //Check if POST (but not File) parameter exists
  if(request->hasParam("prog_name", true) && request->hasParam("yes", true)){
    const AsyncWebParameter* p = request->getParam("yes", true);
    if(p->value().compareTo(String("Yes!"))==0){ // yes user want's to delete program
      const AsyncWebParameter* p = request->getParam("prog_name", true);
      char path[32];
      sprintf(path,"%s/%.*s",PRG_Directory,MAX_PROGNAME,p->value().c_str());
      DBG dbgLog(LOG_DEBUG,"[HTTP] Removing program: %s with fpath:%s\n",p->value().c_str(),path);
      if(LittleFS.exists(path)){
        LittleFS.remove(path);
        Generate_INDEX();
      }
    }
  }
  request->redirect("/programs");
}


// Handle delete - first run, get - are you sure question
//
void GET_Handle_Delete(AsyncWebServerRequest *request){
File tmpf;
String tmps;

  DBG dbgLog(LOG_DEBUG,"[HTTP]  Request type: %d\n",request->method());
  DBG dbgLog(LOG_DEBUG,"[HTTP]  Request url: %s\n",request->url().c_str());

  if(!request->hasParam("prog_name") || !(tmpf=LittleFS.open("/delete.html", "r")) ){  // if no program to delete - skip
    DBG dbgLog(LOG_ERR,"[HTTP]  Failed to find GET or open delete.html");
    request->redirect("/programs");
    return;
  }
 
  const AsyncWebParameter* p = request->getParam("prog_name");
  
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","ESP Async Web Server");

  DBG dbgLog(LOG_DEBUG,"[HTTP] Opened file is %s, program name is:%s\n",tmpf.path(),p->value().c_str());
  
  while(tmpf.available()){
    tmps=tmpf.readStringUntil('\n');
    tmps.replace("~PROGRAM_NAME~",p->value());
    //DBG Serial.printf("-:%s:\n",tmps.c_str());
    response->println(tmps.c_str());
  }

  tmpf.close();
  request->send(response);
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


// Handle json vars for js
//
String handleVars(const String& var){
char str[30];
struct tm timeinfo, *tmm;
KilnTelemetry telem;
Kiln_ReadTelemetry(&telem);

  if(var=="KILN_TEMP") return String(telem.kiln_temp);
  else if(var=="SET_TEMP") return String(telem.set_temp);
  else if(var=="ENV_TEMP") return String(telem.int_temp);
  else if(var=="CASE_TEMP") return String(telem.case_temp);
  else if(var=="HEAT_TIME") return String((pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100*PID_WINDOW_DIVIDER);
  else if(var=="TEMP_CHANGE") return String(temp_incr);
  else if(var=="STEP"){
    if(telem.program_run_state==PR_RUNNING){
      if(temp_incr==0) strcpy(str,"Dwell");
      else strcpy(str,"Running");
    }else strcpy(str,Prog_Run_Names[telem.program_run_state]);
    return String(Program_run_step+1)+" of "+String(Program_run_size)+" - "+String(str);
  }
  else if(var=="CURR_TIME"){
    if(getLocalTime(&timeinfo)){
      strftime (str, 29, "%F %T", &timeinfo);
      return String(str);
    }
  }else if(var=="PROG_START" && Program_run_start){
    tmm = localtime(&Program_run_start);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="PROG_END" && Program_run_end){
    tmm = localtime(&Program_run_end);
    strftime (str, 29, "%F %T", tmm);
    return String(str);
  }else if(var=="LOG_FILE"){
    if(CSVFile) return CSVFile.path();
    else return String("/logs/test.csv");
  }if(var=="PROGRAM_STATUS") return String(Program_run_state);
  else return String(" "); 
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


// Funnctin handling firmware upload/update (from https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino)
//
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
size_t content_len;
  
  if (!index){
    DBG dbgLog(LOG_INFO,"[HTTP] Beginning firmware update\n");
    content_len = request->contentLength();
    // FS image update: Arduino still uses U_SPIFFS for the filesystem partition.
    // Accept both legacy "spiffs" and current "littlefs" in the uploaded filename.
    String fnameLower = filename;
    fnameLower.toLowerCase();
    int cmd = (fnameLower.indexOf("spiffs") > -1 || fnameLower.indexOf("littlefs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
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


// Basic WEB authentication
//
static bool _webAuth(AsyncWebServerRequest *request){
    if(!request->authenticate(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str,NULL,false)) {
      request->requestAuthentication(NULL,false); // force basic auth
      return false;
    }
    return true;
}


/* 
** Setup Webserver screen 
**
*/
void SETUP_WebServer(void) {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/index.html");
  });

  server.serveStatic("/index.html", LittleFS, "/index.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  server.serveStatic("/index_local.html", LittleFS, "/index_local.html").setAuthentication(Prefs[PRF_AUTH_USER].value.str,Prefs[PRF_AUTH_PASS].value.str);
  
  server.on("/index.html", HTTP_POST, handleIndexPost);
  server.on("/index_local.html", HTTP_POST, handleIndexPost);
  
  server.on("/js/chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/js/chart.js", String(), false, Chart_parser);
  });


  // Local JS mode: large libs ship as precompressed *.gz only under data/js/.
  // send(LittleFS, logicalPath) falls back to path.gz and sets Content-Encoding: gzip.
  // Regenerate with scripts/compress_webassets.ps1 after bumping jquery/chart assets.
  if(Prefs[PRF_HTTP_JS_LOCAL].value.uint8){
    server.on("/js/jquery-3.7.1.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      if(!_webAuth(request)) return;
      request->send(LittleFS, "/js/jquery-3.7.1.min.js", "text/javascript");
    });
    server.on("/js/Chart.2.9.4.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      if(!_webAuth(request)) return;
      request->send(LittleFS, "/js/Chart.2.9.4.bundle.min.js", "text/javascript");
    });
    server.on("/js/chartjs-datasource.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      if(!_webAuth(request)) return;
      request->send(LittleFS, "/js/chartjs-datasource.min.js", "text/javascript");
    });
  }else{
    server.on("/js/jquery-3.7.1.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_JQUERY);
    });
    server.on("/js/Chart.2.9.4.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_CHART);
    });
    server.on("/js/chartjs-datasource.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->redirect(JS_CHART_DS);
    });
  }


  server.on("/PIDKiln_vars.json", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!_webAuth(request)) return;
    request->send(LittleFS, "/PIDKiln_vars.json", "application/json", false, handleVars);
  });

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

  server.onNotFound([](AsyncWebServerRequest *request){
    //request->send(404);
    request->send(404, "text/plain", "File not found");
  });

  // Start server
  server.begin();
}


// Stop/disable web server
//
void STOP_WebServer(){
  server.end();
}

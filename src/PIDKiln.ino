/*
** PIDKiln v1.6 - high temperature kiln PID controller for ESP32
**
** Copyright (C) 2019-2025 - Adrian Siemieniak
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
************
**
** Some coding convention - used functions are in separate files, depending on what they do. So LCD stuff to LCD, HTTP/WWW to HTTP, rotary to INPUT etc. 
** All variables beeing global are written with capital leter on start of each word (or at least they should be). All definitions are all capital letters.
**
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include "PIDKiln.h"
#include <PIDKilnLogic.h>

/* 
** Static, editable parameters. Some of them, can be replaces with PIDKiln preferences.
** Please set them up before uploading.
**
** TEMPLATE_PLACEHOLDER, DEBUG, MALLOC/REALLOC are set via platformio.ini build_flags.
*/


// Close cleanly file and delete file from LittleFS (use full path — name() may be basename-only)
//
boolean delete_file(File &newFile){
char filename[64];
 if(newFile){
    // Prefer full path when available (ESP32 Arduino 2.0+/3.x)
    if(newFile.path() && strlen(newFile.path()) > 0){
      strncpy(filename, newFile.path(), sizeof(filename) - 1);
    }else{
      strncpy(filename, newFile.name(), sizeof(filename) - 1);
    }
    filename[sizeof(filename) - 1] = '\0';
    DBG dbgLog(LOG_DEBUG,"[MAIN] Deleting uploaded file: \"%s\"\n",filename);
    newFile.flush();
    newFile.close();
    if(LittleFS.remove(filename)){
      DBG dbgLog(LOG_DEBUG,"[MAIN] Deleted!");
    }
    Generate_INDEX(); // Just in case user wanted to overwrite existing file
    return true;
 }else return false;
}


// Ensure required LittleFS directories exist (real directory FS, unlike SPIFFS flat layout)
//
void Ensure_FS_Dirs(){
  if(!LittleFS.exists(PRG_Directory)){
    LittleFS.mkdir(PRG_Directory);
  }
  if(!LittleFS.exists(LOG_Directory)){
    LittleFS.mkdir(LOG_Directory);
  }
  if(!LittleFS.exists("/etc")){
    LittleFS.mkdir("/etc");
  }
}


// Function check is uploaded file has only ASCII characters - this to be modified in future, perhaps to even narrowed down.
// Currently excluded are non printable characters, except new line and [] brackets. [] are excluded mostly for testing purpose.
// This way it should be faster then traversing char array
boolean check_valid_chars(byte a){
  if(PIDKiln_check_valid_chars(a)) return true;
  DBG Serial.print("[MAIN]  Faulty character ->");
  DBG Serial.print(a,DEC);
  DBG Serial.print(" ");
  DBG Serial.write(a);
  DBG Serial.println();
  return false;
}


// Check filename if it has only letters, numbers and . _ characters - other are not allowed for easy parsing and transfering
//
boolean valid_filename(char *file){
  return PIDKiln_valid_filename(file) ? true : false;
}


// Main setup that invokes other subsetups to initialize other modules
//
void setup() {
  char msg[MAX_CHARS_PL];

  // Keep core WDTs enabled so a stalled Program_Loop cannot leave SSR stuck ON.
  // AsyncTCP feeds the task WDT itself; do not blanket-disable IDLE WDTs.

  // Initialize prefs array with default values
  Setup_prefs();

  // Serial port for debugging purposes
  initSerial();

  // Initialize LittleFS
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    DBG dbgLog(LOG_DEBUG,"[MAIN] An Error has occurred while mounting LittleFS\n");
    return;
  }
  Ensure_FS_Dirs();

  // Load all preferences from disk
  Load_prefs();
  
  // Setup function for LCD display from PIDKiln_LCD.ino
  Setup_LCD();

  // Setup input devices
  Setup_Input();
  
  DBG dbgLog(LOG_DEBUG,"WiFi mode: %d, Retry count: %d, is wifi enabled: %d\n",Prefs[PRF_WIFI_MODE].value.uint8,Prefs[PRF_WIFI_RETRY_CNT].value.uint8,Prefs[PRF_WIFI_SSID].type);
  
  // Connect to WiFi if enabled
  if(Prefs[PRF_WIFI_MODE].value.uint8){ // If we want to have WiFi
    strcpy(msg,"connecting WiFi..");
    load_msg(msg);
    if(Setup_WiFi()){    // !!! Wifi connection FAILED
      DBG dbgLog(LOG_ERR,"[MAIN] WiFi connection failed\n");
      strcpy(msg," WiFi con. failed");
      load_msg(msg);
    }else{
      IPAddress lips;
     
      Return_Current_IP(lips); 
      DBG Serial.println(lips); // Print ESP32 Local IP Address
      
      sprintf(msg," IP: %s",lips.toString().c_str());
      load_msg(msg);
    }
  }else{
    // If we don't have Internet - assume there is no time set
    Setup_start_date(); // in PIDKiln_net
    Disable_WiFi();
    strcpy(msg,"   -- Started! --");
    load_msg(msg);
  }

  // (re)generate programs index file /programs/index.html
  Generate_INDEX();

  // Loads logs index
  Load_LOGS_Dir();
  
  // Clean logs (if neede) on start - this will also call logs index generator
  Clean_LOGS();

  // Generate log index after cleanup
  Generate_LOGS_INDEX();

  // Setup program module
  Program_Setup();

  // Setup all sensors/relays
  Setup_Addons();
}


// Just a tiny loop - to be deleted ;)
//
void loop() {
  vTaskDelete(NULL);
}

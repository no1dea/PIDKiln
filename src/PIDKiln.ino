/*
** PIDKiln v1.6 - high temperature kiln PID controller for ESP32
**
** Copyright (C) 2019-2025 - Adrian Siemieniak
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License.
*/
#include <FS.h>
#include <LittleFS.h>
#include "PIDKiln.h"
#include <PIDKilnLogic.h>

boolean delete_file(File &newFile) {
  char filename[64];
  if (newFile) {
    if (newFile.path() && strlen(newFile.path()) > 0) {
      strncpy(filename, newFile.path(), sizeof(filename) - 1);
    } else {
      strncpy(filename, newFile.name(), sizeof(filename) - 1);
    }
    filename[sizeof(filename) - 1] = '\0';
    DBG dbgLog(LOG_DEBUG, "[MAIN] Deleting uploaded file: \"%s\"\n", filename);
    newFile.flush();
    newFile.close();
    if (LittleFS.remove(filename)) {
      DBG dbgLog(LOG_DEBUG, "[MAIN] Deleted!");
    }
    Generate_INDEX();
    return true;
  } else
    return false;
}

void Ensure_FS_Dirs() {
  if (!LittleFS.exists(PRG_Directory)) {
    LittleFS.mkdir(PRG_Directory);
  }
  if (!LittleFS.exists(LOG_Directory)) {
    LittleFS.mkdir(LOG_Directory);
  }
  if (!LittleFS.exists("/etc")) {
    LittleFS.mkdir("/etc");
  }
}

boolean check_valid_chars(byte a) {
  if (PIDKiln_check_valid_chars(a))
    return true;
  DBG Serial.print("[MAIN]  Faulty character ->");
  DBG Serial.print(a, DEC);
  DBG Serial.print(" ");
  DBG Serial.write(a);
  DBG Serial.println();
  return false;
}

boolean valid_filename(char *file) {
  return PIDKiln_valid_filename(file) ? true : false;
}

void setup() {
  char msg[MAX_CHARS_PL];

  Setup_prefs();
  initSerial();

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    DBG dbgLog(LOG_DEBUG, "[MAIN] An Error has occurred while mounting LittleFS\n");
    return;
  }
  Ensure_FS_Dirs();
  Load_prefs();

#if PK_FEATURE_LOCAL_UI
  Setup_LCD();
  Setup_Input();
#endif

#if PK_FEATURE_WIFI
  DBG dbgLog(LOG_DEBUG,
             "WiFi mode: %d, Retry count: %d, is wifi enabled: %d\n",
             Prefs[PRF_WIFI_MODE].value.uint8, Prefs[PRF_WIFI_RETRY_CNT].value.uint8,
             Prefs[PRF_WIFI_SSID].type);

  if (Prefs[PRF_WIFI_MODE].value.uint8) {
    strcpy(msg, "connecting WiFi..");
    load_msg(msg);
    if (Setup_WiFi()) {
      DBG dbgLog(LOG_ERR, "[MAIN] WiFi connection failed\n");
      strcpy(msg, " WiFi con. failed");
      load_msg(msg);
    } else {
      IPAddress lips;
      Return_Current_IP(lips);
      DBG Serial.println(lips);
      sprintf(msg, " IP: %s", lips.toString().c_str());
      load_msg(msg);
    }
  } else {
    Setup_start_date();
    Disable_WiFi();
    strcpy(msg, "   -- Started! --");
    load_msg(msg);
  }
#else
  Setup_start_date();
  strcpy(msg, "   -- Started! --");
  load_msg(msg);
#endif

  Generate_INDEX();

#if PK_FEATURE_LOGS
  Load_LOGS_Dir();
  Clean_LOGS();
  Generate_LOGS_INDEX();
#endif

  Program_Setup();
  Setup_Addons();
}

void loop() { vTaskDelete(NULL); }

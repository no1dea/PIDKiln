#include <LittleFS.h>
#include "PIDKiln_http_internal.h"

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
    snprintf(path, sizeof(path), "%s/%.*s", PRG_Directory, MAX_PROGNAME, p->value().c_str());
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
  if (!_webAuth(request))
    return;

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

  if (!_webAuth(request))
    return;

  if(!request->hasParam("prog_name")){  // if no program to load - skip
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to load program - no program name");
    request->redirect("/programs");
    return;
  }

  const AsyncWebParameter* p = request->getParam("prog_name");
  strncpy(prname, p->value().c_str(), MAX_FILENAME - 1);
  prname[MAX_FILENAME - 1] = '\0';
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

#include "PIDKiln.h"
#include "PIDKiln_webhost.h"

#if PK_FEATURE_HTTP || PK_FEATURE_API

static AsyncWebServer s_server(80);
static bool s_begun = false;

AsyncWebServer &WebHost_Server() { return s_server; }

bool WebHost_Auth(AsyncWebServerRequest *request) {
  if (!request->authenticate(Prefs[PRF_AUTH_USER].value.str,
                             Prefs[PRF_AUTH_PASS].value.str, NULL, false)) {
    request->requestAuthentication(NULL, false);
    return false;
  }
  return true;
}

void WebHost_Begin() {
  if (s_begun)
    return;
  s_server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "application/json",
                  "{\"err\":\"not_found\",\"msg\":\"not found\",\"code\":404}");
  });
  s_server.begin();
  s_begun = true;
  DBG dbgLog(LOG_INFO, "[WEBHOST] AsyncWebServer started on :80\n");
}

void WebHost_End() {
  if (!s_begun)
    return;
  s_server.end();
  s_begun = false;
  DBG dbgLog(LOG_INFO, "[WEBHOST] AsyncWebServer stopped\n");
}

#endif

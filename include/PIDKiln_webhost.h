#pragma once

#include "PIDKiln_features.h"

#if PK_FEATURE_HTTP || PK_FEATURE_API
#include <ESPAsyncWebServer.h>

AsyncWebServer &WebHost_Server();
bool WebHost_Auth(AsyncWebServerRequest *request);
void WebHost_Begin();
void WebHost_End();
#endif

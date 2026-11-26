#pragma once
/*
 * Shared helpers for PIDKiln_http*.cpp modules.
 */
#include "PIDKiln.h"
#include "PIDKiln_webhost.h"

#define server WebHost_Server()
#define _webAuth WebHost_Auth

String html_escape_string(const char *s);

void POST_Handle_Delete(AsyncWebServerRequest *request);
void GET_Handle_Delete(AsyncWebServerRequest *request);
void GET_Handle_Load(AsyncWebServerRequest *request);
void handleDoUpdate(AsyncWebServerRequest *request, const String &filename, size_t index,
                    uint8_t *data, size_t len, bool final);
#if PK_FEATURE_LOCAL_UI
void do_screenshot(AsyncWebServerRequest *request);
#endif

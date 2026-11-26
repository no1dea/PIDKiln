/*
** Compact JSON remote API (/api/v1) — peer module to the HTML HTTP UI.
*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "PIDKiln.h"
#include "PIDKiln_webhost.h"

#define server WebHost_Server()

static void api_send_json(AsyncWebServerRequest *request, int code, const char *body) {
  AsyncWebServerResponse *r =
      request->beginResponse(code, "application/json; charset=utf-8", body);
  request->send(r);
}

static void api_err(AsyncWebServerRequest *request, int code, const char *err,
                    const char *msg) {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "{\"err\":\"%s\",\"msg\":\"%s\",\"code\":%d}", err, msg, code);
  api_send_json(request, code, buf);
}

static void api_get_root(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  api_send_json(request, 200,
                "{\"api\":\"pidkiln\",\"v\":1,\"features\":[\"run\",\"programs\"]}");
}

static void api_get_health(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"uptime_s\":%lu}",
           (unsigned long)(millis() / 1000UL));
  api_send_json(request, 200, buf);
}

static void api_get_status(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;

  KilnTelemetry t;
  Kiln_ReadTelemetry(&t);

  int heat_pct = (int)PIDKiln_heat_percent(t.pid_out, t.pid_window, PID_WINDOW_DIVIDER);
  if (heat_pct < 0)
    heat_pct = 0;
  if (heat_pct > 100)
    heat_pct = 100;

  char name[MAX_PROGNAME + 1];
  name[0] = '\0';
  if (Program_run_name && Program_run_name[0]) {
    strncpy(name, Program_run_name, MAX_PROGNAME);
    name[MAX_PROGNAME] = '\0';
  }

  char buf[512];
  int n = snprintf(
      buf, sizeof(buf),
      "{\"kiln_c\":%.1f,\"set_c\":%.1f,\"case_c\":%.1f,\"int_c\":%.1f,"
      "\"state\":%u,\"error\":%u,\"step\":%d,\"steps\":%u,\"heat_pct\":%d,"
      "\"ramp_cps\":%.3f",
      t.kiln_temp, t.set_temp, t.case_temp, t.int_temp, (unsigned)t.program_run_state,
      (unsigned)t.program_error, t.program_run_step < 0 ? -1 : t.program_run_step + 1,
      (unsigned)t.program_run_size, heat_pct, (double)t.temp_incr);

  if (n > 0 && (size_t)n < sizeof(buf) && name[0]) {
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, ",\"run_name\":\"%s\"", name);
  }
  if (n > 0 && (size_t)n < sizeof(buf) && t.program_run_start) {
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, ",\"start_ts\":%ld",
                  (long)t.program_run_start);
  }
  if (n > 0 && (size_t)n < sizeof(buf) && t.program_run_end) {
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, ",\"eta_ts\":%ld",
                  (long)t.program_run_end);
  }
  if (n > 0 && (size_t)n < sizeof(buf)) {
    snprintf(buf + n, sizeof(buf) - (size_t)n, "}");
  }
  api_send_json(request, 200, buf);
}

static void api_run_start(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  if (Program_run_state == PR_PAUSED) {
    RESUME_Program();
    request->send(204);
    return;
  }
  if (Program_run_state == PR_NONE) {
    api_err(request, 409, "conflict", "no program loaded");
    return;
  }
  if (Program_run_state == PR_RUNNING || Program_run_state == PR_THRESHOLD) {
    api_err(request, 409, "conflict", "already running");
    return;
  }
  START_Program();
  if (Program_run_state != PR_RUNNING && Program_run_state != PR_THRESHOLD) {
    api_err(request, 409, "conflict", "cannot start");
    return;
  }
  request->send(204);
}

static void api_run_pause(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  if (Program_run_state != PR_RUNNING) {
    api_err(request, 409, "conflict", "not running");
    return;
  }
  PAUSE_Program();
  request->send(204);
}

static void api_run_resume(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  if (Program_run_state != PR_PAUSED) {
    api_err(request, 409, "conflict", "not paused");
    return;
  }
  RESUME_Program();
  request->send(204);
}

static void api_run_end(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  if (Program_run_state != PR_RUNNING && Program_run_state != PR_PAUSED &&
      Program_run_state != PR_THRESHOLD) {
    api_err(request, 409, "conflict", "not active");
    return;
  }
  END_Program();
  request->send(204);
}

static void api_run_abort(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;
  if (Program_run_state != PR_RUNNING && Program_run_state != PR_PAUSED &&
      Program_run_state != PR_THRESHOLD) {
    api_err(request, 409, "conflict", "not active");
    return;
  }
  ABORT_Program(PR_ERR_USER_ABORT);
  request->send(204);
}

/* Accumulate small JSON body for programs/load */
static char s_load_body[160];
static size_t s_load_len = 0;

static bool api_extract_name(const char *json, char *out, size_t out_len) {
  if (!json || !out || out_len < 2)
    return false;
  const char *p = strstr(json, "\"name\"");
  if (!p)
    return false;
  p = strchr(p + 6, ':');
  if (!p)
    return false;
  p++;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p != '"')
    return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < out_len) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

static void api_programs_load_body(AsyncWebServerRequest *request, uint8_t *data,
                                   size_t len, size_t index, size_t total) {
  (void)request;
  (void)total;
  if (index == 0)
    s_load_len = 0;
  if (s_load_len + len >= sizeof(s_load_body))
    len = sizeof(s_load_body) - 1 - s_load_len;
  if (len) {
    memcpy(s_load_body + s_load_len, data, len);
    s_load_len += len;
    s_load_body[s_load_len] = '\0';
  }
}

static void api_programs_load(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;

  char name[MAX_FILENAME];
  name[0] = '\0';

  if (request->hasParam("name", true)) {
    strncpy(name, request->getParam("name", true)->value().c_str(),
            sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  } else if (request->hasParam("name")) {
    strncpy(name, request->getParam("name")->value().c_str(), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  } else if (s_load_len > 0) {
    api_extract_name(s_load_body, name, sizeof(name));
  }

  s_load_len = 0;
  s_load_body[0] = '\0';

  if (!name[0] || !PIDKiln_valid_filename(name)) {
    api_err(request, 400, "bad_request", "missing or invalid name");
    return;
  }

  if (Load_program(name)) {
    api_err(request, 404, "not_found", "load failed");
    return;
  }
  Load_program_to_run();
  request->send(204);
}

static void api_get_run(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;

  KilnTelemetry t;
  Kiln_ReadTelemetry(&t);

  char name[MAX_PROGNAME + 1];
  name[0] = '\0';
  if (Program_run_name && Program_run_name[0]) {
    strncpy(name, Program_run_name, MAX_PROGNAME);
    name[MAX_PROGNAME] = '\0';
  }

  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"state\":%u,\"error\":%u,\"step\":%d,\"steps\":%u,\"name\":\"%s\","
           "\"start_ts\":%ld,\"eta_ts\":%ld}",
           (unsigned)t.program_run_state, (unsigned)t.program_error,
           t.program_run_step < 0 ? -1 : t.program_run_step + 1,
           (unsigned)t.program_run_size, name, (long)t.program_run_start,
           (long)t.program_run_end);
  api_send_json(request, 200, buf);
}

static void api_get_telemetry(AsyncWebServerRequest *request) {
  /* Same payload as /status for clients that prefer a telemetry-named path. */
  api_get_status(request);
}

static void api_get_programs(AsyncWebServerRequest *request) {
  if (!WebHost_Auth(request))
    return;

  AsyncResponseStream *response =
      request->beginResponseStream("application/json; charset=utf-8");
  response->print("{\"programs\":[");
  bool first = true;
  for (uint16_t a = 0; a < Programs_DIR_size; a++) {
    if (!Programs_DIR[a].filename[0])
      continue;
    if (!first)
      response->print(",");
    first = false;
    response->printf("{\"name\":\"%s\",\"bytes\":%u,\"sel\":%u}",
                     Programs_DIR[a].filename, (unsigned)Programs_DIR[a].filesize,
                     (unsigned)Programs_DIR[a].sel);
  }
  response->print("]}");
  request->send(response);
}

void SETUP_Api(void) {
  server.on("/api/v1", HTTP_GET, api_get_root);
  server.on("/api/v1/", HTTP_GET, api_get_root);
  server.on("/api/v1/health", HTTP_GET, api_get_health);
  server.on("/api/v1/status", HTTP_GET, api_get_status);
  server.on("/api/v1/telemetry", HTTP_GET, api_get_telemetry);
  server.on("/api/v1/run", HTTP_GET, api_get_run);
  server.on("/api/v1/programs", HTTP_GET, api_get_programs);

  server.on("/api/v1/run/start", HTTP_POST, api_run_start);
  server.on("/api/v1/run/pause", HTTP_POST, api_run_pause);
  server.on("/api/v1/run/resume", HTTP_POST, api_run_resume);
  server.on("/api/v1/run/end", HTTP_POST, api_run_end);
  server.on("/api/v1/run/abort", HTTP_POST, api_run_abort);

  server.on(
      "/api/v1/programs/load", HTTP_POST, api_programs_load, NULL,
      api_programs_load_body);

  DBG dbgLog(LOG_INFO, "[API] /api/v1 routes registered\n");
}

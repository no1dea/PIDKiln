#include "PIDKilnLogic.h"

#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

const char PIDKILN_ALLOWED_FILENAME_CHARS[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890._";

int PIDKiln_check_valid_chars(unsigned char a) {
  if (a == 0 || a == 9 || a == 95)
    return 1; // EOF, tab, _
  if (a == 10 || a == 13)
    return 1; // LF, CR
  if (a == '{' || a == '}' || a == '[' || a == ']' || a == '\\')
    return 1;
  if (a >= 32 && a <= 90)
    return 1;
  if (a >= 97 && a <= 122)
    return 1;
  return 0;
}

int PIDKiln_valid_filename(const char *file) {
  if (!file || !*file)
    return 0;
  char c;
  while ((c = *file++)) {
    if (strchr(PIDKILN_ALLOWED_FILENAME_CHARS, c))
      continue;
    return 0;
  }
  return 1;
}

uint16_t PIDKiln_rate_to_togo_min(int start_c, int target_c, uint16_t rate_c_per_h) {
  if (rate_c_per_h == 0)
    return 0;
  if (rate_c_per_h >= PIDKILN_RATE_FULL_POWER)
    return 1;
  int delta = target_c - start_c;
  if (delta < 0)
    delta = -delta;
  if (delta == 0)
    return 1;
  uint32_t num = (uint32_t)delta * 60u;
  uint32_t mins = (num + (uint32_t)rate_c_per_h - 1u) / (uint32_t)rate_c_per_h;
  if (mins < 1u)
    mins = 1u;
  if (mins > 65535u)
    mins = 65535u;
  return (uint16_t)mins;
}

void PIDKiln_resolve_rate_step_togo(PIDKiln_ProgramStep *step, uint16_t rate_c_per_h, int start_c) {
  if (!step || rate_c_per_h == 0)
    return;
  step->togo = PIDKiln_rate_to_togo_min(start_c, (int)step->temp, rate_c_per_h);
}

static int copy_cstr_limited(char *dst, size_t dst_sz, const char *src) {
  if (!dst || dst_sz == 0)
    return 0;
  dst[0] = '\0';
  if (!src)
    return 1;
  size_t n = strlen(src);
  if (n >= dst_sz)
    n = dst_sz - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
  return 1;
}

static int json_int_u16(JsonVariantConst v, uint16_t *out) {
  if (!out || v.isNull() || !v.is<int>())
    return 0;
  int n = v.as<int>();
  if (n < 0 || n > 65535)
    return 0;
  *out = (uint16_t)n;
  return 1;
}

int PIDKiln_parse_program_json(const char *json, size_t len, uint16_t max_temp,
                               const int *start_temp_c, PIDKiln_ProgramParsed *out) {
  if (!json || !out || len == 0)
    return PIDKILN_PRG_ERR_IO;

  memset(out, 0, sizeof(*out));

  JsonDocument doc;
  DeserializationError derr = deserializeJson(doc, json, len);
  if (derr)
    return PIDKILN_PRG_ERR_JSON;

  const char *schema = doc["schema"] | (const char *)nullptr;
  if (!schema || strcmp(schema, PIDKILN_PROGRAM_SCHEMA_V1) != 0)
    return PIDKILN_PRG_ERR_JSON;

  if (doc["name"].is<const char *>())
    copy_cstr_limited(out->name, sizeof(out->name), doc["name"].as<const char *>());
  if (doc["description"].is<const char *>())
    copy_cstr_limited(out->description, sizeof(out->description),
                      doc["description"].as<const char *>());
  if (!out->description[0] && out->name[0])
    copy_cstr_limited(out->description, sizeof(out->description), out->name);
  if (!out->description[0])
    copy_cstr_limited(out->description, sizeof(out->description), "No description");

  if (!doc["assume_start_c"].isNull()) {
    if (!doc["assume_start_c"].is<int>())
      return PIDKILN_PRG_ERR_JSON;
    int a = doc["assume_start_c"].as<int>();
    if (a < 0 || a > 65535)
      return PIDKILN_PRG_ERR_JSON;
    out->has_assume_start = 1;
    out->assume_start_c = (int16_t)a;
  }

  JsonArrayConst steps = doc["steps"].as<JsonArrayConst>();
  if (steps.isNull())
    return PIDKILN_PRG_ERR_JSON;
  size_t nsteps = steps.size();
  if (nsteps == 0)
    return PIDKILN_PRG_ERR_STEP;
  if (nsteps > PIDKILN_PROGRAM_MAX_STEPS)
    return PIDKILN_PRG_ERR_TOO_MANY;

  uint16_t prev_temp = 0;
  int have_prev = 0;

  for (size_t i = 0; i < nsteps; i++) {
    JsonObjectConst step = steps[i];
    if (step.isNull())
      return PIDKILN_PRG_ERR_STEP;
    if (!PIDKiln_can_add_program_step(out->size, PIDKILN_PROGRAM_MAX_STEPS))
      return PIDKILN_PRG_ERR_TOO_MANY;

    uint16_t target = 0, hold = 0, ramp_min = 0, rate = 0;
    if (!json_int_u16(step["target_c"], &target))
      return PIDKILN_PRG_ERR_STEP;
    if (!json_int_u16(step["hold_min"], &hold))
      return PIDKILN_PRG_ERR_STEP;
    if (target > max_temp)
      return PIDKILN_PRG_ERR_TEMP;

    const bool has_ramp = !step["ramp_min"].isNull();
    const bool has_rate = !step["rate_c_per_h"].isNull();
    if (has_ramp == has_rate)
      return PIDKILN_PRG_ERR_STEP; /* both or neither */

    uint16_t togo = 0;
    uint16_t stored_rate = 0;

    if (has_ramp) {
      if (!json_int_u16(step["ramp_min"], &ramp_min))
        return PIDKILN_PRG_ERR_STEP;
      togo = ramp_min;
    } else {
      if (!json_int_u16(step["rate_c_per_h"], &rate) || rate < 1)
        return PIDKILN_PRG_ERR_STEP;
      stored_rate = rate;
      if (out->size == 0) {
        if (start_temp_c) {
          togo = PIDKiln_rate_to_togo_min(*start_temp_c, (int)target, rate);
        } else if (out->has_assume_start) {
          togo = PIDKiln_rate_to_togo_min((int)out->assume_start_c, (int)target, rate);
        } else {
          togo = 0; /* unresolved until START / measured temp */
        }
      } else if (have_prev) {
        togo = PIDKiln_rate_to_togo_min((int)prev_temp, (int)target, rate);
      } else {
        return PIDKILN_PRG_ERR_STEP;
      }
    }

    out->steps[out->size].temp = target;
    out->steps[out->size].togo = togo;
    out->steps[out->size].dwell = hold;
    out->rate_c_per_h[out->size] = stored_rate;
    prev_temp = target;
    have_prev = 1;
    out->size++;
  }

  return PIDKILN_PRG_OK;
}

int PIDKiln_inline_comment_pos(const char *line) {
  if (!line)
    return -1;
  const char *p = strchr(line, '#');
  if (!p)
    return -1;
  return (int)(p - line);
}

static void pk_trim_copy(const char *start, const char *end, char *out, size_t cap) {
  if (!out || cap == 0)
    return;
  while (start < end && (*start == ' ' || *start == '\t' || *start == '\r'))
    start++;
  while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
    end--;
  size_t n = (size_t)(end - start);
  if (n >= cap)
    n = cap - 1;
  if (n)
    memcpy(out, start, n);
  out[n] = '\0';
}

int PIDKiln_prefs_split_kv(const char *line, char *key_out, size_t key_cap, char *val_out,
                           size_t val_cap) {
  if (key_out && key_cap)
    key_out[0] = '\0';
  if (val_out && val_cap)
    val_out[0] = '\0';
  if (!line)
    return 0;
  const char *eq = strchr(line, '=');
  if (!eq || eq == line)
    return 0;
  pk_trim_copy(line, eq, key_out, key_cap);
  pk_trim_copy(eq + 1, eq + 1 + strlen(eq + 1), val_out, val_cap);
  if (!key_out || !key_out[0])
    return 0;
  return 1;
}

int PIDKiln_can_add_program_step(unsigned size, unsigned max_steps) {
  return (size < max_steps) ? 1 : 0;
}

int PIDKiln_temp_below_min(double kiln_temp, uint8_t min_temp) {
  return (kiln_temp < (double)min_temp) ? 1 : 0;
}

int PIDKiln_temp_above_max(double kiln_temp, uint16_t max_temp) {
  return (kiln_temp > (double)max_temp) ? 1 : 0;
}

int PIDKiln_housing_above_max(double case_temp, uint16_t max_housing) {
  return (case_temp > (double)max_housing) ? 1 : 0;
}

int PIDKiln_thermal_runaway_heating(float temp_incr, double set_temp, double kiln_temp) {
  return (temp_incr > 0.0f && set_temp >= (kiln_temp + 5.0)) ? 1 : 0;
}

uint32_t PIDKiln_eta_remaining_seconds(const PIDKiln_ProgramStep *steps, unsigned run_step,
                                       unsigned run_size, int dwell_only) {
  uint32_t sec = 0;
  int skip_togo = dwell_only ? 1 : 0;
  if (!steps || run_step >= run_size)
    return 0;
  for (unsigned a = run_step; a < run_size; a++) {
    if (!skip_togo)
      sec += (uint32_t)steps[a].togo * 60u;
    else
      skip_togo = 0;
    sec += (uint32_t)steps[a].dwell * 60u;
  }
  return sec;
}

float PIDKiln_ramp_temp_incr(double from_c, double to_c, uint16_t togo_min) {
  if (togo_min == 0)
    return 0.0f;
  return (float)((to_c - from_c) / ((double)togo_min * 60.0));
}

int PIDKiln_threshold_out_of_band(double kiln_temp, double set_temp, int16_t threshold) {
  if (threshold < 0 || set_temp == 0.0)
    return 0;
  if (kiln_temp + (double)threshold < set_temp || kiln_temp - (double)threshold > set_temp)
    return 1;
  return 0;
}

int PIDKiln_ssr_on(double pid_out, uint16_t window_ms, uint32_t window_start, uint32_t now,
                   int divider) {
  if (window_ms == 0)
    return 0;
  return (pid_out * (double)divider > (double)(now - window_start)) ? 1 : 0;
}

uint32_t PIDKiln_window_advance(uint32_t window_start, uint32_t now, uint16_t window_ms) {
  if (window_ms == 0)
    return window_start;
  while ((now - window_start) > (uint32_t)window_ms)
    window_start += window_ms;
  return window_start;
}

double PIDKiln_heat_percent(double pid_out, uint16_t window_ms, int divider) {
  if (window_ms == 0)
    return 0.0;
  return (pid_out / (double)window_ms) * 100.0 * (double)divider;
}

void PIDKiln_program_fsm_start(PIDKiln_ProgramFsm *f, double kiln_temp, uint32_t now_s) {
  (void)kiln_temp;
  (void)now_s;
  if (!f)
    return;
  f->step = -1;
  f->next_is_dwell = 0;
  f->next_step_end_s = 0;
  /* Force first tick's 10s cadence block to run (mirrors firmware cnt2=999). */
  f->tick10 = 10;
  f->state = PIDKILN_FSM_RUNNING;
  f->set_temp = 0.0;
  f->temp_incr = 0.0f;
}

static void fsm_begin_ramp(PIDKiln_ProgramFsm *f, double kiln_temp, uint32_t now_s) {
  const PIDKiln_ProgramStep *s = &f->steps[f->step];
  f->next_is_dwell = 1;
  f->next_step_end_s = now_s + (uint32_t)s->togo * 60u;

  if (s->togo == 0) {
    f->temp_incr = 0.0f;
    f->set_temp = (double)s->temp;
    return;
  }

  if (f->step > 0) {
    double from = (double)f->steps[f->step - 1].temp;
    f->temp_incr = PIDKiln_ramp_temp_incr(from, (double)s->temp, s->togo);
  } else {
    f->set_temp = kiln_temp;
    f->temp_incr = PIDKiln_ramp_temp_incr(kiln_temp, (double)s->temp, s->togo);
  }
}

static void fsm_begin_dwell(PIDKiln_ProgramFsm *f, uint32_t now_s) {
  const PIDKiln_ProgramStep *s = &f->steps[f->step];
  f->next_is_dwell = 0;
  f->temp_incr = 0.0f;
  f->set_temp = (double)s->temp;
  f->next_step_end_s = now_s + (uint32_t)s->dwell * 60u;
}

int PIDKiln_program_fsm_tick(PIDKiln_ProgramFsm *f, double kiln_temp, uint32_t now_s) {
  int rc = PIDKILN_FSM_OK;

  if (!f || !f->steps || f->size == 0 || f->state == PIDKILN_FSM_ENDED)
    return PIDKILN_FSM_TICK_ENDED;

  f->tick10++;
  if (f->tick10 > 10) {
    f->tick10 = 1;

    if (f->state == PIDKILN_FSM_PAUSED) {
      f->next_step_end_s += 10;
      return PIDKILN_FSM_TICK_ETA;
    }

    if (PIDKiln_threshold_out_of_band(kiln_temp, f->set_temp, f->threshold)) {
      f->state = PIDKILN_FSM_THRESHOLD;
      f->next_step_end_s += 10;
      return PIDKILN_FSM_TICK_ETA;
    } else if (f->state == PIDKILN_FSM_THRESHOLD) {
      f->state = PIDKILN_FSM_RUNNING;
    }

    /* step < 0: no prior phase — due immediately (firmware used next_end=0 vs unix time). */
    if (f->step < 0 || now_s > f->next_step_end_s) {
      if (!f->next_is_dwell) {
        f->step++;
        if (f->step >= (int)f->size) {
          f->state = PIDKILN_FSM_ENDED;
          f->temp_incr = 0.0f;
          return PIDKILN_FSM_TICK_ENDED;
        }
        fsm_begin_ramp(f, kiln_temp, now_s);
        rc = PIDKILN_FSM_TICK_ETA_RAMP;
      } else {
        fsm_begin_dwell(f, now_s);
        rc = PIDKILN_FSM_TICK_ETA_DWELL;
      }
    }
  }

  if (f->state != PIDKILN_FSM_PAUSED && f->state != PIDKILN_FSM_THRESHOLD)
    f->set_temp += (double)f->temp_incr;

  return rc;
}

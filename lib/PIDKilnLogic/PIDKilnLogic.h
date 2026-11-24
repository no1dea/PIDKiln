#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIDKILN_PROGRAM_SCHEMA_V1 "pidkiln.program/v1"
#define PIDKILN_PROGRAM_MAX_STEPS 40
#define PIDKILN_PROGRAM_DESC_MAX 160
#define PIDKILN_PROGRAM_NAME_MAX 32
#define PIDKILN_RATE_FULL_POWER 9999

/* Load / parse result codes (also returned by firmware Load_program). */
#define PIDKILN_PRG_OK 0
#define PIDKILN_PRG_ERR_IO 1
#define PIDKILN_PRG_ERR_JSON 2
#define PIDKILN_PRG_ERR_STEP 3
#define PIDKILN_PRG_ERR_TEMP 4
#define PIDKILN_PRG_ERR_TOO_MANY 5

typedef struct {
  uint16_t temp;
  uint16_t togo;
  uint16_t dwell;
} PIDKiln_ProgramStep;

typedef struct {
  PIDKiln_ProgramStep steps[PIDKILN_PROGRAM_MAX_STEPS];
  /* 0 = step authored with ramp_min; else °C/h for recomputation. */
  uint16_t rate_c_per_h[PIDKILN_PROGRAM_MAX_STEPS];
  unsigned size;
  int has_assume_start;
  int16_t assume_start_c;
  char description[PIDKILN_PROGRAM_DESC_MAX];
  char name[PIDKILN_PROGRAM_NAME_MAX];
} PIDKiln_ProgramParsed;

/* Same charset as firmware MAX_FILENAME / upload rules. */
extern const char PIDKILN_ALLOWED_FILENAME_CHARS[];

/* 1 = ok, 0 = reject */
int PIDKiln_check_valid_chars(unsigned char a);
int PIDKiln_valid_filename(const char *file);

/*
 * Convert |target-start| at rate_c_per_h into ramp minutes (ceil).
 * rate >= PIDKILN_RATE_FULL_POWER or zero delta → 1 minute.
 */
uint16_t PIDKiln_rate_to_togo_min(int start_c, int target_c, uint16_t rate_c_per_h);

/* Recompute step0.to go when step was rate-authored (rate_c_per_h > 0). */
void PIDKiln_resolve_rate_step_togo(PIDKiln_ProgramStep *step, uint16_t rate_c_per_h, int start_c);

/*
 * Parse pidkiln.program/v1 JSON into out.
 * start_temp_c: optional measured kiln °C for rate-based step 0; NULL = omit.
 * Returns PIDKILN_PRG_* codes.
 */
int PIDKiln_parse_program_json(const char *json, size_t len, uint16_t max_temp,
                               const int *start_temp_c, PIDKiln_ProgramParsed *out);

/* Index of first '#' for inline comments, or -1 if none. Used by prefs loader. */
int PIDKiln_inline_comment_pos(const char *line);

/* 1 if another program step may be stored, 0 if at capacity. */
int PIDKiln_can_add_program_step(unsigned size, unsigned max_steps);

/* Safety predicates (pure; used by firmware SAFETY_Check and host tests). */
int PIDKiln_temp_below_min(double kiln_temp, uint8_t min_temp);
int PIDKiln_temp_above_max(double kiln_temp, uint16_t max_temp);
int PIDKiln_housing_above_max(double case_temp, uint16_t max_housing);
/* 1 when ramping up with setpoint at least 5C above kiln temp. */
int PIDKiln_thermal_runaway_heating(float temp_incr, double set_temp, double kiln_temp);

/* ETA: seconds to add for remaining steps from run_step (dwell_only skips first togo). */
uint32_t PIDKiln_eta_remaining_seconds(const PIDKiln_ProgramStep *steps, unsigned run_step,
                                       unsigned run_size, int dwell_only);

/* --- Control helpers (pure; host-testable) --- */

/* °C per second for a ramp. togo_min == 0 → 0 (caller jumps setpoint). */
float PIDKiln_ramp_temp_incr(double from_c, double to_c, uint16_t togo_min);

/*
 * 1 if set_temp is outside kiln ± threshold (wait/catch-up).
 * threshold < 0 or set_temp == 0 → always in-band (0).
 */
int PIDKiln_threshold_out_of_band(double kiln_temp, double set_temp, int16_t threshold);

/* Time-proportioned SSR: 1 when heater should be on in the current window. */
int PIDKiln_ssr_on(double pid_out, uint16_t window_ms, uint32_t window_start, uint32_t now,
                   int divider);

/* Advance window_start until within one window of now (catch-up after lag). */
uint32_t PIDKiln_window_advance(uint32_t window_start, uint32_t now, uint16_t window_ms);

/* Heat percent for UI: (pid_out / window) * 100 * divider; 0 if window == 0. */
double PIDKiln_heat_percent(double pid_out, uint16_t window_ms, int divider);

/* Program step FSM states (independent of firmware PROGRAM_RUN_STATE). */
#define PIDKILN_FSM_RUNNING 0
#define PIDKILN_FSM_PAUSED 1
#define PIDKILN_FSM_THRESHOLD 2
#define PIDKILN_FSM_ENDED 3

/* Tick return codes */
#define PIDKILN_FSM_OK 0
#define PIDKILN_FSM_TICK_ENDED 1
#define PIDKILN_FSM_TICK_ETA 2       /* pause/threshold stretch — refresh ETA */
#define PIDKILN_FSM_TICK_ETA_RAMP 3  /* entered a new ramp phase */
#define PIDKILN_FSM_TICK_ETA_DWELL 4 /* entered a new dwell phase */

typedef struct {
  const PIDKiln_ProgramStep *steps;
  unsigned size;
  int step;                 /* -1 before first */
  int next_is_dwell;        /* after a ramp, next phase is dwell */
  uint32_t next_step_end_s; /* absolute simulated/wall seconds */
  uint8_t tick10;           /* cadence counter (like firmware cnt2) */
  int state;                /* PIDKILN_FSM_* */
  double set_temp;
  float temp_incr;
  int16_t threshold; /* -1 = off */
} PIDKiln_ProgramFsm;

/* Initialize FSM; first tick() performs the initial step transition. */
void PIDKiln_program_fsm_start(PIDKiln_ProgramFsm *f, double kiln_temp, uint32_t now_s);

/*
 * Advance one second. Caller may set f->state to PAUSED/RUNNING before calling.
 * kiln_temp is an input (measured or simulated). Updates set_temp / temp_incr / step / state.
 */
int PIDKiln_program_fsm_tick(PIDKiln_ProgramFsm *f, double kiln_temp, uint32_t now_s);

#ifdef __cplusplus
}
#endif

#include <LittleFS.h>
#include <time.h>
#include "PIDKiln.h"
/*
** Logging functionality + live chart ring (RAM)
** CSV writes run on a low-prio logger task so Program_Loop never blocks on LittleFS.
*/

double EnW_last = 0;

/* ~24h at 30s LOG_Window; PSRAM preferred via MALLOC */
#define CHART_RING_CAP 2880
/* Cap full chart HTTP dumps (incremental ?since= stays small) */
#define CHART_HTTP_FULL_MAX_POINTS 720
#define LOG_QUEUE_LEN 16
#define LOG_FLUSH_EVERY 4

struct ChartSample {
  uint32_t t;
  int16_t k;
  int16_t c;
  int16_t e;
};

struct LogQueueItem {
  uint32_t ts;
  int16_t k;
  int16_t c;
  int16_t e;
};

static ChartSample *chart_ring = NULL;
static uint16_t chart_head = 0;
static uint16_t chart_count = 0;
static uint32_t chart_gen = 0;

static QueueHandle_t log_queue = NULL;
static SemaphoreHandle_t log_fs_mutex = NULL;
static uint8_t log_unflushed = 0;

static bool chart_ring_ensure(void) {
  if (chart_ring)
    return true;
  chart_ring = (ChartSample *)MALLOC(sizeof(ChartSample) * CHART_RING_CAP);
  if (!chart_ring)
    chart_ring = (ChartSample *)malloc(sizeof(ChartSample) * CHART_RING_CAP);
  return chart_ring != NULL;
}

void ChartRing_Clear() {
  chart_head = 0;
  chart_count = 0;
  chart_gen++;
  if (!chart_ring)
    chart_ring_ensure();
}

void ChartRing_Push(uint32_t ts, int16_t kiln_c, int16_t case_c, int16_t energy) {
  if (!chart_ring_ensure())
    return;
  chart_ring[chart_head].t = ts;
  chart_ring[chart_head].k = kiln_c;
  chart_ring[chart_head].c = case_c;
  chart_ring[chart_head].e = energy;
  chart_head = (uint16_t)((chart_head + 1) % CHART_RING_CAP);
  if (chart_count < CHART_RING_CAP)
    chart_count++;
}

uint32_t ChartRing_Gen() { return chart_gen; }

size_t ChartRing_Count() { return chart_count; }

void ChartRing_PrintPointsJson(Print &out, uint32_t since_ts) {
  out.print('[');
  if (!chart_ring || !chart_count) {
    out.print(']');
    return;
  }
  uint16_t start =
      (uint16_t)((chart_head + CHART_RING_CAP - chart_count) % CHART_RING_CAP);
  uint16_t emit_from = 0;
  /* Full dump: skip oldest beyond HTTP cap to bound response size */
  if (since_ts == 0 && chart_count > CHART_HTTP_FULL_MAX_POINTS)
    emit_from = (uint16_t)(chart_count - CHART_HTTP_FULL_MAX_POINTS);

  bool first = true;
  for (uint16_t i = emit_from; i < chart_count; i++) {
    const ChartSample &s = chart_ring[(start + i) % CHART_RING_CAP];
    if (s.t <= since_ts)
      continue;
    if (!first)
      out.print(',');
    first = false;
#if PK_FEATURE_ENERGY
    out.printf("{\"t\":%lu,\"k\":%d,\"c\":%d,\"e\":%d}", (unsigned long)s.t,
               (int)s.k, (int)s.c, (int)s.e);
#else
    out.printf("{\"t\":%lu,\"k\":%d,\"c\":%d}", (unsigned long)s.t, (int)s.k,
               (int)s.c);
#endif
  }
  out.print(']');
}

static void log_fs_lock(void) {
  if (log_fs_mutex)
    xSemaphoreTake(log_fs_mutex, portMAX_DELAY);
}

static void log_fs_unlock(void) {
  if (log_fs_mutex)
    xSemaphoreGive(log_fs_mutex);
}

static void log_write_sample(const LogQueueItem &item) {
  char timestr[32];
  char line[80];
  struct tm timeinfo;

  if (!CSVFile)
    return;

  time_t t = (time_t)item.ts;
  if (localtime_r(&t, &timeinfo))
    strftime(timestr, sizeof(timestr), "%F %T", &timeinfo);
  else
    snprintf(timestr, sizeof(timestr), "%lu", (unsigned long)item.ts);

#if PK_FEATURE_ENERGY
  snprintf(line, sizeof(line), "%s,%d,%d,%d", timestr, (int)item.k, (int)item.c,
           (int)item.e);
#else
  snprintf(line, sizeof(line), "%s,%d,%d", timestr, (int)item.k, (int)item.c);
#endif

  CSVFile.println();
  CSVFile.print(line);
  if (++log_unflushed >= LOG_FLUSH_EVERY) {
    CSVFile.flush();
    log_unflushed = 0;
  }
}

static void Logger_Loop(void *parameter) {
  (void)parameter;
  LogQueueItem item;
  for (;;) {
    if (xQueueReceive(log_queue, &item, portMAX_DELAY) != pdTRUE)
      continue;
    log_fs_lock();
    log_write_sample(item);
    log_fs_unlock();
  }
}

static void Logs_EnsureWorker(void) {
  if (!log_fs_mutex)
    log_fs_mutex = xSemaphoreCreateMutex();
  if (!log_queue) {
    log_queue = xQueueCreate(LOG_QUEUE_LEN, sizeof(LogQueueItem));
    if (log_queue) {
      xTaskCreatePinnedToCore(Logger_Loop, "Logger", 4096, NULL, 1, NULL, 1);
    }
  }
}

void Setup_Logs(void) { Logs_EnsureWorker(); }

static void log_queue_drain(uint32_t timeout_ms) {
  if (!log_queue)
    return;
  TickType_t start = xTaskGetTickCount();
  TickType_t budget = pdMS_TO_TICKS(timeout_ms);
  while (uxQueueMessagesWaiting(log_queue) > 0) {
    if ((xTaskGetTickCount() - start) > budget)
      break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Init_log_file() {
  char str[33];
  struct tm timeinfo, *tmm;

  Logs_EnsureWorker();
  log_queue_drain(500);
  log_fs_lock();

  ChartRing_Clear();
  log_unflushed = 0;

  if (CSVFile)
    CSVFile.close();
  if (getLocalTime(&timeinfo))
    strftime(str, 32, "/logs/%y%m%d_%H%M%S.csv", &timeinfo);
  else
    sprintf(str, "/logs/%d.csv", (int)millis());
  DBG dbgLog(LOG_INFO, "[LOG] Trying to create log file:%s\n", str);

  if ((CSVFile = LittleFS.open(str, "w"))) {
    DBG dbgLog(LOG_INFO, "[LOG] Created new log file %s\n", str);
#if PK_FEATURE_ENERGY
    CSVFile.print("Date,Temperature,Housing,Energy");
#else
    CSVFile.print("Date,Temperature,Housing");
#endif
  }

  char *tmpstr = strstr(str, ".csv");
  if (tmpstr) {
    strncpy(tmpstr, ".log", 4);
    if ((LOGFile = LittleFS.open(str, "w"))) {
      LOGFile.printf("Program name: %s\n", Program_run_name);
      LOGFile.printf("Program desc: %s\n", Program_run_desc);
      if ((tmm = localtime(&Program_run_start))) {
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Started at: %s\n", str);
      }
      if ((tmm = localtime(&Program_run_end))) {
        strftime(str, 29, "%F %T", tmm);
        LOGFile.printf("Possible end at: %s\n", str);
      }
      LOGFile.printf("PID values. Kp:%.2f Ki:%.2f Kd:%.2f\n",
                     Prefs[PRF_PID_KP].value.vfloat, Prefs[PRF_PID_KI].value.vfloat,
                     Prefs[PRF_PID_KD].value.vfloat);
      LOGFile.printf("Start temperature: %.1fC\n", kiln_temp);
      LOGFile.printf("CSV filename: %s\n-=-=-= Starting program =-=-=-=-\n", str);
      LOGFile.flush();
    } else {
      DBG dbgLog(LOG_ERR, "[LOG] Failed to create .log file: %s\n", str);
    }
  }

  EnW_last = 0;
  log_fs_unlock();

  Generate_LOGS_INDEX();
}

void Add_log_line() {
  LogQueueItem item;
  int energy_delta = 0;

  item.ts = (uint32_t)time(NULL);
  item.k = (int16_t)kiln_temp;
  item.c = (int16_t)case_temp;

#if PK_FEATURE_ENERGY
  energy_delta = (int)(Energy_Usage - EnW_last);
  EnW_last = Energy_Usage;
#endif
  item.e = (int16_t)energy_delta;

  /* RAM chart stays on the caller — never wait on FS */
  ChartRing_Push(item.ts, item.k, item.c, item.e);

  Logs_EnsureWorker();
  if (!log_queue || !CSVFile)
    return;

  /* Non-blocking: drop sample rather than stall SSR/PID */
  if (xQueueSend(log_queue, &item, 0) != pdTRUE) {
    DBG dbgLog(LOG_WARNING, "[LOG] Queue full — CSV sample dropped\n");
  }
}

void Close_log_file() {
  struct tm *tmm;
  char str[33];

  log_queue_drain(2000);
  log_fs_lock();

  if (CSVFile) {
    CSVFile.flush();
    CSVFile.close();
  }
  log_unflushed = 0;

  if (LOGFile) {
    if ((tmm = localtime(&Program_run_end))) {
      strftime(str, 29, "%F %T", tmm);
      LOGFile.printf("Program ended at: %s\n", str);
    }
    LOGFile.printf("End temperature: %.1fC\n", kiln_temp);
    if (Energy_Wattage)
      LOGFile.printf("Used power: %.1f W/h\n", Energy_Wattage);
    if (Program_error)
      LOGFile.printf("Program aborted with error: %d\n", Program_error);
    LOGFile.flush();
    LOGFile.close();
  }
  log_fs_unlock();

  Clean_LOGS();
  Generate_LOGS_INDEX();
}

void Clean_LOGS() {
  char fname[MAX_FILENAME];

  if (Logs_DIR_size <= Prefs[PRF_LOG_LIMIT].value.uint16)
    return;
  DBG dbgLog(LOG_INFO, "[LOG] Cleaning logs...\n");
  for (uint16_t a = Prefs[PRF_LOG_LIMIT].value.uint16; a < Logs_DIR_size; a++) {
    sprintf(fname, "%s/%s", LOG_Directory, Logs_DIR[a].filename);
    DBG dbgLog(LOG_INFO, "[LOG] Deleting file:%s\n", fname);
    LittleFS.remove(fname);
  }
}

uint8_t Load_LOGS_Dir() {
  uint16_t count = 0;
  File dir, file;

  dir = LittleFS.open(LOG_Directory);
  if (!dir)
    return 1;
  DBG dbgLog(LOG_INFO, "[LOG] Loading dir: Loading logs directory...\n");
  while (dir.openNextFile())
    count++;

  DBG dbgLog(LOG_INFO, "[LOG] Loading dir:\tcounted %d files\n", count);
  if (Logs_DIR) {
    free(Logs_DIR);
    Logs_DIR = NULL;
  }
  Logs_DIR = (DIRECTORY *)MALLOC(sizeof(DIRECTORY) * count);
  Logs_DIR_size = 0;
  dir.rewindDirectory();
  while ((file = dir.openNextFile()) && Logs_DIR_size <= count) {
    char tmp[32];
    uint8_t len2;

    vTaskDelay(20);
    strcpy(tmp, file.name());
    len2 = strlen(tmp);
    if (len2 > 31 || len2 < 2)
      return 2;

    if (!strcmp(tmp, "index.html"))
      continue;

    strcpy(Logs_DIR[Logs_DIR_size].filename, tmp);
    Logs_DIR[Logs_DIR_size].filesize = file.size();
    Logs_DIR[Logs_DIR_size].sel = 0;

    DBG dbgLog(LOG_DEBUG, "[LOG] FName: %s\t FSize:%d\tSel:%d\n",
               Logs_DIR[Logs_DIR_size].filename, Logs_DIR[Logs_DIR_size].filesize,
               Logs_DIR[Logs_DIR_size].sel);

    Logs_DIR_size++;
  }
  dir.close();

  bool nok = true;
  while (nok) {
    nok = false;
    if (Logs_DIR_size > 1)
      for (int a = 0; a < Logs_DIR_size - 1; a++) {
        DIRECTORY tmp;
        if (strcmp(Logs_DIR[a].filename, Logs_DIR[a + 1].filename) < 0) {
          tmp = Logs_DIR[a];
          Logs_DIR[a] = Logs_DIR[a + 1];
          Logs_DIR[a + 1] = tmp;
          nok = true;
        }
      }
  }

  return 0;
}

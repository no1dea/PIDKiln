#include "PIDKiln.h"

/* LEGACY names: historically rewrote LittleFS index.html; now only refresh in-memory lists. */
void Generate_INDEX() { Load_programs_dir(); }

void Generate_LOGS_INDEX() {
#if PK_FEATURE_LOGS
  Load_LOGS_Dir();
#endif
}

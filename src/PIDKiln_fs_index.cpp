#include "PIDKiln.h"

void Generate_INDEX() { Load_programs_dir(); }

void Generate_LOGS_INDEX() {
#if PK_FEATURE_LOGS
  Load_LOGS_Dir();
#endif
}

#ifndef HEADER_DarwinProcess
#define HEADER_DarwinProcess
/*
htop - DarwinProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/sysctl.h>

#include "Machine.h"
#include "darwin/DarwinProcessTable.h"


#define PROCESS_FLAG_TTY 0x00000100

typedef struct DarwinProcess_ {
   Process super;

   uint64_t utime;
   uint64_t stime;
   bool taskAccess;
   bool translated;
} DarwinProcess;

extern const ProcessClass DarwinProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* DarwinProcess_new(const Machine* settings);

void Process_delete(Object* cast);

void DarwinProcess_setFromKInfoProc(Process* proc, const struct kinfo_proc* ps, bool exists);

void DarwinProcess_setFromLibprocPidinfo(DarwinProcess* proc, DarwinProcessTable* dpt, double timeIntervalNS);

/*
 * Scan threads for process state information.
 * Based on: http://stackoverflow.com/questions/6788274/ios-mac-cpu-usage-for-thread
 * and       https://github.com/max-horvath/htop-osx/blob/e86692e869e30b0bc7264b3675d2a4014866ef46/ProcessList.c
 */
void DarwinProcess_scanThreads(DarwinProcess* dp);

#endif

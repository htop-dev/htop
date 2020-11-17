#ifndef HEADER_DarwinProcess
#define HEADER_DarwinProcess
/*
htop - DarwinProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/sysctl.h>

#include "DarwinProcessList.h"
#include "Settings.h"


typedef struct DarwinProcess_ {
   Process super;

   uint64_t utime;
   uint64_t stime;
   bool taskAccess;
} DarwinProcess;

extern const ProcessClass DarwinProcess_class;

Process* DarwinProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

void DarwinProcess_setFromKInfoProc(Process* proc, const struct kinfo_proc* ps, bool exists);

void DarwinProcess_setFromLibprocPidinfo(DarwinProcess* proc, DarwinProcessList* dpl);

/*
 * Scan threads for process state information.
 * Based on: http://stackoverflow.com/questions/6788274/ios-mac-cpu-usage-for-thread
 * and       https://github.com/max-horvath/htop-osx/blob/e86692e869e30b0bc7264b3675d2a4014866ef46/ProcessList.c
 */
void DarwinProcess_scanThreads(DarwinProcess* dp);

#endif

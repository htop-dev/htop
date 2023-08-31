/*
htop - DarwinProcessTable.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/DarwinProcessTable.h"

#include <errno.h>
#include <libproc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include "CRT.h"
#include "ProcessTable.h"
#include "darwin/DarwinMachine.h"
#include "darwin/DarwinProcess.h"
#include "darwin/Platform.h"
#include "darwin/PlatformHelpers.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


static struct kinfo_proc* ProcessTable_getKInfoProcs(size_t* count) {
   int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
   struct kinfo_proc* processes = NULL;

   for (unsigned int retry = 0; retry < 4; retry++) {
      size_t size = 0;
      if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0 || size == 0) {
         CRT_fatalError("Unable to get size of kproc_infos");
      }

      size += 16 * retry * retry * sizeof(struct kinfo_proc);
      processes = xRealloc(processes, size);

      if (sysctl(mib, 4, processes, &size, NULL, 0) == 0) {
         *count = size / sizeof(struct kinfo_proc);
         return processes;
      }

      if (errno != ENOMEM)
         break;
   }

   CRT_fatalError("Unable to get kinfo_procs");
}

ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   DarwinProcessTable* this = xCalloc(1, sizeof(DarwinProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(DarwinProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   DarwinProcessTable* this = (DarwinProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   const Machine* host = super->super.host;
   const DarwinMachine* dhost = (const DarwinMachine*) host;
   DarwinProcessTable* dpt = (DarwinProcessTable*) super;
   bool preExisting = true;
   struct kinfo_proc* ps;
   size_t count;
   DarwinProcess* proc;

   /* Get the time difference */
   dpt->global_diff = 0;
   for (unsigned int i = 0; i < host->existingCPUs; ++i) {
      for (size_t j = 0; j < CPU_STATE_MAX; ++j) {
         dpt->global_diff += dhost->curr_load[i].cpu_ticks[j] - dhost->prev_load[i].cpu_ticks[j];
      }
   }

   const double time_interval_ns = Platform_schedulerTicksToNanoseconds(dpt->global_diff) / (double) host->activeCPUs;

   /* We use kinfo_procs for initial data since :
    *
    * 1) They always succeed.
    * 2) They contain the basic information.
    *
    * We attempt to fill-in additional information with libproc.
    */
   ps = ProcessTable_getKInfoProcs(&count);

   for (size_t i = 0; i < count; ++i) {
      proc = (DarwinProcess*)ProcessTable_getProcess(super, ps[i].kp_proc.p_pid, &preExisting, DarwinProcess_new);

      DarwinProcess_setFromKInfoProc(&proc->super, &ps[i], preExisting);
      DarwinProcess_setFromLibprocPidinfo(proc, dpt, time_interval_ns);

      if (proc->super.st_uid != ps[i].kp_eproc.e_ucred.cr_uid) {
         proc->super.st_uid = ps[i].kp_eproc.e_ucred.cr_uid;
         proc->super.user = UsersTable_getRef(host->usersTable, proc->super.st_uid);
      }

      // Disabled for High Sierra due to bug in macOS High Sierra
      bool isScanThreadSupported = !Platform_KernelVersionIsBetween((KernelVersion) {17, 0, 0}, (KernelVersion) {17, 5, 0});

      if (isScanThreadSupported) {
         DarwinProcess_scanThreads(proc);
      }

      super->totalTasks += 1;

      if (!preExisting) {
         ProcessTable_add(super, &proc->super);
      }
   }

   free(ps);
}

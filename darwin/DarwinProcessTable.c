/*
htop - DarwinProcessTable.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/DarwinProcessTable.h"

#include <errno.h>
#include <libproc.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include "CRT.h"
#include "Macros.h"
#include "ProcessTable.h"
#include "Settings.h"
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

/*
 * Read per-process disk I/O counters (proc_pid_rusage).
 *
 * Unlike Linux, macOS does not expose separate char/syscall counters; it only
 * provides cumulative storage bytes read and written. Rates are computed from
 * the difference between consecutive scans, mirroring LinuxProcessTable_readIoFile.
 */
static void DarwinProcessTable_readProcRusage(DarwinProcess* proc) {
   const Machine* host = proc->super.super.host;

   struct rusage_info_v2 ri;
   if (proc_pid_rusage(Process_getPid(&proc->super), RUSAGE_INFO_V2, (rusage_info_t *)&ri) != 0) {
      proc->io_rate_read_bps = NAN;
      proc->io_rate_write_bps = NAN;
      proc->io_read_bytes = ULLONG_MAX;
      proc->io_write_bytes = ULLONG_MAX;
      proc->io_last_scan_time_ms = host->monotonicMs;
      return;
   }

   uint64_t last_read = proc->io_read_bytes;
   uint64_t last_write = proc->io_write_bytes;

   /* Only compute a rate once a previous valid sample exists; otherwise the
    * delta is meaningless on the first scan and zero after a failed scan.
    * Machine_scanTables seeds monotonicMs with 1 on its first scan, so treat
    * that sentinel as "no timestamp yet". Use the monotonic clock so that
    * wall-clock adjustments cannot produce a zero or inflated time delta. */
   bool haveBaseline = proc->io_last_scan_time_ms > 1 && last_read != ULLONG_MAX;
   uint64_t time_delta = saturatingSub(host->monotonicMs, proc->io_last_scan_time_ms);

   proc->io_read_bytes = ri.ri_diskio_bytesread;
   proc->io_write_bytes = ri.ri_diskio_byteswritten;

   if (haveBaseline && time_delta) {
      proc->io_rate_read_bps = saturatingSub(proc->io_read_bytes, last_read) * /*ms to s*/1000. / time_delta;
      proc->io_rate_write_bps = saturatingSub(proc->io_write_bytes, last_write) * /*ms to s*/1000. / time_delta;
   } else {
      proc->io_rate_read_bps = NAN;
      proc->io_rate_write_bps = NAN;
   }

   proc->io_last_scan_time_ms = host->monotonicMs;
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

      if (host->settings->ss->flags & PROCESS_FLAG_IO) {
         DarwinProcessTable_readProcRusage(proc);
      }

      // Deduce further process states not covered in the libproc call above
      if (ps[i].kp_proc.p_stat == SZOMB) {
         proc->super.state = ZOMBIE;
      } else if (ps[i].kp_proc.p_stat == SSTOP) {
         proc->super.state = STOPPED;
      }

      if (proc->super.st_uid != ps[i].kp_eproc.e_ucred.cr_uid) {
         proc->super.st_uid = ps[i].kp_eproc.e_ucred.cr_uid;
         proc->super.user = UsersTable_getRef(host->usersTable, proc->super.st_uid);
      }

      // Disabled for High Sierra due to bug in macOS High Sierra
      bool isScanThreadSupported = !Platform_KernelVersionIsBetween((KernelVersion) {17, 0, 0}, (KernelVersion) {17, 5, 0});

      if (isScanThreadSupported) {
         DarwinProcess_scanThreads(proc, dpt);
      }

      super->totalTasks += 1;

      if (!preExisting) {
         ProcessTable_add(super, &proc->super);
      }
   }

   free(ps);
}

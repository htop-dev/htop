/*
htop - DarwinProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/DarwinProcessList.h"

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
#include "ProcessList.h"
#include "darwin/DarwinProcess.h"
#include "darwin/Platform.h"
#include "darwin/PlatformHelpers.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


static void ProcessList_getHostInfo(host_basic_info_data_t* p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;

   if (0 != host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size)) {
      CRT_fatalError("Unable to retrieve host info");
   }
}

static void ProcessList_freeCPULoadInfo(processor_cpu_load_info_t* p) {
   if (NULL != p && NULL != *p) {
      if (0 != munmap(*p, vm_page_size)) {
         CRT_fatalError("Unable to free old CPU load information");
      }
      *p = NULL;
   }
}

static unsigned ProcessList_allocateCPULoadInfo(processor_cpu_load_info_t* p) {
   mach_msg_type_number_t info_size = sizeof(processor_cpu_load_info_t);
   unsigned cpu_count;

   // TODO Improving the accuracy of the load counts woule help a lot.
   if (0 != host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t*)p, &info_size)) {
      CRT_fatalError("Unable to retrieve CPU info");
   }

   return cpu_count;
}

static void ProcessList_getVMStats(vm_statistics_t p) {
   mach_msg_type_number_t info_size = HOST_VM_INFO_COUNT;

   if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)p, &info_size) != 0) {
      CRT_fatalError("Unable to retrieve VM statistics");
   }
}

static struct kinfo_proc* ProcessList_getKInfoProcs(size_t* count) {
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

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   DarwinProcessList* this = xCalloc(1, sizeof(DarwinProcessList));

   ProcessList_init(&this->super, Class(DarwinProcess), usersTable, pidMatchList, userId);

   /* Initialize the CPU information */
   this->super.activeCPUs = ProcessList_allocateCPULoadInfo(&this->prev_load);
   // TODO: support offline CPUs and hot swapping
   this->super.existingCPUs = this->super.activeCPUs;
   ProcessList_getHostInfo(&this->host_info);
   ProcessList_allocateCPULoadInfo(&this->curr_load);

   /* Initialize the VM statistics */
   ProcessList_getVMStats(&this->vm_stats);

   /* Initialize the ZFS kstats, if zfs.kext loaded */
   openzfs_sysctl_init(&this->zfs);
   openzfs_sysctl_updateArcStats(&this->zfs);

   this->super.kernelThreads = 0;
   this->super.userlandThreads = 0;
   this->super.totalTasks = 0;
   this->super.runningTasks = 0;

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   DarwinProcessList* dpl = (DarwinProcessList*)super;
   bool preExisting = true;
   struct kinfo_proc* ps;
   size_t count;
   DarwinProcess* proc;

   /* Update the global data (CPU times and VM stats) */
   ProcessList_freeCPULoadInfo(&dpl->prev_load);
   dpl->prev_load = dpl->curr_load;
   ProcessList_allocateCPULoadInfo(&dpl->curr_load);
   ProcessList_getVMStats(&dpl->vm_stats);
   openzfs_sysctl_updateArcStats(&dpl->zfs);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   /* Get the time difference */
   dpl->global_diff = 0;
   for (unsigned int i = 0; i < dpl->super.existingCPUs; ++i) {
      for (size_t j = 0; j < CPU_STATE_MAX; ++j) {
         dpl->global_diff += dpl->curr_load[i].cpu_ticks[j] - dpl->prev_load[i].cpu_ticks[j];
      }
   }

   const double time_interval_ns = Platform_schedulerTicksToNanoseconds(dpl->global_diff) / (double) dpl->super.activeCPUs;

   /* Clear the thread counts */
   super->kernelThreads = 0;
   super->userlandThreads = 0;
   super->totalTasks = 0;
   super->runningTasks = 0;

   /* We use kinfo_procs for initial data since :
    *
    * 1) They always succeed.
    * 2) The contain the basic information.
    *
    * We attempt to fill-in additional information with libproc.
    */
   ps = ProcessList_getKInfoProcs(&count);

   for (size_t i = 0; i < count; ++i) {
      proc = (DarwinProcess*)ProcessList_getProcess(super, ps[i].kp_proc.p_pid, &preExisting, DarwinProcess_new);

      DarwinProcess_setFromKInfoProc(&proc->super, &ps[i], preExisting);
      DarwinProcess_setFromLibprocPidinfo(proc, dpl, time_interval_ns);

      if (proc->super.st_uid != ps[i].kp_eproc.e_ucred.cr_uid) {
         proc->super.st_uid = ps[i].kp_eproc.e_ucred.cr_uid;
         proc->super.user = UsersTable_getRef(super->usersTable, proc->super.st_uid);
      }

      // Disabled for High Sierra due to bug in macOS High Sierra
      bool isScanThreadSupported  = !Platform_KernelVersionIsBetween((KernelVersion) {17, 0, 0}, (KernelVersion) {17, 5, 0});

      if (isScanThreadSupported) {
         DarwinProcess_scanThreads(proc);
      }

      super->totalTasks += 1;

      if (!preExisting) {
         ProcessList_add(super, &proc->super);
      }
   }

   free(ps);
}

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id) {
   assert(id < super->existingCPUs);

   // TODO: support offline CPUs and hot swapping
   (void) super; (void) id;

   return true;
}

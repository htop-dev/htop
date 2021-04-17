/*
htop - DarwinProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "DarwinProcessList.h"

#include <errno.h>
#include <libproc.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include "CRT.h"
#include "DarwinProcess.h"
#include "Platform.h"
#include "ProcessList.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"

#ifdef HAVE_POWER_GADGET
#include "IntelPowerGadget/PowerGadgetLib.h"
#endif


struct kern {
   short int version[3];
};

static void GetKernelVersion(struct kern* k) {
   static short int version_[3] = {0};
   if (!version_[0]) {
      // just in case it fails someday
      version_[0] = version_[1] = version_[2] = -1;
      char str[256] = {0};
      size_t size = sizeof(str);
      int ret = sysctlbyname("kern.osrelease", str, &size, NULL, 0);
      if (ret == 0) {
         sscanf(str, "%hd.%hd.%hd", &version_[0], &version_[1], &version_[2]);
      }
   }
   memcpy(k->version, version_, sizeof(version_));
}

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
static int CompareKernelVersion(short int major, short int minor, short int component) {
   struct kern k;
   GetKernelVersion(&k);

   if (k.version[0] != major) {
      return k.version[0] - major;
   }
   if (k.version[1] != minor) {
      return k.version[1] - minor;
   }
   if (k.version[2] != component) {
      return k.version[2] - component;
   }

   return 0;
}

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

   for (int retry = 3; retry > 0; retry--) {
      size_t size = 0;
      if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0 || size == 0) {
         CRT_fatalError("Unable to get size of kproc_infos");
      }

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

#ifdef HAVE_POWER_GADGET
// in order to enable separate distribution, we link weakly with the
// framework — overriding the initialization function allows us to
// easily check for its presence
extern bool PG_Initialize(void) __attribute__((weak_import));

static void DarwinProcessList_sampleCPU(DarwinProcessList* this) {
   const bool show_freq = this->super.settings->showCPUFrequency;
   const bool show_temp = this->super.settings->showCPUTemperature;

   // htop numbers "cpus" consecutively, but power gadget itemizes
   // them per core; in order to work around this, we use a separate
   // index, and just increment it whenever we see a core
   const int cpu_count = this->super.cpuCount;
   int cpu_idx = 0;
   int package_count = 0;

   for (int i = 0; i < cpu_count; i++)
      this->cpu_freqs[i] = this->cpu_temps[i] = NAN;

   if (!show_freq && !show_freq)
      return;

   // PG_Initialize will be NULL when the framework is unavailable
   if (!this->initialized_power_gadget && PG_Initialize != NULL)
      this->initialized_power_gadget = PG_Initialize();

   if (!this->initialized_power_gadget)
      return;

   if (!PG_GetNumPackages(&package_count))
      return;

   // note the check for cpu_count below; we definitely don't want to
   // corrupt memory should the package/cpu count be inconsistent
   for (int p = 0; p < package_count && p < cpu_count; p++) {
      PGSampleID old_sample = this->cpu_samples[p];
      PGSampleID new_sample = 0;
      int core_count = 0;

      if (!PG_GetNumCores(p, &core_count))
         continue;

      if (!PG_ReadSample(p, &new_sample)) {
         cpu_idx += core_count;
         continue;
      }

      // as above, concerning cpu_count
      for (int c = 0; c < core_count && cpu_idx < cpu_count; c++) {
         double mean, min, max;

         if (show_freq && old_sample)
            if (PGSample_GetIACoreFrequency(old_sample, new_sample, c, &mean, &min, &max))
               this->cpu_freqs[cpu_idx] = mean;

         if (show_temp)
            if (PGSample_GetIACoreTemperature(new_sample, c, &mean, &min, &max))
               this->cpu_temps[cpu_idx] = mean;

         cpu_idx += 1;
      }

      if (old_sample)
         PGSample_Release(old_sample);

      this->cpu_samples[p] = new_sample;
   }
}
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   DarwinProcessList* this = xCalloc(1, sizeof(DarwinProcessList));

   ProcessList_init(&this->super, Class(DarwinProcess), usersTable, pidMatchList, userId);

   /* Initialize the CPU information */
   this->super.cpuCount = ProcessList_allocateCPULoadInfo(&this->prev_load);

#ifdef HAVE_POWER_GADGET
   this->cpu_freqs = xCalloc(this->super.cpuCount, sizeof(*this->cpu_freqs));
   this->cpu_temps = xCalloc(this->super.cpuCount, sizeof(*this->cpu_temps));

   // we only use one sample per package, but we don't know the amount
   // of packages yet. we do know that "cpus" outnumber them, though
   this->cpu_samples = xCalloc(this->super.cpuCount, sizeof(*this->cpu_samples));
#endif

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

void ProcessList_delete(ProcessList* super) {
   DarwinProcessList* this = (DarwinProcessList*) super;

#ifdef HAVE_POWER_GADGET
   if (this->initialized_power_gadget) {
      for (int i = 0; i < this->cpu_count; i++)
         if (this->cpu_samples[i])
            PGSample_Release(this->cpu_samples[i]);

      PG_Shutdown();
   }

   free(this->cpu_freqs);
   free(this->cpu_temps);
   free(this->cpu_samples);
#endif

   ProcessList_done(super);

   free(this);
}

static double ticksToNanoseconds(const double ticks) {
   const double nanos_per_sec = 1e9;
   return (ticks / Platform_timebaseToNS) * (nanos_per_sec / (double) Platform_clockTicksPerSec);
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

#ifdef HAVE_POWER_GADGET
   DarwinProcessList_sampleCPU(dpl);
#endif

   /* Get the time difference */
   dpl->global_diff = 0;
   for (unsigned int i = 0; i < dpl->super.cpuCount; ++i) {
      for (size_t j = 0; j < CPU_STATE_MAX; ++j) {
         dpl->global_diff += dpl->curr_load[i].cpu_ticks[j] - dpl->prev_load[i].cpu_ticks[j];
      }
   }

   const double time_interval = ticksToNanoseconds(dpl->global_diff) / (double) dpl->super.cpuCount;

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
      DarwinProcess_setFromLibprocPidinfo(proc, dpl, time_interval);

      // Disabled for High Sierra due to bug in macOS High Sierra
      bool isScanThreadSupported  = ! ( CompareKernelVersion(17, 0, 0) >= 0 && CompareKernelVersion(17, 5, 0) < 0);

      if (isScanThreadSupported) {
         DarwinProcess_scanThreads(proc);
      }

      super->totalTasks += 1;

      if (!preExisting) {
         proc->super.user = UsersTable_getRef(super->usersTable, proc->super.st_uid);

         ProcessList_add(super, &proc->super);
      }
   }

   free(ps);
}

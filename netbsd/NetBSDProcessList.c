/*
htop - NetBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "netbsd/NetBSDProcessList.h"

#include <kvm.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/swap.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uvm/uvm_extern.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "Settings.h"
#include "XUtils.h"
#include "netbsd/NetBSDProcess.h"


static long fscale;
static int pageSize;
static int pageSizeKB;

static const struct {
   const char* name;
   long int scale;
} freqSysctls[] = {
   { "machdep.est.frequency.current",            1 },
   { "machdep.powernow.frequency.current",       1 },
   { "machdep.intrepid.frequency.current",       1 },
   { "machdep.loongson.frequency.current",       1 },
   { "machdep.cpu.frequency.current",            1 },
   { "machdep.frequency.current",                1 },
   { "machdep.tsc_freq",                   1000000 },
};

static void NetBSDProcessList_updateCPUcount(ProcessList* super) {
   NetBSDProcessList* opl = (NetBSDProcessList*) super;

   // Definitions for sysctl(3), cf. https://nxr.netbsd.org/xref/src/sys/sys/sysctl.h#813
   const int mib_ncpu_existing[] = { CTL_HW, HW_NCPU }; // Number of existing CPUs
   const int mib_ncpu_online[] = { CTL_HW, HW_NCPUONLINE }; // Number of online/active CPUs

   int r;
   unsigned int value;
   size_t size;

   bool change = false;

   // Query the number of active/online CPUs.
   size = sizeof(value);
   r = sysctl(mib_ncpu_online, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = 1;
   }

   if (value != super->activeCPUs) {
      super->activeCPUs = value;
      change = true;
   }

   // Query the total number of CPUs.
   size = sizeof(value);
   r = sysctl(mib_ncpu_existing, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = super->activeCPUs;
   }

   if (value != super->existingCPUs) {
      opl->cpuData = xReallocArray(opl->cpuData, value + 1, sizeof(CPUData));
      super->existingCPUs = value;
      change = true;
   }

   // Reset CPU stats when number of online/existing CPU cores changed
   if (change) {
      CPUData* dAvg = &opl->cpuData[0];
      memset(dAvg, '\0', sizeof(CPUData));
      dAvg->totalTime = 1;
      dAvg->totalPeriod = 1;

      for (unsigned int i = 0; i < super->existingCPUs; i++) {
         CPUData* d = &opl->cpuData[i + 1];
         memset(d, '\0', sizeof(CPUData));
         d->totalTime = 1;
         d->totalPeriod = 1;
      }
   }
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId) {
   const int fmib[] = { CTL_KERN, KERN_FSCALE };
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   NetBSDProcessList* npl = xCalloc(1, sizeof(NetBSDProcessList));
   ProcessList* pl = (ProcessList*) npl;
   ProcessList_init(pl, Class(NetBSDProcess), usersTable, dynamicMeters, dynamicColumns, pidMatchList, userId);

   NetBSDProcessList_updateCPUcount(pl);

   size = sizeof(fscale);
   if (sysctl(fmib, 2, &fscale, &size, NULL, 0) < 0) {
      CRT_fatalError("fscale sysctl call failed");
   }

   if ((pageSize = sysconf(_SC_PAGESIZE)) == -1)
      CRT_fatalError("pagesize sysconf call failed");
   pageSizeKB = pageSize / ONE_K;

   npl->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (npl->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   NetBSDProcessList* npl = (NetBSDProcessList*) this;

   if (npl->kd) {
      kvm_close(npl->kd);
   }

   free(npl->cpuData);

   ProcessList_done(this);
   free(this);
}

static void NetBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   static int uvmexp_mib[] = {CTL_VM, VM_UVMEXP2};
   struct uvmexp_sysctl uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      CRT_fatalError("uvmexp sysctl call failed");
   }

   pl->totalMem = uvmexp.npages * pageSizeKB;
   pl->buffersMem = 0;
   pl->cachedMem = (uvmexp.filepages + uvmexp.execpages) * pageSizeKB;
   pl->usedMem = (uvmexp.active + uvmexp.wired) * pageSizeKB;
   pl->totalSwap = uvmexp.swpages * pageSizeKB;
   pl->usedSwap = uvmexp.swpginuse * pageSizeKB;
}

static void NetBSDProcessList_updateExe(const struct kinfo_proc2* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC_ARGS, kproc->p_pid, KERN_PROC_PATHNAME };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
      Process_updateExe(proc, NULL);
      return;
   }

   /* Kernel threads return an empty buffer */
   if (buffer[0] == '\0') {
      Process_updateExe(proc, NULL);
      return;
   }

   Process_updateExe(proc, buffer);
}

static void NetBSDProcessList_updateCwd(const struct kinfo_proc2* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC_ARGS, kproc->p_pid, KERN_PROC_CWD };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   /* Kernel threads return an empty buffer */
   if (buffer[0] == '\0') {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   free_and_xStrdup(&proc->procCwd, buffer);
}

static void NetBSDProcessList_updateProcessName(kvm_t* kd, const struct kinfo_proc2* kproc, Process* proc) {
   Process_updateComm(proc, kproc->p_comm);

   /*
    * Like NetBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   char** arg = kvm_getargv2(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   size_t len = 0;
   for (int i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }

   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   char* s;
   if ((s = malloc(len)) == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   *s = '\0';

   int start = 0;
   int end = 0;
   for (int i = 0; arg[i] != NULL; i++) {
      size_t n = strlcat(s, arg[i], len);
      if (i == 0) {
         end = MINIMUM(n, len - 1);
         /* check if cmdline ended earlier, e.g 'kdeinit5: Running...' */
         for (int j = end; j > 0; j--) {
            if (arg[0][j] == ' ' && arg[0][j - 1] != '\\') {
               end = (arg[0][j - 1] == ':') ? (j - 1) : j;
            }
         }
      }
      /* the trailing space should get truncated anyway */
      strlcat(s, " ", len);
   }

   Process_updateCmdline(proc, s, start, end);

   free(s);
}

/*
 * Borrowed with modifications from NetBSD's top(1).
 */
static double getpcpu(const struct kinfo_proc2* kp) {
   if (fscale == 0)
      return 0.0;

   return 100.0 * (double)kp->p_pctcpu / fscale;
}

static void NetBSDProcessList_scanProcs(NetBSDProcessList* this) {
   const Settings* settings = this->super.settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   int count = 0;

   const struct kinfo_proc2* kprocs = kvm_getproc2(this->kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2), &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc2* kproc = &kprocs[i];

      bool preExisting = false;
      Process* proc = ProcessList_getProcess(&this->super, kproc->p_pid, &preExisting, NetBSDProcess_new);

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         proc->pid = kproc->p_pid;
         proc->ppid = kproc->p_ppid;
         proc->tpgid = kproc->p_tpgid;
         proc->tgid = kproc->p_pid;
         proc->session = kproc->p_sid;
         proc->pgrp = kproc->p__pgid;
         proc->isKernelThread = !!(kproc->p_flag & P_SYSTEM);
         proc->isUserlandThread = proc->pid != proc->tgid;
         proc->starttime_ctime = kproc->p_ustart_sec;
         Process_fillStarttimeBuffer(proc);
         ProcessList_add(&this->super, proc);

         proc->tty_nr = kproc->p_tdev;
         const char* name = ((dev_t)kproc->p_tdev != KERN_PROC_TTY_NODEV) ? devname(kproc->p_tdev, S_IFCHR) : NULL;
         if (!name) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }

         NetBSDProcessList_updateExe(kproc, proc);
         NetBSDProcessList_updateProcessName(this->kd, kproc, proc);
      } else {
         if (settings->updateProcessNames) {
            NetBSDProcessList_updateProcessName(this->kd, kproc, proc);
         }
      }

      if (settings->ss->flags & PROCESS_FLAG_CWD) {
         NetBSDProcessList_updateCwd(kproc, proc);
      }

      if (proc->st_uid != kproc->p_uid) {
         proc->st_uid = kproc->p_uid;
         proc->user = UsersTable_getRef(this->super.usersTable, proc->st_uid);
      }

      proc->m_virt = kproc->p_vm_vsize;
      proc->m_resident = kproc->p_vm_rssize;

      proc->percent_mem = (proc->m_resident * pageSizeKB) / (double)(this->super.totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->super.activeCPUs * 100.0);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->nlwp = kproc->p_nlwps;
      proc->nice = kproc->p_nice - 20;
      proc->time = 100 * (kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000));
      proc->priority = kproc->p_priority - PZERO;
      proc->processor = kproc->p_cpuid;
      proc->minflt = kproc->p_uru_minflt;
      proc->majflt = kproc->p_uru_majflt;

      int nlwps = 0;
      const struct kinfo_lwp* klwps = kvm_getlwps(this->kd, kproc->p_pid, kproc->p_paddr, sizeof(struct kinfo_lwp), &nlwps);

      /* TODO: According to the link below, SDYING should be a regarded state */
      /* Taken from: https://ftp.netbsd.org/pub/NetBSD/NetBSD-current/src/sys/sys/proc.h */
      switch (kproc->p_realstat) {
      case SIDL:     proc->state = IDLE; break;
      case SACTIVE:
         // We only consider the first LWP with a one of the below states.
         for (int j = 0; j < nlwps; j++) {
            if (klwps) {
               switch (klwps[j].l_stat) {
               case LSONPROC: proc->state = RUNNING; break;
               case LSRUN:    proc->state = RUNNABLE; break;
               case LSSLEEP:  proc->state = SLEEPING; break;
               case LSSTOP:   proc->state = STOPPED; break;
               default:       proc->state = UNKNOWN;
               }
               if (proc->state != UNKNOWN)
                  break;
            } else {
               proc->state = UNKNOWN;
               break;
            }
         }
         break;
      case SSTOP:    proc->state = STOPPED; break;
      case SZOMB:    proc->state = ZOMBIE; break;
      case SDEAD:    proc->state = DEFUNCT; break;
      default:       proc->state = UNKNOWN;
      }

      if (Process_isKernelThread(proc)) {
         this->super.kernelThreads++;
      } else if (Process_isUserlandThread(proc)) {
         this->super.userlandThreads++;
      }

      this->super.totalTasks++;
      if (proc->state == RUNNING) {
         this->super.runningTasks++;
      }
      proc->updated = true;
   }
}

static void getKernelCPUTimes(int cpuId, u_int64_t* times) {
   const int mib[] = { CTL_KERN, KERN_CP_TIME, cpuId };
   size_t length = sizeof(*times) * CPUSTATES;
   if (sysctl(mib, 3, times, &length, NULL, 0) == -1 || length != sizeof(*times) * CPUSTATES) {
      CRT_fatalError("sysctl kern.cp_time2 failed");
   }
}

static void kernelCPUTimesToHtop(const u_int64_t* times, CPUData* cpu) {
   unsigned long long totalTime = 0;
   for (int i = 0; i < CPUSTATES; i++) {
      totalTime += times[i];
   }

   unsigned long long sysAllTime = times[CP_INTR] + times[CP_SYS];

   cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
   cpu->userPeriod = saturatingSub(times[CP_USER], cpu->userTime);
   cpu->nicePeriod = saturatingSub(times[CP_NICE], cpu->niceTime);
   cpu->sysPeriod = saturatingSub(times[CP_SYS], cpu->sysTime);
   cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
   cpu->intrPeriod = saturatingSub(times[CP_INTR], cpu->intrTime);
   cpu->idlePeriod = saturatingSub(times[CP_IDLE], cpu->idleTime);

   cpu->totalTime = totalTime;
   cpu->userTime = times[CP_USER];
   cpu->niceTime = times[CP_NICE];
   cpu->sysTime = times[CP_SYS];
   cpu->sysAllTime = sysAllTime;
   cpu->intrTime = times[CP_INTR];
   cpu->idleTime = times[CP_IDLE];
}

static void NetBSDProcessList_scanCPUTime(NetBSDProcessList* this) {
   u_int64_t kernelTimes[CPUSTATES] = {0};
   u_int64_t avg[CPUSTATES] = {0};

   for (unsigned int i = 0; i < this->super.existingCPUs; i++) {
      getKernelCPUTimes(i, kernelTimes);
      CPUData* cpu = &this->cpuData[i + 1];
      kernelCPUTimesToHtop(kernelTimes, cpu);

      avg[CP_USER] += cpu->userTime;
      avg[CP_NICE] += cpu->niceTime;
      avg[CP_SYS] += cpu->sysTime;
      avg[CP_INTR] += cpu->intrTime;
      avg[CP_IDLE] += cpu->idleTime;
   }

   for (int i = 0; i < CPUSTATES; i++) {
      avg[i] /= this->super.activeCPUs;
   }

   kernelCPUTimesToHtop(avg, &this->cpuData[0]);
}

static void NetBSDProcessList_scanCPUFrequency(NetBSDProcessList* this) {
   unsigned int cpus = this->super.existingCPUs;
   bool match = false;
   char name[64];
   long int freq = 0;
   size_t freqSize;

   for (unsigned int i = 0; i < cpus; i++) {
      this->cpuData[i + 1].frequency = NAN;
   }

   /* newer hardware supports per-core frequency, for e.g. ARM big.LITTLE */
   for (unsigned int i = 0; i < cpus; i++) {
      xSnprintf(name, sizeof(name), "machdep.cpufreq.cpu%u.current", i);
      freqSize = sizeof(freq);
      if (sysctlbyname(name, &freq, &freqSize, NULL, 0) != -1) {
         this->cpuData[i + 1].frequency = freq; /* already in MHz */
         match = true;
      }
   }

   if (match) {
      return;
   }

   /*
    * Iterate through legacy sysctl nodes for single-core frequency until
    * we find a match...
    */
   for (size_t i = 0; i < ARRAYSIZE(freqSysctls); i++) {
      freqSize = sizeof(freq);
      if (sysctlbyname(freqSysctls[i].name, &freq, &freqSize, NULL, 0) != -1) {
         freq /= freqSysctls[i].scale; /* scale to MHz */
         match = true;
         break;
      }
   }

   if (match) {
      for (unsigned int i = 0; i < cpus; i++) {
         this->cpuData[i + 1].frequency = freq;
      }
   }
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   NetBSDProcessList* npl = (NetBSDProcessList*) super;

   NetBSDProcessList_scanMemoryInfo(super);
   NetBSDProcessList_scanCPUTime(npl);

   if (super->settings->showCPUFrequency) {
      NetBSDProcessList_scanCPUFrequency(npl);
   }

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   NetBSDProcessList_scanProcs(npl);
}

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id) {
   assert(id < super->existingCPUs);

   // TODO: Support detecting online / offline CPUs.
   return true;
}

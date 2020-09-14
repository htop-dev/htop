/*
htop - OpenBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CRT.h"
#include "ProcessList.h"
#include "OpenBSDProcessList.h"
#include "OpenBSDProcess.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long fscale;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   int mib[] = { CTL_HW, HW_NCPU };
   int fmib[] = { CTL_KERN, KERN_FSCALE };
   int i, e;
   OpenBSDProcessList* opl;
   ProcessList* pl;
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   opl = xCalloc(1, sizeof(OpenBSDProcessList));
   pl = (ProcessList*) opl;
   size = sizeof(pl->cpuCount);
   ProcessList_init(pl, Class(OpenBSDProcess), usersTable, pidMatchList, userId);

   e = sysctl(mib, 2, &pl->cpuCount, &size, NULL, 0);
   if (e == -1 || pl->cpuCount < 1) {
      pl->cpuCount = 1;
   }
   opl->cpus = xCalloc(pl->cpuCount + 1, sizeof(CPUData));

   size = sizeof(fscale);
   if (sysctl(fmib, 2, &fscale, &size, NULL, 0) < 0) {
      err(1, "fscale sysctl call failed");
   }

   for (i = 0; i <= pl->cpuCount; i++) {
      CPUData *d = opl->cpus + i;
      d->totalTime = 1;
      d->totalPeriod = 1;
   }

   opl->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (opl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const OpenBSDProcessList* opl = (OpenBSDProcessList*) this;

   if (opl->kd) {
      kvm_close(opl->kd);
   }

   free(opl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void OpenBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   static int uvmexp_mib[] = {CTL_VM, VM_UVMEXP};
   struct uvmexp uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      err(1, "uvmexp sysctl call failed");
   }

   pl->totalMem = uvmexp.npages * PAGE_SIZE_KB;

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   static int bcache_mib[] = {CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT};
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);

   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      err(1, "cannot get vfs.bcachestat");
   }

   pl->cachedMem = bcstats.numbufpages * PAGE_SIZE_KB;
   pl->freeMem = uvmexp.free * PAGE_SIZE_KB;
   pl->usedMem = (uvmexp.npages - uvmexp.free - uvmexp.paging) * PAGE_SIZE_KB;

   /*
   const OpenBSDProcessList* opl = (OpenBSDProcessList*) pl;

   size_t len = sizeof(pl->totalMem);
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(pl->usedMem), &len, NULL, 0);
   pl->usedMem *= PAGE_SIZE_KB;
   pl->freeMem = pl->totalMem - pl->usedMem;
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(pl->cachedMem), &len, NULL, 0);
   pl->cachedMem *= PAGE_SIZE_KB;

   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(opl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= PAGE_SIZE_KB;
   pl->usedSwap *= PAGE_SIZE_KB;

   pl->sharedMem = 0;  // currently unused
   pl->buffersMem = 0; // not exposed to userspace
   */
}

char *OpenBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd) {
   char *s, **arg;
   size_t len = 0, n;
   int i;

   /*
    * Like OpenBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   arg = kvm_getargv(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      return xStrdup(kproc->p_comm);
   }
   for (i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }
   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   if ((s = malloc(len)) == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      return xStrdup(kproc->p_comm);
   }

   *s = '\0';

   for (i = 0; arg[i] != NULL; i++) {
      n = strlcat(s, arg[i], len);
      if (i == 0) {
         /* TODO: rename all basenameEnd to basenameLen, make size_t */
         *basenameEnd = MINIMUM(n, len-1);
      }
      /* the trailing space should get truncated anyway */
      strlcat(s, " ", len);
   }

   return s;
}

/*
 * Taken from OpenBSD's ps(1).
 */
static double getpcpu(const struct kinfo_proc *kp) {
   if (fscale == 0)
      return (0.0);

#define   fxtofl(fixpt)   ((double)(fixpt) / fscale)

   return (100.0 * fxtofl(kp->p_pctcpu));
}

static inline void OpenBSDProcessList_scanProcs(OpenBSDProcessList* this) {
   Settings* settings = this->super.settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   struct kinfo_proc* kproc;
   bool preExisting;
   Process* proc;
   OpenBSDProcess* fp;
   struct tm date;
   struct timeval tv;
   int count = 0;
   int i;

   // use KERN_PROC_KTHREAD to also include kernel threads
   struct kinfo_proc* kprocs = kvm_getprocs(this->kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc), &count);
   //struct kinfo_proc* kprocs = getprocs(KERN_PROC_ALL, 0, &count);

   gettimeofday(&tv, NULL);

   for (i = 0; i < count; i++) {
      kproc = &kprocs[i];

      preExisting = false;
      proc = ProcessList_getProcess(&this->super, kproc->p_pid, &preExisting, (Process_New) OpenBSDProcess_new);
      fp = (OpenBSDProcess*) proc;

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc))
                  || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         proc->ppid = kproc->p_ppid;
         proc->tpgid = kproc->p_tpgid;
         proc->tgid = kproc->p_pid;
         proc->session = kproc->p_sid;
         proc->tty_nr = kproc->p_tdev;
         proc->pgrp = kproc->p__pgid;
         proc->st_uid = kproc->p_uid;
         proc->starttime_ctime = kproc->p_ustart_sec;
         proc->user = UsersTable_getRef(this->super.usersTable, proc->st_uid);
         ProcessList_add(&this->super, proc);
         proc->comm = OpenBSDProcessList_readProcessName(this->kd, kproc, &proc->basenameOffset);
         (void) localtime_r((time_t*) &kproc->p_ustart_sec, &date);
         strftime(proc->starttime_show, 7, ((proc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
      } else {
         if (settings->updateProcessNames) {
            free(proc->comm);
            proc->comm = OpenBSDProcessList_readProcessName(this->kd, kproc, &proc->basenameOffset);
         }
      }

      proc->m_size = kproc->p_vm_dsize;
      proc->m_resident = kproc->p_vm_rssize;
      proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(this->super.totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->super.cpuCount*100.0);
      //proc->nlwp = kproc->p_numthreads;
      //proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 10);
      proc->nice = kproc->p_nice - 20;
      proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000);
      proc->time *= 100;
      proc->priority = kproc->p_priority - PZERO;

      switch (kproc->p_stat) {
         case SIDL:    proc->state = 'I'; break;
         case SRUN:    proc->state = 'R'; break;
         case SSLEEP:  proc->state = 'S'; break;
         case SSTOP:   proc->state = 'T'; break;
         case SZOMB:   proc->state = 'Z'; break;
         case SDEAD:   proc->state = 'D'; break;
         case SONPROC: proc->state = 'P'; break;
         default:      proc->state = '?';
      }

      if (Process_isKernelThread(proc)) {
         this->super.kernelThreads++;
      }

      this->super.totalTasks++;
      // SRUN ('R') means runnable, not running
      if (proc->state == 'P') {
         this->super.runningTasks++;
      }
      proc->updated = true;
   }
}

static unsigned long long saturatingSub(unsigned long long a, unsigned long long b) {
   return a > b ? a - b : 0;
}

static void getKernelCPUTimes(int cpuId, u_int64_t* times) {
   int mib[] = { CTL_KERN, KERN_CPTIME2, cpuId };
   size_t length = sizeof(u_int64_t) * CPUSTATES;
   if (sysctl(mib, 3, times, &length, NULL, 0) == -1 ||
         length != sizeof(u_int64_t) * CPUSTATES) {
      CRT_fatalError("sysctl kern.cp_time2 failed");
   }
}

static void kernelCPUTimesToHtop(const u_int64_t* times, CPUData* cpu) {
   unsigned long long totalTime = 0;
   for (int i = 0; i < CPUSTATES; i++) {
      totalTime += times[i];
   }

   unsigned long long sysAllTime = times[CP_INTR] + times[CP_SYS];

   // XXX Not sure if CP_SPIN should be added to sysAllTime.
   // See https://github.com/openbsd/src/commit/531d8034253fb82282f0f353c086e9ad827e031c
   #ifdef CP_SPIN
   sysAllTime += times[CP_SPIN];
   #endif

   cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
   cpu->userPeriod = saturatingSub(times[CP_USER], cpu->userTime);
   cpu->nicePeriod = saturatingSub(times[CP_NICE], cpu->niceTime);
   cpu->sysPeriod = saturatingSub(times[CP_SYS], cpu->sysTime);
   cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
   #ifdef CP_SPIN
   cpu->spinPeriod = saturatingSub(times[CP_SPIN], cpu->spinTime);
   #endif
   cpu->intrPeriod = saturatingSub(times[CP_INTR], cpu->intrTime);
   cpu->idlePeriod = saturatingSub(times[CP_IDLE], cpu->idleTime);

   cpu->totalTime = totalTime;
   cpu->userTime = times[CP_USER];
   cpu->niceTime = times[CP_NICE];
   cpu->sysTime = times[CP_SYS];
   cpu->sysAllTime = sysAllTime;
   #ifdef CP_SPIN
   cpu->spinTime = times[CP_SPIN];
   #endif
   cpu->intrTime = times[CP_INTR];
   cpu->idleTime = times[CP_IDLE];
}

static void OpenBSDProcessList_scanCPUTime(OpenBSDProcessList* this) {
   u_int64_t kernelTimes[CPUSTATES] = {0};
   u_int64_t avg[CPUSTATES] = {0};

   for (int i = 0; i < this->super.cpuCount; i++) {
      getKernelCPUTimes(i, kernelTimes);
      CPUData* cpu = this->cpus + i + 1;
      kernelCPUTimesToHtop(kernelTimes, cpu);

      avg[CP_USER] += cpu->userTime;
      avg[CP_NICE] += cpu->niceTime;
      avg[CP_SYS] += cpu->sysTime;
      #ifdef CP_SPIN
      avg[CP_SPIN] += cpu->spinTime;
      #endif
      avg[CP_INTR] += cpu->intrTime;
      avg[CP_IDLE] += cpu->idleTime;
   }

   for (int i = 0; i < CPUSTATES; i++) {
      avg[i] /= this->super.cpuCount;
   }

   kernelCPUTimesToHtop(avg, this->cpus);
}

void ProcessList_goThroughEntries(ProcessList* this) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) this;

   OpenBSDProcessList_scanMemoryInfo(this);
   OpenBSDProcessList_scanProcs(opl);
   OpenBSDProcessList_scanCPUTime(opl);
}

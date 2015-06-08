/*
htop - FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "FreeBSDProcessList.h"
#include "FreeBSDProcess.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <fcntl.h>
#include <string.h>

/*{

#include <kvm.h>

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int totalPeriod;
} CPUData;

typedef struct FreeBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   CPUData* cpus;

} FreeBSDProcessList;

}*/

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_hw_physmem[2];

static int pageSizeKb;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   FreeBSDProcessList* fpl = calloc(1, sizeof(FreeBSDProcessList));
   ProcessList* pl = (ProcessList*) fpl;
   ProcessList_init(pl, Class(FreeBSDProcess), usersTable, pidWhiteList, userId);

   int cpus = 1;
   size_t sizeof_cpus = sizeof(cpus);
   int err = sysctlbyname("hw.ncpu", &cpus, &sizeof_cpus, NULL, 0);
   if (err) cpus = 1;
   pl->cpuCount = MAX(cpus, 1);
   fpl->cpus = realloc(fpl->cpus, cpus * sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      fpl->cpus[i].totalTime = 1;
      fpl->cpus[i].totalPeriod = 1;
   }
   
   size_t len;
   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count",  MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", MIB_vm_stats_vm_v_cache_count, &len);
   len = 2; sysctlnametomib("hw.physmem",                MIB_hw_physmem, &len);
   pageSizeKb = PAGE_SIZE_KB;   
   
   fpl->kd = kvm_open(NULL, "/dev/null", NULL, 0, NULL);
   assert(fpl->kd);

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;
   if (fpl->kd) kvm_close(fpl->kd);
  
   ProcessList_done(this);
   free(this);
}

static inline void FreeBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;
   
   size_t len = sizeof(pl->totalMem);
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(pl->usedMem), &len, NULL, 0);
   pl->usedMem *= pageSizeKb;
   pl->freeMem = pl->totalMem - pl->usedMem;
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(pl->cachedMem), &len, NULL, 0);
   pl->cachedMem *= pageSizeKb;
   
   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(fpl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= pageSizeKb;
   pl->usedSwap *= pageSizeKb;
   
   pl->sharedMem = 0;  // currently unused
   pl->buffersMem = 0; // not exposed to userspace
}

char* FreeBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd) {
   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv) {
      return strdup(kproc->ki_comm);
   }
   int len = 0;
   for (int i = 0; argv[i]; i++) {
      len += strlen(argv[i]) + 1;
   }
   char* comm = malloc(len * sizeof(char));
   char* at = comm;
   *basenameEnd = 0;
   for (int i = 0; argv[i]; i++) {
      at = stpcpy(at, argv[i]);
      if (!*basenameEnd) {
         *basenameEnd = at - comm;
      }
      *at = ' ';
      at++;
   }
   at--;
   *at = '\0';
   return comm;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;
   Settings* settings = this->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   
   FreeBSDProcessList_scanMemoryInfo(this);
   
   int count = 0;
   struct kinfo_proc* kprocs = kvm_getprocs(fpl->kd, KERN_PROC_ALL, 0, &count);
   
   for (int i = 0; i < count; i++) {
      struct kinfo_proc* kproc = &kprocs[i];
      
      bool preExisting = false;
      Process* proc = ProcessList_getProcess(this, kproc->ki_pid, &preExisting, (Process_New) FreeBSDProcess_new);
      FreeBSDProcess* fp = (FreeBSDProcess*) proc;

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));
      
      if (!preExisting) {
         proc->ppid = kproc->ki_ppid;
         proc->tpgid = kproc->ki_tpgid;
         proc->tgid = kproc->ki_pid;
         proc->session = kproc->ki_sid;
         proc->tty_nr = kproc->ki_tdev;
         proc->pgrp = kproc->ki_pgid;
         proc->st_uid = kproc->ki_uid;
         proc->starttime_ctime = kproc->ki_start.tv_sec;
         proc->user = UsersTable_getRef(this->usersTable, proc->st_uid);
         ProcessList_add((ProcessList*)this, proc);
         proc->comm = FreeBSDProcessList_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
      } else {
         if (settings->updateProcessNames) {
            free(proc->comm);
            proc->comm = FreeBSDProcessList_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
         }
      }

      proc->m_size = kproc->ki_size / pageSizeKb / 1000;
      proc->m_resident = kproc->ki_rssize; // * pageSizeKb;
      proc->nlwp = kproc->ki_numthreads;
      proc->time = (kproc->ki_runtime + 5000) / 10000;
      proc->priority = kproc->ki_pri.pri_level - PZERO;
      if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
         proc->nice = kproc->ki_nice - NZERO;
      } else if (PRI_IS_REALTIME(kproc->ki_pri.pri_class)) {
         proc->nice = PRIO_MIN - 1 - (PRI_MAX_REALTIME - kproc->ki_pri.pri_level);
      } else {
         proc->nice = PRIO_MAX + 1 + kproc->ki_pri.pri_level - PRI_MIN_IDLE;
      }

      switch (kproc->ki_stat) {
      case SIDL:   proc->state = 'I'; break;
      case SRUN:   proc->state = 'R'; break;
      case SSLEEP: proc->state = 'S'; break;
      case SSTOP:  proc->state = 'T'; break;
      case SZOMB:  proc->state = 'Z'; break;
      case SWAIT:  proc->state = 'D'; break;
      case SLOCK:  proc->state = 'L'; break;
      default:     proc->state = '?';
      }

      if (Process_isKernelThread(proc)) {
         this->kernelThreads++;
      }
      
      this->totalTasks++;
      if (proc->state == 'R')
         this->runningTasks++;
      proc->updated = true;
   }
}

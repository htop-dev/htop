/*
htop - DragonFlyBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "DragonFlyBSDProcessList.h"
#include "DragonFlyBSDProcess.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/param.h>


#define _UNUSED_ __attribute__((unused))

static int MIB_hw_physmem[2];
static int MIB_vm_stats_vm_v_page_count[4];
static int pageSize;
static int pageSizeKb;

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_active_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_vm_stats_vm_v_inactive_count[4];
static int MIB_vm_stats_vm_v_free_count[4];

static int MIB_vfs_bufspace[2];

static int MIB_kern_cp_time[2];
static int MIB_kern_cp_times[2];
static int kernelFScale;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   size_t len;
   char errbuf[_POSIX2_LINE_MAX];
   DragonFlyBSDProcessList* dfpl = xCalloc(1, sizeof(DragonFlyBSDProcessList));
   ProcessList* pl = (ProcessList*) dfpl;
   ProcessList_init(pl, Class(DragonFlyBSDProcess), usersTable, pidMatchList, userId);

   // physical memory in system: hw.physmem
   // physical page size: hw.pagesize
   // usable pagesize : vm.stats.vm.v_page_size
   len = 2; sysctlnametomib("hw.physmem", MIB_hw_physmem, &len);

   len = sizeof(pageSize);
   if (sysctlbyname("vm.stats.vm.v_page_size", &pageSize, &len, NULL, 0) == -1) {
      pageSize = PAGE_SIZE;
      pageSizeKb = PAGE_SIZE_KB;
   } else {
      pageSizeKb = pageSize / ONE_K;
   }

   // usable page count vm.stats.vm.v_page_count
   // actually usable memory : vm.stats.vm.v_page_count * vm.stats.vm.v_page_size
   len = 4; sysctlnametomib("vm.stats.vm.v_page_count", MIB_vm_stats_vm_v_page_count, &len);

   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count", MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_active_count", MIB_vm_stats_vm_v_active_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", MIB_vm_stats_vm_v_cache_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_inactive_count", MIB_vm_stats_vm_v_inactive_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_free_count", MIB_vm_stats_vm_v_free_count, &len);

   len = 2; sysctlnametomib("vfs.bufspace", MIB_vfs_bufspace, &len);

   int cpus = 1;
   len = sizeof(cpus);
   if (sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0) != 0) {
      cpus = 1;
   }

   size_t sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   len = 2; sysctlnametomib("kern.cp_time", MIB_kern_cp_time, &len);
   dfpl->cp_time_o = xCalloc(cpus, sizeof_cp_time_array);
   dfpl->cp_time_n = xCalloc(cpus, sizeof_cp_time_array);
   len = sizeof_cp_time_array;

   // fetch initial single (or average) CPU clicks from kernel
   sysctl(MIB_kern_cp_time, 2, dfpl->cp_time_o, &len, NULL, 0);

   // on smp box, fetch rest of initial CPU's clicks
   if (cpus > 1) {
      len = 2; sysctlnametomib("kern.cp_times", MIB_kern_cp_times, &len);
      dfpl->cp_times_o = xCalloc(cpus, sizeof_cp_time_array);
      dfpl->cp_times_n = xCalloc(cpus, sizeof_cp_time_array);
      len = cpus * sizeof_cp_time_array;
      sysctl(MIB_kern_cp_times, 2, dfpl->cp_times_o, &len, NULL, 0);
   }

   pl->cpuCount = MAXIMUM(cpus, 1);

   if (cpus == 1 ) {
     dfpl->cpus = xRealloc(dfpl->cpus, sizeof(CPUData));
   } else {
     // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
     dfpl->cpus = xRealloc(dfpl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }

   len = sizeof(kernelFScale);
   if (sysctlbyname("kern.fscale", &kernelFScale, &len, NULL, 0) == -1) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      kernelFScale = 2048;
   }

   dfpl->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (dfpl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) this;
   if (dfpl->kd) kvm_close(dfpl->kd);

   if (dfpl->jails) {
      Hashtable_delete(dfpl->jails);
   }
   free(dfpl->cp_time_o);
   free(dfpl->cp_time_n);
   free(dfpl->cp_times_o);
   free(dfpl->cp_times_n);
   free(dfpl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void DragonFlyBSDProcessList_scanCPUTime(ProcessList* pl) {
   const DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) pl;

   int cpus   = pl->cpuCount;   // actual CPU count
   int maxcpu = cpus;           // max iteration (in case we have average + smp)
   int cp_times_offset;

   assert(cpus > 0);

   size_t sizeof_cp_time_array;

   unsigned long     *cp_time_n; // old clicks state
   unsigned long     *cp_time_o; // current clicks state

   unsigned long cp_time_d[CPUSTATES];
   double        cp_time_p[CPUSTATES];

   // get averages or single CPU clicks
   sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   sysctl(MIB_kern_cp_time, 2, dfpl->cp_time_n, &sizeof_cp_time_array, NULL, 0);

   // get rest of CPUs
   if (cpus > 1) {
       // on smp systems DragonFlyBSD kernel concats all CPU states into one long array in
       // kern.cp_times sysctl OID
       // we store averages in dfpl->cpus[0], and actual cores after that
       maxcpu = cpus + 1;
       sizeof_cp_time_array = cpus * sizeof(unsigned long) * CPUSTATES;
       sysctl(MIB_kern_cp_times, 2, dfpl->cp_times_n, &sizeof_cp_time_array, NULL, 0);
   }

   for (int i = 0; i < maxcpu; i++) {
      if (cpus == 1) {
         // single CPU box
         cp_time_n = dfpl->cp_time_n;
         cp_time_o = dfpl->cp_time_o;
      } else {
         if (i == 0 ) {
           // average
           cp_time_n = dfpl->cp_time_n;
           cp_time_o = dfpl->cp_time_o;
         } else {
           // specific smp cores
           cp_times_offset = i - 1;
           cp_time_n = dfpl->cp_times_n + (cp_times_offset * CPUSTATES);
           cp_time_o = dfpl->cp_times_o + (cp_times_offset * CPUSTATES);
         }
      }

      // diff old vs new
      unsigned long long total_o = 0;
      unsigned long long total_n = 0;
      unsigned long long total_d = 0;
      for (int s = 0; s < CPUSTATES; s++) {
        cp_time_d[s] = cp_time_n[s] - cp_time_o[s];
        total_o += cp_time_o[s];
        total_n += cp_time_n[s];
      }

      // totals
      total_d = total_n - total_o;
      if (total_d < 1 ) total_d = 1;

      // save current state as old and calc percentages
      for (int s = 0; s < CPUSTATES; ++s) {
        cp_time_o[s] = cp_time_n[s];
        cp_time_p[s] = ((double)cp_time_d[s]) / ((double)total_d) * 100;
      }

      CPUData* cpuData = &(dfpl->cpus[i]);
      cpuData->userPercent      = cp_time_p[CP_USER];
      cpuData->nicePercent      = cp_time_p[CP_NICE];
      cpuData->systemPercent    = cp_time_p[CP_SYS];
      cpuData->irqPercent       = cp_time_p[CP_INTR];
      cpuData->systemAllPercent = cp_time_p[CP_SYS] + cp_time_p[CP_INTR];
      // this one is not really used, but we store it anyway
      cpuData->idlePercent      = cp_time_p[CP_IDLE];
   }
}

static inline void DragonFlyBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) pl;

   // @etosan:
   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired
   //  htop_cache (for cache meter)   = buffers + cache
   //  user_free (avail to procs)     = buffers + inactive + cache + free
   size_t len = sizeof(pl->totalMem);

   //disabled for now, as it is always smaller than phycal amount of memory...
   //...to avoid "where is my memory?" questions
   //sysctl(MIB_vm_stats_vm_v_page_count, 4, &(pl->totalMem), &len, NULL, 0);
   //pl->totalMem *= pageSizeKb;
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;

   sysctl(MIB_vm_stats_vm_v_active_count, 4, &(dfpl->memActive), &len, NULL, 0);
   dfpl->memActive *= pageSizeKb;

   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(dfpl->memWire), &len, NULL, 0);
   dfpl->memWire *= pageSizeKb;

   sysctl(MIB_vfs_bufspace, 2, &(pl->buffersMem), &len, NULL, 0);
   pl->buffersMem /= 1024;

   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(pl->cachedMem), &len, NULL, 0);
   pl->cachedMem *= pageSizeKb;
   pl->usedMem = dfpl->memActive + dfpl->memWire;

   //currently unused, same as with arc, custom meter perhaps
   //sysctl(MIB_vm_stats_vm_v_inactive_count, 4, &(dfpl->memInactive), &len, NULL, 0);
   //sysctl(MIB_vm_stats_vm_v_free_count, 4, &(dfpl->memFree), &len, NULL, 0);
   //pl->freeMem  = dfpl->memInactive + dfpl->memFree;
   //pl->freeMem *= pageSizeKb;

   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(dfpl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= pageSizeKb;
   pl->usedSwap *= pageSizeKb;

   pl->sharedMem = 0;  // currently unused
}

char* DragonFlyBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd) {
   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv) {
      return xStrdup(kproc->kp_comm);
   }
   int len = 0;
   for (int i = 0; argv[i]; i++) {
      len += strlen(argv[i]) + 1;
   }
   char* comm = xMalloc(len);
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

static inline void DragonFlyBSDProcessList_scanJails(DragonFlyBSDProcessList* dfpl) {
   size_t len;
   char *jls; /* Jail list */
   char *curpos;
   char *nextpos;

   if (sysctlbyname("jail.list", NULL, &len, NULL, 0) == -1) {
      fprintf(stderr, "initial sysctlbyname / jail.list failed\n");
      exit(3);
   }
retry:
   if (len == 0)
      return;

   jls = xMalloc(len);
   if (jls == NULL) {
      fprintf(stderr, "xMalloc failed\n");
      exit(4);
   }
   if (sysctlbyname("jail.list", jls, &len, NULL, 0) == -1) {
      if (errno == ENOMEM) {
         free(jls);
         goto retry;
      }
      fprintf(stderr, "sysctlbyname / jail.list failed\n");
      exit(5);
   }

   if (dfpl->jails) {
      Hashtable_delete(dfpl->jails);
   }
   dfpl->jails = Hashtable_new(20, true);
   curpos = jls;
   while (curpos) {
      int jailid;
      char *str_hostname;
      nextpos = strchr(curpos, '\n');
      if (nextpos)
         *nextpos++ = 0;

      jailid = atoi(strtok(curpos, " "));
      str_hostname = strtok(NULL, " ");

      char *jname = (char *) (Hashtable_get(dfpl->jails, jailid));
      if (jname == NULL) {
         jname = xStrdup(str_hostname);
         Hashtable_put(dfpl->jails, jailid, jname);
      }

      curpos = nextpos;
  }
  free(jls);
}

char* DragonFlyBSDProcessList_readJailName(DragonFlyBSDProcessList* dfpl, int jailid) {
   char*  hostname;
   char*  jname;

   if (jailid != 0 && dfpl->jails && (hostname = (char *)Hashtable_get(dfpl->jails, jailid))) {
      jname = xStrdup(hostname);
   } else {
      jname = xStrdup("-");
   }
   return jname;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) this;
   Settings* settings = this->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   DragonFlyBSDProcessList_scanMemoryInfo(this);
   DragonFlyBSDProcessList_scanCPUTime(this);
   DragonFlyBSDProcessList_scanJails(dfpl);

   int count = 0;

   // TODO Kernel Threads seem to be skipped, need to figure out the correct flag
   struct kinfo_proc* kprocs = kvm_getprocs(dfpl->kd, KERN_PROC_ALL | (!hideUserlandThreads ? KERN_PROC_FLAG_LWP : 0), 0, &count);

   for (int i = 0; i < count; i++) {
      struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting = false;
      bool _UNUSED_ isIdleProcess = false;

      // note: dragonflybsd kernel processes all have the same pid, so we misuse the kernel thread address to give them a unique identifier
      Process* proc = ProcessList_getProcess(this, kproc->kp_ktaddr ? (pid_t)kproc->kp_ktaddr : kproc->kp_pid, &preExisting, (Process_New) DragonFlyBSDProcess_new);
      DragonFlyBSDProcess* dfp = (DragonFlyBSDProcess*) proc;

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(dfp)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         dfp->jid = kproc->kp_jailid;
         if (kproc->kp_ktaddr && kproc->kp_flags & P_SYSTEM) {
            // dfb kernel threads all have the same pid, so we misuse the kernel thread address to give them a unique identifier
            proc->pid = (pid_t)kproc->kp_ktaddr;
            dfp->kernel = 1;
         } else {
            proc->pid = kproc->kp_pid;		// process ID
            dfp->kernel = 0;
         }
         proc->ppid = kproc->kp_ppid;		// parent process id
         proc->tpgid = kproc->kp_tpgid;		// tty process group id
         //proc->tgid = kproc->kp_lwp.kl_tid;	// thread group id
         proc->tgid = kproc->kp_pid;		// thread group id
         proc->pgrp = kproc->kp_pgid;		// process group id
         proc->session = kproc->kp_sid;
         proc->tty_nr = kproc->kp_tdev;		// control terminal device number
         proc->st_uid = kproc->kp_uid;		// user ID
         proc->processor = kproc->kp_lwp.kl_origcpu;
         proc->starttime_ctime = kproc->kp_start.tv_sec;
         proc->user = UsersTable_getRef(this->usersTable, proc->st_uid);

         ProcessList_add((ProcessList*)this, proc);
         proc->comm = DragonFlyBSDProcessList_readProcessName(dfpl->kd, kproc, &proc->basenameOffset);
         dfp->jname = DragonFlyBSDProcessList_readJailName(dfpl, kproc->kp_jailid);
      } else {
         proc->processor = kproc->kp_lwp.kl_cpuid;
         if(dfp->jid != kproc->kp_jailid) {	// process can enter jail anytime
            dfp->jid = kproc->kp_jailid;
            free(dfp->jname);
            dfp->jname = DragonFlyBSDProcessList_readJailName(dfpl, kproc->kp_jailid);
         }
         if (proc->ppid != kproc->kp_ppid) {	// if there are reapers in the system, process can get reparented anytime
            proc->ppid = kproc->kp_ppid;
         }
         if(proc->st_uid != kproc->kp_uid) {	// some processes change users (eg. to lower privs)
            proc->st_uid = kproc->kp_uid;
            proc->user = UsersTable_getRef(this->usersTable, proc->st_uid);
         }
         if (settings->updateProcessNames) {
            free(proc->comm);
            proc->comm = DragonFlyBSDProcessList_readProcessName(dfpl->kd, kproc, &proc->basenameOffset);
         }
      }

      proc->m_size = kproc->kp_vm_map_size / 1024 / pageSizeKb;
      proc->m_resident = kproc->kp_vm_rssize;
      proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;
      proc->nlwp = kproc->kp_nthreads;		// number of lwp thread
      proc->time = (kproc->kp_swtime + 5000) / 10000;

      proc->percent_cpu = 100.0 * ((double)kproc->kp_lwp.kl_pctcpu / (double)kernelFScale);
      proc->percent_mem = 100.0 * (proc->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem);

      if (proc->percent_cpu > 0.1) {
         // system idle process should own all CPU time left regardless of CPU count
         if ( strcmp("idle", kproc->kp_comm) == 0 ) {
            isIdleProcess = true;
         }
      }

      if (kproc->kp_lwp.kl_pid != -1)
         proc->priority = kproc->kp_lwp.kl_prio;
      else
         proc->priority = -kproc->kp_lwp.kl_tdprio;

      switch(kproc->kp_lwp.kl_rtprio.type) {
        case RTP_PRIO_REALTIME:
                proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX + kproc->kp_lwp.kl_rtprio.prio;
                break;
        case RTP_PRIO_IDLE:
                proc->nice = PRIO_MAX + 1 + kproc->kp_lwp.kl_rtprio.prio;
                break;
        case RTP_PRIO_THREAD:
                proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX - kproc->kp_lwp.kl_rtprio.prio;
                break;
        default:
                proc->nice = kproc->kp_nice;
                break;
      }

      // would be nice if we could store multiple states in proc->state (as enum) and have writeField render them
      switch (kproc->kp_stat) {
      case SIDL:   proc->state = 'I'; isIdleProcess = true; break;
      case SACTIVE:
         switch (kproc->kp_lwp.kl_stat) {
            case LSSLEEP:
               if (kproc->kp_lwp.kl_flags & LWP_SINTR)					// interruptable wait short/long
                  if (kproc->kp_lwp.kl_slptime >= MAXSLP) {
                     proc->state = 'I';
                     isIdleProcess = true;
                  } else {
                     proc->state = 'S';
                  }
               else if (kproc->kp_lwp.kl_tdflags & TDF_SINTR)				// interruptable lwkt wait
                  proc->state = 'S';
               else if (kproc->kp_paddr)						// uninterruptable wait
                  proc->state = 'D';
               else									// uninterruptable lwkt wait
                  proc->state = 'B';
               break;
            case LSRUN:
               if (kproc->kp_lwp.kl_stat == LSRUN) {
                  if (!(kproc->kp_lwp.kl_tdflags & (TDF_RUNNING | TDF_RUNQ)))
                     proc->state = 'Q';
                  else
                     proc->state = 'R';
               }
               break;
            case LSSTOP:
               proc->state = 'T';
               break;
            default:
               proc->state = 'A';
               break;
         }
         break;
      case SSTOP:  proc->state = 'T'; break;
      case SZOMB:  proc->state = 'Z'; break;
      case SCORE:  proc->state = 'C'; break;
      default:     proc->state = '?';
      }

      if (kproc->kp_flags & P_SWAPPEDOUT) {
         proc->state = 'W';
      }
      if (kproc->kp_flags & P_TRACED) {
         proc->state = 'T';
      }
      if (kproc->kp_flags & P_JAILED) {
         proc->state = 'J';
      }

      if (Process_isKernelThread(dfp)) {
         this->kernelThreads++;
      }

      this->totalTasks++;
      if (proc->state == 'R')
         this->runningTasks++;
      proc->updated = true;
   }
}

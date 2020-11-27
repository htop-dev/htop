/*
htop - FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "FreeBSDProcessList.h"

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_iovec.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/param.h> // needs to be included before <sys/jail.h> for MAXPATHLEN
#include <sys/jail.h>
#include <sys/priority.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include "CRT.h"
#include "Compat.h"
#include "FreeBSDProcess.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "Settings.h"
#include "XUtils.h"
#include "zfs/ZfsArcStats.h"
#include "zfs/openzfs_sysctl.h"


char jail_errmsg[JAIL_ERRMSGLEN];

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
   FreeBSDProcessList* fpl = xCalloc(1, sizeof(FreeBSDProcessList));
   ProcessList* pl = (ProcessList*) fpl;
   ProcessList_init(pl, Class(FreeBSDProcess), usersTable, pidMatchList, userId);

   // physical memory in system: hw.physmem
   // physical page size: hw.pagesize
   // usable pagesize : vm.stats.vm.v_page_size
   len = 2; sysctlnametomib("hw.physmem", MIB_hw_physmem, &len);

   len = sizeof(pageSize);
   if (sysctlbyname("vm.stats.vm.v_page_size", &pageSize, &len, NULL, 0) == -1) {
      pageSize = CRT_pageSize;
      pageSizeKb = CRT_pageSize;
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

   openzfs_sysctl_init(&fpl->zfs);
   openzfs_sysctl_updateArcStats(&fpl->zfs);

   int smp = 0;
   len = sizeof(smp);

   if (sysctlbyname("kern.smp.active", &smp, &len, NULL, 0) != 0 || len != sizeof(smp)) {
      smp = 0;
   }

   int cpus = 1;
   len = sizeof(cpus);

   if (smp) {
      int err = sysctlbyname("kern.smp.cpus", &cpus, &len, NULL, 0);
      if (err) {
         cpus = 1;
      }
   } else {
      cpus = 1;
   }

   size_t sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   len = 2; sysctlnametomib("kern.cp_time", MIB_kern_cp_time, &len);
   fpl->cp_time_o = xCalloc(cpus, sizeof_cp_time_array);
   fpl->cp_time_n = xCalloc(cpus, sizeof_cp_time_array);
   len = sizeof_cp_time_array;

   // fetch initial single (or average) CPU clicks from kernel
   sysctl(MIB_kern_cp_time, 2, fpl->cp_time_o, &len, NULL, 0);

   // on smp box, fetch rest of initial CPU's clicks
   if (cpus > 1) {
      len = 2; sysctlnametomib("kern.cp_times", MIB_kern_cp_times, &len);
      fpl->cp_times_o = xCalloc(cpus, sizeof_cp_time_array);
      fpl->cp_times_n = xCalloc(cpus, sizeof_cp_time_array);
      len = cpus * sizeof_cp_time_array;
      sysctl(MIB_kern_cp_times, 2, fpl->cp_times_o, &len, NULL, 0);
   }

   pl->cpuCount = MAXIMUM(cpus, 1);

   if (cpus == 1 ) {
      fpl->cpus = xRealloc(fpl->cpus, sizeof(CPUData));
   } else {
      // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
      fpl->cpus = xRealloc(fpl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }


   len = sizeof(kernelFScale);
   if (sysctlbyname("kern.fscale", &kernelFScale, &len, NULL, 0) == -1) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      kernelFScale = 2048;
   }

   fpl->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (fpl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   fpl->ttys = Hashtable_new(20, true);

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;

   Hashtable_delete(fpl->ttys);

   if (fpl->kd) {
      kvm_close(fpl->kd);
   }

   free(fpl->cp_time_o);
   free(fpl->cp_time_n);
   free(fpl->cp_times_o);
   free(fpl->cp_times_n);
   free(fpl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void FreeBSDProcessList_scanCPUTime(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

   int cpus   = pl->cpuCount;   // actual CPU count
   int maxcpu = cpus;           // max iteration (in case we have average + smp)
   int cp_times_offset;

   assert(cpus > 0);

   size_t sizeof_cp_time_array;

   unsigned long* cp_time_n; // old clicks state
   unsigned long* cp_time_o; // current clicks state

   unsigned long cp_time_d[CPUSTATES];
   double        cp_time_p[CPUSTATES];

   // get averages or single CPU clicks
   sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   sysctl(MIB_kern_cp_time, 2, fpl->cp_time_n, &sizeof_cp_time_array, NULL, 0);

   // get rest of CPUs
   if (cpus > 1) {
      // on smp systems FreeBSD kernel concats all CPU states into one long array in
      // kern.cp_times sysctl OID
      // we store averages in fpl->cpus[0], and actual cores after that
      maxcpu = cpus + 1;
      sizeof_cp_time_array = cpus * sizeof(unsigned long) * CPUSTATES;
      sysctl(MIB_kern_cp_times, 2, fpl->cp_times_n, &sizeof_cp_time_array, NULL, 0);
   }

   for (int i = 0; i < maxcpu; i++) {
      if (cpus == 1) {
         // single CPU box
         cp_time_n = fpl->cp_time_n;
         cp_time_o = fpl->cp_time_o;
      } else {
         if (i == 0 ) {
            // average
            cp_time_n = fpl->cp_time_n;
            cp_time_o = fpl->cp_time_o;
         } else {
            // specific smp cores
            cp_times_offset = i - 1;
            cp_time_n = fpl->cp_times_n + (cp_times_offset * CPUSTATES);
            cp_time_o = fpl->cp_times_o + (cp_times_offset * CPUSTATES);
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
      if (total_d < 1 ) {
         total_d = 1;
      }

      // save current state as old and calc percentages
      for (int s = 0; s < CPUSTATES; ++s) {
         cp_time_o[s] = cp_time_n[s];
         cp_time_p[s] = ((double)cp_time_d[s]) / ((double)total_d) * 100;
      }

      CPUData* cpuData = &(fpl->cpus[i]);
      cpuData->userPercent      = cp_time_p[CP_USER];
      cpuData->nicePercent      = cp_time_p[CP_NICE];
      cpuData->systemPercent    = cp_time_p[CP_SYS];
      cpuData->irqPercent       = cp_time_p[CP_INTR];
      cpuData->systemAllPercent = cp_time_p[CP_SYS] + cp_time_p[CP_INTR];
      // this one is not really used, but we store it anyway
      cpuData->idlePercent      = cp_time_p[CP_IDLE];
   }
}

static inline void FreeBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

   // @etosan:
   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired
   //  htop_cache (for cache meter)   = buffers + cache
   //  user_free (avail to procs)     = buffers + inactive + cache + free
   //
   // with ZFS ARC situation becomes bit muddled, as ARC behaves like "user_free"
   // and belongs into cache, but is reported as wired by kernel
   //
   // htop_used   = active + (wired - arc)
   // htop_cache  = buffers + cache + arc
   u_long totalMem;
   u_int memActive, memWire, cachedMem;
   long buffersMem;
   size_t len;

   //disabled for now, as it is always smaller than phycal amount of memory...
   //...to avoid "where is my memory?" questions
   //sysctl(MIB_vm_stats_vm_v_page_count, 4, &(pl->totalMem), &len, NULL, 0);
   //pl->totalMem *= pageSizeKb;
   len = sizeof(totalMem);
   sysctl(MIB_hw_physmem, 2, &(totalMem), &len, NULL, 0);
   totalMem /= 1024;
   pl->totalMem = totalMem;

   len = sizeof(memActive);
   sysctl(MIB_vm_stats_vm_v_active_count, 4, &(memActive), &len, NULL, 0);
   memActive *= pageSizeKb;
   fpl->memActive = memActive;

   len = sizeof(memWire);
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(memWire), &len, NULL, 0);
   memWire *= pageSizeKb;
   fpl->memWire = memWire;

   len = sizeof(buffersMem);
   sysctl(MIB_vfs_bufspace, 2, &(buffersMem), &len, NULL, 0);
   buffersMem /= 1024;
   pl->buffersMem = buffersMem;

   len = sizeof(cachedMem);
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(cachedMem), &len, NULL, 0);
   cachedMem *= pageSizeKb;
   pl->cachedMem = cachedMem;

   if (fpl->zfs.enabled) {
      fpl->memWire -= fpl->zfs.size;
      pl->cachedMem += fpl->zfs.size;
   }

   pl->usedMem = fpl->memActive + fpl->memWire;

   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(fpl->kd, swap, ARRAYSIZE(swap), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= pageSizeKb;
   pl->usedSwap *= pageSizeKb;
}

static void FreeBSDProcessList_scanTTYs(ProcessList* pl) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

   // scan /dev/tty*
   {
      DIR* dirPtr = opendir("/dev");
      if (!dirPtr)
         return;

      int dirFd = dirfd(dirPtr);
      if (dirFd < 0)
         goto err1;

      const struct dirent* entry;
      while ((entry = readdir(dirPtr))) {
         if (!String_startsWith(entry->d_name, "tty"))
            continue;

         struct stat info;
         if (Compat_fstatat(dirFd, "/dev", entry->d_name, &info, 0) < 0)
            continue;

         if (!S_ISCHR(info.st_mode))
            continue;

         if (!Hashtable_get(fpl->ttys, info.st_rdev))
            Hashtable_put(fpl->ttys, info.st_rdev, xStrdup(entry->d_name));
      }

err1:
      closedir(dirPtr);
   }

   // scan /dev/pts/*
   {
      DIR* dirPtr = opendir("/dev/pts");
      if (!dirPtr)
         return;

      int dirFd = dirfd(dirPtr);
      if (dirFd < 0)
         goto err2;

      const struct dirent* entry;
      while ((entry = readdir(dirPtr))) {
         struct stat info;
         if (Compat_fstatat(dirFd, "/dev/pts", entry->d_name, &info, 0) < 0)
            continue;

         if (!S_ISCHR(info.st_mode))
            continue;

         if (!Hashtable_get(fpl->ttys, info.st_rdev)) {
            char* path;
            xAsprintf(&path, "pts/%s", entry->d_name);
            Hashtable_put(fpl->ttys, info.st_rdev, path);
         }
      }

err2:
      closedir(dirPtr);
   }
}

static char* FreeBSDProcessList_readProcessName(kvm_t* kd, const struct kinfo_proc* kproc, int* basenameEnd) {
   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv) {
      return xStrdup(kproc->ki_comm);
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

static char* FreeBSDProcessList_readJailName(const struct kinfo_proc* kproc) {
   char*  jname = NULL;
   char   jnamebuf[MAXHOSTNAMELEN];

   if (kproc->ki_jid != 0 ) {
      struct iovec jiov[6];

      memset(jnamebuf, 0, sizeof(jnamebuf));
IGNORE_WCASTQUAL_BEGIN
      *(const void**)&jiov[0].iov_base = "jid";
      jiov[0].iov_len = sizeof("jid");
      jiov[1].iov_base = (void*) &kproc->ki_jid;
      jiov[1].iov_len = sizeof(kproc->ki_jid);
      *(const void**)&jiov[2].iov_base = "name";
      jiov[2].iov_len = sizeof("name");
      jiov[3].iov_base = jnamebuf;
      jiov[3].iov_len = sizeof(jnamebuf);
      *(const void**)&jiov[4].iov_base = "errmsg";
      jiov[4].iov_len = sizeof("errmsg");
      jiov[5].iov_base = jail_errmsg;
      jiov[5].iov_len = JAIL_ERRMSGLEN;
IGNORE_WCASTQUAL_END
      jail_errmsg[0] = 0;

      int jid = jail_get(jiov, 6, 0);
      if (jid < 0) {
         if (!jail_errmsg[0]) {
            xSnprintf(jail_errmsg, JAIL_ERRMSGLEN, "jail_get: %s", strerror(errno));
         }
      } else if (jid == kproc->ki_jid) {
         jname = xStrdup(jnamebuf);
      }
   } else {
      jname = xStrdup("-");
   }

   return jname;
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) super;
   const Settings* settings = super->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   openzfs_sysctl_updateArcStats(&fpl->zfs);
   FreeBSDProcessList_scanMemoryInfo(super);
   FreeBSDProcessList_scanCPUTime(super);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   if (settings->flags & PROCESS_FLAG_FREEBSD_TTY) {
      FreeBSDProcessList_scanTTYs(super);
   }

   int count = 0;
   struct kinfo_proc* kprocs = kvm_getprocs(fpl->kd, KERN_PROC_PROC, 0, &count);

   for (int i = 0; i < count; i++) {
      struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting = false;
      // TODO: bool isIdleProcess = false;
      Process* proc = ProcessList_getProcess(super, kproc->ki_pid, &preExisting, FreeBSDProcess_new);
      FreeBSDProcess* fp = (FreeBSDProcess*) proc;

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         fp->jid = kproc->ki_jid;
         proc->pid = kproc->ki_pid;
         if ( ! ((kproc->ki_pid == 0) || (kproc->ki_pid == 1) ) && kproc->ki_flag & P_SYSTEM) {
            fp->kernel = 1;
         } else {
            fp->kernel = 0;
         }
         proc->ppid = kproc->ki_ppid;
         proc->tpgid = kproc->ki_tpgid;
         proc->tgid = kproc->ki_pid;
         proc->session = kproc->ki_sid;
         proc->pgrp = kproc->ki_pgid;
         proc->st_uid = kproc->ki_uid;
         proc->starttime_ctime = kproc->ki_start.tv_sec;
         Process_fillStarttimeBuffer(proc);
         proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);
         ProcessList_add(super, proc);
         proc->comm = FreeBSDProcessList_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
         fp->jname = FreeBSDProcessList_readJailName(kproc);
      } else {
         if (fp->jid != kproc->ki_jid) {
            // process can enter jail anytime
            fp->jid = kproc->ki_jid;
            free(fp->jname);
            fp->jname = FreeBSDProcessList_readJailName(kproc);
         }
         // if there are reapers in the system, process can get reparented anytime
         proc->ppid = kproc->ki_ppid;
         if (proc->st_uid != kproc->ki_uid) {
            // some processes change users (eg. to lower privs)
            proc->st_uid = kproc->ki_uid;
            proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);
         }
         if (settings->updateProcessNames) {
            free(proc->comm);
            proc->comm = FreeBSDProcessList_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
         }
      }

      // from FreeBSD source /src/usr.bin/top/machine.c
      proc->m_virt = kproc->ki_size / pageSize;
      proc->m_resident = kproc->ki_rssize;
      proc->nlwp = kproc->ki_numthreads;
      proc->time = (kproc->ki_runtime + 5000) / 10000;

      proc->percent_cpu = 100.0 * ((double)kproc->ki_pctcpu / (double)kernelFScale);
      proc->percent_mem = 100.0 * (proc->m_resident * pageSizeKb) / (double)(super->totalMem);

      /*
       * TODO
       * if (proc->percent_cpu > 0.1) {
       *     // system idle process should own all CPU time left regardless of CPU count
       *     if ( strcmp("idle", kproc->ki_comm) == 0 ) {
       *         isIdleProcess = true;
       *     }
       * }
       */

      proc->priority = kproc->ki_pri.pri_level - PZERO;

      if (strcmp("intr", kproc->ki_comm) == 0 && kproc->ki_flag & P_SYSTEM) {
         proc->nice = 0; //@etosan: intr kernel process (not thread) has weird nice value
      } else if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
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

      if (settings->flags & PROCESS_FLAG_FREEBSD_TTY) {
         fp->ttyPath = (kproc->ki_tdev == NODEV) ? nodevStr : Hashtable_get(fpl->ttys, kproc->ki_tdev);
      }

      if (Process_isKernelThread(proc))
         super->kernelThreads++;

      super->totalTasks++;
      if (proc->state == 'R')
         super->runningTasks++;
      proc->updated = true;
   }
}

/*
htop - FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "freebsd/FreeBSDProcessList.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_iovec.h>
#include <sys/errno.h>
#include <sys/param.h> // needs to be included before <sys/jail.h> for MAXPATHLEN
#include <sys/jail.h>
#include <sys/priority.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include "CRT.h"
#include "Compat.h"
#include "FreeBSDProcess.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "Scheduling.h"
#include "Settings.h"
#include "XUtils.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


static int MIB_hw_physmem[2];
static int MIB_vm_stats_vm_v_page_count[4];
static int pageSize;
static int pageSizeKb;

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_active_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_vm_stats_vm_v_inactive_count[4];
static int MIB_vm_stats_vm_v_free_count[4];
static int MIB_vm_vmtotal[2];

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
   if (sysctlbyname("vm.stats.vm.v_page_size", &pageSize, &len, NULL, 0) == -1)
      CRT_fatalError("Cannot get pagesize by sysctl");
   pageSizeKb = pageSize / ONE_K;

   // usable page count vm.stats.vm.v_page_count
   // actually usable memory : vm.stats.vm.v_page_count * vm.stats.vm.v_page_size
   len = 4; sysctlnametomib("vm.stats.vm.v_page_count", MIB_vm_stats_vm_v_page_count, &len);

   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count", MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_active_count", MIB_vm_stats_vm_v_active_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", MIB_vm_stats_vm_v_cache_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_inactive_count", MIB_vm_stats_vm_v_inactive_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_free_count", MIB_vm_stats_vm_v_free_count, &len);
   len = 2; sysctlnametomib("vm.vmtotal", MIB_vm_vmtotal, &len);

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
   fpl->cp_time_o = xCalloc(CPUSTATES, sizeof(unsigned long));
   fpl->cp_time_n = xCalloc(CPUSTATES, sizeof(unsigned long));
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

   pl->existingCPUs = MAXIMUM(cpus, 1);
   // TODO: support offline CPUs and hot swapping
   pl->activeCPUs = pl->existingCPUs;

   if (cpus == 1 ) {
      fpl->cpus = xRealloc(fpl->cpus, sizeof(CPUData));
   } else {
      // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
      fpl->cpus = xRealloc(fpl->cpus, (pl->existingCPUs + 1) * sizeof(CPUData));
   }


   len = sizeof(kernelFScale);
   if (sysctlbyname("kern.fscale", &kernelFScale, &len, NULL, 0) == -1) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      kernelFScale = 2048;
   }

   fpl->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (fpl->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;

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

static inline void FreeBSDProcessList_scanCPU(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

   unsigned int cpus   = pl->existingCPUs; // actual CPU count
   unsigned int maxcpu = cpus;             // max iteration (in case we have average + smp)
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

   for (unsigned int i = 0; i < maxcpu; i++) {
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
      // this one is not really used
      //cpuData->idlePercent      = cp_time_p[CP_IDLE];

      cpuData->temperature = NAN;
      cpuData->frequency = NAN;

      const int coreId = (cpus == 1) ? 0 : ((int)i - 1);
      if (coreId < 0)
         continue;

      // TODO: test with hyperthreading and multi-cpu systems
      if (pl->settings->showCPUTemperature) {
         int temperature;
         size_t len = sizeof(temperature);
         char mibBuffer[32];
         xSnprintf(mibBuffer, sizeof(mibBuffer), "dev.cpu.%d.temperature", coreId);
         int r = sysctlbyname(mibBuffer, &temperature, &len, NULL, 0);
         if (r == 0)
            cpuData->temperature = (double)(temperature - 2732) / 10.0; // convert from deci-Kelvin to Celsius
      }

      // TODO: test with hyperthreading and multi-cpu systems
      if (pl->settings->showCPUFrequency) {
         int frequency;
         size_t len = sizeof(frequency);
         char mibBuffer[32];
         xSnprintf(mibBuffer, sizeof(mibBuffer), "dev.cpu.%d.freq", coreId);
         int r = sysctlbyname(mibBuffer, &frequency, &len, NULL, 0);
         if (r == 0)
            cpuData->frequency = frequency; // keep in MHz
      }
   }

   // calculate max temperature and avg frequency for average meter and
   // propagate frequency to all cores if only supplied for CPU 0
   if (cpus > 1) {
      if (pl->settings->showCPUTemperature) {
         double maxTemp = NAN;
         for (unsigned int i = 1; i < maxcpu; i++) {
            const double coreTemp = fpl->cpus[i].temperature;
            if (isnan(coreTemp))
               continue;

            maxTemp = MAXIMUM(maxTemp, coreTemp);
         }

         fpl->cpus[0].temperature = maxTemp;
      }

      if (pl->settings->showCPUFrequency) {
         const double coreZeroFreq = fpl->cpus[1].frequency;
         double freqSum = coreZeroFreq;
         if (!isnan(coreZeroFreq)) {
            for (unsigned int i = 2; i < maxcpu; i++) {
               if (isnan(fpl->cpus[i].frequency))
                  fpl->cpus[i].frequency = coreZeroFreq;

               freqSum += fpl->cpus[i].frequency;
            }

            fpl->cpus[0].frequency = freqSum / (maxcpu - 1);
         }
      }
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
   struct vmtotal vmtotal;

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

   len = sizeof(vmtotal);
   sysctl(MIB_vm_vmtotal, 2, &(vmtotal), &len, NULL, 0);
   pl->sharedMem = vmtotal.t_rmshr * pageSizeKb;

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

static void FreeBSDProcessList_updateExe(const struct kinfo_proc* kproc, Process* proc) {
   if (Process_isKernelThread(proc)) {
      Process_updateExe(proc, NULL);
      return;
   }

   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, kproc->ki_pid };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
      Process_updateExe(proc, NULL);
      return;
   }

   Process_updateExe(proc, buffer);
}

static void FreeBSDProcessList_updateCwd(const struct kinfo_proc* kproc, Process* proc) {
#ifdef KERN_PROC_CWD
   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_CWD, kproc->ki_pid };
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
#else
   proc->procCwd = NULL;
#endif
}

static void FreeBSDProcessList_updateProcessName(kvm_t* kd, const struct kinfo_proc* kproc, Process* proc) {
   Process_updateComm(proc, kproc->ki_comm);

   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv || !argv[0]) {
      Process_updateCmdline(proc, kproc->ki_comm, 0, strlen(kproc->ki_comm));
      return;
   }

   size_t len = 0;
   for (int i = 0; argv[i]; i++) {
      len += strlen(argv[i]) + 1;
   }

   char* cmdline = xMalloc(len);
   char* at = cmdline;
   int end = 0;
   for (int i = 0; argv[i]; i++) {
      at = stpcpy(at, argv[i]);
      if (end == 0) {
         end = at - cmdline;
      }
      *at++ = ' ';
   }
   at--;
   *at = '\0';

   Process_updateCmdline(proc, cmdline, 0, end);

   free(cmdline);
}

static char* FreeBSDProcessList_readJailName(const struct kinfo_proc* kproc) {
   if (kproc->ki_jid == 0)
      return xStrdup("-");

   char jnamebuf[MAXHOSTNAMELEN] = {0};
   struct iovec jiov[4];

IGNORE_WCASTQUAL_BEGIN
   *(const void**)&jiov[0].iov_base = "jid";
   jiov[0].iov_len = sizeof("jid");
   jiov[1].iov_base = (void*) &kproc->ki_jid;
   jiov[1].iov_len = sizeof(kproc->ki_jid);
   *(const void**)&jiov[2].iov_base = "name";
   jiov[2].iov_len = sizeof("name");
   jiov[3].iov_base = jnamebuf;
   jiov[3].iov_len = sizeof(jnamebuf);
IGNORE_WCASTQUAL_END

   int jid = jail_get(jiov, 4, 0);
   if (jid == kproc->ki_jid)
      return xStrdup(jnamebuf);

   return NULL;
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) super;
   const Settings* settings = super->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   openzfs_sysctl_updateArcStats(&fpl->zfs);
   FreeBSDProcessList_scanMemoryInfo(super);
   FreeBSDProcessList_scanCPU(super);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   int count = 0;
   const struct kinfo_proc* kprocs = kvm_getprocs(fpl->kd, KERN_PROC_PROC, 0, &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting = false;
      Process* proc = ProcessList_getProcess(super, kproc->ki_pid, &preExisting, FreeBSDProcess_new);
      FreeBSDProcess* fp = (FreeBSDProcess*) proc;

      if (!preExisting) {
         fp->jid = kproc->ki_jid;
         proc->pid = kproc->ki_pid;
         proc->isKernelThread = kproc->ki_pid != 1 && (kproc->ki_flag & P_SYSTEM);
         proc->isUserlandThread = false;
         proc->ppid = kproc->ki_ppid;
         proc->tpgid = kproc->ki_tpgid;
         proc->tgid = kproc->ki_pid;
         proc->session = kproc->ki_sid;
         proc->pgrp = kproc->ki_pgid;
         proc->st_uid = kproc->ki_uid;
         proc->starttime_ctime = kproc->ki_start.tv_sec;
         if (proc->starttime_ctime < 0) {
            proc->starttime_ctime = super->realtimeMs / 1000;
         }
         Process_fillStarttimeBuffer(proc);
         proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);
         ProcessList_add(super, proc);

         FreeBSDProcessList_updateExe(kproc, proc);
         FreeBSDProcessList_updateProcessName(fpl->kd, kproc, proc);

         if (settings->ss->flags & PROCESS_FLAG_CWD) {
            FreeBSDProcessList_updateCwd(kproc, proc);
         }

         fp->jname = FreeBSDProcessList_readJailName(kproc);

         proc->tty_nr = kproc->ki_tdev;
         const char* name = (kproc->ki_tdev != NODEV) ? devname(kproc->ki_tdev, S_IFCHR) : NULL;
         if (!name) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }
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
            FreeBSDProcessList_updateProcessName(fpl->kd, kproc, proc);
         }
      }

      free_and_xStrdup(&fp->emul, kproc->ki_emul);

      // from FreeBSD source /src/usr.bin/top/machine.c
      proc->m_virt = kproc->ki_size / ONE_K;
      proc->m_resident = kproc->ki_rssize * pageSizeKb;
      proc->nlwp = kproc->ki_numthreads;
      proc->time = (kproc->ki_runtime + 5000) / 10000;

      proc->percent_cpu = 100.0 * ((double)kproc->ki_pctcpu / (double)kernelFScale);
      proc->percent_mem = 100.0 * proc->m_resident / (double)(super->totalMem);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      if (kproc->ki_stat == SRUN && kproc->ki_oncpu != NOCPU) {
         proc->processor = kproc->ki_oncpu;
      } else {
         proc->processor = kproc->ki_lastcpu;
      }

      proc->majflt = kproc->ki_cow;

      proc->priority = kproc->ki_pri.pri_level - PZERO;

      if (String_eq("intr", kproc->ki_comm) && (kproc->ki_flag & P_SYSTEM)) {
         proc->nice = 0; //@etosan: intr kernel process (not thread) has weird nice value
      } else if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
         proc->nice = kproc->ki_nice - NZERO;
      } else if (PRI_IS_REALTIME(kproc->ki_pri.pri_class)) {
         proc->nice = PRIO_MIN - 1 - (PRI_MAX_REALTIME - kproc->ki_pri.pri_level);
      } else {
         proc->nice = PRIO_MAX + 1 + kproc->ki_pri.pri_level - PRI_MIN_IDLE;
      }

      /* Taken from: https://github.com/freebsd/freebsd-src/blob/1ad2d87778970582854082bcedd2df0394fd4933/sys/sys/proc.h#L851 */
      switch (kproc->ki_stat) {
      case SIDL:   proc->state = IDLE; break;
      case SRUN:   proc->state = RUNNING; break;
      case SSLEEP: proc->state = SLEEPING; break;
      case SSTOP:  proc->state = STOPPED; break;
      case SZOMB:  proc->state = ZOMBIE; break;
      case SWAIT:  proc->state = WAITING; break;
      case SLOCK:  proc->state = BLOCKED; break;
      default:     proc->state = UNKNOWN;
      }

      if (Process_isKernelThread(proc))
         super->kernelThreads++;

#ifdef SCHEDULER_SUPPORT
      if (settings->ss->flags & PROCESS_FLAG_SCHEDPOL)
         Scheduling_readProcessPolicy(proc);
#endif

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      super->totalTasks++;
      if (proc->state == RUNNING)
         super->runningTasks++;
      proc->updated = true;
   }
}

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id) {
   assert(id < super->existingCPUs);

   // TODO: support offline CPUs and hot swapping
   (void) super; (void) id;

   return true;
}

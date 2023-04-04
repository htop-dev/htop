/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


#include "solaris/SolarisProcessList.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/user.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <time.h>

#include "CRT.h"
#include "solaris/Platform.h"
#include "solaris/SolarisProcess.h"


#define GZONE "global    "
#define UZONE "unknown   "

static int pageSize;
static int pageSizeKB;

static char* SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
   char* zname;

   if ( sproc->zoneid == 0 ) {
      zname = xStrdup(GZONE);
   } else if ( kd == NULL ) {
      zname = xStrdup(UZONE);
   } else {
      kstat_t* ks = kstat_lookup_wrapper( kd, "zones", sproc->zoneid, NULL );
      zname = xStrdup(ks == NULL ? UZONE : ks->ks_name);
   }

   return zname;
}

static void SolarisProcessList_updateCPUcount(ProcessList* super) {
   SolarisProcessList* spl = (SolarisProcessList*) super;
   long int s;
   bool change = false;

   s = sysconf(_SC_NPROCESSORS_CONF);
   if (s < 1)
      CRT_fatalError("Cannot get existing CPU count by sysconf(_SC_NPROCESSORS_CONF)");

   if (s != super->existingCPUs) {
      if (s == 1) {
         spl->cpus = xRealloc(spl->cpus, sizeof(CPUData));
         spl->cpus[0].online = true;
      } else {
         spl->cpus = xReallocArray(spl->cpus, s + 1, sizeof(CPUData));
         spl->cpus[0].online = true; /* average is always "online" */
         for (int i = 1; i < s + 1; i++) {
            spl->cpus[i].online = false;
         }
      }

      change = true;
      super->existingCPUs = s;
   }

   s = sysconf(_SC_NPROCESSORS_ONLN);
   if (s < 1)
      CRT_fatalError("Cannot get active CPU count by sysconf(_SC_NPROCESSORS_ONLN)");

   if (s != super->activeCPUs) {
      change = true;
      super->activeCPUs = s;
   }

   if (change) {
      kstat_close(spl->kd);
      spl->kd = kstat_open();
      if (!spl->kd)
         CRT_fatalError("Cannot open kstat handle");
   }
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidMatchList, userId);

   spl->kd = kstat_open();
   if (!spl->kd)
      CRT_fatalError("Cannot open kstat handle");

   pageSize = sysconf(_SC_PAGESIZE);
   if (pageSize == -1)
      CRT_fatalError("Cannot get pagesize by sysconf(_SC_PAGESIZE)");
   pageSizeKB = pageSize / 1024;

   SolarisProcessList_updateCPUcount(pl);

   return pl;
}

static inline void SolarisProcessList_scanCPUTime(ProcessList* pl) {
   const SolarisProcessList* spl = (SolarisProcessList*) pl;
   unsigned int activeCPUs = pl->activeCPUs;
   unsigned int existingCPUs = pl->existingCPUs;
   kstat_t* cpuinfo = NULL;
   kstat_named_t* idletime = NULL;
   kstat_named_t* intrtime = NULL;
   kstat_named_t* krnltime = NULL;
   kstat_named_t* usertime = NULL;
   kstat_named_t* cpu_freq = NULL;
   double idlebuf = 0;
   double intrbuf = 0;
   double krnlbuf = 0;
   double userbuf = 0;
   int arrskip = 0;

   assert(existingCPUs > 0);
   assert(spl->kd);

   if (existingCPUs > 1) {
      // Store values for the stats loop one extra element up in the array
      // to leave room for the average to be calculated afterwards
      arrskip++;
   }

   // Calculate per-CPU statistics first
   for (unsigned int i = 0; i < existingCPUs; i++) {
      CPUData* cpuData = &(spl->cpus[i + arrskip]);

      if ((cpuinfo = kstat_lookup_wrapper(spl->kd, "cpu", i, "sys")) != NULL) {
         cpuData->online = true;
         if (kstat_read(spl->kd, cpuinfo, NULL) != -1) {
            idletime = kstat_data_lookup_wrapper(cpuinfo, "cpu_nsec_idle");
            intrtime = kstat_data_lookup_wrapper(cpuinfo, "cpu_nsec_intr");
            krnltime = kstat_data_lookup_wrapper(cpuinfo, "cpu_nsec_kernel");
            usertime = kstat_data_lookup_wrapper(cpuinfo, "cpu_nsec_user");
         }
      } else {
         cpuData->online = false;
         continue;
      }

      assert( (idletime != NULL) && (intrtime != NULL)
           && (krnltime != NULL) && (usertime != NULL) );

      if (pl->settings->showCPUFrequency) {
         if ((cpuinfo = kstat_lookup_wrapper(spl->kd, "cpu_info", i, NULL)) != NULL) {
            if (kstat_read(spl->kd, cpuinfo, NULL) != -1) {
               cpu_freq = kstat_data_lookup_wrapper(cpuinfo, "current_clock_Hz");
            }
         }

         assert( cpu_freq != NULL );
      }

      uint64_t totaltime = (idletime->value.ui64 - cpuData->lidle)
                         + (intrtime->value.ui64 - cpuData->lintr)
                         + (krnltime->value.ui64 - cpuData->lkrnl)
                         + (usertime->value.ui64 - cpuData->luser);

      // Calculate percentages of deltas since last reading
      cpuData->userPercent      = ((usertime->value.ui64 - cpuData->luser) / (double)totaltime) * 100.0;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = ((krnltime->value.ui64 - cpuData->lkrnl) / (double)totaltime) * 100.0;
      cpuData->irqPercent       = ((intrtime->value.ui64 - cpuData->lintr) / (double)totaltime) * 100.0;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = ((idletime->value.ui64 - cpuData->lidle) / (double)totaltime) * 100.0;
      // Store current values to use for the next round of deltas
      cpuData->luser            = usertime->value.ui64;
      cpuData->lkrnl            = krnltime->value.ui64;
      cpuData->lintr            = intrtime->value.ui64;
      cpuData->lidle            = idletime->value.ui64;
      // Add frequency in MHz
      cpuData->frequency        = pl->settings->showCPUFrequency ? (double)cpu_freq->value.ui64 / 1E6 : NAN;
      // Accumulate the current percentages into buffers for later average calculation
      if (existingCPUs > 1) {
         userbuf               += cpuData->userPercent;
         krnlbuf               += cpuData->systemPercent;
         intrbuf               += cpuData->irqPercent;
         idlebuf               += cpuData->idlePercent;
      }
   }

   if (existingCPUs > 1) {
      CPUData* cpuData          = &(spl->cpus[0]);
      cpuData->userPercent      = userbuf / activeCPUs;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = krnlbuf / activeCPUs;
      cpuData->irqPercent       = intrbuf / activeCPUs;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = idlebuf / activeCPUs;
   }
}

static inline void SolarisProcessList_scanMemoryInfo(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   static kstat_t      *meminfo = NULL;
   int                 ksrphyserr = -1;
   kstat_named_t       *totalmem_pgs = NULL;
   kstat_named_t       *freemem_pgs = NULL;
   kstat_named_t       *pages = NULL;
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   int                 nswap = 0;
   char                *spath = NULL;
   char                *spathbase = NULL;

   // Part 1 - physical memory
   if (spl->kd != NULL && meminfo == NULL) {
      // Look up the kstat chain just once, it never changes
      meminfo = kstat_lookup_wrapper(spl->kd, "unix", 0, "system_pages");
   }
   if (meminfo != NULL) {
      ksrphyserr = kstat_read(spl->kd, meminfo, NULL);
   }
   if (ksrphyserr != -1) {
      totalmem_pgs   = kstat_data_lookup_wrapper(meminfo, "physmem");
      freemem_pgs    = kstat_data_lookup_wrapper(meminfo, "freemem");
      pages          = kstat_data_lookup_wrapper(meminfo, "pagestotal");

      pl->totalMem   = totalmem_pgs->value.ui64 * pageSizeKB;
      if (pl->totalMem > freemem_pgs->value.ui64 * pageSizeKB) {
         pl->usedMem  = pl->totalMem - freemem_pgs->value.ui64 * pageSizeKB;
      } else {
         pl->usedMem  = 0;   // This can happen in non-global zone (in theory)
      }
      // Not sure how to implement this on Solaris - suggestions welcome!
      pl->cachedMem  = 0;
      // Not really "buffers" but the best Solaris analogue that I can find to
      // "memory in use but not by programs or the kernel itself"
      pl->buffersMem = (totalmem_pgs->value.ui64 - pages->value.ui64) * pageSizeKB;
   } else {
      // Fall back to basic sysconf if kstat isn't working
      pl->totalMem = sysconf(_SC_PHYS_PAGES) * pageSize;
      pl->buffersMem = 0;
      pl->cachedMem  = 0;
      pl->usedMem    = pl->totalMem - (sysconf(_SC_AVPHYS_PAGES) * pageSize);
   }

   // Part 2 - swap
   nswap = swapctl(SC_GETNSWP, NULL);
   if (nswap > 0) {
      sl = xMalloc((nswap * sizeof(swapent_t)) + sizeof(int));
   }
   if (sl != NULL) {
      spathbase = xMalloc( nswap * MAXPATHLEN );
   }
   if (spathbase != NULL) {
      spath = spathbase;
      swapdev = sl->swt_ent;
      for (int i = 0; i < nswap; i++, swapdev++) {
         swapdev->ste_path = spath;
         spath += MAXPATHLEN;
      }
      sl->swt_n = nswap;
   }
   nswap = swapctl(SC_LIST, sl);
   if (nswap > 0) {
      swapdev = sl->swt_ent;
      for (int i = 0; i < nswap; i++, swapdev++) {
         totalswap += swapdev->ste_pages;
         totalfree += swapdev->ste_free;
      }
   }
   free(spathbase);
   free(sl);
   pl->totalSwap = totalswap * pageSizeKB;
   pl->usedSwap  = pl->totalSwap - (totalfree * pageSizeKB);
}

static inline void SolarisProcessList_scanZfsArcstats(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   kstat_t             *arcstats = NULL;
   int                 ksrphyserr = -1;
   kstat_named_t       *cur_kstat = NULL;

   if (spl->kd != NULL) {
      arcstats = kstat_lookup_wrapper(spl->kd, "zfs", 0, "arcstats");
   }
   if (arcstats != NULL) {
      ksrphyserr = kstat_read(spl->kd, arcstats, NULL);
   }
   if (ksrphyserr != -1) {
      cur_kstat = kstat_data_lookup_wrapper( arcstats, "size" );
      spl->zfs.size = cur_kstat->value.ui64 / 1024;
      spl->zfs.enabled = spl->zfs.size > 0 ? 1 : 0;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "c_max" );
      spl->zfs.max = cur_kstat->value.ui64 / 1024;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "mfu_size" );
      spl->zfs.MFU = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "mru_size" );
      spl->zfs.MRU = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "anon_size" );
      spl->zfs.anon = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "hdr_size" );
      spl->zfs.header = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "other_size" );
      spl->zfs.other = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

      if ((cur_kstat = kstat_data_lookup_wrapper( arcstats, "compressed_size" )) != NULL) {
         spl->zfs.compressed = cur_kstat->value.ui64 / 1024;
         spl->zfs.isCompressed = 1;

         cur_kstat = kstat_data_lookup_wrapper( arcstats, "uncompressed_size" );
         spl->zfs.uncompressed = cur_kstat->value.ui64 / 1024;
      } else {
         spl->zfs.isCompressed = 0;
      }
   }
}

void ProcessList_delete(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   ProcessList_done(pl);
   free(spl->cpus);
   if (spl->kd) {
      kstat_close(spl->kd);
   }
   free(spl);
}

static void SolarisProcessList_updateExe(pid_t pid, Process* proc) {
   char path[32];
   xSnprintf(path, sizeof(path), "/proc/%d/path/a.out", pid);

   char target[PATH_MAX];
   ssize_t ret = readlink(path, target, sizeof(target) - 1);
   if (ret <= 0)
      return;

   target[ret] = '\0';
   Process_updateExe(proc, target);
}

static void SolarisProcessList_updateCwd(pid_t pid, Process* proc) {
   char path[32];
   xSnprintf(path, sizeof(path), "/proc/%d/cwd", pid);

   char target[PATH_MAX];
   ssize_t ret = readlink(path, target, sizeof(target) - 1);
   if (ret <= 0)
      return;

   target[ret] = '\0';
   free_and_xStrdup(&proc->procCwd, target);
}

/* Taken from: https://docs.oracle.com/cd/E19253-01/817-6223/6mlkidlom/index.html#tbl-sched-state */
static inline ProcessState SolarisProcessList_getProcessState(char state) {
   switch (state) {
      case 'S': return SLEEPING;
      case 'R': return RUNNABLE;
      case 'O': return RUNNING;
      case 'Z': return ZOMBIE;
      case 'T': return STOPPED;
      case 'I': return IDLE;
      default: return UNKNOWN;
   }
}

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */

static int SolarisProcessList_walkproc(psinfo_t* _psinfo, lwpsinfo_t* _lwpsinfo, void* listptr) {
   bool preExisting;
   pid_t getpid;

   // Setup process list
   ProcessList* pl = (ProcessList*) listptr;
   SolarisProcessList* spl = (SolarisProcessList*) listptr;

   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) {
      return 0;
   }

   pid_t lwpid   = (_psinfo->pr_pid * 1024) + lwpid_real;
   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);
   if (onMasterLWP) {
      getpid = _psinfo->pr_pid * 1024;
   } else {
      getpid = lwpid;
   }

   Process* proc             = ProcessList_getProcess(pl, getpid, &preExisting, SolarisProcess_new);
   SolarisProcess* sproc     = (SolarisProcess*) proc;

   // Common code pass 1
   proc->show               = false;
   sproc->taskid            = _psinfo->pr_taskid;
   sproc->projid            = _psinfo->pr_projid;
   sproc->poolid            = _psinfo->pr_poolid;
   sproc->contid            = _psinfo->pr_contract;
   proc->priority           = _lwpsinfo->pr_pri;
   proc->nice               = _lwpsinfo->pr_nice - NZERO;
   proc->processor          = _lwpsinfo->pr_onpro;
   proc->state              = SolarisProcessList_getProcessState(_lwpsinfo->pr_sname);
   // NOTE: This 'percentage' is a 16-bit BINARY FRACTIONS where 1.0 = 0x8000
   // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
   // (accessed on 18 November 2017)
   proc->percent_mem        = ((uint16_t)_psinfo->pr_pctmem / (double)32768) * (double)100.0;
   proc->pgrp               = _psinfo->pr_pgid;
   proc->nlwp               = _psinfo->pr_nlwp;
   proc->session            = _psinfo->pr_sid;

   proc->tty_nr             = _psinfo->pr_ttydev;
   const char* name = (_psinfo->pr_ttydev != PRNODEV) ? ttyname(_psinfo->pr_ttydev) : NULL;
   if (!name) {
      free(proc->tty_name);
      proc->tty_name = NULL;
   } else {
      free_and_xStrdup(&proc->tty_name, name);
   }

   proc->m_resident         = _psinfo->pr_rssize;  // KB
   proc->m_virt             = _psinfo->pr_size;    // KB

   if (proc->st_uid != _psinfo->pr_euid) {
      proc->st_uid          = _psinfo->pr_euid;
      proc->user            = UsersTable_getRef(pl->usersTable, proc->st_uid);
   }

   if (!preExisting) {
      sproc->realpid        = _psinfo->pr_pid;
      sproc->lwpid          = lwpid_real;
      sproc->zoneid         = _psinfo->pr_zoneid;
      sproc->zname          = SolarisProcessList_readZoneName(spl->kd, sproc);
      SolarisProcessList_updateExe(_psinfo->pr_pid, proc);

      Process_updateComm(proc, _psinfo->pr_fname);
      Process_updateCmdline(proc, _psinfo->pr_psargs, 0, 0);

      if (proc->settings->ss->flags & PROCESS_FLAG_CWD) {
         SolarisProcessList_updateCwd(_psinfo->pr_pid, proc);
      }
   }

   // End common code pass 1

   if (onMasterLWP) { // Are we on the representative LWP?
      proc->ppid            = (_psinfo->pr_ppid * 1024);
      proc->tgid            = (_psinfo->pr_ppid * 1024);
      sproc->realppid       = _psinfo->pr_ppid;
      sproc->realtgid       = _psinfo->pr_ppid;

      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu / (double)32768) * (double)100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->time            = _psinfo->pr_time.tv_sec * 100 + _psinfo->pr_time.tv_nsec / 10000000;
      if (!preExisting) { // Tasks done only for NEW processes
         proc->isUserlandThread = false;
         proc->starttime_ctime = _psinfo->pr_start.tv_sec;
      }

      // Update proc and thread counts based on settings
      if (proc->isKernelThread && !pl->settings->hideKernelThreads) {
         pl->kernelThreads += proc->nlwp;
         pl->totalTasks += proc->nlwp + 1;
         if (proc->state == RUNNING) {
            pl->runningTasks++;
         }
      } else if (!proc->isKernelThread) {
         if (proc->state == RUNNING) {
            pl->runningTasks++;
         }
         if (pl->settings->hideUserlandThreads) {
            pl->totalTasks++;
         } else {
            pl->userlandThreads += proc->nlwp;
            pl->totalTasks += proc->nlwp + 1;
         }
      }
      proc->show = !(pl->settings->hideKernelThreads && proc->isKernelThread);
   } else { // We are not in the master LWP, so jump to the LWP handling code
      proc->percent_cpu        = ((uint16_t)_lwpsinfo->pr_pctcpu / (double)32768) * (double)100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->time               = _lwpsinfo->pr_time.tv_sec * 100 + _lwpsinfo->pr_time.tv_nsec / 10000000;
      if (!preExisting) { // Tasks done only for NEW LWPs
         proc->isUserlandThread    = true;
         proc->ppid            = _psinfo->pr_pid * 1024;
         proc->tgid            = _psinfo->pr_pid * 1024;
         sproc->realppid       = _psinfo->pr_pid;
         sproc->realtgid       = _psinfo->pr_pid;
         proc->starttime_ctime = _lwpsinfo->pr_start.tv_sec;
      }

      // Top-level process only gets this for the representative LWP
      if (proc->isKernelThread && !pl->settings->hideKernelThreads) {
         proc->show = true;
      }
      if (!proc->isKernelThread && !pl->settings->hideUserlandThreads) {
         proc->show = true;
      }
   } // Top-level LWP or subordinate LWP

   // Common code pass 2

   if (!preExisting) {
      if ((sproc->realppid <= 0) && !(sproc->realpid <= 1)) {
         proc->isKernelThread = true;
      } else {
         proc->isKernelThread = false;
      }

      Process_fillStarttimeBuffer(proc);
      ProcessList_add(pl, proc);
   }

   proc->updated = true;

   // End common code pass 2

   return 0;
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   SolarisProcessList_updateCPUcount(super);
   SolarisProcessList_scanCPUTime(super);
   SolarisProcessList_scanMemoryInfo(super);
   SolarisProcessList_scanZfsArcstats(super);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   super->kernelThreads = 1;
   proc_walk(&SolarisProcessList_walkproc, super, PR_WALK_LWP);
}

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id) {
   assert(id < super->existingCPUs);

   const SolarisProcessList* spl = (const SolarisProcessList*) super;

   return (super->existingCPUs == 1) ? true : spl->cpus[id + 1].online;
}

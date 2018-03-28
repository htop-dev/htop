/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "SolarisProcess.h"
#include "SolarisProcessList.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/user.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <time.h>

#define MAXCMDLINE 255

/*{

#include <kstat.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>

#define ZONE_ERRMSGLEN 1024
char zone_errmsg[ZONE_ERRMSGLEN];

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   CPUData* cpus;
} SolarisProcessList;

}*/

char* SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
  char* zname;
  if ( sproc->zoneid == 0 ) {
     zname = xStrdup("global    ");
  } else if ( kd == NULL ) {
     zname = xStrdup("unknown   ");
  } else {
     kstat_t* ks = kstat_lookup( kd, "zones", sproc->zoneid, NULL );
     zname = xStrdup(ks->ks_name);
  }
  return zname;
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidWhiteList, userId);

   spl->kd = kstat_open();

   pl->cpuCount = sysconf(_SC_NPROCESSORS_ONLN);

   if (pl->cpuCount == 1 ) {
      spl->cpus = xRealloc(spl->cpus, sizeof(CPUData));
   } else {
      spl->cpus = xRealloc(spl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }

   return pl;
}

static inline void SolarisProcessList_scanCPUTime(ProcessList* pl) {
   const SolarisProcessList* spl = (SolarisProcessList*) pl;
   int cpus = pl->cpuCount;
   kstat_t *cpuinfo = NULL;
   int kchain = 0;
   kstat_named_t *idletime = NULL;
   kstat_named_t *intrtime = NULL;
   kstat_named_t *krnltime = NULL;
   kstat_named_t *usertime = NULL;
   double idlebuf = 0;
   double intrbuf = 0;
   double krnlbuf = 0;
   double userbuf = 0;
   uint64_t totaltime = 0;
   int arrskip = 0;

   assert(cpus > 0);

   if (cpus > 1) {
       // Store values for the stats loop one extra element up in the array
       // to leave room for the average to be calculated afterwards
       arrskip++;
   }

   // Calculate per-CPU statistics first
   for (int i = 0; i < cpus; i++) {
      if (spl->kd != NULL) { cpuinfo = kstat_lookup(spl->kd,"cpu",i,"sys"); }
      if (cpuinfo != NULL) { kchain = kstat_read(spl->kd,cpuinfo,NULL); }
      if (kchain  != -1  ) {
         idletime = kstat_data_lookup(cpuinfo,"cpu_nsec_idle");
         intrtime = kstat_data_lookup(cpuinfo,"cpu_nsec_intr");
         krnltime = kstat_data_lookup(cpuinfo,"cpu_nsec_kernel");
         usertime = kstat_data_lookup(cpuinfo,"cpu_nsec_user");
      }

      assert( (idletime != NULL) && (intrtime != NULL)
           && (krnltime != NULL) && (usertime != NULL) );

      CPUData* cpuData = &(spl->cpus[i+arrskip]);
      totaltime = (idletime->value.ui64 - cpuData->lidle)
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
      // Accumulate the current percentages into buffers for later average calculation
      if (cpus > 1) {
         userbuf               += cpuData->userPercent;
         krnlbuf               += cpuData->systemPercent;
         intrbuf               += cpuData->irqPercent;
         idlebuf               += cpuData->idlePercent;
      }
   }
   
   if (cpus > 1) {
      CPUData* cpuData          = &(spl->cpus[0]);
      cpuData->userPercent      = userbuf / cpus;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = krnlbuf / cpus;
      cpuData->irqPercent       = intrbuf / cpus;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = idlebuf / cpus;
   }
}

static inline void SolarisProcessList_scanMemoryInfo(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   kstat_t             *meminfo = NULL;
   int                 ksrphyserr = -1;
   kstat_named_t       *totalmem_pgs = NULL;
   kstat_named_t       *lockedmem_pgs = NULL;
   kstat_named_t       *pages = NULL;
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   int                 nswap = 0;
   char                *spath = NULL; 

   // Part 1 - physical memory
   if (spl->kd != NULL) { meminfo    = kstat_lookup(spl->kd,"unix",0,"system_pages"); }
   if (meminfo != NULL) { ksrphyserr = kstat_read(spl->kd,meminfo,NULL); }
   if (ksrphyserr != -1) {
      totalmem_pgs   = kstat_data_lookup( meminfo, "physmem" );
      lockedmem_pgs  = kstat_data_lookup( meminfo, "pageslocked" );
      pages          = kstat_data_lookup( meminfo, "pagestotal" );

      pl->totalMem   = totalmem_pgs->value.ui64 * PAGE_SIZE_KB;
      pl->usedMem    = lockedmem_pgs->value.ui64 * PAGE_SIZE_KB;
      // Not sure how to implement this on Solaris - suggestions welcome!
      pl->cachedMem  = 0;     
      // Not really "buffers" but the best Solaris analogue that I can find to
      // "memory in use but not by programs or the kernel itself"
      pl->buffersMem = (totalmem_pgs->value.ui64 - pages->value.ui64) * PAGE_SIZE_KB;
   } else {
      // Fall back to basic sysconf if kstat isn't working
      pl->totalMem = sysconf(_SC_PHYS_PAGES) * PAGE_SIZE;
      pl->buffersMem = 0;
      pl->cachedMem  = 0;
      pl->usedMem    = pl->totalMem - (sysconf(_SC_AVPHYS_PAGES) * PAGE_SIZE);
   }
   
   // Part 2 - swap
   nswap = swapctl(SC_GETNSWP, NULL);
   if (nswap >     0) { sl  = malloc(nswap * sizeof(swapent_t) + sizeof(int)); }
   if (sl    != NULL) { spath = malloc( nswap * MAXPATHLEN ); }
   if (spath != NULL) { 
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
         free(swapdev->ste_path);
      }
      free(sl);
   }
   pl->totalSwap = totalswap * PAGE_SIZE_KB;
   pl->usedSwap  = pl->totalSwap - (totalfree * PAGE_SIZE_KB); 
}

void ProcessList_delete(ProcessList* this) {
   const SolarisProcessList* spl = (SolarisProcessList*) this;
   if (spl->kd) kstat_close(spl->kd);
   free(spl->cpus);
   ProcessList_done(this);
   free(this);
}

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */ 

int SolarisProcessList_walkproc(psinfo_t *_psinfo, lwpsinfo_t *_lwpsinfo, void *listptr) {
   struct timeval tv;
   struct tm date;
   bool preExistingP = false;
   bool preExistingL = false;
   bool preExisting;
   Process *cproc;
   SolarisProcess *csproc;

   // Setup process list
   ProcessList *pl = (ProcessList*) listptr;
   SolarisProcessList *spl = (SolarisProcessList*) listptr;

   // Setup Process entry
   Process *proc           = ProcessList_getProcess(pl, _psinfo->pr_pid * 1024, &preExistingP, (Process_New) SolarisProcess_new);
   SolarisProcess *sproc   = (SolarisProcess*) proc;

   // Setup LWP entry
   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) return 0;
   pid_t lwpid   = proc->pid + lwpid_real;
   Process *lwp            = ProcessList_getProcess(pl, lwpid, &preExistingL, (Process_New) SolarisProcess_new);
   SolarisProcess *slwp    = (SolarisProcess*) lwp;

   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);

   // Determine whether we're updating proc info or LWP info
   // based on whether or not we're on the representative LWP
   // for a given proc
   if (onMasterLWP) {
      cproc = proc;
      csproc = sproc;
      preExisting = preExistingP;
   } else {
      cproc = lwp;
      csproc = slwp;
      preExisting = preExistingL;
   }

   gettimeofday(&tv, NULL);

   // Common code pass 1
   cproc->show              = false;
   csproc->zoneid           = _psinfo->pr_zoneid;
   csproc->zname            = SolarisProcessList_readZoneName(spl->kd,sproc);
   csproc->taskid           = _psinfo->pr_taskid;
   csproc->projid           = _psinfo->pr_projid;
   csproc->poolid           = _psinfo->pr_poolid;
   csproc->contid           = _psinfo->pr_contract;
   cproc->priority          = _lwpsinfo->pr_pri;
   cproc->nice              = _lwpsinfo->pr_nice;
   cproc->processor         = _lwpsinfo->pr_onpro;
   cproc->state             = _lwpsinfo->pr_sname;
   // This could potentially get bungled if the master LWP is not the first
   // one enumerated.  Unaware of any workaround right now.
   if ((cproc->state == 'O') && !onMasterLWP) proc->state = 'O';
   // NOTE: This 'percentage' is a 16-bit BINARY FRACTIONS where 1.0 = 0x8000
   // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
   // (accessed on 18 November 2017)
   cproc->percent_mem       = ((uint16_t)_psinfo->pr_pctmem/(double)32768)*(double)100.0;
   cproc->st_uid            = _psinfo->pr_euid;
   cproc->user              = UsersTable_getRef(pl->usersTable, proc->st_uid);
   cproc->pgrp              = _psinfo->pr_pgid;
   cproc->nlwp              = _psinfo->pr_nlwp;
   cproc->comm              = xStrdup(_psinfo->pr_fname);
   cproc->commLen           = strnlen(_psinfo->pr_fname,PRFNSZ);
   cproc->tty_nr            = _psinfo->pr_ttydev;
   cproc->m_resident        = _psinfo->pr_rssize/PAGE_SIZE_KB;
   cproc->m_size            = _psinfo->pr_size/PAGE_SIZE_KB;

   if (!preExisting) {
      csproc->realpid          = _psinfo->pr_pid;
      csproc->lwpid            = lwpid_real;
   }

   // End common code pass 1

   if (onMasterLWP) { // Are we on the representative LWP?
      proc->ppid            = (_psinfo->pr_ppid * 1024);
      proc->tgid            = (_psinfo->pr_ppid * 1024);
      sproc->realppid       = _psinfo->pr_ppid;
      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu/(double)32768)*(double)100.0;
      proc->time            = _psinfo->pr_time.tv_sec;
      if(!preExistingP) { // Tasks done only for NEW processes
         sproc->is_lwp = false;
         proc->starttime_ctime = _psinfo->pr_start.tv_sec;
      }

      // Update proc and thread counts based on settings
      if (sproc->kernel && !pl->settings->hideKernelThreads) {
         pl->kernelThreads += proc->nlwp;
         pl->totalTasks += proc->nlwp+1;
         if (proc->state == 'O') pl->runningTasks++;
      } else if (!sproc->kernel) {
         if (proc->state == 'O') pl->runningTasks++;
         if (pl->settings->hideUserlandThreads) {
            pl->totalTasks++;
         } else {
            pl->userlandThreads += proc->nlwp;
            pl->totalTasks += proc->nlwp+1;
         }
      }
      proc->show = !(pl->settings->hideKernelThreads && sproc->kernel);
   } else { // We are not in the master LWP, so jump to the LWP handling code
      lwp->percent_cpu        = ((uint16_t)_lwpsinfo->pr_pctcpu/(double)32768)*(double)100.0;
      lwp->time               = _lwpsinfo->pr_time.tv_sec;
      if (!preExistingL) { // Tasks done only for NEW LWPs
         slwp->is_lwp         = true; 
         lwp->basenameOffset  = -1;
         lwp->ppid            = proc->pid;
         lwp->tgid            = proc->pid;
         slwp->realppid       = sproc->realpid;
         lwp->starttime_ctime = _lwpsinfo->pr_start.tv_sec;
      }

      // Top-level process only gets this for the representative LWP
      if (slwp->kernel  && !pl->settings->hideKernelThreads)   lwp->show = true;
      if (!slwp->kernel && !pl->settings->hideUserlandThreads) lwp->show = true;
   } // Top-level LWP or subordinate LWP

   // Common code pass 2

   if (!preExisting) {
      if ((sproc->realppid <= 0) && !(sproc->realpid <= 1)) {
         csproc->kernel = true;
      } else {
         csproc->kernel = false;
      }
      (void) localtime_r((time_t*) &cproc->starttime_ctime, &date);
      strftime(cproc->starttime_show, 7, ((cproc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
      ProcessList_add(pl, cproc);
   }
   cproc->updated = true;

   // End common code pass 2

   return 0;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);
   this->kernelThreads = 1;
   proc_walk(&SolarisProcessList_walkproc, this, PR_WALK_LWP);
}


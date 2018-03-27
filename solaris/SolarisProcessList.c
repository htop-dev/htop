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
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>
#include <math.h>
#include <time.h>

#define MAXCMDLINE 255

/*{

#include <kstat.h>
#include <sys/param.h>
#include <zone.h>
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

void ProcessList_enumerateLWPs(Process* proc, char* name, ProcessList* pl, struct timeval tv) {
   Process *lwp;
   SolarisProcess *slwp;
   SolarisProcess *sproc = (SolarisProcess*) proc;
   char lwpdir[MAX_NAME+1];
   DIR* dir = NULL;
   FILE* fp = NULL;
   pid_t lwpid;
   bool preExisting = false;   
   char filename[MAX_NAME+1];
   lwpsinfo_t _lwpsinfo;
   prusage_t _lwprusage;
   struct tm date;
   xSnprintf(lwpdir, MAX_NAME, "%s/%s/lwp", PROCDIR, name);
   struct dirent* entry;
   char* lwpname;
   bool haveUsage = false;

   dir = opendir(lwpdir);
   if (!dir) return;
   while ((entry = readdir(dir)) != NULL) {
      lwpname = entry->d_name;
      // With 10 bits to spare, we can only list up to 1023 unique LWPs per process
      if (atoi(lwpname) > 1023) break;
      lwpid   = proc->pid + atoi(lwpname);
      lwp     = ProcessList_getProcess(pl, lwpid, &preExisting, (Process_New) SolarisProcess_new);
      slwp    = (SolarisProcess*) lwp;
      xSnprintf(filename, MAX_NAME, "%s/%s/lwp/%s/lwpsinfo", PROCDIR, name, lwpname);
      fp   = fopen(filename, "r");
      if ( fp == NULL ) continue;
      fread(&_lwpsinfo,sizeof(lwpsinfo_t),1,fp);
      fclose(fp);
      xSnprintf(filename, MAX_NAME, "%s/%s/lwp/%s/lwpusage", PROCDIR, name, lwpname);
      fp   = fopen(filename, "r");
      if ( fp != NULL ) {
         haveUsage = true;
         fread(&_lwprusage,sizeof(prusage_t),1,fp);
         fclose(fp);
      }

      // Common items set for both new and refreshed LWPs
      slwp->zoneid            = sproc->zoneid;
      lwp->percent_cpu        = ((uint16_t)_lwpsinfo.pr_pctcpu/(double)32768)*(double)100.0;
      lwp->pgrp               = proc->pgrp;
      lwp->st_uid             = proc->st_uid;
      lwp->user               = UsersTable_getRef(pl->usersTable, lwp->st_uid);
      lwp->session            = proc->session;
      lwp->comm               = xStrdup(proc->comm);
      lwp->commLen            = strnlen(proc->comm,PRFNSZ);
      slwp->zname             = sproc->zname;
      lwp->tty_nr             = proc->tty_nr;
      if (haveUsage) {
         lwp->majflt          = _lwprusage.pr_majf;
         lwp->minflt          = _lwprusage.pr_minf;
      } else {
         lwp->majflt          = 0;
         lwp->minflt          = 0;
      }
      lwp->priority           = _lwpsinfo.pr_pri;
      lwp->nice               = _lwpsinfo.pr_nice;
      lwp->processor          = _lwpsinfo.pr_onpro;
      lwp->state              = _lwpsinfo.pr_sname;
      lwp->time               = _lwpsinfo.pr_time.tv_sec;
      slwp->taskid            = sproc->taskid;
      slwp->projid            = sproc->projid;
      slwp->poolid            = sproc->poolid;
      slwp->contid            = sproc->contid;
      lwp->show               = false;

      // Tasks done only for NEW LWPs  
      if (!preExisting) {
         slwp->is_lwp         = true; 
         lwp->basenameOffset  = -1;
         slwp->kernel         = sproc->kernel;
         // Fake values used for sorting
         // Only set once because threads don't generally
         // move... between... processes.
         lwp->pid             = lwpid;
         lwp->ppid            = proc->pid;
         lwp->tgid            = proc->pid;
         // Corresponding real values used for display
         slwp->realpid        = sproc->realpid;
         slwp->realppid       = sproc->realpid;
         slwp->lwpid          = atoi(lwpname);
         // Not tracked per thread
         lwp->percent_mem     = (double)0.0;
         lwp->nlwp            = 0;
         lwp->m_resident      = 0;
         lwp->m_size          = 0;
         lwp->starttime_ctime = _lwpsinfo.pr_start.tv_sec;
         (void) localtime_r((time_t*) &lwp->starttime_ctime, &date);
         strftime(lwp->starttime_show, 7, ((lwp->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
         ProcessList_add(pl, lwp);
      }

      // Top-level process only gets this for the representative LWP
      if (lwp->state == 'O') proc->state = 'O';
      if (slwp->kernel  && !pl->settings->hideKernelThreads)   lwp->show = true;
      if (!slwp->kernel && !pl->settings->hideUserlandThreads) lwp->show = true;
      lwp->updated = true;

   }

   closedir(dir);

}


void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList* spl = (SolarisProcessList*) this;
   DIR* dir = NULL;
   struct dirent* entry = NULL;
   char*  name = NULL;
   int    pid;
   bool   preExisting = false;
   Process* proc = NULL;
   SolarisProcess* sproc = NULL;
   psinfo_t _psinfo;
   pstatus_t _pstatus;
   prusage_t _prusage;
   char filename[MAX_NAME+1];
   FILE *fp = NULL;
   struct timeval tv;
   struct tm date;

   gettimeofday(&tv, NULL);

   // If these fail, then the relevant metrics will simply display as zero
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);

   dir = opendir(PROCDIR); 
   if (!dir) return; // Is proc mounted?
   // We always count the scheduler
   this->kernelThreads = 1;
   while ((entry = readdir(dir)) != NULL) {
      name = entry->d_name;
      pid = atoi(name);
      proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) SolarisProcess_new);
      sproc = (SolarisProcess *) proc;
      xSnprintf(filename, MAX_NAME, "%s/%s/psinfo", PROCDIR, name);
      fp = fopen(filename, "r");
      if ( fp == NULL ) continue;
      fread(&_psinfo,sizeof(psinfo_t),1,fp);
      fclose(fp);
      xSnprintf(filename, MAX_NAME, "%s/%s/status", PROCDIR, name);
      fp   = fopen(filename, "r");
      if ( fp != NULL ) {
         fread(&_pstatus,sizeof(pstatus_t),1,fp);
      }
      fclose(fp);
      xSnprintf(filename, MAX_NAME, "%s/%s/usage", PROCDIR, name);
      fp = fopen(filename,"r");
      if ( fp == NULL ) continue;
      fread(&_prusage,sizeof(prusage_t),1,fp);
      fclose(fp);

      // Common items set for both new and refreshed processes
      proc->ppid            = (_psinfo.pr_ppid * 1024);
      proc->tgid            = (_psinfo.pr_ppid * 1024);
      sproc->realppid       = _psinfo.pr_ppid;
      sproc->zoneid         = _psinfo.pr_zoneid;
      sproc->zname          = SolarisProcessList_readZoneName(spl->kd,sproc);
      // NOTE: These 'percentages' are 16-bit BINARY FRACTIONS where 1.0 = 0x8000
      // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
      // (accessed on 18 November 2017)
      proc->percent_cpu     = ((uint16_t)_psinfo.pr_pctcpu/(double)32768)*(double)100.0;
      proc->percent_mem     = ((uint16_t)_psinfo.pr_pctmem/(double)32768)*(double)100.0;
      proc->st_uid          = _psinfo.pr_euid;
      proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
      proc->pgrp            = _psinfo.pr_pgid;
      proc->nlwp            = _psinfo.pr_nlwp;
      proc->session         = _pstatus.pr_sid;
      proc->comm            = xStrdup(_psinfo.pr_fname);
      proc->commLen         = strnlen(_psinfo.pr_fname,PRFNSZ);
      proc->tty_nr          = _psinfo.pr_ttydev;
      proc->majflt          = _prusage.pr_majf;
      proc->minflt          = _prusage.pr_minf;
      proc->m_resident      = _psinfo.pr_rssize/PAGE_SIZE_KB;
      proc->m_size          = _psinfo.pr_size/PAGE_SIZE_KB;
      proc->priority        = _psinfo.pr_lwp.pr_pri;
      proc->nice            = _psinfo.pr_lwp.pr_nice;
      proc->processor       = _psinfo.pr_lwp.pr_onpro;
      proc->state           = _psinfo.pr_lwp.pr_sname;
      proc->time            = _psinfo.pr_time.tv_sec;
      sproc->taskid         = _psinfo.pr_taskid;
      sproc->projid         = _psinfo.pr_projid;
      sproc->poolid         = _psinfo.pr_poolid;
      sproc->contid         = _psinfo.pr_contract;
         
      // Tasks done only for NEW processes
      if(!preExisting) {
         sproc->is_lwp = false;
         // Fake PID values used for sorting, since Solaris LWPs lack unique PIDs
         proc->pid             = (_psinfo.pr_pid * 1024);
         // Corresponding real values used for display
         sproc->realpid        = _psinfo.pr_pid;
         sproc->lwpid          = 0;
         proc->starttime_ctime = _psinfo.pr_start.tv_sec;
         if ((sproc->realppid <= 0) && !(sproc->realpid <= 1)) {
            sproc->kernel = true;
         } else {
            sproc->kernel = false;
         }
         (void) localtime_r((time_t*) &proc->starttime_ctime, &date);
         strftime(proc->starttime_show, 7, ((proc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
         ProcessList_add(this, proc);
      }

      if (proc->nlwp > 1) {
         ProcessList_enumerateLWPs(proc, name, this, tv);
      }

      proc->show = !(this->settings->hideKernelThreads && sproc->kernel);

      if (sproc->kernel && !this->settings->hideKernelThreads) {
         this->kernelThreads += proc->nlwp;
         this->totalTasks += proc->nlwp+1;
         if (proc->state == 'O') this->runningTasks++;
      } else if (!sproc->kernel) {
         if (proc->state == 'O') this->runningTasks++;
         if (this->settings->hideUserlandThreads) {
            this->totalTasks++;
         } else {
            this->userlandThreads += proc->nlwp;
            this->totalTasks += proc->nlwp+1;
         }
      }

      proc->updated = true;

   } // while ((entry = readdir(dir)) != NULL)

   closedir(dir);

}


/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "SolarisProcessList.h"
#include "SolarisProcess.h"

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
#include <sys/zone.h>
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

static void setCommand(Process* process, const char* command, int len) {
   if (process->comm && process->commLen >= len) {
      strncpy(process->comm, command, len + 1);
   } else {
      free(process->comm);
      process->comm = xStrdup(command);
   }
   process->commLen = len;
}

static void setZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
  if ( sproc->zoneid == 0 ) {
     strncpy( sproc->zname, "global    ", 11);
  } else if ( kd == NULL ) {
     strncpy( sproc->zname, "unknown   ", 11);
  } else {
     kstat_t* ks = kstat_lookup( kd, "zones", sproc->zoneid, NULL );
     strncpy( sproc->zname, ks->ks_name, strlen(ks->ks_name) );
  }
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
   int                 ksrphyserr = 0;
   kstat_named_t       *totalmem_pgs = NULL;
   kstat_named_t       *lockedmem_pgs = NULL;
   kstat_named_t       *pages = NULL;
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   int                 nswap = 0;
   char                *spath = NULL; 
   // PAGE_SIZE is a macro to a function call.
   // Since we use it so much in here, go ahead copy
   // the value locally.
   int              pgsiz = PAGE_SIZE;

   // Part 1 - physical memory
   if (spl->kd != NULL) { meminfo    = kstat_lookup(spl->kd,"unix",0,"system_pages"); }
   if (meminfo != NULL) { ksrphyserr = kstat_read(spl->kd,meminfo,NULL); }
   if (ksrphyserr != -1) {
      totalmem_pgs   = kstat_data_lookup( meminfo, "physmem" );
      lockedmem_pgs  = kstat_data_lookup( meminfo, "pageslocked" );
      pages          = kstat_data_lookup( meminfo, "pagestotal" );

      pl->totalMem   = ((totalmem_pgs->value.ui64)/1024)  * pgsiz;
      pl->usedMem    = ((lockedmem_pgs->value.ui64)/1024) * pgsiz;
      // Not sure how to implement this on Solaris - suggestions welcome!
      pl->cachedMem  = 0;     
      // Not really "buffers" but the best Solaris analogue that I can find to
      // "memory in use but not by programs or the kernel itself"
      pl->buffersMem = (((totalmem_pgs->value.ui64)/1024) - (pages->value.ui64)/1024) * pgsiz;
   } else {
      // Fall back to basic sysconf if kstat isn't working
      pl->totalMem = sysconf(_SC_PHYS_PAGES) * pgsiz;
      pl->buffersMem = 0;
      pl->cachedMem  = 0;
      pl->usedMem    = pl->totalMem - (sysconf(_SC_AVPHYS_PAGES) * pgsiz);
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
   pl->totalSwap = (totalswap * pgsiz)/1024;
   pl->usedSwap  = pl->totalSwap - ((totalfree * pgsiz)/1024); 
}

void ProcessList_delete(ProcessList* this) {
   const SolarisProcessList* spl = (SolarisProcessList*) this;
   if (spl->kd) kstat_close(spl->kd);
   free(spl->cpus);
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList* spl = (SolarisProcessList*) this;
   Settings* settings = this->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   DIR* dir = NULL;
   struct dirent* entry = NULL;
   char*  name = NULL;
   int    pid;
   bool   preExisting = false;
   Process* proc = NULL;
   Process* parent = NULL;
   SolarisProcess* sproc = NULL;
   psinfo_t _psinfo;
   pstatus_t _pstatus;
   prusage_t _prusage;
   char filename[MAX_NAME+1];
   FILE *fp = NULL;
   uint64_t addRunning = 0;
   uint64_t addTotal = 0;
   struct timeval tv;
   struct tm date;

   gettimeofday(&tv, NULL);

   // If these fail, then the relevant metrics will simply display as zero
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);

   dir = opendir(PROCDIR); 
   if (!dir) return; // Is proc mounted?
   while ((entry = readdir(dir)) != NULL) {
      addRunning = 0;
      addTotal = 0;
      name = entry->d_name;
      pid = atoi(name);
      proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) SolarisProcess_new);
      proc->tgid = parent ? parent->pid : pid;
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

      if(!preExisting) {
         sproc->kernel         = false;
         proc->pid             = _psinfo.pr_pid;
         proc->ppid            = _psinfo.pr_ppid;
         proc->tgid            = _psinfo.pr_pid;
         sproc->zoneid         = _psinfo.pr_zoneid;
         proc->tty_nr          = _psinfo.pr_ttydev;
         proc->pgrp            = _psinfo.pr_pgid;
         // NOTE: These 'percentages' are 16-bit BINARY FRACTIONS where 1.0 = 0x8000
         // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
         // (accessed on 18 November 2017)
         proc->percent_cpu     = ((uint16_t)_psinfo.pr_pctcpu/(double)32768)*(double)100.0;
         proc->percent_mem     = ((uint16_t)_psinfo.pr_pctmem/(double)32768)*(double)100.0;
         proc->st_uid          = _psinfo.pr_euid;
         proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
         proc->nlwp            = _psinfo.pr_nlwp;
         proc->session         = _pstatus.pr_sid;
         setCommand(proc,_psinfo.pr_fname,PRFNSZ);
         setZoneName(spl->kd,sproc);
         proc->majflt          = _prusage.pr_majf;
         proc->minflt          = _prusage.pr_minf; 
         proc->m_resident      = (_psinfo.pr_rssize)/8;
         proc->m_size          = (_psinfo.pr_size)/8;
         proc->priority        = _psinfo.pr_lwp.pr_pri;
         proc->nice            = _psinfo.pr_lwp.pr_nice;
         proc->processor       = _psinfo.pr_lwp.pr_onpro;
         proc->state           = _psinfo.pr_lwp.pr_sname;
         proc->time            = _psinfo.pr_time.tv_sec;
         sproc->taskid         = _psinfo.pr_taskid;
         sproc->projid         = _psinfo.pr_projid;
         sproc->poolid         = _psinfo.pr_poolid;
         sproc->contid         = _psinfo.pr_contract;
         proc->starttime_ctime = _psinfo.pr_start.tv_sec;
         (void) localtime_r((time_t*) &proc->starttime_ctime, &date);
         strftime(proc->starttime_show, 7, ((proc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date); 
         ProcessList_add(this, proc);
      } else {
         proc->ppid            = _psinfo.pr_ppid;
         sproc->zoneid         = _psinfo.pr_zoneid;
         // See note above about these percentages
         proc->percent_cpu     = ((uint16_t)_psinfo.pr_pctcpu/(double)32768)*(double)100.0;
         proc->percent_mem     = ((uint16_t)_psinfo.pr_pctmem/(double)32768)*(double)100.0;
         proc->st_uid          = _psinfo.pr_euid;
         proc->pgrp            = _psinfo.pr_pgid;
         proc->nlwp            = _psinfo.pr_nlwp;
         proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
         setCommand(proc,_psinfo.pr_fname,PRFNSZ);
         setZoneName(spl->kd,sproc);
         proc->majflt          = _prusage.pr_majf;
         proc->minflt          = _prusage.pr_minf;
         proc->m_resident      = (_psinfo.pr_rssize)/8;
         proc->m_size          = (_psinfo.pr_size)/8;
         proc->priority        = _psinfo.pr_lwp.pr_pri;
         proc->nice            = _psinfo.pr_lwp.pr_nice;
         proc->processor       = _psinfo.pr_lwp.pr_onpro;
         proc->state           = _psinfo.pr_lwp.pr_sname;
         proc->time            = _psinfo.pr_time.tv_sec;
         sproc->taskid         = _psinfo.pr_taskid;
         sproc->projid         = _psinfo.pr_projid;
         sproc->poolid         = _psinfo.pr_poolid;
         sproc->contid         = _psinfo.pr_contract;
      }
      proc->show = !(hideKernelThreads && (_pstatus.pr_flags & PR_ISSYS));
      if (_pstatus.pr_flags & PR_ISSYS) {
         if (hideKernelThreads) {
            addRunning = 0;
            addTotal   = 0;
         } else {
            this->kernelThreads += proc->nlwp;
            if (proc->state == 'O') {
               addRunning++;
               addTotal = proc->nlwp+1;
            } else {
               addTotal = proc->nlwp+1;
            }
         }
      } else {
         if (hideUserlandThreads) {
            if(proc->state == 'O') {
               addRunning++;
               addTotal++;
            } else {
               addTotal++;
            }
         } else {
            this->userlandThreads += proc->nlwp;
            if(proc->state == 'O') {
               addRunning++;
               addTotal = proc->nlwp+1;
            } else {
               addTotal = proc->nlwp+1;
            }
         }
      }
      this->runningTasks+=addRunning;
      this->totalTasks+=addTotal;
      proc->updated = true;
   } // while ((entry = readdir(dir)) != NULL)
   closedir(dir);
}


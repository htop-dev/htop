/*
htop - SolarisMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "solaris/SolarisMachine.h"

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


static void SolarisMachine_updateCPUcount(SolarisMachine* this) {
   Machine* super = &this->super;
   long int s;
   bool change = false;

   s = sysconf(_SC_NPROCESSORS_CONF);
   if (s < 1)
      CRT_fatalError("Cannot get existing CPU count by sysconf(_SC_NPROCESSORS_CONF)");

   if (s != super->existingCPUs) {
      if (s == 1) {
         this->cpus = xRealloc(this->cpus, sizeof(CPUData));
         this->cpus[0].online = true;
      } else {
         this->cpus = xReallocArray(this->cpus, s + 1, sizeof(CPUData));
         this->cpus[0].online = true; /* average is always "online" */
         for (int i = 1; i < s + 1; i++) {
            this->cpus[i].online = false;
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
      hsuper->activeCPUs = s;
   }

   if (change) {
      kstat_close(this->kd);
      this->kd = kstat_open();
      if (!this->kd)
         CRT_fatalError("Cannot open kstat handle");
   }
}


static void SolarisMachine_scanCPUTime(SolarisMachine* this) {
   Machine* super = &this->super;
   unsigned int activeCPUs = super->activeCPUs;
   unsigned int existingCPUs = super->existingCPUs;
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
   assert(this->kd);

   if (existingCPUs > 1) {
      // Store values for the stats loop one extra element up in the array
      // to leave room for the average to be calculated afterwards
      arrskip++;
   }

   // Calculate per-CPU statistics first
   for (unsigned int i = 0; i < existingCPUs; i++) {
      CPUData* cpuData = &(this->cpus[i + arrskip]);

      if ((cpuinfo = kstat_lookup_wrapper(this->kd, "cpu", i, "sys")) != NULL) {
         cpuData->online = true;
         if (kstat_read(this->kd, cpuinfo, NULL) != -1) {
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

      if (super->settings->showCPUFrequency) {
         if ((cpuinfo = kstat_lookup_wrapper(this->kd, "cpu_info", i, NULL)) != NULL) {
            if (kstat_read(this->kd, cpuinfo, NULL) != -1) {
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
      cpuData->frequency        = super->settings->showCPUFrequency ? (double)cpu_freq->value.ui64 / 1E6 : NAN;
      // Accumulate the current percentages into buffers for later average calculation
      if (existingCPUs > 1) {
         userbuf               += cpuData->userPercent;
         krnlbuf               += cpuData->systemPercent;
         intrbuf               += cpuData->irqPercent;
         idlebuf               += cpuData->idlePercent;
      }
   }

   if (existingCPUs > 1) {
      CPUData* cpuData          = &(this->cpus[0]);
      cpuData->userPercent      = userbuf / activeCPUs;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = krnlbuf / activeCPUs;
      cpuData->irqPercent       = intrbuf / activeCPUs;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = idlebuf / activeCPUs;
   }
}

static void SolarisMachine_scanMemoryInfo(SolarisMachine* this) {
   Machine*            super = &this->super;
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
   if (this->kd != NULL && meminfo == NULL) {
      // Look up the kstat chain just once, it never changes
      meminfo = kstat_lookup_wrapper(this->kd, "unix", 0, "system_pages");
   }
   if (meminfo != NULL) {
      ksrphyserr = kstat_read(this->kd, meminfo, NULL);
   }
   if (ksrphyserr != -1) {
      totalmem_pgs   = kstat_data_lookup_wrapper(meminfo, "physmem");
      freemem_pgs    = kstat_data_lookup_wrapper(meminfo, "freemem");
      pages          = kstat_data_lookup_wrapper(meminfo, "pagestotal");

      super->totalMem = totalmem_pgs->value.ui64 * this->pageSizeKB;
      if (super->totalMem > freemem_pgs->value.ui64 * this->pageSizeKB) {
         super->usedMem = super->totalMem - freemem_pgs->value.ui64 * this->pageSizeKB;
      } else {
         super->usedMem = 0;   // This can happen in non-global zone (in theory)
      }
      // Not sure how to implement this on Solaris - suggestions welcome!
      super->cachedMem = 0;
      // Not really "buffers" but the best Solaris analogue that I can find to
      // "memory in use but not by programs or the kernel itself"
      super->buffersMem = (totalmem_pgs->value.ui64 - pages->value.ui64) * this->pageSizeKB;
   } else {
      // Fall back to basic sysconf if kstat isn't working
      super->totalMem = sysconf(_SC_PHYS_PAGES) * this->pageSize;
      super->buffersMem = 0;
      super->cachedMem = 0;
      super->usedMem = super->totalMem - (sysconf(_SC_AVPHYS_PAGES) * this->pageSize);
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
   super->totalSwap = totalswap * this->pageSizeKB;
   super->usedSwap  = super->totalSwap - (totalfree * this->pageSizeKB);
}

static void SolarisMachine_scanZfsArcstats(SolarisMachine* this) {
   kstat_named_t       *cur_kstat;
   kstat_t             *arcstats;
   int                 ksrphyserr;

   if (this->kd == NULL)
      return;

   arcstats = kstat_lookup_wrapper(this->kd, "zfs", 0, "arcstats");
   if (arcstats == NULL)
      return;

   ksrphyserr = kstat_read(this->kd, arcstats, NULL);
   if (ksrphyserr == -1)
      return;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "size" );
   this->zfs.size = cur_kstat->value.ui64 / 1024;
   this->zfs.enabled = this->zfs.size > 0 ? 1 : 0;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "c_max" );
   this->zfs.max = cur_kstat->value.ui64 / 1024;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "mfu_size" );
   this->zfs.MFU = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "mru_size" );
   this->zfs.MRU = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "anon_size" );
   this->zfs.anon = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "hdr_size" );
   this->zfs.header = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

   cur_kstat = kstat_data_lookup_wrapper( arcstats, "other_size" );
   this->zfs.other = cur_kstat != NULL ? cur_kstat->value.ui64 / 1024 : 0;

   if ((cur_kstat = kstat_data_lookup_wrapper( arcstats, "compressed_size" )) != NULL) {
      this->zfs.compressed = cur_kstat->value.ui64 / 1024;
      this->zfs.isCompressed = 1;

      cur_kstat = kstat_data_lookup_wrapper( arcstats, "uncompressed_size" );
      this->zfs.uncompressed = cur_kstat->value.ui64 / 1024;
   } else {
      this->zfs.isCompressed = 0;
   }
}

void Machine_scan(Machine* super) {
   SolarisMachine* this = (SolarisMachine*) super;

   SolarisMachine_updateCPUcount(this);
   SolarisMachine_scanCPUTime(this);
   SolarisMachine_scanMemoryInfo(this);
   SolarisMachine_scanZfsArcstats(this);
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   SolarisMachine* this = xCalloc(1, sizeof(SolarisMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   this->pageSize = sysconf(_SC_PAGESIZE);
   if (this->pageSize == -1)
      CRT_fatalError("Cannot get pagesize by sysconf(_SC_PAGESIZE)");
   this->pageSizeKB = this->pageSize / 1024;

   SolarisMachine_updateCPUcount(this);

   return super;
}

void Machine_delete(Machine* super) {
   SolarisMachine* this = (SolarisMachine*) super;

   Machine_done(super);

   free(this->cpus);
   if (this->kd) {
      kstat_close(this->kd);
   }
   free(this);
}

bool Machine_isCPUonline(const Machine* super, unsigned int id) {
   assert(id < super->existingCPUs);

   const SolarisMachine* this = (const SolarisMachine*) super;

   return (super->existingCPUs == 1) ? true : this->cpus[id + 1].online;
}

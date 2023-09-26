/*
htop - CygwinMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "cygwin/CygwinMachine.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "CRT.h"
#include "XUtils.h"


static void CygwinMachine_updateCPUcount(CygwinMachine* this) {
   Machine* super = &this->super;
   long int s;

   s = sysconf(_SC_NPROCESSORS_CONF);
   if (s < 1)
      CRT_fatalError("Cannot get existing CPU count by sysconf(_SC_NPROCESSORS_CONF)");

   if (s != super->existingCPUs) {
      if (s == 1) {
         this->cpuData = xRealloc(this->cpuData, sizeof(CPUData));
         this->cpuData[0].online = true;
      } else {
         this->cpuData = xReallocArray(this->cpuData, s + 1, sizeof(CPUData));
         this->cpuData[0].online = true; /* average is always "online" */
         for (int i = 1; i < s + 1; i++) {
            this->cpuData[i].online = true;  // TODO: support offline CPUs and hot swapping
         }
      }

      super->existingCPUs = s;
   }

   s = sysconf(_SC_NPROCESSORS_ONLN);
   if (s < 1)
      CRT_fatalError("Cannot get active CPU count by sysconf(_SC_NPROCESSORS_ONLN)");

   if (s != super->activeCPUs) {
      super->activeCPUs = s;
   }
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   CygwinMachine* this = xCalloc(1, sizeof(CygwinMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   CygwinMachine_updateCPUcount(this);

   return super;
}

void Machine_delete(Machine* super) {
   CygwinMachine* this = (CygwinMachine*) super;

   Machine_done(super);
   free(this->cpuData);
   free(this);
}

static void CygwinMachine_scanMemoryInfo(CygwinMachine* this) {
   Machine* host = &this->super;

   memory_t freeMem = 0;
   memory_t totalMem = 0;
   memory_t swapTotalMem = 0;
   memory_t swapFreeMem = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);

   char buffer[128];
   while (fgets(buffer, sizeof(buffer), file)) {

      #define tryRead(label, variable)                                       \
         if (String_startsWith(buffer, label)) {                             \
            memory_t parsed_;                                                \
            if (sscanf(buffer + strlen(label), "%llu kB", &parsed_) == 1) {  \
               (variable) = parsed_;                                         \
            }                                                                \
            break;                                                           \
         } else (void) 0 /* Require a ";" after the macro use. */

      switch (buffer[0]) {
      case 'M':
         tryRead("MemFree:", freeMem);
         tryRead("MemTotal:", totalMem);
         break;
      case 'S':
         switch (buffer[1]) {
         case 'w':
            tryRead("SwapTotal:", swapTotalMem);
            tryRead("SwapFree:", swapFreeMem);
            break;
         }
         break;
      }

      #undef tryRead
   }

   fclose(file);

   host->totalMem = totalMem;
   host->usedMem = totalMem - freeMem;
   host->totalSwap = swapTotalMem;
   host->usedSwap = swapTotalMem - swapFreeMem;
}

static void CygwinMachine_scanCPUTime(CygwinMachine* this) {
   const Machine* super = &this->super;

   CygwinMachine_updateCPUcount(this);

   FILE* file = fopen(PROCSTATFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCSTATFILE);

   unsigned int lastAdjCpuId = 0;

   for (unsigned int i = 0; i <= super->existingCPUs; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait = 0, irq = 0, softIrq = 0, steal = 0, guest = 0, guestnice = 0;

      const char* ok = fgets(buffer, sizeof(buffer), file);
      if (!ok)
         break;

      // cpu fields are sorted first
      if (!String_startsWith(buffer, "cpu"))
         break;

      // As for Cygwin 3.4.9,
      // only the first 4 fields are set,
      // the rest will remain at zero.
      unsigned int adjCpuId;
      if (i == 0) {
         (void) sscanf(buffer, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         adjCpuId = 0;
      } else {
         unsigned int cpuid;
         (void) sscanf(buffer, "cpu%4u %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         adjCpuId = cpuid + 1;
      }

      if (adjCpuId > super->existingCPUs)
         break;

      for (unsigned int j = lastAdjCpuId + 1; j < adjCpuId; j++) {
         // Skipped an ID, but /proc/stat is ordered => got offline CPU
         memset(&(this->cpuData[j]), '\0', sizeof(CPUData));
      }
      lastAdjCpuId = adjCpuId;

      // Guest time is already accounted in usertime
      usertime -= guest;
      nicetime -= guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(this->cpuData[adjCpuId]);
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      cpuData->userPeriod = saturatingSub(usertime, cpuData->userTime);
      cpuData->nicePeriod = saturatingSub(nicetime, cpuData->niceTime);
      cpuData->systemPeriod = saturatingSub(systemtime, cpuData->systemTime);
      cpuData->systemAllPeriod = saturatingSub(systemalltime, cpuData->systemAllTime);
      cpuData->idleAllPeriod = saturatingSub(idlealltime, cpuData->idleAllTime);
      cpuData->idlePeriod = saturatingSub(idletime, cpuData->idleTime);
      cpuData->ioWaitPeriod = saturatingSub(ioWait, cpuData->ioWaitTime);
      cpuData->irqPeriod = saturatingSub(irq, cpuData->irqTime);
      cpuData->softIrqPeriod = saturatingSub(softIrq, cpuData->softIrqTime);
      cpuData->stealPeriod = saturatingSub(steal, cpuData->stealTime);
      cpuData->guestPeriod = saturatingSub(virtalltime, cpuData->guestTime);
      cpuData->totalPeriod = saturatingSub(totaltime, cpuData->totalTime);
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }

   this->period = (double)this->cpuData[0].totalPeriod / super->activeCPUs;

   fclose(file);
}

void Machine_scan(Machine* super) {
   CygwinMachine* this = (CygwinMachine*) super;

   CygwinMachine_updateCPUcount(this);
   CygwinMachine_scanMemoryInfo(this);
   CygwinMachine_scanCPUTime(this);
}

bool Machine_isCPUonline(const Machine* super, unsigned int id) {
   const CygwinMachine* this = (const CygwinMachine*) super;

   assert(id < super->existingCPUs);
   return this->cpuData[id + 1].online;
}

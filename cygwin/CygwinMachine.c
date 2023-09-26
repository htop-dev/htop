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
   // TODO
   (void) this;
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

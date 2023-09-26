/*
htop - CygwinMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
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
   // TODO
   Machine* super = &this->super;

   super->activeCPUs = 1;
   super->existingCPUs = 1;
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
   // TODO
   assert(id < super->existingCPUs);

   (void) super; (void) id;
   return true;
}

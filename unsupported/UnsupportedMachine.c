/*
htop - UnsupportedMachine.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "UnsupportedMachine.h"

#include <stdlib.h>
#include <string.h>

#include "Machine.h"


Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   UnsupportedMachine* this = xCalloc(1, sizeof(UnsupportedMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   super->existingCPUs = 1;
   super->activeCPUs = 1;

   return super;
}

void Machine_delete(Machine* super) {
   UnsupportedMachine* this = (UnsupportedMachine*) super;
   Machine_done(super);
   free(this);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);

   (void) host; (void) id;

   return true;
}

void Machine_scan(Machine* super) {
   super->existingCPUs = 1;
   super->activeCPUs = 1;

   super->totalMem = 0;
   super->usedMem = 0;
   super->buffersMem = 0;
   super->cachedMem = 0;
   super->sharedMem = 0;
   super->availableMem = 0;

   super->totalSwap = 0;
   super->usedSwap = 0;
   super->cachedSwap = 0;
}

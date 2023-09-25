/*
htop - CygwinMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "cygwin/CygwinMachine.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

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
   // TODO
   Machine* host = &this->super;

   host->totalMem = 0;
   host->usedMem = 0;
   host->totalSwap = 0;
   host->usedSwap = 0;
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

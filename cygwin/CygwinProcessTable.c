/*
htop - CygwinProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "cygwin/CygwinProcessTable.h"

#include "Object.h"
#include "ProcessTable.h"
#include "XUtils.h"
#include "cygwin/CygwinProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   CygwinProcessTable* this = xCalloc(1, sizeof(CygwinProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(CygwinProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* super) {
   CygwinProcessTable* this = (CygwinProcessTable*) super;
   ProcessTable_done(&this->super);
   free(this);
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   // TODO
   (void) super;
}

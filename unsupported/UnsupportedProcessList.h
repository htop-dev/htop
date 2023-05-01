#ifndef HEADER_UnsupportedProcessList
#define HEADER_UnsupportedProcessList
/*
htop - UnsupportedProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"


ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

Machine* Machine_new(UsersTable* usersTable, uid_t userId);

bool Machine_isCPUonline(const Machine* host, unsigned int id);

void Machine_delete(Machine* host);

#endif

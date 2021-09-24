#ifndef HEADER_UnsupportedProcessList
#define HEADER_UnsupportedProcessList
/*
htop - UnsupportedProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);

#endif

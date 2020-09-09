#ifndef HEADER_UnsupportedProcessList
#define HEADER_UnsupportedProcessList
/*
htop - UnsupportedProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super);

#endif

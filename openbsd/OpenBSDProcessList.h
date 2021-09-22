#ifndef HEADER_OpenBSDProcessList
#define HEADER_OpenBSDProcessList
/*
htop - OpenBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kvm.h>
#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"


typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int niceTime;
   unsigned long long int sysTime;
   unsigned long long int sysAllTime;
   unsigned long long int spinTime;
   unsigned long long int intrTime;
   unsigned long long int idleTime;

   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int nicePeriod;
   unsigned long long int sysPeriod;
   unsigned long long int sysAllPeriod;
   unsigned long long int spinPeriod;
   unsigned long long int intrPeriod;
   unsigned long long int idlePeriod;

   bool online;
} CPUData;

typedef struct OpenBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   CPUData* cpuData;
   int cpuSpeed;

} OpenBSDProcessList;


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);

#endif

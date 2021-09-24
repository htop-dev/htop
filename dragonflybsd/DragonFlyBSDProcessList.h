#ifndef HEADER_DragonFlyBSDProcessList
#define HEADER_DragonFlyBSDProcessList
/*
htop - DragonFlyBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/types.h>  // required for kvm.h
#include <kvm.h>
#include <osreldate.h>
#include <stdbool.h>
#include <sys/jail.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

#include "dragonflybsd/DragonFlyBSDProcess.h"


typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct DragonFlyBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;

   CPUData* cpus;

   unsigned long* cp_time_o;
   unsigned long* cp_time_n;

   unsigned long* cp_times_o;
   unsigned long* cp_times_n;

   Hashtable* jails;
} DragonFlyBSDProcessList;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);

#endif

#ifndef HEADER_FreeBSDProcessList
#define HEADER_FreeBSDProcessList
/*
htop - FreeBSDProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "zfs/ZfsArcStats.h"

#include <kvm.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>

#define JAIL_ERRMSGLEN	1024
extern char jail_errmsg[JAIL_ERRMSGLEN];

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct FreeBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;

   ZfsArcStats zfs;

   CPUData* cpus;

   unsigned long   *cp_time_o;
   unsigned long   *cp_time_n;

   unsigned long  *cp_times_o;
   unsigned long  *cp_times_n;

} FreeBSDProcessList;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

char* FreeBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd);

char* FreeBSDProcessList_readJailName(struct kinfo_proc* kproc);

void ProcessList_goThroughEntries(ProcessList* this);

#endif

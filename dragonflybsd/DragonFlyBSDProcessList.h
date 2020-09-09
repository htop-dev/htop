#ifndef HEADER_DragonFlyBSDProcessList
#define HEADER_DragonFlyBSDProcessList
/*
htop - DragonFlyBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <kvm.h>
#include <sys/param.h>
#include <osreldate.h>
#include <sys/kinfo.h>
#include <kinfo.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include "Hashtable.h"
#include "DragonFlyBSDProcess.h"

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

typedef struct DragonFlyBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;

   CPUData* cpus;

   unsigned long   *cp_time_o;
   unsigned long   *cp_time_n;

   unsigned long  *cp_times_o;
   unsigned long  *cp_times_n;

   Hashtable *jails;
} DragonFlyBSDProcessList;

#define _UNUSED_ __attribute__((unused))

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

char* DragonFlyBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd);

char* DragonFlyBSDProcessList_readJailName(DragonFlyBSDProcessList* dfpl, int jailid);

void ProcessList_goThroughEntries(ProcessList* this);

#endif

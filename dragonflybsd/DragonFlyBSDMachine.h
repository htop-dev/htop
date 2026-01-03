#ifndef HEADER_DragonFlyBSDMachine
#define HEADER_DragonFlyBSDMachine
/*
htop - DragonFlyBSDMachine.h
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
#include "Machine.h"
#include "ProcessTable.h"
#include "UsersTable.h"


typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct DragonFlyBSDMachine_ {
   Machine super;
   kvm_t* kd;

   Hashtable* jails;

   int pageSize;
   int pageSizeKb;
   int kernelFScale;

   memory_t wiredMem;
   memory_t buffersMem;
   memory_t activeMem;
   memory_t inactiveMem;
   memory_t cacheMem;

   CPUData* cpus;

   unsigned long* cp_time_o;
   unsigned long* cp_time_n;

   unsigned long* cp_times_o;
   unsigned long* cp_times_n;
} DragonFlyBSDMachine;

char* DragonFlyBSDMachine_readJailName(const DragonFlyBSDMachine* host, int jailid);

#endif

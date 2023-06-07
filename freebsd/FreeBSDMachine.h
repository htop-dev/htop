#ifndef HEADER_FreeBSDMachine
#define HEADER_FreeBSDMachine
/*
htop - FreeBSDMachine.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kvm.h>
#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Machine.h"
#include "UsersTable.h"
#include "zfs/ZfsArcStats.h"


typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double systemAllPercent;

   double frequency;
   double temperature;
} CPUData;

typedef struct FreeBSDMachine_ {
   Machine super;
   kvm_t* kd;

   int pageSize;
   int pageSizeKb;
   int kernelFScale;

   unsigned long long int memWire;
   unsigned long long int memActive;

   ZfsArcStats zfs;

   CPUData* cpus;

   unsigned long* cp_time_o;
   unsigned long* cp_time_n;

   unsigned long* cp_times_o;
   unsigned long* cp_times_n;

} FreeBSDMachine;

#endif

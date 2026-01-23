#ifndef HEADER_OpenBSDMachine
#define HEADER_OpenBSDMachine
/*
htop - OpenBSDMachine.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kvm.h>
#include <stdbool.h>
#include <stddef.h>

#include "Machine.h"


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

typedef struct OpenBSDMachine_ {
   Machine super;
   kvm_t* kd;

   memory_t totalMem;
   memory_t wiredMem;
   memory_t cacheMem;
   memory_t activeMem;
   memory_t pagingMem;
   memory_t inactiveMem;

   CPUData* cpuData;

   long fscale;
   int cpuSpeed;
   size_t pageSize;
   size_t pageSizeKB;

} OpenBSDMachine;

#endif

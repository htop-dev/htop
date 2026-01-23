#ifndef HEADER_NetBSDMachine
#define HEADER_NetBSDMachine
/*
htop - NetBSDMachine.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kvm.h>
#include <stdbool.h>
#include <stddef.h>

#include "Machine.h"
#include "ProcessTable.h"


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

   double frequency;
} CPUData;

typedef struct NetBSDMachine_ {
   Machine super;
   kvm_t* kd;

   long fscale;
   size_t pageSize;
   size_t pageSizeKB;

   memory_t totalMem;
   memory_t wiredMem;
   memory_t activeMem;
   memory_t pagedMem;
   memory_t inactiveMem;

   CPUData* cpuData;
} NetBSDMachine;

#endif

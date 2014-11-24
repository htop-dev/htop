/*
htop - UnsupportedProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include <stdlib.h>

/*{

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* this = calloc(1, sizeof(ProcessList));
   ProcessList_init(this, usersTable, pidWhiteList);

   // Update CPU count:
   this->cpuCount = 1;
   this->cpus = calloc(1, sizeof(CPUData));
   this->cpus[0].totalTime = 1;
   this->cpus[0].totalPeriod = 1;
   
   return this;
}

void ProcessList_scan(ProcessList* this) {
   (void) this;
   // stub!
}

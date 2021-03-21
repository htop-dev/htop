#ifndef HEADER_FreeBSDProcess
#define HEADER_FreeBSDProcess
/*
htop - FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Object.h"
#include "Process.h"
#include "Settings.h"


typedef struct FreeBSDProcess_ {
   Process super;
   bool  isKernelThread;
   int   jid;
   char* jname;
} FreeBSDProcess;

static inline bool Process_isKernelThread(const Process* this) {
   return ((const FreeBSDProcess*)this)->isKernelThread;
}

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern const ProcessClass FreeBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* FreeBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

#endif

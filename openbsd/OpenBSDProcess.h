#ifndef HEADER_OpenBSDProcess
#define HEADER_OpenBSDProcess
/*
htop - OpenBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Object.h"
#include "Process.h"
#include "Settings.h"


typedef struct OpenBSDProcess_ {
   Process super;

   /* 'Kernel virtual addr of u-area' to detect main threads */
   uint64_t addr;
} OpenBSDProcess;

static inline bool Process_isKernelThread(const Process* this) {
   return this->pgrp == 0;
}

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern const ProcessClass OpenBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* OpenBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

#endif

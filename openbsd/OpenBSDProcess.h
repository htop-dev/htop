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


typedef enum OpenBSDProcessFields_ {
   // Add platform-specific fields here, with ids >= 100
   LAST_PROCESSFIELD = 100,
} OpenBSDProcessField;

typedef struct OpenBSDProcess_ {
   Process super;
} OpenBSDProcess;

#define Process_isKernelThread(_process) (_process->pgrp == 0)

#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)

extern const ProcessClass OpenBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

Process* OpenBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

#endif

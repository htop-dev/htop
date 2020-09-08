#ifndef HEADER_OpenBSDProcess
#define HEADER_OpenBSDProcess
/*
htop - OpenBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

typedef enum OpenBSDProcessFields_ {
   // Add platform-specific fields here, with ids >= 100
   LAST_PROCESSFIELD = 100,
} OpenBSDProcessField;

typedef struct OpenBSDProcess_ {
   Process super;
} OpenBSDProcess;

#define Process_isKernelThread(_process) (_process->pgrp == 0)

#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)

extern ProcessClass OpenBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

OpenBSDProcess* OpenBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void OpenBSDProcess_writeField(Process* this, RichString* str, ProcessField field);

long OpenBSDProcess_compare(const void* v1, const void* v2);

bool Process_isThread(Process* this);

#endif

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


#define PROCESS_FLAG_FREEBSD_TTY   0x0100

extern const char* const nodevStr;

typedef enum FreeBSDProcessFields_ {
   // Add platform-specific fields here, with ids >= 100
   JID   = 100,
   JAIL  = 101,
   LAST_PROCESSFIELD = 102,
} FreeBSDProcessField;

typedef struct FreeBSDProcess_ {
   Process super;
   int   kernel;
   int   jid;
   char* jname;
   const char* ttyPath;
} FreeBSDProcess;

static inline bool Process_isKernelThread(const Process* this) {
   return ((const FreeBSDProcess*)this)->kernel == 1;
}

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern const ProcessClass FreeBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

Process* FreeBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

#endif

#ifndef HEADER_FreeBSDProcess
#define HEADER_FreeBSDProcess
/*
htop - FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

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
} FreeBSDProcess;

#define Process_isKernelThread(_process) (_process->kernel == 1)

#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)

extern ProcessClass FreeBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

FreeBSDProcess* FreeBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void FreeBSDProcess_writeField(Process* this, RichString* str, ProcessField field);

long FreeBSDProcess_compare(const void* v1, const void* v2);

bool Process_isThread(Process* this);

#endif

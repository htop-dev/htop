#ifndef HEADER_DragonFlyBSDProcess
#define HEADER_DragonFlyBSDProcess
/*
htop - dragonflybsd/DragonFlyBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

typedef enum DragonFlyBSDProcessFields {
   // Add platform-specific fields here, with ids >= 100
   JID   = 100,
   JAIL  = 101,
   LAST_PROCESSFIELD = 102,
} DragonFlyBSDProcessField;

typedef struct DragonFlyBSDProcess_ {
   Process super;
   int   kernel;
   int   jid;
   char* jname;
} DragonFlyBSDProcess;

#define Process_isKernelThread(_process) (_process->kernel == 1)

//#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)
#define Process_isUserlandThread(_process) (_process->nlwp > 1)

extern ProcessClass DragonFlyBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

DragonFlyBSDProcess* DragonFlyBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void DragonFlyBSDProcess_writeField(Process* this, RichString* str, ProcessField field);

long DragonFlyBSDProcess_compare(const void* v1, const void* v2);

bool Process_isThread(Process* this);

#endif

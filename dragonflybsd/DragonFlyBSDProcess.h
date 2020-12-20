#ifndef HEADER_DragonFlyBSDProcess
#define HEADER_DragonFlyBSDProcess
/*
htop - dragonflybsd/DragonFlyBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

typedef struct DragonFlyBSDProcess_ {
   Process super;
   int   kernel;
   int   jid;
   char* jname;
} DragonFlyBSDProcess;

#define Process_isKernelThread(_process) (_process->kernel == 1)

//#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)
#define Process_isUserlandThread(_process) (_process->nlwp > 1)

extern const ProcessClass DragonFlyBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* DragonFlyBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

bool Process_isThread(const Process* this);

#endif

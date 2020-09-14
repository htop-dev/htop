#ifndef HEADER_Process
#define HEADER_Process
/*
htop - Process.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifdef __ANDROID__
#define SYS_ioprio_get __NR_ioprio_get
#define SYS_ioprio_set __NR_ioprio_set
#endif

// On Linux, this works only with glibc 2.1+. On earlier versions
// the behavior is similar to have a hardcoded page size.
#ifndef PAGE_SIZE
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) )
#endif
#define PAGE_SIZE_KB ( PAGE_SIZE / ONE_K )

#include "Object.h"

#include <sys/types.h>

#define PROCESS_FLAG_IO 0x0001

typedef enum ProcessFields {
   NULL_PROCESSFIELD = 0,
   PID = 1,
   COMM = 2,
   STATE = 3,
   PPID = 4,
   PGRP = 5,
   SESSION = 6,
   TTY_NR = 7,
   TPGID = 8,
   MINFLT = 10,
   MAJFLT = 12,
   PRIORITY = 18,
   NICE = 19,
   STARTTIME = 21,
   PROCESSOR = 38,
   M_SIZE = 39,
   M_RESIDENT = 40,
   ST_UID = 46,
   PERCENT_CPU = 47,
   PERCENT_MEM = 48,
   USER = 49,
   TIME = 50,
   NLWP = 51,
   TGID = 52,
} ProcessField;

typedef struct ProcessPidColumn_ {
   int id;
   const char* label;
} ProcessPidColumn;

typedef struct Process_ {
   Object super;

   struct Settings_* settings;

   unsigned long long int time;
   pid_t pid;
   pid_t ppid;
   pid_t tgid;
   char* comm;
   int commLen;
   int indent;

   int basenameOffset;
   bool updated;

   char state;
   bool tag;
   bool showChildren;
   bool show;
   unsigned int pgrp;
   unsigned int session;
   unsigned int tty_nr;
   int tpgid;
   uid_t st_uid;
   unsigned long int flags;
   int processor;

   float percent_cpu;
   float percent_mem;
   char* user;

   long int priority;
   long int nice;
   long int nlwp;
   char starttime_show[8];
   time_t starttime_ctime;

   long m_size;
   long m_resident;

   int exit_signal;

   unsigned long int minflt;
   unsigned long int majflt;
   #ifdef DEBUG
   long int itrealvalue;
   unsigned long int vsize;
   long int rss;
   unsigned long int rlim;
   unsigned long int startcode;
   unsigned long int endcode;
   unsigned long int startstack;
   unsigned long int kstkesp;
   unsigned long int kstkeip;
   unsigned long int signal;
   unsigned long int blocked;
   unsigned long int sigignore;
   unsigned long int sigcatch;
   unsigned long int wchan;
   unsigned long int nswap;
   unsigned long int cnswap;
   #endif

} Process;

typedef struct ProcessFieldData_ {
   const char* name;
   const char* title;
   const char* description;
   int flags;
} ProcessFieldData;

// Implemented in platform-specific code:
void Process_writeField(Process* this, RichString* str, ProcessField field);
long Process_compare(const void* v1, const void* v2);
void Process_delete(Object* cast);
bool Process_isThread(Process* this);
extern ProcessFieldData Process_fields[];
extern ProcessPidColumn Process_pidColumns[];
extern char Process_pidFormat[20];

typedef Process*(*Process_New)(struct Settings_*);
typedef void (*Process_WriteField)(Process*, RichString*, ProcessField);

typedef struct ProcessClass_ {
   const ObjectClass super;
   const Process_WriteField writeField;
} ProcessClass;

#define As_Process(this_)              ((ProcessClass*)((this_)->super.klass))

#define Process_getParentPid(process_)    (process_->tgid == process_->pid ? process_->ppid : process_->tgid)

#define Process_isChildOf(process_, pid_) (process_->tgid == pid_ || (process_->tgid == process_->pid && process_->ppid == pid_))

#define Process_sortState(state) ((state) == 'I' ? 0x100 : (state))


#define ONE_K 1024L
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)
#define ONE_T ((long long)ONE_G * ONE_K)

#define ONE_DECIMAL_K 1000L
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)
#define ONE_DECIMAL_T ((long long)ONE_DECIMAL_G * ONE_DECIMAL_K)

extern char Process_pidFormat[20];

void Process_setupColumnWidths();

void Process_humanNumber(RichString* str, unsigned long number, bool coloring);

void Process_colorNumber(RichString* str, unsigned long long number, bool coloring);

void Process_printTime(RichString* str, unsigned long long totalHundredths);

void Process_outputRate(RichString* str, char* buffer, int n, double rate, int coloring);

void Process_writeField(Process* this, RichString* str, ProcessField field);

void Process_display(Object* cast, RichString* out);

void Process_done(Process* this);

extern ProcessClass Process_class;

void Process_init(Process* this, struct Settings_* settings);

void Process_toggleTag(Process* this);

bool Process_setPriority(Process* this, int priority);

bool Process_changePriorityBy(Process* this, Arg delta);

bool Process_sendSignal(Process* this, Arg sgn);

long Process_pidCompare(const void* v1, const void* v2);

long Process_compare(const void* v1, const void* v2);

#endif

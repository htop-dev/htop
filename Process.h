#ifndef HEADER_Process
#define HEADER_Process
/*
htop - Process.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "Object.h"
#include "ProcessField.h"
#include "RichString.h"


#define PROCESS_FLAG_IO 0x0001
#define DEFAULT_HIGHLIGHT_SECS 5

typedef enum ProcessField_ {
   NULL_PROCESSFIELD = 0,
   PID = 1,
   COMM = 2,
   STATE = 3,
   PPID = 4,
   PGRP = 5,
   SESSION = 6,
   TTY = 7,
   TPGID = 8,
   MINFLT = 10,
   MAJFLT = 12,
   PRIORITY = 18,
   NICE = 19,
   STARTTIME = 21,
   PROCESSOR = 38,
   M_VIRT = 39,
   M_RESIDENT = 40,
   ST_UID = 46,
   PERCENT_CPU = 47,
   PERCENT_MEM = 48,
   USER = 49,
   TIME = 50,
   NLWP = 51,
   TGID = 52,
   PERCENT_NORM_CPU = 53,

   /* Platform specific fields, defined in ${platform}/ProcessField.h */
   PLATFORM_PROCESS_FIELDS

   LAST_PROCESSFIELD
} ProcessField;

struct Settings_;

typedef struct Process_ {
   /* Super object for emulated OOP */
   Object super;

   /* Pointer to quasi-global data structures */
   const struct ProcessList_* processList;
   const struct Settings_* settings;

   /* Process runtime (in hundredth of a second) */
   unsigned long long int time;

   /* Process identifier */
   pid_t pid;

   /* Parent process identifier */
   pid_t ppid;

   /* Thread group identifier */
   pid_t tgid;

   /* Process group identifier */
   int pgrp;

   /* Session identifier */
   int session;

   /* Foreground group identifier of the controlling terminal */
   int tpgid;

   /* Controlling terminal identifier of the process */
   unsigned long int tty_nr;

   /* Controlling terminal name of the process */
   char* tty_name;

   /* User identifier */
   uid_t st_uid;

   /* User name */
   const char* user;

   /*
    * Process name including arguments.
    * Use Process_getCommand() for Command actually displayed.
    */
   char* comm;

   /* Offset in comm of the process basename */
   int basenameOffset;

   /* CPU number last executed on */
   int processor;

   /* CPU usage during last cycle (in percent) */
   float percent_cpu;

   /* Memory usage during last cycle (in percent) */
   float percent_mem;

   /* Scheduling priority */
   long int priority;

   /* Nice value */
   long int nice;

   /* Number of threads in this process */
   long int nlwp;

   /* Process start time (in seconds elapsed since the Epoch) */
   time_t starttime_ctime;

   /* Process start time (cached formatted string) */
   char starttime_show[8];

   /* Total program size (in kilobytes) */
   long m_virt;

   /* Resident set size (in kilobytes) */
   long m_resident;

   /* Number of minor faults the process has made which have not required loading a memory page from disk */
   unsigned long int minflt;

   /* Number of major faults the process has made which have required loading a memory page from disk */
   unsigned long int majflt;

   /*
    * Process state (platform dependent):
    *   D  -  Waiting
    *   I  -  Idle
    *   L  -  Acquiring lock
    *   R  -  Running
    *   S  -  Sleeping
    *   T  -  Stopped (on a signal)
    *   X  -  Dead
    *   Z  -  Zombie
    *   t  -  Tracing stop
    *   ?  -  Unknown
    */
   char state;

   /* Whether the process was updated during the current scan */
   bool updated;

   /* Whether the process was tagged by the user */
   bool tag;

   /* Whether to display this process */
   bool show;

   /* Whether this process was shown last cycle */
   bool wasShown;

   /* Whether to show children of this process in tree-mode */
   bool showChildren;

   /*
    * Internal time counts for showing new and exited processes.
    */
   uint64_t seenStampMs;
   uint64_t tombStampMs;

   /*
    * Internal state for tree-mode.
    */
   int indent;
   unsigned int tree_left;
   unsigned int tree_right;
   unsigned int tree_depth;
   unsigned int tree_index;
} Process;

typedef struct ProcessFieldData_ {
   /* Name (displayed in setup menu) */
   const char* name;

   /* Title (display in main screen); must have same width as the printed values */
   const char* title;

   /* Description (displayed in setup menu) */
   const char* description;

   /* Scan flag to enable scan-method otherwise not run */
   uint32_t flags;

   /* Whether the values are process identifies; adjusts the width of title and values if true */
   bool pidColumn;

   /* Whether the column should be sorted in descending order by default */
   bool defaultSortDesc;
} ProcessFieldData;

// Implemented in platform-specific code:
void Process_writeField(const Process* this, RichString* str, ProcessField field);
int Process_compare(const void* v1, const void* v2);
void Process_delete(Object* cast);
bool Process_isThread(const Process* this);
extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];
#define PROCESS_MAX_PID_DIGITS 19
extern int Process_pidDigits;

typedef Process*(*Process_New)(const struct Settings_*);
typedef void (*Process_WriteField)(const Process*, RichString*, ProcessField);
typedef int (*Process_CompareByKey)(const Process*, const Process*, ProcessField);
typedef const char* (*Process_GetCommandStr)(const Process*);

typedef struct ProcessClass_ {
   const ObjectClass super;
   const Process_WriteField writeField;
   const Process_CompareByKey compareByKey;
   const Process_GetCommandStr getCommandStr;
} ProcessClass;

#define As_Process(this_)                              ((const ProcessClass*)((this_)->super.klass))

#define Process_getCommand(this_)                      (As_Process(this_)->getCommandStr ? As_Process(this_)->getCommandStr((const Process*)(this_)) : ((const Process*)(this_))->comm)
#define Process_compareByKey(p1_, p2_, key_)           (As_Process(p1_)->compareByKey ? (As_Process(p1_)->compareByKey(p1_, p2_, key_)) : Process_compareByKey_Base(p1_, p2_, key_))

static inline pid_t Process_getParentPid(const Process* this) {
   return this->tgid == this->pid ? this->ppid : this->tgid;
}

static inline bool Process_isChildOf(const Process* this, pid_t pid) {
   return pid == Process_getParentPid(this);
}

#define ONE_K 1024UL
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)
#define ONE_T (1ULL * ONE_G * ONE_K)
#define ONE_P (1ULL * ONE_T * ONE_K)

#define ONE_DECIMAL_K 1000UL
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)
#define ONE_DECIMAL_T (1ULL * ONE_DECIMAL_G * ONE_DECIMAL_K)
#define ONE_DECIMAL_P (1ULL * ONE_DECIMAL_T * ONE_DECIMAL_K)

void Process_setupColumnWidths(void);

/* Takes number in bytes (base 1024). Prints 6 columns. */
void Process_printBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number in kilo bytes (base 1024). Prints 6 columns. */
void Process_printKBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number as count (base 1000). Prints 12 columns. */
void Process_printCount(RichString* str, unsigned long long number, bool coloring);

/* Takes time in hundredths of a seconds. Prints 9 columns. */
void Process_printTime(RichString* str, unsigned long long totalHundredths, bool coloring);

/* Takes rate in bare unit (base 1024) per second. Prints 12 columns. */
void Process_printRate(RichString* str, double rate, bool coloring);

void Process_fillStarttimeBuffer(Process* this);

void Process_printLeftAlignedField(RichString* str, int attr, const char* content, unsigned int width);

void Process_display(const Object* cast, RichString* out);

void Process_done(Process* this);

extern const ProcessClass Process_class;

void Process_init(Process* this, const struct Settings_* settings);

void Process_toggleTag(Process* this);

bool Process_isNew(const Process* this);

bool Process_isTomb(const Process* this);

bool Process_setPriority(Process* this, int priority);

bool Process_changePriorityBy(Process* this, Arg delta);

bool Process_sendSignal(Process* this, Arg sgn);

int Process_pidCompare(const void* v1, const void* v2);

int Process_compareByKey_Base(const Process* p1, const Process* p2, ProcessField key);

#endif

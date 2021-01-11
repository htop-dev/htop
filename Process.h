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
   TTY_NR = 7,
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
   Object super;

   const struct ProcessList_* processList;
   const struct Settings_* settings;

   unsigned long long int time;
   pid_t pid;
   pid_t ppid;
   pid_t tgid;
   char* comm;  /* use Process_getCommand() for Command actually displayed */
   int commLen;
   int indent;

   int basenameOffset;
   bool updated;

   char state;
   bool tag;
   bool showChildren;
   bool show;
   bool wasShown;
   unsigned int pgrp;
   unsigned int session;
   unsigned int tty_nr;
   int tpgid;
   uid_t st_uid;
   unsigned long int flags;
   int processor;

   float percent_cpu;
   float percent_mem;
   const char* user;

   long int priority;
   long int nice;
   long int nlwp;
   char starttime_show[8];
   time_t starttime_ctime;

   long m_virt;
   long m_resident;

   int exit_signal;

   time_t seenTs;
   time_t tombTs;

   unsigned long int minflt;
   unsigned long int majflt;

   unsigned int tree_left;
   unsigned int tree_right;
   unsigned int tree_depth;
   unsigned int tree_index;
} Process;

typedef struct ProcessFieldData_ {
   const char* name;
   const char* title;
   const char* description;
   uint32_t flags;
   bool pidColumn;
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

#define Process_sortState(state) ((state) == 'I' ? 0x100 : (state))


#define ONE_K 1024UL
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)
#define ONE_T (1ULL * ONE_G * ONE_K)

#define ONE_DECIMAL_K 1000UL
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)
#define ONE_DECIMAL_T (1ULL * ONE_DECIMAL_G * ONE_DECIMAL_K)

void Process_setupColumnWidths(void);

void Process_humanNumber(RichString* str, unsigned long long number, bool coloring);

void Process_colorNumber(RichString* str, unsigned long long number, bool coloring);

void Process_printTime(RichString* str, unsigned long long totalHundredths);

void Process_fillStarttimeBuffer(Process* this);

void Process_outputRate(RichString* str, char* buffer, size_t n, double rate, int coloring);

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

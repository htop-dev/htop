#ifndef HEADER_Process
#define HEADER_Process
/*
htop - Process.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "Object.h"
#include "RichString.h"
#include "Row.h"
#include "RowField.h"


#define PROCESS_FLAG_IO              0x00000001
#define PROCESS_FLAG_CWD             0x00000002
#define PROCESS_FLAG_SCHEDPOL        0x00000004

#define DEFAULT_HIGHLIGHT_SECS 5

/* Core process states (shared by platforms)
 * NOTE: The enum has an ordering that is important!
 * See processStateChar in process.c for ProcessSate -> letter mapping */
typedef enum ProcessState_ {
   UNKNOWN = 1,
   RUNNABLE,
   RUNNING,
   QUEUED,
   WAITING,
   UNINTERRUPTIBLE_WAIT,
   BLOCKED,
   PAGING,
   STOPPED,
   TRACED,
   ZOMBIE,
   DEFUNCT,
   IDLE,
   SLEEPING
} ProcessState;

struct Machine_;  // IWYU pragma: keep
struct Settings_; // IWYU pragma: keep

/* Holds information about regions of the cmdline that should be
 * highlighted (e.g. program basename, delimiter, comm). */
typedef struct ProcessCmdlineHighlight_ {
   size_t offset; /* first character to highlight */
   size_t length; /* How many characters to highlight, zero if unused */
   int attr;      /* The attributes used to highlight */
   int flags;     /* Special flags used for selective highlighting, zero for always */
} ProcessCmdlineHighlight;

/* ProcessMergedCommand is populated by Process_makeCommandStr: It
 * contains the merged Command string, and the information needed by
 * Process_writeCommand to color the string. str will be NULL for kernel
 * threads and zombies */
typedef struct ProcessMergedCommand_ {
   uint64_t lastUpdate;                        /* Marker based on settings->lastUpdate to track when the rendering needs refreshing */
   char* str;                                  /* merged Command string */
   size_t highlightCount;                      /* how many portions of cmdline to highlight */
   ProcessCmdlineHighlight highlights[8];      /* which portions of cmdline to highlight */
} ProcessMergedCommand;

typedef struct Process_ {
   /* Super object for emulated OOP */
   Row super;

   /* Process group identifier */
   int pgrp;

   /* Session identifier */
   int session;

   /* Foreground group identifier of the controlling terminal */
   int tpgid;

   /* This is a kernel (helper) task */
   bool isKernelThread;

   /* This is a userland thread / LWP */
   bool isUserlandThread;

   /* This process is running inside a container */
   bool isRunningInContainer;

   /* Controlling terminal identifier of the process */
   unsigned long int tty_nr;

   /* Controlling terminal name of the process */
   char* tty_name;

   /* User identifier */
   uid_t st_uid;

   /* User name */
   const char* user;

   /* Non root owned process with elevated privileges
    * Linux:
    *   - from file capabilities
    *   - inherited from the ambient set
    */
   bool elevated_priv;

   /* Process runtime (in hundredth of a second) */
   unsigned long long int time;

   /*
    * Process name including arguments.
    * Use Process_getCommand() for Command actually displayed.
    */
   char* cmdline;

   /* End Offset in cmdline of the process basename */
   int cmdlineBasenameEnd;

   /* Start Offset in cmdline of the process basename */
   int cmdlineBasenameStart;

   /* The process' "command" name */
   char* procComm;

   /* The main process executable */
   char* procExe;

   /* The process/thread working directory */
   char* procCwd;

   /* Offset in procExe of the process basename */
   int procExeBasenameOffset;

   /* Tells if the executable has been replaced in the filesystem since start */
   bool procExeDeleted;

   /* Tells if the process uses replaced shared libraries since start */
   bool usesDeletedLib;

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

   /* Process state enum field (platform dependent) */
   ProcessState state;

   /* Current scheduling policy */
   int scheduling_policy;

   /*
    * Internal state for merged Command display
    */
   ProcessMergedCommand mergedCommand;
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

   /* Whether the values are process identifiers; adjusts the width of title and values if true */
   bool pidColumn;

   /* Whether the column should be sorted in descending order by default */
   bool defaultSortDesc;

   /* Whether the column width is dynamically adjusted (the minimum width is determined by the title length) */
   bool autoWidth;
} ProcessFieldData;

#define LAST_PROCESSFIELD LAST_RESERVED_FIELD
typedef int32_t ProcessField;  /* see ReservedField list in RowField.h */

// Implemented in platform-specific code:
void Process_writeField(const Process* row, RichString* str, ProcessField field);
int Process_compare(const void* v1, const void* v2);
int Process_compareByParent(const Row* r1, const Row* v2);
void Process_delete(Object* cast);
extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];
#define Process_pidDigits Row_pidDigits
#define Process_uidDigits Row_uidDigits

typedef Process* (*Process_New)(const struct Machine_*);
typedef int (*Process_CompareByKey)(const Process*, const Process*, ProcessField);

typedef struct ProcessClass_ {
   const RowClass super;
   const Process_CompareByKey compareByKey;
} ProcessClass;

#define As_Process(this_)   ((const ProcessClass*)((this_)->super.super.klass))

#define Process_compareByKey(p1_, p2_, key_)   (As_Process(p1_)->compareByKey ? (As_Process(p1_)->compareByKey(p1_, p2_, key_)) : Process_compareByKey_Base(p1_, p2_, key_))


static inline void Process_setPid(Process* this, pid_t pid) {
   this->super.id = pid;
}

static inline pid_t Process_getPid(const Process* this) {
   return (pid_t)this->super.id;
}

static inline void Process_setThreadGroup(Process* this, pid_t pid) {
   this->super.group = pid;
}

static inline pid_t Process_getThreadGroup(const Process* this) {
   return (pid_t)this->super.group;
}

static inline void Process_setParent(Process* this, pid_t pid) {
   this->super.parent = pid;
}

static inline pid_t Process_getParent(const Process* this) {
   return (pid_t)this->super.parent;
}

static inline pid_t Process_getGroupOrParent(const Process* this) {
   return Row_getGroupOrParent(&this->super);
}

static inline bool Process_isKernelThread(const Process* this) {
   return this->isKernelThread;
}

static inline bool Process_isUserlandThread(const Process* this) {
   return this->isUserlandThread;
}

static inline bool Process_isThread(const Process* this) {
   return Process_isUserlandThread(this) || Process_isKernelThread(this);
}

#define CMDLINE_HIGHLIGHT_FLAG_SEPARATOR  0x00000001
#define CMDLINE_HIGHLIGHT_FLAG_BASENAME   0x00000002
#define CMDLINE_HIGHLIGHT_FLAG_COMM       0x00000004
#define CMDLINE_HIGHLIGHT_FLAG_DELETED    0x00000008
#define CMDLINE_HIGHLIGHT_FLAG_PREFIXDIR  0x00000010

void Process_fillStarttimeBuffer(Process* this);

void Process_done(Process* this);

extern const ProcessClass Process_class;

void Process_init(Process* this, const struct Machine_* host);

const char* Process_rowGetSortKey(Row* super);

bool Process_rowSetPriority(Row* super, int priority);

bool Process_rowChangePriorityBy(Row* super, Arg delta);

bool Process_rowSendSignal(Row* super, Arg sgn);

bool Process_rowIsHighlighted(const Row* super);

bool Process_rowIsVisible(const Row* super, const struct Table_* table);

bool Process_rowMatchesFilter(const Row* super, const struct Table_* table);

static inline int Process_pidEqualCompare(const void* v1, const void* v2) {
   return Row_idEqualCompare(v1, v2);
}

int Process_compareByKey_Base(const Process* p1, const Process* p2, ProcessField key);

const char* Process_getCommand(const Process* this);

void Process_updateComm(Process* this, const char* comm);
void Process_updateCmdline(Process* this, const char* cmdline, int basenameStart, int basenameEnd);
void Process_updateExe(Process* this, const char* exe);

/* This function constructs the string that is displayed by
 * Process_writeCommand and also returned by Process_getCommand */
void Process_makeCommandStr(Process* this, const struct Settings_ *settings);

void Process_writeCommand(const Process* this, int attr, int baseAttr, RichString* str);

void Process_updateCPUFieldWidths(float percentage);

#endif

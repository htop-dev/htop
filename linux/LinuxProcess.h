#ifndef HEADER_LinuxProcess
#define HEADER_LinuxProcess
/*
htop - LinuxProcess.h
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>

#include "IOPriority.h"
#include "Object.h"
#include "Process.h"
#include "Settings.h"


#define PROCESS_FLAG_LINUX_IOPRIO    0x00000100
#define PROCESS_FLAG_LINUX_OPENVZ    0x00000200
#define PROCESS_FLAG_LINUX_VSERVER   0x00000400
#define PROCESS_FLAG_LINUX_CGROUP    0x00000800
#define PROCESS_FLAG_LINUX_OOM       0x00001000
#define PROCESS_FLAG_LINUX_SMAPS     0x00002000
#define PROCESS_FLAG_LINUX_CTXT      0x00004000
#define PROCESS_FLAG_LINUX_SECATTR   0x00008000
#define PROCESS_FLAG_LINUX_LRS_FIX   0x00010000
#define PROCESS_FLAG_LINUX_CWD       0x00020000
#define PROCESS_FLAG_LINUX_DELAYACCT 0x00040000


/* LinuxProcessMergedCommand is populated by LinuxProcess_makeCommandStr: It
 * contains the merged Command string, and the information needed by
 * LinuxProcess_writeCommand to color the string. str will be NULL for kernel
 * threads and zombies */
typedef struct LinuxProcessMergedCommand_ {
   char *str;           /* merged Command string */
   int maxLen;          /* maximum expected length of Command string */
   int baseStart;       /* basename's start offset */
   int baseEnd;         /* basename's end offset */
   int commStart;       /* comm's start offset */
   int commEnd;         /* comm's end offset */
   int sep1;            /* first field separator, used if non-zero */
   int sep2;            /* second field separator, used if non-zero */
   bool separateComm;   /* whether comm is a separate field */
   bool unmatchedExe;   /* whether exe matched with cmdline */
   bool cmdlineChanged; /* whether cmdline changed */
   bool exeChanged;     /* whether exe changed */
   bool commChanged;    /* whether comm changed */
   bool prevMergeSet;   /* whether showMergedCommand was set */
   bool prevPathSet;    /* whether showProgramPath was set */
   bool prevCommSet;    /* whether findCommInCmdline was set */
   bool prevCmdlineSet; /* whether findCommInCmdline was set */
} LinuxProcessMergedCommand;

typedef struct LinuxProcess_ {
   Process super;
   char *procComm;
   char *procExe;
   int procExeLen;
   int procExeBasenameOffset;
   bool procExeDeleted;
   int procCmdlineBasenameOffset;
   int procCmdlineBasenameEnd;
   LinuxProcessMergedCommand mergedCommand;
   bool isKernelThread;
   IOPriority ioPriority;
   unsigned long int cminflt;
   unsigned long int cmajflt;
   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long m_share;
   long m_pss;
   long m_swap;
   long m_psswp;
   long m_trs;
   long m_drs;
   long m_lrs;
   long m_dt;

   /* Data read (in bytes) */
   unsigned long long io_rchar;

   /* Data written (in bytes) */
   unsigned long long io_wchar;

   /* Number of read(2) syscalls */
   unsigned long long io_syscr;

   /* Number of write(2) syscalls */
   unsigned long long io_syscw;

   /* Storage data read (in bytes) */
   unsigned long long io_read_bytes;

   /* Storage data written (in bytes) */
   unsigned long long io_write_bytes;

   /* Storage data cancelled (in bytes) */
   unsigned long long io_cancelled_write_bytes;

   /* Point in time of last io scan (in milliseconds elapsed since the Epoch) */
   unsigned long long io_last_scan_time_ms;

   /* Storage data read (in bytes per second) */
   double io_rate_read_bps;

   /* Storage data written (in bytes per second) */
   double io_rate_write_bps;

   #ifdef HAVE_OPENVZ
   char* ctid;
   pid_t vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif
   char* cgroup;
   unsigned int oom;
   #ifdef HAVE_DELAYACCT
   unsigned long long int delay_read_time;
   unsigned long long cpu_delay_total;
   unsigned long long blkio_delay_total;
   unsigned long long swapin_delay_total;
   float cpu_delay_percent;
   float blkio_delay_percent;
   float swapin_delay_percent;
   #endif
   unsigned long ctxt_total;
   unsigned long ctxt_diff;
   char* secattr;
   unsigned long long int last_mlrs_calctime;
   char* cwd;
} LinuxProcess;

#define Process_isKernelThread(_process) (((const LinuxProcess*)(_process))->isKernelThread)

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern int pageSize;

extern int pageSizeKB;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

extern const ProcessClass LinuxProcess_class;

Process* LinuxProcess_new(const Settings* settings);

void Process_delete(Object* cast);

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this);

bool LinuxProcess_setIOPriority(Process* this, Arg ioprio);

/* This function constructs the string that is displayed by
 * LinuxProcess_writeCommand and also returned by LinuxProcess_getCommandStr */
void LinuxProcess_makeCommandStr(Process *this);

bool Process_isThread(const Process* this);

#endif

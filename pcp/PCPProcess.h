#ifndef HEADER_PCPProcess
#define HEADER_PCPProcess
/*
htop - PCPProcess.h
(C) 2014 Hisham H. Muhammad
(C) 2020 htop dev team
(C) 2020-2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include <stdbool.h>
#include <sys/types.h>

#include "Object.h"
#include "Process.h"
#include "RichString.h"
#include "Settings.h"

#define PROCESS_FLAG_LINUX_CGROUP   0x0800
#define PROCESS_FLAG_LINUX_OOM      0x1000
#define PROCESS_FLAG_LINUX_SMAPS    0x2000
#define PROCESS_FLAG_LINUX_CTXT     0x4000
#define PROCESS_FLAG_LINUX_SECATTR  0x8000

/* PCPProcessMergedCommand is populated by PCPProcess_makeCommandStr: It
 * contains the merged Command string, and the information needed by
 * PCPProcess_writeCommand to color the string. str will be NULL for kernel
 * threads and zombies */
typedef struct PCPProcessMergedCommand_ {
   char *str;           /* merged Command string */
   int maxLen;          /* maximum expected length of Command string */
   int baseStart;       /* basename's start offset */
   int baseEnd;         /* basename's end offset */
   int commStart;       /* comm's start offset */
   int commEnd;         /* comm's end offset */
   int sep1;            /* first field separator, used if non-zero */
   int sep2;            /* second field separator, used if non-zero */
   bool separateComm;   /* whether comm is a separate field */
   bool cmdlineChanged; /* whether cmdline changed */
   bool commChanged;    /* whether comm changed */
   bool prevMergeSet;   /* whether showMergedCommand was set */
   bool prevPathSet;    /* whether showProgramPath was set */
   bool prevCommSet;    /* whether findCommInCmdline was set */
   bool prevCmdlineSet; /* whether findCommInCmdline was set */
} PCPProcessMergedCommand;

typedef struct PCPProcess_ {
   Process super;
   char *procComm;
   int procCmdlineBasenameOffset;
   int procCmdlineBasenameEnd;
   PCPProcessMergedCommand mergedCommand;
   bool isKernelThread;
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

   /* Data read (in kilobytes) */
   unsigned long long io_rchar;

   /* Data written (in kilobytes) */
   unsigned long long io_wchar;

   /* Number of read(2) syscalls */
   unsigned long long io_syscr;

   /* Number of write(2) syscalls */
   unsigned long long io_syscw;

   /* Storage data read (in kilobytes) */
   unsigned long long io_read_bytes;

   /* Storage data written (in kilobytes) */
   unsigned long long io_write_bytes;

   /* Storage data cancelled (in kilobytes) */
   unsigned long long io_cancelled_write_bytes;

   /* Point in time of last io scan (in seconds elapsed since the Epoch) */
   unsigned long long io_last_scan_time;

   double io_rate_read_bps;
   double io_rate_write_bps;
   char* cgroup;
   unsigned int oom;
   char* ttyDevice;
   unsigned long long int delay_read_time;
   unsigned long long cpu_delay_total;
   unsigned long long blkio_delay_total;
   unsigned long long swapin_delay_total;
   float cpu_delay_percent;
   float blkio_delay_percent;
   float swapin_delay_percent;
   unsigned long ctxt_total;
   unsigned long ctxt_diff;
   char* secattr;
   unsigned long long int last_mlrs_calctime;
} PCPProcess;

static inline void Process_setKernelThread(Process* this, bool truth) {
   ((PCPProcess*)this)->isKernelThread = truth;
}

static inline bool Process_isKernelThread(const Process* this) {
   return ((const PCPProcess*)this)->isKernelThread;
}

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

extern const ProcessClass PCPProcess_class;

Process* PCPProcess_new(const Settings* settings);

void Process_delete(Object* cast);

/* This function constructs the string that is displayed by
 * PCPProcess_writeCommand and also returned by PCPProcess_getCommandStr */
void PCPProcess_makeCommandStr(Process *this);

bool Process_isThread(const Process* this);

#endif

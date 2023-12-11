#ifndef HEADER_LinuxProcess
#define HEADER_LinuxProcess
/*
htop - LinuxProcess.h
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Machine.h"
#include "Object.h"
#include "Process.h"
#include "Row.h"

#include "linux/IOPriority.h"


#define PROCESS_FLAG_LINUX_IOPRIO    0x00000100
#define PROCESS_FLAG_LINUX_OPENVZ    0x00000200
#define PROCESS_FLAG_LINUX_VSERVER   0x00000400
#define PROCESS_FLAG_LINUX_CGROUP    0x00000800
#define PROCESS_FLAG_LINUX_OOM       0x00001000
#define PROCESS_FLAG_LINUX_SMAPS     0x00002000
#define PROCESS_FLAG_LINUX_CTXT      0x00004000
#define PROCESS_FLAG_LINUX_SECATTR   0x00008000
#define PROCESS_FLAG_LINUX_LRS_FIX   0x00010000
#define PROCESS_FLAG_LINUX_DELAYACCT 0x00040000
#define PROCESS_FLAG_LINUX_AUTOGROUP 0x00080000

typedef struct LinuxProcess_ {
   Process super;
   IOPriority ioPriority;
   unsigned long int cminflt;
   unsigned long int cmajflt;
   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long m_share;
   long m_priv;
   long m_pss;
   long m_swap;
   long m_psswp;
   long m_trs;
   long m_drs;
   long m_lrs;

   /* Process flags */
   unsigned long int flags;

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
   char* cgroup_short;
   char* container_short;
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

   /* Autogroup scheduling (CFS) information */
   long int autogroup_id;
   int autogroup_nice;
} LinuxProcess;

extern int pageSize;

extern int pageSizeKB;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

extern const ProcessClass LinuxProcess_class;

Process* LinuxProcess_new(const Machine* host);

void Process_delete(Object* cast);

IOPriority LinuxProcess_updateIOPriority(Process* proc);

bool LinuxProcess_rowSetIOPriority(Row* super, Arg ioprio);

bool LinuxProcess_isAutogroupEnabled(void);

bool LinuxProcess_rowChangeAutogroupPriorityBy(Row* super, Arg delta);

bool Process_isThread(const Process* this);

#endif

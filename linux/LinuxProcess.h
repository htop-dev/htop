#ifndef HEADER_LinuxProcess
#define HEADER_LinuxProcess
/*
htop - LinuxProcess.h
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include <stdbool.h>
#include <sys/types.h>

#include "IOPriority.h"
#include "Object.h"
#include "Process.h"
#include "RichString.h"
#include "Settings.h"

#define PROCESS_FLAG_LINUX_IOPRIO   0x0100
#define PROCESS_FLAG_LINUX_OPENVZ   0x0200
#define PROCESS_FLAG_LINUX_VSERVER  0x0400
#define PROCESS_FLAG_LINUX_CGROUP   0x0800
#define PROCESS_FLAG_LINUX_OOM      0x1000
#define PROCESS_FLAG_LINUX_SMAPS    0x2000
#define PROCESS_FLAG_LINUX_CTXT     0x4000
#define PROCESS_FLAG_LINUX_SECATTR  0x8000

typedef enum UnsupportedProcessFields {
   FLAGS = 9,
   ITREALVALUE = 20,
   VSIZE = 22,
   RSS = 23,
   RLIM = 24,
   STARTCODE = 25,
   ENDCODE = 26,
   STARTSTACK = 27,
   KSTKESP = 28,
   KSTKEIP = 29,
   SIGNAL = 30,
   BLOCKED = 31,
   SSIGIGNORE = 32,
   SIGCATCH = 33,
   WCHAN = 34,
   NSWAP = 35,
   CNSWAP = 36,
   EXIT_SIGNAL = 37,
} UnsupportedProcessField;

typedef enum LinuxProcessFields {
   CMINFLT = 11,
   CMAJFLT = 13,
   UTIME = 14,
   STIME = 15,
   CUTIME = 16,
   CSTIME = 17,
   M_SHARE = 41,
   M_TRS = 42,
   M_DRS = 43,
   M_LRS = 44,
   M_DT = 45,
   #ifdef HAVE_OPENVZ
   CTID = 100,
   VPID = 101,
   #endif
   #ifdef HAVE_VSERVER
   VXID = 102,
   #endif
   #ifdef HAVE_TASKSTATS
   RCHAR = 103,
   WCHAR = 104,
   SYSCR = 105,
   SYSCW = 106,
   RBYTES = 107,
   WBYTES = 108,
   CNCLWB = 109,
   IO_READ_RATE = 110,
   IO_WRITE_RATE = 111,
   IO_RATE = 112,
   #endif
   #ifdef HAVE_CGROUP
   CGROUP = 113,
   #endif
   OOM = 114,
   IO_PRIORITY = 115,
   #ifdef HAVE_DELAYACCT
   PERCENT_CPU_DELAY = 116,
   PERCENT_IO_DELAY = 117,
   PERCENT_SWAP_DELAY = 118,
   #endif
   M_PSS = 119,
   M_SWAP = 120,
   M_PSSWP = 121,
   CTXT = 122,
   SECATTR = 123,
   LAST_PROCESSFIELD = 124,
} LinuxProcessField;

typedef struct LinuxProcess_ {
   Process super;
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
   #ifdef HAVE_TASKSTATS
   unsigned long long io_rchar;
   unsigned long long io_wchar;
   unsigned long long io_syscr;
   unsigned long long io_syscw;
   unsigned long long io_read_bytes;
   unsigned long long io_write_bytes;
   unsigned long long io_cancelled_write_bytes;
   unsigned long long io_rate_read_time;
   unsigned long long io_rate_write_time;
   double io_rate_read_bps;
   double io_rate_write_bps;
   #endif
   #ifdef HAVE_OPENVZ
   char* ctid;
   pid_t vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif
   #ifdef HAVE_CGROUP
   char* cgroup;
   #endif
   unsigned int oom;
   char* ttyDevice;
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
} LinuxProcess;

#define Process_isKernelThread(_process) (((const LinuxProcess*)(_process))->isKernelThread)

static inline bool Process_isUserlandThread(const Process* this) {
   return this->pid != this->tgid;
}

extern long long btime;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

extern const ProcessClass LinuxProcess_class;

Process* LinuxProcess_new(const Settings* settings);

void Process_delete(Object* cast);

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this);

bool LinuxProcess_setIOPriority(Process* this, Arg ioprio);

#ifdef HAVE_DELAYACCT
void LinuxProcess_printDelay(float delay_percent, char* buffer, int n);
#endif

bool Process_isThread(const Process* this);

#endif

/*
htop - Process.c
(C) 2004-2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"

#include "Settings.h"
#include "CRT.h"
#include "String.h"
#include "RichString.h"
#include "Platform.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <pwd.h>
#include <time.h>
#include <assert.h>

// On Linux, this works only with glibc 2.1+. On earlier versions
// the behavior is similar to have a hardcoded page size.
#ifndef PAGE_SIZE
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) )
#endif
#define PAGE_SIZE_KB ( PAGE_SIZE / ONE_K )

/*{
#include "Object.h"

#include <sys/types.h>

#define PROCESS_FLAG_IO 1
#define PROCESS_FLAG_IOPRIO 2
#define PROCESS_FLAG_OPENVZ 4
#define PROCESS_FLAG_VSERVER 8
#define PROCESS_FLAG_CGROUP 16

#ifndef Process_isKernelThread
#define Process_isKernelThread(_process) (_process->pgrp == 0)
#endif

#ifndef Process_isUserlandThread
#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)
#endif

#ifndef Process_isThread
#define Process_isThread(_process) (Process_isUserlandThread(_process) || Process_isKernelThread(_process))
#endif

typedef enum ProcessField_ {
   PID = 1, COMM, STATE, PPID, PGRP, SESSION, TTY_NR, TPGID, FLAGS, MINFLT, CMINFLT, MAJFLT, CMAJFLT, UTIME,
   STIME, CUTIME, CSTIME, PRIORITY, NICE, ITREALVALUE, STARTTIME, VSIZE, RSS, RLIM, STARTCODE, ENDCODE,
   STARTSTACK, KSTKESP, KSTKEIP, SIGNAL, BLOCKED, SSIGIGNORE, SIGCATCH, WCHAN, NSWAP, CNSWAP, EXIT_SIGNAL,
   PROCESSOR, M_SIZE, M_RESIDENT, M_SHARE, M_TRS, M_DRS, M_LRS, M_DT, ST_UID, PERCENT_CPU, PERCENT_MEM,
   USER, TIME, NLWP, TGID,
   #ifdef HAVE_OPENVZ
   CTID, VPID,
   #endif
   #ifdef HAVE_VSERVER
   VXID,
   #endif
   #ifdef HAVE_TASKSTATS
   RCHAR, WCHAR, SYSCR, SYSCW, RBYTES, WBYTES, CNCLWB, IO_READ_RATE, IO_WRITE_RATE, IO_RATE,
   #endif
   #ifdef HAVE_CGROUP
   CGROUP,
   #endif
   #ifdef HAVE_OOM
   OOM,
   #endif
   IO_PRIORITY,
   LAST_PROCESSFIELD
} ProcessField;

typedef struct Process_ {
   Object super;

   struct Settings_* settings;

   pid_t pid;
   char* comm;
   int indent;
   char state;
   bool tag;
   bool showChildren;
   bool show;
   pid_t ppid;
   unsigned int pgrp;
   unsigned int session;
   unsigned int tty_nr;
   pid_t tgid;
   int tpgid;
   unsigned long int flags;

   uid_t st_uid;
   float percent_cpu;
   float percent_mem;
   char* user;

   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long int priority;
   long int nice;
   long int nlwp;
   char starttime_show[8];
   time_t starttime_ctime;

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

   int processor;
   long m_size;
   long m_resident;
   long m_share;
   long m_trs;
   long m_drs;
   long m_lrs;
   long m_dt;

   #ifdef HAVE_OPENVZ
   unsigned int ctid;
   unsigned int vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif

   #ifdef HAVE_CGROUP
   char* cgroup;
   #endif
   #ifdef HAVE_OOM
   unsigned int oom;
   #endif

   int exit_signal;
   int basenameOffset;
   bool updated;

   unsigned long int minflt;
   unsigned long int cminflt;
   unsigned long int majflt;
   unsigned long int cmajflt;
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

void Process_writeField(Process* this, RichString* str, ProcessField field);
long Process_compare(const void* v1, const void* v2);

}*/

ProcessFieldData Process_fields[] = {
   { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   { .name = "SESSION", .title = "   SESN ", .description = "Process's session ID", .flags = 0, },
   { .name = "TTY_NR", .title = "  TTY ", .description = "Controlling terminal", .flags = 0, },
   { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   { .name = "FLAGS", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
   { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, },
   { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, },
   { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, },
   { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, },
   { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   { .name = "ITREALVALUE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   { .name = "VSIZE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "RSS", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "RLIM", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTCODE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "ENDCODE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTSTACK", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "KSTKESP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "KSTKEIP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "BLOCKED", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGIGNORE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGCATCH", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "WCHAN", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "NSWAP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "CNSWAP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "EXIT_SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   { .name = "M_LRS", .title = " LIB ", .description = "The library size of the process", .flags = 0, },
   { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process", .flags = 0, },
   { .name = "ST_UID", .title = " UID ", .description = "User ID of the process owner", .flags = 0, },
   { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   { .name = "CTID", .title = "   CTID ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_OPENVZ, },
   { .name = "VPID", .title = " VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_VSERVER, },
#endif
#ifdef HAVE_TASKSTATS
   { .name = "RCHAR", .title = "    RD_CHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, },
   { .name = "WCHAR", .title = "    WR_CHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, },
   { .name = "SYSCR", .title = "    RD_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "SYSCW", .title = "    WR_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "RBYTES", .title = "  IO_RBYTES ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "WBYTES", .title = "  IO_WBYTES ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "CNCLWB", .title = "  IO_CANCEL ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, },
#endif
#ifdef HAVE_CGROUP
   { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_CGROUP, },
#endif
#ifdef HAVE_OOM
   { .name = "OOM", .title = "    OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = 0, },
#endif
   { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_IOPRIO, },
   { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

static int Process_getuid = -1;

static char* Process_pidFormat = "%7u ";
static char* Process_tpgidFormat = "%7u ";

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1) return;
   if (maxPid > 99999) {
      Process_fields[PID].title =     "    PID ";
      Process_fields[PPID].title =    "   PPID ";
      #ifdef HAVE_OPENVZ
      Process_fields[VPID].title =    "   VPID ";
      #endif
      Process_fields[TPGID].title =   "  TPGID ";
      Process_fields[TGID].title =    "   TGID ";
      Process_fields[PGRP].title =    "   PGRP ";
      Process_fields[SESSION].title = "   SESN ";
      #ifdef HAVE_OOM
      Process_fields[OOM].title =     "    OOM ";
      #endif
      Process_pidFormat = "%7u ";
      Process_tpgidFormat = "%7d ";
   } else {
      Process_fields[PID].title =     "  PID ";
      Process_fields[PPID].title =    " PPID ";
      #ifdef HAVE_OPENVZ
      Process_fields[VPID].title =    " VPID ";
      #endif
      Process_fields[TPGID].title =   "TPGID ";
      Process_fields[TGID].title =    " TGID ";
      Process_fields[PGRP].title =    " PGRP ";
      Process_fields[SESSION].title = " SESN ";
      #ifdef HAVE_OOM
      Process_fields[OOM].title =     "  OOM ";
      #endif
      Process_pidFormat = "%5u ";
      Process_tpgidFormat = "%5d ";
   }
}

#define ONE_K 1024L
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)

#define ONE_DECIMAL_K 1000L
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)

static void Process_humanNumber(RichString* str, unsigned long number, bool coloring) {
   char buffer[11];
   int len;
   
   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int processMegabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int processColor = CRT_colors[PROCESS];
   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      processMegabytesColor = CRT_colors[PROCESS];
   }
 
   if(number >= (10 * ONE_DECIMAL_M)) {
      #ifdef __LP64__
      if(number >= (100 * ONE_DECIMAL_G)) {
         len = snprintf(buffer, 10, "%4ldT ", number / ONE_G);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      } else if (number >= (1000 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, 10, "%4.1lfT ", (double)number / ONE_G);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      #endif
      if(number >= (100 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, 10, "%4ldG ", number / ONE_M);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      len = snprintf(buffer, 10, "%4.1lfG ", (double)number / ONE_M);
      RichString_appendn(str, largeNumberColor, buffer, len);
      return;
   } else if (number >= 100000) {
      len = snprintf(buffer, 10, "%4ldM ", number / ONE_K);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      return;
   } else if (number >= 1000) {
      len = snprintf(buffer, 10, "%2ld", number/1000);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      number %= 1000;
      len = snprintf(buffer, 10, "%03lu ", number);
      RichString_appendn(str, processColor, buffer, len);
      return;
   }
   len = snprintf(buffer, 10, "%5lu ", number);
   RichString_appendn(str, processColor, buffer, len);
}

static void Process_colorNumber(RichString* str, unsigned long long number, bool coloring) {
   char buffer[14];

   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int processMegabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int processColor = CRT_colors[PROCESS];
   int processShadowColor = CRT_colors[PROCESS_SHADOW];
   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      processMegabytesColor = CRT_colors[PROCESS];
      processShadowColor = CRT_colors[PROCESS];
   }

   if (number > 10000000000) {
      snprintf(buffer, 13, "%11lld ", number / 1000);
      RichString_appendn(str, largeNumberColor, buffer, 5);
      RichString_appendn(str, processMegabytesColor, buffer+5, 3);
      RichString_appendn(str, processColor, buffer+8, 4);
   } else {
      snprintf(buffer, 13, "%11llu ", number);
      RichString_appendn(str, largeNumberColor, buffer, 2);
      RichString_appendn(str, processMegabytesColor, buffer+2, 3);
      RichString_appendn(str, processColor, buffer+5, 3);
      RichString_appendn(str, processShadowColor, buffer+8, 4);
   }
}

static double jiffy = 0.0;

static void Process_printTime(RichString* str, unsigned long long t) {
   if(jiffy == 0.0) jiffy = sysconf(_SC_CLK_TCK);
   double jiffytime = 1.0 / jiffy;

   double realTime = t * jiffytime;
   unsigned long long iRealTime = (unsigned long long) realTime;

   unsigned long long hours = iRealTime / 3600;
   int minutes = (iRealTime / 60) % 60;
   int seconds = iRealTime % 60;
   int hundredths = (realTime - iRealTime) * 100;
   char buffer[11];
   if (hours >= 100) {
      snprintf(buffer, 10, "%7lluh ", hours);
      RichString_append(str, CRT_colors[LARGE_NUMBER], buffer);
   } else {
      if (hours) {
         snprintf(buffer, 10, "%2lluh", hours);
         RichString_append(str, CRT_colors[LARGE_NUMBER], buffer);
         snprintf(buffer, 10, "%02d:%02d ", minutes, seconds);
      } else {
         snprintf(buffer, 10, "%2d:%02d.%02d ", minutes, seconds, hundredths);
      }
      RichString_append(str, CRT_colors[DEFAULT_COLOR], buffer);
   }
}

static inline void Process_writeCommand(Process* this, int attr, int baseattr, RichString* str) {
   int start = RichString_size(str);
   RichString_append(str, attr, this->comm);
   if (this->settings->highlightBaseName) {
      int finish = RichString_size(str) - 1;
      if (this->basenameOffset != -1)
         finish = (start + this->basenameOffset) - 1;
      int colon = RichString_findChar(str, ':', start);
      if (colon != -1 && colon < finish) {
         finish = colon;
      } else {
         for (int i = finish - start; i >= 0; i--) {
            if (this->comm[i] == '/') {
               start += i+1;
               break;
            }
         }
      }
      RichString_setAttrn(str, baseattr, start, finish);
   }
}

static inline void Process_outputRate(RichString* str, char* buffer, int n, double rate, int coloring) {
   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int processMegabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int processColor = CRT_colors[PROCESS];
   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      processMegabytesColor = CRT_colors[PROCESS];
   }
   if (rate < ONE_K) {
      int len = snprintf(buffer, n, "%7.2f B/s ", rate);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_K * ONE_K) {
      int len = snprintf(buffer, n, "%7.2f K/s ", rate / ONE_K);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_K * ONE_K * ONE_K) {
      int len = snprintf(buffer, n, "%7.2f M/s ", rate / ONE_K / ONE_K);
      RichString_appendn(str, processMegabytesColor, buffer, len);
   } else {
      int len = snprintf(buffer, n, "%7.2f G/s ", rate / ONE_K / ONE_K / ONE_K);
      RichString_appendn(str, largeNumberColor, buffer, len);
   }
}

void Process_writeDefaultField(Process* this, RichString* str, ProcessField field) {
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int baseattr = CRT_colors[PROCESS_BASENAME];
   int n = sizeof(buffer) - 1;
   bool coloring = this->settings->highlightMegabytes;

   switch (field) {
   case PID: snprintf(buffer, n, Process_pidFormat, this->pid); break;
   case PPID: snprintf(buffer, n, Process_pidFormat, this->ppid); break;
   case PGRP: snprintf(buffer, n, Process_pidFormat, this->pgrp); break;
   case SESSION: snprintf(buffer, n, Process_pidFormat, this->session); break;
   case TTY_NR: snprintf(buffer, n, "%5u ", this->tty_nr); break;
   case TGID: snprintf(buffer, n, Process_pidFormat, this->tgid); break;
   case TPGID: snprintf(buffer, n, Process_tpgidFormat, this->tpgid); break;
   case MINFLT: Process_colorNumber(str, this->minflt, coloring); return;
   case CMINFLT: Process_colorNumber(str, this->cminflt, coloring); return;
   case MAJFLT: Process_colorNumber(str, this->majflt, coloring); return;
   case CMAJFLT: Process_colorNumber(str, this->cmajflt, coloring); return;
   case PROCESSOR: snprintf(buffer, n, "%3d ", Settings_cpuId(this->settings, this->processor)); break;
   case NLWP: snprintf(buffer, n, "%4ld ", this->nlwp); break;
   case COMM: {
      if (this->settings->highlightThreads && Process_isThread(this)) {
         attr = CRT_colors[PROCESS_THREAD];
         baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
      }
      if (!this->settings->treeView || this->indent == 0) {
         Process_writeCommand(this, attr, baseattr, str);
         return;
      } else {
         char* buf = buffer;
         int maxIndent = 0;
         bool lastItem = (this->indent < 0);
         int indent = (this->indent < 0 ? -this->indent : this->indent);

         for (int i = 0; i < 32; i++)
            if (indent & (1 << i))
               maxIndent = i+1;
          for (int i = 0; i < maxIndent - 1; i++) {
            int written;
            if (indent & (1 << i))
               written = snprintf(buf, n, "%s  ", CRT_treeStr[TREE_STR_VERT]);
            else
               written = snprintf(buf, n, "   ");
            buf += written;
            n -= written;
         }
         const char* draw = CRT_treeStr[lastItem ? (this->settings->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
         snprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
         RichString_append(str, CRT_colors[PROCESS_TREE], buffer);
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }
   }
   case STATE: {
      snprintf(buffer, n, "%c ", this->state);
      switch(this->state) {
          case 'R':
              attr = CRT_colors[PROCESS_R_STATE];
              break;
          case 'D':
              attr = CRT_colors[PROCESS_D_STATE];
              break;
      }
      break;
   }
   case PRIORITY: {
      if(this->priority == -100)
         snprintf(buffer, n, " RT ");
      else
         snprintf(buffer, n, "%3ld ", this->priority);
      break;
   }
   case NICE: {
      snprintf(buffer, n, "%3ld ", this->nice);
      attr = this->nice < 0 ? CRT_colors[PROCESS_HIGH_PRIORITY]
           : this->nice > 0 ? CRT_colors[PROCESS_LOW_PRIORITY]
           : attr;
      break;
   }
   case M_DRS: Process_humanNumber(str, this->m_drs * PAGE_SIZE_KB, coloring); return;
   case M_DT: Process_humanNumber(str, this->m_dt * PAGE_SIZE_KB, coloring); return;
   case M_LRS: Process_humanNumber(str, this->m_lrs * PAGE_SIZE_KB, coloring); return;
   case M_TRS: Process_humanNumber(str, this->m_trs * PAGE_SIZE_KB, coloring); return;
   case M_SIZE: Process_humanNumber(str, this->m_size * PAGE_SIZE_KB, coloring); return;
   case M_RESIDENT: Process_humanNumber(str, this->m_resident * PAGE_SIZE_KB, coloring); return;
   case M_SHARE: Process_humanNumber(str, this->m_share * PAGE_SIZE_KB, coloring); return;
   case ST_UID: snprintf(buffer, n, "%4d ", this->st_uid); break;
   case USER: {
      if (Process_getuid != (int) this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];
      if (this->user) {
         snprintf(buffer, n, "%-9s ", this->user);
      } else {
         snprintf(buffer, n, "%-9d ", this->st_uid);
      }
      if (buffer[9] != '\0') {
         buffer[9] = ' ';
         buffer[10] = '\0';
      }
      break;
   }
   case UTIME: Process_printTime(str, this->utime); return;
   case STIME: Process_printTime(str, this->stime); return;
   case CUTIME: Process_printTime(str, this->cutime); return;
   case CSTIME: Process_printTime(str, this->cstime); return;
   case TIME: Process_printTime(str, this->utime + this->stime); return;
   case PERCENT_CPU: {
      if (this->percent_cpu > 999.9) {
         snprintf(buffer, n, "%4d ", (unsigned int)this->percent_cpu); 
      } else if (this->percent_cpu > 99.9) {
         snprintf(buffer, n, "%3d. ", (unsigned int)this->percent_cpu); 
      } else {
         snprintf(buffer, n, "%4.1f ", this->percent_cpu);
      }
      break;
   }
   case PERCENT_MEM: {
      if (this->percent_mem > 99.9) {
         snprintf(buffer, n, "100. "); 
      } else {
         snprintf(buffer, n, "%4.1f ", this->percent_mem);
      }
      break;
   }
   case STARTTIME: snprintf(buffer, n, "%s", this->starttime_show); break;
   #ifdef HAVE_OPENVZ
   case CTID: snprintf(buffer, n, "%7u ", this->ctid); break;
   case VPID: snprintf(buffer, n, Process_pidFormat, this->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: snprintf(buffer, n, "%5u ", this->vxid); break;
   #endif
   #ifdef HAVE_TASKSTATS
   case RCHAR:  Process_colorNumber(str, this->io_rchar, coloring); return;
   case WCHAR:  Process_colorNumber(str, this->io_wchar, coloring); return;
   case SYSCR:  Process_colorNumber(str, this->io_syscr, coloring); return;
   case SYSCW:  Process_colorNumber(str, this->io_syscw, coloring); return;
   case RBYTES: Process_colorNumber(str, this->io_read_bytes, coloring); return;
   case WBYTES: Process_colorNumber(str, this->io_write_bytes, coloring); return;
   case CNCLWB: Process_colorNumber(str, this->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Process_outputRate(str, buffer, n, this->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_outputRate(str, buffer, n, this->io_rate_write_bps, coloring); return;
   case IO_RATE: Process_outputRate(str, buffer, n, this->io_rate_read_bps + this->io_rate_write_bps, coloring); return;
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP: snprintf(buffer, n, "%-10s ", this->cgroup); break;
   #endif
   #ifdef HAVE_OOM
   case OOM: snprintf(buffer, n, Process_pidFormat, this->oom); break;
   #endif
   default:
      snprintf(buffer, n, "- ");
   }
   RichString_append(str, attr, buffer);
}

static void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->settings->fields;
   RichString_prune(out);
   for (int i = 0; fields[i]; i++)
      Process_writeField(this, out, fields[i]);
   if (this->settings->shadowOtherUsers && (int)this->st_uid != Process_getuid)
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   assert(out->chlen > 0);
}

void Process_delete(Object* cast) {
   Process* this = (Process*) cast;
   assert (this != NULL);
   free(this->comm);
#ifdef HAVE_CGROUP
   free(this->cgroup);
#endif
   free(this);
}

ObjectClass Process_class = {
   .extends = Class(Object),
   .display = Process_display,
   .delete = Process_delete,
   .compare = Process_compare
};

Process* Process_new(struct Settings_* settings) {
   Process* this = calloc(1, sizeof(Process));
   Object_setClass(this, Class(Process));
   this->pid = 0;
   this->settings = settings;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->updated = false;
   this->utime = 0;
   this->stime = 0;
   this->comm = NULL;
   this->basenameOffset = -1;
   this->indent = 0;
#ifdef HAVE_CGROUP
   this->cgroup = NULL;
#endif
   if (Process_getuid == -1) Process_getuid = getuid();
   return this;
}

void Process_toggleTag(Process* this) {
   this->tag = this->tag == true ? false : true;
}

bool Process_setPriority(Process* this, int priority) {
   int old_prio = getpriority(PRIO_PROCESS, this->pid);
   int err = setpriority(PRIO_PROCESS, this->pid, priority);
   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, this->pid)) {
      this->nice = priority;
   }
   return (err == 0);
}

bool Process_changePriorityBy(Process* this, size_t delta) {
   return Process_setPriority(this, this->nice + delta);
}

void Process_sendSignal(Process* this, size_t sgn) {
   kill(this->pid, (int) sgn);
}

long Process_pidCompare(const void* v1, const void* v2) {
   Process* p1 = (Process*)v1;
   Process* p2 = (Process*)v2;
   return (p1->pid - p2->pid);
}

long Process_defaultCompare(const void* v1, const void* v2) {
   Process *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (Process*)v1;
      p2 = (Process*)v2;
   } else {
      p2 = (Process*)v1;
      p1 = (Process*)v2;
   }
   long long diff;
   switch (settings->sortKey) {
   case PID:
      return (p1->pid - p2->pid);
   case PPID:
      return (p1->ppid - p2->ppid);
   case USER:
      return strcmp(p1->user ? p1->user : "", p2->user ? p2->user : "");
   case PRIORITY:
      return (p1->priority - p2->priority);
   case PROCESSOR:
      return (p1->processor - p2->processor);
   case SESSION:
      return (p1->session - p2->session);
   case STATE:
      return (p1->state - p2->state);
   case NICE:
      return (p1->nice - p2->nice);
   case M_DRS:
      return (p2->m_drs - p1->m_drs);
   case M_DT:
      return (p2->m_dt - p1->m_dt);
   case M_LRS:
      return (p2->m_lrs - p1->m_lrs);
   case M_TRS:
      return (p2->m_trs - p1->m_trs);
   case M_SIZE:
      return (p2->m_size - p1->m_size);
   case M_RESIDENT:
      return (p2->m_resident - p1->m_resident);
   case M_SHARE:
      return (p2->m_share - p1->m_share);
   case PERCENT_CPU:
      return (p2->percent_cpu > p1->percent_cpu ? 1 : -1);
   case PERCENT_MEM:
      return (p2->m_resident - p1->m_resident);
   case UTIME:  diff = p2->utime - p1->utime; goto test_diff;
   case CUTIME: diff = p2->cutime - p1->cutime; goto test_diff;
   case STIME:  diff = p2->stime - p1->stime; goto test_diff;
   case CSTIME: diff = p2->cstime - p2->cstime; goto test_diff;
   case TIME:
      return ((p2->utime+p2->stime) - (p1->utime+p1->stime));
   case COMM:
      return strcmp(p1->comm, p2->comm);
   case NLWP:
      return (p1->nlwp - p2->nlwp);
   case STARTTIME: {
      if (p1->starttime_ctime == p2->starttime_ctime)
         return (p1->pid - p2->pid);
      else
         return (p1->starttime_ctime - p2->starttime_ctime);
   }
   #ifdef HAVE_OPENVZ
   case CTID:
      return (p1->ctid - p2->ctid);
   case VPID:
      return (p1->vpid - p2->vpid);
   #endif
   #ifdef HAVE_VSERVER
   case VXID:
      return (p1->vxid - p2->vxid);
   #endif
   #ifdef HAVE_TASKSTATS
   case RCHAR:  diff = p2->io_rchar - p1->io_rchar; goto test_diff;
   case WCHAR:  diff = p2->io_wchar - p1->io_wchar; goto test_diff;
   case SYSCR:  diff = p2->io_syscr - p1->io_syscr; goto test_diff;
   case SYSCW:  diff = p2->io_syscw - p1->io_syscw; goto test_diff;
   case RBYTES: diff = p2->io_read_bytes - p1->io_read_bytes; goto test_diff;
   case WBYTES: diff = p2->io_write_bytes - p1->io_write_bytes; goto test_diff;
   case CNCLWB: diff = p2->io_cancelled_write_bytes - p1->io_cancelled_write_bytes; goto test_diff;
   case IO_READ_RATE:  diff = p2->io_rate_read_bps - p1->io_rate_read_bps; goto test_diff;
   case IO_WRITE_RATE: diff = p2->io_rate_write_bps - p1->io_rate_write_bps; goto test_diff;
   case IO_RATE: diff = (p2->io_rate_read_bps + p2->io_rate_write_bps) - (p1->io_rate_read_bps + p1->io_rate_write_bps); goto test_diff;
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP:
      return strcmp(p1->cgroup ? p1->cgroup : "", p2->cgroup ? p2->cgroup : "");
   #endif
   #ifdef HAVE_OOM
   case OOM:
      return (p1->oom - p2->oom);
   #endif
   default:
      return (p1->pid - p2->pid);
   }
   test_diff:
   return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

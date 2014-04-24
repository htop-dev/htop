/*
htop - Process.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"

#include "ProcessList.h"
#include "CRT.h"
#include "String.h"
#include "RichString.h"

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
#include <sched.h>
#include <time.h>
#include <assert.h>
#include <sys/syscall.h>

#ifdef HAVE_LIBHWLOC
#include <hwloc/linux.h>
#endif

// This works only with glibc 2.1+. On earlier versions
// the behavior is similar to have a hardcoded page size.
#ifndef PAGE_SIZE
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) )
#endif
#define PAGE_SIZE_KB ( PAGE_SIZE / ONE_K )

/*{
#include "Object.h"
#include "Affinity.h"
#include "IOPriority.h"
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

struct ProcessList_;

typedef struct Process_ {
   Object super;

   struct ProcessList_ *pl;

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
   IOPriority ioPriority;
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
   double io_rate_read_bps;
   unsigned long long io_rate_read_time;
   double io_rate_write_bps;
   unsigned long long io_rate_write_time;   
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

}*/

const char *Process_fieldNames[] = {
   "", "PID", "Command", "STATE", "PPID", "PGRP", "SESSION",
   "TTY_NR", "TPGID", "FLAGS", "MINFLT", "CMINFLT", "MAJFLT", "CMAJFLT",
   "UTIME", "STIME", "CUTIME", "CSTIME", "PRIORITY", "NICE", "ITREALVALUE",
   "STARTTIME", "VSIZE", "RSS", "RLIM", "STARTCODE", "ENDCODE", "STARTSTACK",
   "KSTKESP", "KSTKEIP", "SIGNAL", "BLOCKED", "SIGIGNORE", "SIGCATCH", "WCHAN",
   "NSWAP", "CNSWAP", "EXIT_SIGNAL", "PROCESSOR", "M_SIZE", "M_RESIDENT", "M_SHARE",
   "M_TRS", "M_DRS", "M_LRS", "M_DT", "ST_UID", "PERCENT_CPU", "PERCENT_MEM",
   "USER", "TIME", "NLWP", "TGID", 
#ifdef HAVE_OPENVZ
   "CTID", "VPID",
#endif
#ifdef HAVE_VSERVER
   "VXID",
#endif
#ifdef HAVE_TASKSTATS
   "RCHAR", "WCHAR", "SYSCR", "SYSCW", "RBYTES", "WBYTES", "CNCLWB",
   "IO_READ_RATE", "IO_WRITE_RATE", "IO_RATE",
#endif
#ifdef HAVE_CGROUP
   "CGROUP",
#endif
#ifdef HAVE_OOM
   "OOM",
#endif
   "IO_PRIORITY",
"*** report bug! ***"
};

const int Process_fieldFlags[] = {
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0,
#ifdef HAVE_OPENVZ
   PROCESS_FLAG_OPENVZ, PROCESS_FLAG_OPENVZ,
#endif
#ifdef HAVE_VSERVER
   PROCESS_FLAG_VSERVER,
#endif
#ifdef HAVE_TASKSTATS
   PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, 
   PROCESS_FLAG_IO, PROCESS_FLAG_IO, PROCESS_FLAG_IO, 
#endif
#ifdef HAVE_CGROUP
   PROCESS_FLAG_CGROUP,
#endif
#ifdef HAVE_OOM
   0,
#endif
   PROCESS_FLAG_IOPRIO
};

const char *Process_fieldTitles[] = {
   "", "    PID ", "Command ", "S ", "   PPID ", "   PGRP ", "   SESN ",
   "  TTY ", "  TPGID ", "- ", "     MINFLT ", "    CMINFLT ", "     MAJFLT ", "    CMAJFLT ",
   " UTIME+  ", " STIME+  ", " CUTIME+ ", " CSTIME+ ", "PRI ", " NI ", "- ",
   "START ", "- ", "- ", "- ", "- ", "- ", "- ",
   "- ", "- ", "- ", "- ", "- ", "- ", "- ",
   "- ", "- ", "- ", "CPU ", " VIRT ", "  RES ", "  SHR ",
   " CODE ", " DATA ", " LIB ", " DIRTY ", " UID ", "CPU% ", "MEM% ",
   "USER      ", "  TIME+  ", "NLWP ", "   TGID ",
#ifdef HAVE_OPENVZ
   " CTID ", " VPID ",
#endif
#ifdef HAVE_VSERVER
   " VXID ",
#endif
#ifdef HAVE_TASKSTATS
   "    RD_CHAR ", "    WR_CHAR ", "    RD_SYSC ", "    WR_SYSC ", "  IO_RBYTES ", "  IO_WBYTES ", "  IO_CANCEL ",
   " IORR ", " IOWR ", " IORW ",
#endif
#ifdef HAVE_CGROUP
   "    CGROUP ",
#endif
#ifdef HAVE_OOM
   "    OOM ",
#endif
   "IO ",
"*** report bug! ***"
};

static int Process_getuid = -1;

static char* Process_pidFormat = "%7u ";
static char* Process_tpgidFormat = "%7u ";

void Process_getMaxPid() {
   FILE* file = fopen(PROCDIR "/sys/kernel/pid_max", "r");
   if (!file) return;
   int maxPid = 4194303;
   fscanf(file, "%32d", &maxPid);
   fclose(file);
   if (maxPid > 99999) {
      Process_fieldTitles[PID] =     "    PID ";
      Process_fieldTitles[PPID] =    "   PPID ";
      Process_fieldTitles[TPGID] =   "  TPGID ";
      Process_fieldTitles[TGID] =    "   TGID ";
      Process_fieldTitles[PGRP] =    "   PGRP ";
      Process_fieldTitles[SESSION] = "   SESN ";
      #ifdef HAVE_OOM
      Process_fieldTitles[OOM] =     "    OOM ";
      #endif
      Process_pidFormat = "%7u ";
      Process_tpgidFormat = "%7d ";
   } else {
      Process_fieldTitles[PID] =     "  PID ";
      Process_fieldTitles[PPID] =    " PPID ";
      Process_fieldTitles[TPGID] =   "TPGID ";
      Process_fieldTitles[TGID] =    " TGID ";
      Process_fieldTitles[PGRP] =    " PGRP ";
      Process_fieldTitles[SESSION] = " SESN ";
      #ifdef HAVE_OOM
      Process_fieldTitles[OOM] =     "  OOM ";
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
   if (this->pl->highlightBaseName) {
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

static inline void Process_outputRate(RichString* str, int attr, char* buffer, int n, double rate, int coloring) {
   rate = rate / 1024;
   if (rate < 0.01)
      snprintf(buffer, n, "    0 ");
   else if (rate <= 10)
      snprintf(buffer, n, "%5.2f ", rate);
   else if (rate <= 100)
      snprintf(buffer, n, "%5.1f ", rate);
   else {
      Process_humanNumber(str, rate, coloring);
      return;
   }
   RichString_append(str, attr, buffer);
}

static void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int baseattr = CRT_colors[PROCESS_BASENAME];
   int n = sizeof(buffer) - 1;
   bool coloring = this->pl->highlightMegabytes;

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
   case PROCESSOR: snprintf(buffer, n, "%3d ", ProcessList_cpuId(this->pl, this->processor)); break;
   case NLWP: snprintf(buffer, n, "%4ld ", this->nlwp); break;
   case COMM: {
      if (this->pl->highlightThreads && Process_isThread(this)) {
         attr = CRT_colors[PROCESS_THREAD];
         baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
      }
      if (!this->pl->treeView || this->indent == 0) {
         Process_writeCommand(this, attr, baseattr, str);
         return;
      } else {
         char* buf = buffer;
         int maxIndent = 0;
         const char **treeStr = this->pl->treeStr;
         bool lastItem = (this->indent < 0);
         int indent = (this->indent < 0 ? -this->indent : this->indent);
         if (treeStr == NULL)
             treeStr = ProcessList_treeStrAscii;

         for (int i = 0; i < 32; i++)
            if (indent & (1 << i))
               maxIndent = i+1;
          for (int i = 0; i < maxIndent - 1; i++) {
            int written;
            if (indent & (1 << i))
               written = snprintf(buf, n, "%s  ", treeStr[TREE_STR_VERT]);
            else
               written = snprintf(buf, n, "   ");
            buf += written;
            n -= written;
         }
         const char* draw = treeStr[lastItem ? (this->pl->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
         snprintf(buf, n, "%s%s ", draw, this->showChildren ? treeStr[TREE_STR_SHUT] : treeStr[TREE_STR_OPEN] );
         RichString_append(str, CRT_colors[PROCESS_TREE], buffer);
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }
   }
   case STATE: {
      snprintf(buffer, n, "%c ", this->state);
      attr = this->state == 'R'
           ? CRT_colors[PROCESS_R_STATE]
           : attr;
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
   case CTID: snprintf(buffer, n, "%5u ", this->ctid); break;
   case VPID: snprintf(buffer, n, "%5u ", this->vpid); break;
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
   case IO_READ_RATE:  Process_outputRate(str, attr, buffer, n, this->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_outputRate(str, attr, buffer, n, this->io_rate_write_bps, coloring); return;
   case IO_RATE: Process_outputRate(str, attr, buffer, n, this->io_rate_read_bps + this->io_rate_write_bps, coloring); return;
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP: snprintf(buffer, n, "%-10s ", this->cgroup); break;
   #endif
   #ifdef HAVE_OOM
   case OOM: snprintf(buffer, n, Process_pidFormat, this->oom); break;
   #endif
   case IO_PRIORITY: {
      int klass = IOPriority_class(this->ioPriority);
      if (klass == IOPRIO_CLASS_NONE) {
         // see note [1] above
         snprintf(buffer, n, "B%1d ", (int) (this->nice + 20) / 5);
      } else if (klass == IOPRIO_CLASS_BE) {
         snprintf(buffer, n, "B%1d ", IOPriority_data(this->ioPriority));
      } else if (klass == IOPRIO_CLASS_RT) {
         attr = CRT_colors[PROCESS_HIGH_PRIORITY];
         snprintf(buffer, n, "R%1d ", IOPriority_data(this->ioPriority));
      } else if (this->ioPriority == IOPriority_Idle) {
         attr = CRT_colors[PROCESS_LOW_PRIORITY]; 
         snprintf(buffer, n, "id ");
      } else {
         snprintf(buffer, n, "?? ");
      }
      break;
   }
   default:
      snprintf(buffer, n, "- ");
   }
   RichString_append(str, attr, buffer);
}

static void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->pl->fields;
   RichString_prune(out);
   for (int i = 0; fields[i]; i++)
      Process_writeField(this, out, fields[i]);
   if (this->pl->shadowOtherUsers && (int)this->st_uid != Process_getuid)
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   assert(out->chlen > 0);
}

void Process_delete(Object* cast) {
   Process* this = (Process*) cast;
   assert (this != NULL);
   if (this->comm) free(this->comm);
#ifdef HAVE_CGROUP
   if (this->cgroup) free(this->cgroup);
#endif
   free(this);
}

ObjectClass Process_class = {
   .extends = Class(Object),
   .display = Process_display,
   .delete = Process_delete,
   .compare = Process_compare
};

Process* Process_new(struct ProcessList_ *pl) {
   Process* this = calloc(1, sizeof(Process));
   Object_setClass(this, Class(Process));
   this->pid = 0;
   this->pl = pl;
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

IOPriority Process_updateIOPriority(Process* this) {
   IOPriority ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->pid);
   this->ioPriority = ioprio;
   return ioprio;
}

bool Process_setIOPriority(Process* this, IOPriority ioprio) {
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->pid, ioprio);
   return (Process_updateIOPriority(this) == ioprio);
}

/*
[1] Note that before kernel 2.6.26 a process that has not asked for
an io priority formally uses "none" as scheduling class, but the
io scheduler will treat such processes as if it were in the best
effort class. The priority within the best effort class will  be
dynamically  derived  from  the  cpu  nice level of the process:
io_priority = (cpu_nice + 20) / 5. -- From ionice(1) man page
*/
#define Process_effectiveIOPriority(p_) (IOPriority_class(p_->ioPriority) == IOPRIO_CLASS_NONE ? IOPriority_tuple(IOPRIO_CLASS_BE, (p_->nice + 20) / 5) : p_->ioPriority)

#ifdef HAVE_LIBHWLOC

Affinity* Process_getAffinity(Process* this) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   bool ok = (hwloc_linux_get_tid_cpubind(this->pl->topology, this->pid, cpuset) == 0);
   Affinity* affinity = NULL;
   if (ok) {
      affinity = Affinity_new();
      if (hwloc_bitmap_last(cpuset) == -1) {
         for (int i = 0; i < this->pl->cpuCount; i++) {
            Affinity_add(affinity, i);
         }
      } else {
         unsigned int id;
         hwloc_bitmap_foreach_begin(id, cpuset);
            Affinity_add(affinity, id);
         hwloc_bitmap_foreach_end();
      }
   }
   hwloc_bitmap_free(cpuset);
   return affinity;
}

bool Process_setAffinity(Process* this, Affinity* affinity) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   for (int i = 0; i < affinity->used; i++) {
      hwloc_bitmap_set(cpuset, affinity->cpus[i]);
   }
   bool ok = (hwloc_linux_set_tid_cpubind(this->pl->topology, this->pid, cpuset) == 0);
   hwloc_bitmap_free(cpuset);
   return ok;
}

#elif HAVE_NATIVE_AFFINITY

Affinity* Process_getAffinity(Process* this) {
   cpu_set_t cpuset;
   bool ok = (sched_getaffinity(this->pid, sizeof(cpu_set_t), &cpuset) == 0);
   if (!ok) return NULL;
   Affinity* affinity = Affinity_new();
   for (int i = 0; i < this->pl->cpuCount; i++) {
      if (CPU_ISSET(i, &cpuset))
         Affinity_add(affinity, i);
   }
   return affinity;
}

bool Process_setAffinity(Process* this, Affinity* affinity) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   for (int i = 0; i < affinity->used; i++) {
      CPU_SET(affinity->cpus[i], &cpuset);
   }
   bool ok = (sched_setaffinity(this->pid, sizeof(unsigned long), &cpuset) == 0);
   return ok;
}

#endif

void Process_sendSignal(Process* this, size_t sgn) {
   kill(this->pid, (int) sgn);
}

int Process_pidCompare(const void* v1, const void* v2) {
   Process* p1 = (Process*)v1;
   Process* p2 = (Process*)v2;
   return (p1->pid - p2->pid);
}

int Process_compare(const void* v1, const void* v2) {
   Process *p1, *p2;
   ProcessList *pl = ((Process*)v1)->pl;
   if (pl->direction == 1) {
      p1 = (Process*)v1;
      p2 = (Process*)v2;
   } else {
      p2 = (Process*)v1;
      p1 = (Process*)v2;
   }
   long long diff;
   switch (pl->sortKey) {
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
   case UTIME:
      return (p2->utime - p1->utime);
   case STIME:
      return (p2->stime - p1->stime);
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
   case IO_PRIORITY:
      return Process_effectiveIOPriority(p1) - Process_effectiveIOPriority(p2);
   default:
      return (p1->pid - p2->pid);
   }
   test_diff:
   return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

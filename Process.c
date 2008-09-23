/*
htop - Process.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define _GNU_SOURCE
#include "ProcessList.h"
#include "Object.h"
#include "CRT.h"
#include "String.h"
#include "Process.h"
#include "RichString.h"

#include "debug.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <pwd.h>
#include <sched.h>

#include <plpa.h>

// This works only with glibc 2.1+. On earlier versions
// the behavior is similar to have a hardcoded page size.
#ifndef PAGE_SIZE
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) / 1024 )
#endif

#define PROCESS_COMM_LEN 300

/*{

typedef enum ProcessField_ {
   PID = 1, COMM, STATE, PPID, PGRP, SESSION, TTY_NR, TPGID, FLAGS, MINFLT, CMINFLT, MAJFLT, CMAJFLT, UTIME,
   STIME, CUTIME, CSTIME, PRIORITY, NICE, ITREALVALUE, STARTTIME, VSIZE, RSS, RLIM, STARTCODE, ENDCODE,
   STARTSTACK, KSTKESP, KSTKEIP, SIGNAL, BLOCKED, SSIGIGNORE, SIGCATCH, WCHAN, NSWAP, CNSWAP, EXIT_SIGNAL,
   PROCESSOR, M_SIZE, M_RESIDENT, M_SHARE, M_TRS, M_DRS, M_LRS, M_DT, ST_UID, PERCENT_CPU, PERCENT_MEM,
   USER, TIME, NLWP, TGID,
   #ifdef HAVE_OPENVZ
   VEID, VPID,
   #endif
   #ifdef HAVE_VSERVER
   VXID,
   #endif
   #ifdef HAVE_TASKSTATS
   RCHAR, WCHAR, SYSCR, SYSCW, RBYTES, WBYTES, CNCLWB, IO_READ_RATE, IO_WRITE_RATE, IO_RATE,
   #endif
   LAST_PROCESSFIELD
} ProcessField;

struct ProcessList_;

typedef struct Process_ {
   Object super;

   struct ProcessList_ *pl;
   bool updated;

   unsigned int pid;
   char* comm;
   int indent;
   char state;
   bool tag;
   unsigned int ppid;
   unsigned int pgrp;
   unsigned int session;
   unsigned int tty_nr;
   unsigned int tgid;
   int tpgid;
   unsigned long int flags;
   #ifdef DEBUG
   unsigned long int minflt;
   unsigned long int cminflt;
   unsigned long int majflt;
   unsigned long int cmajflt;
   #endif
   unsigned long int utime;
   unsigned long int stime;
   long int cutime;
   long int cstime;
   long int priority;
   long int nice;
   long int nlwp;
   #ifdef DEBUG
   long int itrealvalue;
   unsigned long int starttime;
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
   int exit_signal;
   int processor;
   int m_size;
   int m_resident;
   int m_share;
   int m_trs;
   int m_drs;
   int m_lrs;
   int m_dt;
   uid_t st_uid;
   float percent_cpu;
   float percent_mem;
   char* user;
   #ifdef HAVE_OPENVZ
   unsigned int veid;
   unsigned int vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif
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
} Process;

}*/

#ifdef DEBUG
char* PROCESS_CLASS = "Process";
#else
#define PROCESS_CLASS NULL
#endif

char *Process_fieldNames[] = {
   "", "PID", "Command", "STATE", "PPID", "PGRP", "SESSION",
   "TTY_NR", "TPGID", "FLAGS", "MINFLT", "CMINFLT", "MAJFLT", "CMAJFLT",
   "UTIME", "STIME", "CUTIME", "CSTIME", "PRIORITY", "NICE", "ITREALVALUE",
   "STARTTIME", "VSIZE", "RSS", "RLIM", "STARTCODE", "ENDCODE", "STARTSTACK",
   "KSTKESP", "KSTKEIP", "SIGNAL", "BLOCKED", "SIGIGNORE", "SIGCATCH", "WCHAN",
   "NSWAP", "CNSWAP", "EXIT_SIGNAL", "PROCESSOR", "M_SIZE", "M_RESIDENT", "M_SHARE",
   "M_TRS", "M_DRS", "M_LRS", "M_DT", "ST_UID", "PERCENT_CPU", "PERCENT_MEM",
   "USER", "TIME", "NLWP", "TGID", 
#ifdef HAVE_OPENVZ
   "VEID", "VPID",
#endif
#ifdef HAVE_VSERVER
   "VXID",
#endif
#ifdef HAVE_TASKSTATS
   "RCHAR", "WCHAR", "SYSCR", "SYSCW", "RBYTES", "WBYTES", "CNCLWB",
   "IO_READ_RATE", "IO_WRITE_RATE", "IO_RATE",
#endif
"*** report bug! ***"
};

char *Process_fieldTitles[] = {
   "", "  PID ", "Command ", "S ", " PPID ", " PGRP ", " SESN ",
   "  TTY ", "TPGID ", "- ", "- ", "- ", "- ", "- ",
   " UTIME+  ", " STIME+  ",  "- ", "- ", "PRI ", " NI ", "- ",
   "- ", "- ", "- ", "- ", "- ", "- ", "- ",
   "- ", "- ", "- ", "- ", "- ", "- ", "- ",
   "- ", "- ", "- ", "CPU ", " VIRT ", "  RES ", "  SHR ",
   " CODE ", " DATA ", " LIB ", " DIRTY ", " UID ", "CPU% ", "MEM% ",
   "USER     ", "  TIME+  ", "NLWP ", " TGID ",
#ifdef HAVE_OPENVZ
   " VEID ", " VPID ",
#endif
#ifdef HAVE_VSERVER
   " VXID ",
#endif
#ifdef HAVE_TASKSTATS
   "   RD_CHAR ", "   WR_CHAR ", "   RD_SYSC ", "   WR_SYSC ", "     IO_RD ", "     IO_WR ", " IO_CANCEL ",
   " IORR ", " IOWR ", "   IO ",
#endif
};

static int Process_getuid = -1;

#define ONE_K 1024
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)

static void Process_printLargeNumber(Process* this, RichString *str, unsigned long number) {
   char buffer[11];
   int len;
   if(number >= (1000 * ONE_M)) {
      len = snprintf(buffer, 10, "%4.2fG ", (float)number / ONE_M);
      RichString_appendn(str, CRT_colors[LARGE_NUMBER], buffer, len);
   } else if(number >= (100000)) {
      len = snprintf(buffer, 10, "%4ldM ", number / ONE_K);
      int attr = this->pl->highlightMegabytes
               ? CRT_colors[PROCESS_MEGABYTES]
               : CRT_colors[PROCESS];
      RichString_appendn(str, attr, buffer, len);
   } else if (this->pl->highlightMegabytes && number >= 1000) {
      len = snprintf(buffer, 10, "%2ld", number/1000);
      RichString_appendn(str, CRT_colors[PROCESS_MEGABYTES], buffer, len);
      number %= 1000;
      len = snprintf(buffer, 10, "%03ld ", number);
      RichString_appendn(str, CRT_colors[PROCESS], buffer, len);
   } else {
      len = snprintf(buffer, 10, "%5ld ", number);
      RichString_appendn(str, CRT_colors[PROCESS], buffer, len);
   }
}

static double jiffy = 0.0;

static void Process_printTime(RichString* str, unsigned long t) {
   if(jiffy == 0.0) jiffy = sysconf(_SC_CLK_TCK);
   double jiffytime = 1.0 / jiffy;

   double realTime = t * jiffytime;
   int iRealTime = (int) realTime;

   int hours = iRealTime / 3600;
   int minutes = (iRealTime / 60) % 60;
   int seconds = iRealTime % 60;
   int hundredths = (realTime - iRealTime) * 100;
   char buffer[11];
   if (hours) {
      snprintf(buffer, 10, "%2dh", hours);
      RichString_append(str, CRT_colors[LARGE_NUMBER], buffer);
      snprintf(buffer, 10, "%02d:%02d ", minutes, seconds);
   } else {
      snprintf(buffer, 10, "%2d:%02d.%02d ", minutes, seconds, hundredths);
   }
   RichString_append(str, CRT_colors[DEFAULT_COLOR], buffer);
}

static inline void Process_writeCommand(Process* this, int attr, int baseattr, RichString* str) {
   int start = str->len;
   RichString_append(str, attr, this->comm);
   if (this->pl->highlightBaseName) {
      int finish = str->len - 1;
      int space = RichString_findChar(str, ' ', start);
      if (space != -1)
         finish = space - 1;
      for (;;) {
         int slash = RichString_findChar(str, '/', start);
         if (slash == -1 || slash > finish)
            break;
         start = slash + 1;
      }
      RichString_setAttrn(str, baseattr, start, finish);
   }
}

static inline void Process_outputRate(Process* this, RichString* str, int attr, char* buffer, int n, double rate) {
   rate = rate / 1024;
   if (rate < 0.01)
      snprintf(buffer, n, "    0 ");
   else if (rate <= 10)
      snprintf(buffer, n, "%5.2f ", rate);
   else if (rate <= 100)
      snprintf(buffer, n, "%5.1f ", rate);
   else {
      Process_printLargeNumber(this, str, rate);
      return;
   }
   RichString_append(str, attr, buffer);
}

static void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[PROCESS_COMM_LEN];
   int attr = CRT_colors[DEFAULT_COLOR];
   int baseattr = CRT_colors[PROCESS_BASENAME];
   int n = PROCESS_COMM_LEN;

   switch (field) {
   case PID: snprintf(buffer, n, "%5u ", this->pid); break;
   case PPID: snprintf(buffer, n, "%5u ", this->ppid); break;
   case PGRP: snprintf(buffer, n, "%5u ", this->pgrp); break;
   case SESSION: snprintf(buffer, n, "%5u ", this->session); break;
   case TTY_NR: snprintf(buffer, n, "%5u ", this->tty_nr); break;
   case TGID: snprintf(buffer, n, "%5u ", this->tgid); break;
   case TPGID: snprintf(buffer, n, "%5d ", this->tpgid); break;
   case PROCESSOR: snprintf(buffer, n, "%3d ", this->processor+1); break;
   case NLWP: snprintf(buffer, n, "%4ld ", this->nlwp); break;
   case COMM: {
      if (this->pl->highlightThreads && (this->pid != this->tgid || this->m_size == 0)) {
         attr = CRT_colors[PROCESS_THREAD];
         baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
      }
      if (!this->pl->treeView || this->indent == 0) {
         Process_writeCommand(this, attr, baseattr, str);
         return;
      } else {
         char* buf = buffer;
         int maxIndent = 0;
         for (int i = 0; i < 32; i++)
            if (this->indent & (1 << i))
               maxIndent = i+1;
          for (int i = 0; i < maxIndent - 1; i++) {
            if (this->indent & (1 << i))
               snprintf(buf, n, " |  ");
            else
               snprintf(buf, n, "    ");
            buf += 4;
            n -= 4;
         }
         if (this->pl->direction == 1)
            snprintf(buf, n, " `- ");
         else
            snprintf(buf, n, " ,- ");
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
   case M_DRS: Process_printLargeNumber(this, str, this->m_drs * PAGE_SIZE); return;
   case M_DT: Process_printLargeNumber(this, str, this->m_dt * PAGE_SIZE); return;
   case M_LRS: Process_printLargeNumber(this, str, this->m_lrs * PAGE_SIZE); return;
   case M_TRS: Process_printLargeNumber(this, str, this->m_trs * PAGE_SIZE); return;
   case M_SIZE: Process_printLargeNumber(this, str, this->m_size * PAGE_SIZE); return;
   case M_RESIDENT: Process_printLargeNumber(this, str, this->m_resident * PAGE_SIZE); return;
   case M_SHARE: Process_printLargeNumber(this, str, this->m_share * PAGE_SIZE); return;
   case ST_UID: snprintf(buffer, n, "%4d ", this->st_uid); break;
   case USER: {
      if (Process_getuid != this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];
      if (this->user) {
      snprintf(buffer, n, "%-8s ", this->user);
      } else {
      snprintf(buffer, n, "%-8d ", this->st_uid);
      }
      if (buffer[8] != '\0') {
         buffer[8] = ' ';
         buffer[9] = '\0';
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
   #ifdef HAVE_OPENVZ
   case VEID: snprintf(buffer, n, "%5u ", this->veid); break;
   case VPID: snprintf(buffer, n, "%5u ", this->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: snprintf(buffer, n, "%5u ", this->vxid); break;
   #endif
   #ifdef HAVE_TASKSTATS
   case RCHAR:  snprintf(buffer, n, "%10llu ", this->io_rchar); break;
   case WCHAR:  snprintf(buffer, n, "%10llu ", this->io_wchar); break;   
   case SYSCR:  snprintf(buffer, n, "%10llu ", this->io_syscr); break;   
   case SYSCW:  snprintf(buffer, n, "%10llu ", this->io_syscw); break; 
   case RBYTES: snprintf(buffer, n, "%10llu ", this->io_read_bytes); break; 
   case WBYTES: snprintf(buffer, n, "%10llu ", this->io_write_bytes); break; 
   case CNCLWB: snprintf(buffer, n, "%10llu ", this->io_cancelled_write_bytes); break; 
   case IO_READ_RATE:  Process_outputRate(this, str, attr, buffer, n, this->io_rate_read_bps); return;
   case IO_WRITE_RATE: Process_outputRate(this, str, attr, buffer, n, this->io_rate_write_bps); return;
   case IO_RATE: Process_outputRate(this, str, attr, buffer, n, this->io_rate_read_bps + this->io_rate_write_bps); return;
   #endif

   default:
      snprintf(buffer, n, "- ");
   }
   RichString_append(str, attr, buffer);
}

static void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->pl->fields;
   RichString_init(out);
   for (int i = 0; fields[i]; i++)
      Process_writeField(this, out, fields[i]);
   if (this->pl->shadowOtherUsers && this->st_uid != Process_getuid)
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   assert(out->len > 0);
}

void Process_delete(Object* cast) {
   Process* this = (Process*) cast;
   assert (this != NULL);
   if (this->comm) free(this->comm);
   free(this);
}

Process* Process_new(struct ProcessList_ *pl) {
   Process* this = calloc(sizeof(Process), 1);
   Object_setClass(this, PROCESS_CLASS);
   ((Object*)this)->display = Process_display;
   ((Object*)this)->delete = Process_delete;
   this->pid = 0;
   this->pl = pl;
   this->tag = false;
   this->updated = false;
   this->utime = 0;
   this->stime = 0;
   this->comm = NULL;
   this->indent = 0;
   if (Process_getuid == -1) Process_getuid = getuid();
   return this;
}

Process* Process_clone(Process* this) {
   Process* clone = malloc(sizeof(Process));
   #if HAVE_TASKSTATS
   this->io_rchar = 0;
   this->io_wchar = 0;
   this->io_syscr = 0;
   this->io_syscw = 0;
   this->io_read_bytes = 0;
   this->io_rate_read_bps = 0;
   this->io_rate_read_time = 0;
   this->io_write_bytes = 0;
   this->io_rate_write_bps = 0;
   this->io_rate_write_time = 0;
   this->io_cancelled_write_bytes = 0;
   #endif
   memcpy(clone, this, sizeof(Process));
   this->comm = NULL;
   this->pid = 0;
   return clone;
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

unsigned long Process_getAffinity(Process* this) {
   unsigned long mask = 0;
   plpa_sched_getaffinity(this->pid, sizeof(unsigned long), (plpa_cpu_set_t*) &mask);
   return mask;
}

bool Process_setAffinity(Process* this, unsigned long mask) {
   return (plpa_sched_setaffinity(this->pid, sizeof(unsigned long), (plpa_cpu_set_t*) &mask) == 0);
}

void Process_sendSignal(Process* this, int signal) {
   kill(this->pid, signal);
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
      return strcmp(p1->user, p2->user);
   case PRIORITY:
      return (p1->priority - p2->priority);
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
   #ifdef HAVE_OPENVZ
   case VEID:
      return (p1->veid - p2->veid);
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

   default:
      return (p1->pid - p2->pid);
   }
   test_diff:
   return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

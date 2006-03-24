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

// This works only with glibc 2.1+. On earlier versions
// the behavior is similar to have a hardcoded page size.
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) / 1024 )

#define PROCESS_COMM_LEN 300
#define PROCESS_USER_LEN 10

/*{

typedef enum ProcessField_ {
   PID = 1, COMM, STATE, PPID, PGRP, SESSION, TTY_NR, TPGID, FLAGS, MINFLT, CMINFLT, MAJFLT, CMAJFLT, UTIME,
   STIME, CUTIME, CSTIME, PRIORITY, NICE, ITREALVALUE, STARTTIME, VSIZE, RSS, RLIM, STARTCODE, ENDCODE,
   STARTSTACK, KSTKESP, KSTKEIP, SIGNAL, BLOCKED, SSIGIGNORE, SIGCATCH, WCHAN, NSWAP, CNSWAP, EXIT_SIGNAL,
   PROCESSOR, M_SIZE, M_RESIDENT, M_SHARE, M_TRS, M_DRS, M_LRS, M_DT, ST_UID, PERCENT_CPU, PERCENT_MEM,
   USER, TIME, LAST_PROCESSFIELD
} ProcessField;

struct ProcessList_;

typedef struct Process_ {
   Object super;

   struct ProcessList_ *pl;
   bool updated;

   int pid;
   char* comm;
   int indent;
   char state;
   bool tag;
   int ppid;
   int pgrp;
   int session;
   int tty_nr;
   int tpgid;
   unsigned long int flags;
   unsigned long int minflt;
   unsigned long int cminflt;
   unsigned long int majflt;
   unsigned long int cmajflt;
   unsigned long int utime;
   unsigned long int stime;
   long int cutime;
   long int cstime;
   long int priority;
   long int nice;
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
   char user[PROCESS_USER_LEN + 1];
} Process;

extern char* PROCESS_CLASS;

extern char* Process_fieldNames[];

}*/

/* private property */
char* PROCESS_CLASS = "Process";

/* private property */
char *Process_fieldNames[] = { "", "PID", "Command", "STATE", "PPID", "PGRP", "SESSION", "TTY_NR", "TPGID", "FLAGS", "MINFLT", "CMINFLT", "MAJFLT", "CMAJFLT", "UTIME", "STIME", "CUTIME", "CSTIME", "PRIORITY", "NICE", "ITREALVALUE", "STARTTIME", "VSIZE", "RSS", "RLIM", "STARTCODE", "ENDCODE", "STARTSTACK", "KSTKESP", "KSTKEIP", "SIGNAL", "BLOCKED", "SIGIGNORE", "SIGCATCH", "WCHAN", "NSWAP", "CNSWAP", "EXIT_SIGNAL",  "PROCESSOR", "M_SIZE", "M_RESIDENT", "M_SHARE", "M_TRS", "M_DRS", "M_LRS", "M_DT", "ST_UID", "PERCENT_CPU", "PERCENT_MEM", "USER", "TIME", "*** report bug! ***"};

Process* Process_new(struct ProcessList_ *pl) {
   Process* this = malloc(sizeof(Process));
   ((Object*)this)->class = PROCESS_CLASS;
   ((Object*)this)->display = Process_display;
   ((Object*)this)->compare = Process_compare;
   ((Object*)this)->delete = Process_delete;
   this->pl = pl;
   this->tag = false;
   this->updated = false;
   this->utime = 0;
   this->stime = 0;
   this->comm = NULL;
   return this;
}

Process* Process_clone(Process* this) {
   Process* clone = malloc(sizeof(Process));
   memcpy(clone, this, sizeof(Process));
   return clone;
}

void Process_delete(Object* cast) {
   Process* this = (Process*) cast;
   if (this->comm) free(this->comm);
   assert (this != NULL);
   free(this);
}

void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->pl->fields;
   RichString_prune(out);
   for (int i = 0; fields[i]; i++)
      Process_writeField(this, out, fields[i]);
   if (this->pl->shadowOtherUsers && this->st_uid != getuid())
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   assert(out->len > 0);
}

void Process_toggleTag(Process* this) {
   this->tag = this->tag == true ? false : true;
}

void Process_setPriority(Process* this, int priority) {
   int old_prio = getpriority(PRIO_PROCESS, this->pid);
   int err = setpriority(PRIO_PROCESS, this->pid, priority);
   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, this->pid)) {
      this->nice = priority;
   }
}

void Process_sendSignal(Process* this, int signal) {
   kill(this->pid, signal);
}

#define ONE_K 1024
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)

/* private */
void Process_printLargeNumber(Process* this, RichString *str, unsigned int number) {
   char buffer[11];
   int len;
   if(number >= (1000 * ONE_M)) {
      len = snprintf(buffer, 10, "%4.2fG ", (float)number / ONE_M);
      RichString_appendn(str, CRT_colors[LARGE_NUMBER], buffer, len);
   } else if(number >= (100000)) {
      len = snprintf(buffer, 10, "%4dM ", number / ONE_K);
      int attr = this->pl->highlightMegabytes
               ? CRT_colors[PROCESS_MEGABYTES]
               : CRT_colors[PROCESS];
      RichString_appendn(str, attr, buffer, len);
   } else if (this->pl->highlightMegabytes && number >= 1000) {
      len = snprintf(buffer, 10, "%2d", number/1000);
      RichString_appendn(str, CRT_colors[PROCESS_MEGABYTES], buffer, len);
      number %= 1000;
      len = snprintf(buffer, 10, "%03d ", number);
      RichString_appendn(str, CRT_colors[PROCESS], buffer, len);
   } else {
      len = snprintf(buffer, 10, "%5d ", number);
      RichString_appendn(str, CRT_colors[PROCESS], buffer, len);
   }
}

/* private property */
double jiffy = 0.0;

/* private */
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

/* private */
inline static void Process_writeCommand(Process* this, int attr, RichString* str) {
   if (this->pl->highlightBaseName) {
      char* firstSpace = strchr(this->comm, ' ');
      if (firstSpace) {
         char* slash = firstSpace;
         while (slash > this->comm && *slash != '/')
            slash--;
         if (slash > this->comm) {
            slash++;
            RichString_appendn(str, attr, this->comm, slash - this->comm);
         }
         RichString_appendn(str, CRT_colors[PROCESS_BASENAME], slash, firstSpace - slash);
         RichString_append(str, attr, firstSpace);
      } else {
         RichString_append(str, CRT_colors[PROCESS_BASENAME], this->comm);
      }
   } else {
      RichString_append(str, attr, this->comm);
   }
}

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[PROCESS_COMM_LEN];
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = PROCESS_COMM_LEN;

   switch (field) {
   case PID: snprintf(buffer, n, "%5d ", this->pid); break;
   case PPID: snprintf(buffer, n, "%5d ", this->ppid); break;
   case PGRP: snprintf(buffer, n, "%5d ", this->pgrp); break;
   case SESSION: snprintf(buffer, n, "%5d ", this->session); break;
   case TTY_NR: snprintf(buffer, n, "%5d ", this->tty_nr); break;
   case TPGID: snprintf(buffer, n, "%5d ", this->tpgid); break;
   case PROCESSOR: snprintf(buffer, n, "%3d ", this->processor+1); break;
   case COMM: {
      if (!this->pl->treeView || this->indent == 0) {
         Process_writeCommand(this, attr, str);
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
         Process_writeCommand(this, attr, str);
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
   case M_SIZE: Process_printLargeNumber(this, str, this->m_size * PAGE_SIZE); return;
   case M_RESIDENT: Process_printLargeNumber(this, str, this->m_resident * PAGE_SIZE); return;
   case M_SHARE: Process_printLargeNumber(this, str, this->m_share * PAGE_SIZE); return;
   case ST_UID: snprintf(buffer, n, "%4d ", this->st_uid); break;
   case USER: {
      if (getuid() != this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];
      snprintf(buffer, n, "%-8s ", this->user);
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
      if (this->percent_cpu > 99.9) {
         snprintf(buffer, n, "100. "); 
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
   default:
      snprintf(buffer, n, "- ");
   }
   RichString_append(str, attr, buffer);
   return;
}

int Process_compare(const Object* v1, const Object* v2) {
   Process* p1 = (Process*)v1;
   Process* p2 = (Process*)v2;
   int direction = p1->pl->direction;
   switch (p1->pl->sortKey) {
   case PID:
      return (p2->pid - p1->pid) * direction;
   case PPID:
      return (p2->ppid - p1->ppid) * direction;
   case USER:
      return strcmp(p2->user, p1->user) * direction;
   case PRIORITY:
      return (p2->priority - p1->priority) * direction;
   case STATE:
      return (p2->state - p1->state) * direction;
   case NICE:
      return (p2->nice - p1->nice) * direction;
   case M_SIZE:
      return (p1->m_size - p2->m_size) * direction;
   case M_RESIDENT:
      return (p1->m_resident - p2->m_resident) * direction;
   case M_SHARE:
      return (p1->m_share - p2->m_share) * direction;
   case PERCENT_CPU:
      return (p1->percent_cpu < p2->percent_cpu ? -1 : 1) * direction;
   case PERCENT_MEM:
      return (p1->percent_mem < p2->percent_mem ? -1 : 1) * direction;
   case UTIME:
      return (p1->utime - p2->utime) * direction;
   case STIME:
      return (p1->stime - p2->stime) * direction;
   case TIME:
      return ((p1->utime+p1->stime) - (p2->utime+p2->stime)) * direction;
   case COMM:
      return strcmp(p2->comm, p1->comm) * direction;
   default:
      return (p2->pid - p1->pid) * direction;
   }
}

char* Process_printField(ProcessField field) {
   switch (field) {
   case PID: return "  PID ";
   case PPID: return " PPID ";
   case PGRP: return " PGRP ";
   case SESSION: return " SESN ";
   case TTY_NR: return "  TTY ";
   case TPGID: return " TGID ";
   case COMM: return "Command ";
   case STATE: return "S ";
   case PRIORITY: return "PRI ";
   case NICE: return " NI ";
   case M_SIZE: return " VIRT ";
   case M_RESIDENT: return "  RES ";
   case M_SHARE: return "  SHR ";
   case ST_UID: return " UID ";
   case USER: return "USER     ";
   case UTIME: return " UTIME+  ";
   case STIME: return " STIME+  ";
   case TIME: return "  TIME+  ";
   case PERCENT_CPU: return "CPU% ";
   case PERCENT_MEM: return "MEM% ";
   case PROCESSOR: return "CPU ";
   default: return "- ";
   }
}

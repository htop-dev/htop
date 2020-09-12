/*
htop - Process.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "Settings.h"

#include "config.h"

#include "CRT.h"
#include "StringUtils.h"
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
#include <math.h>
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#endif

static int Process_getuid = -1;

char Process_pidFormat[20] = "%7d ";

static char Process_titleBuffer[20][20];

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1) return;
   int digits = ceil(log10(maxPid));
   assert(digits < 20);
   for (int i = 0; Process_pidColumns[i].label; i++) {
      assert(i < 20);
      xSnprintf(Process_titleBuffer[i], 20, "%*s ", digits, Process_pidColumns[i].label);
      Process_fields[Process_pidColumns[i].id].title = Process_titleBuffer[i];
   }
   xSnprintf(Process_pidFormat, sizeof(Process_pidFormat), "%%%dd ", digits);
}

void Process_humanNumber(RichString* str, unsigned long number, bool coloring) {
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
         len = snprintf(buffer, 10, "%4luT ", number / ONE_G);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      } else if (number >= (1000 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, 10, "%4.1lfT ", (double)number / ONE_G);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      #endif
      if(number >= (100 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, 10, "%4luG ", number / ONE_M);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      len = snprintf(buffer, 10, "%4.1lfG ", (double)number / ONE_M);
      RichString_appendn(str, largeNumberColor, buffer, len);
      return;
   } else if (number >= 100000) {
      len = snprintf(buffer, 10, "%4luM ", number / ONE_K);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      return;
   } else if (number >= 1000) {
      len = snprintf(buffer, 10, "%2lu", number/1000);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      number %= 1000;
      len = snprintf(buffer, 10, "%03lu ", number);
      RichString_appendn(str, processColor, buffer, len);
      return;
   }
   len = snprintf(buffer, 10, "%5lu ", number);
   RichString_appendn(str, processColor, buffer, len);
}

void Process_colorNumber(RichString* str, unsigned long long number, bool coloring) {
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

   if ((long long) number == -1LL) {
      int len = snprintf(buffer, 13, "    no perm ");
      RichString_appendn(str, CRT_colors[PROCESS_SHADOW], buffer, len);
   } else if (number >= 100000LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, 13, "%11llu ", number / ONE_DECIMAL_G);
      RichString_appendn(str, largeNumberColor, buffer, 12);
   } else if (number >= 100LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, 13, "%11llu ", number / ONE_DECIMAL_M);
      RichString_appendn(str, largeNumberColor, buffer, 8);
      RichString_appendn(str, processMegabytesColor, buffer+8, 4);
   } else if (number >= 10LL * ONE_DECIMAL_G) {
      xSnprintf(buffer, 13, "%11llu ", number / ONE_DECIMAL_K);
      RichString_appendn(str, largeNumberColor, buffer, 5);
      RichString_appendn(str, processMegabytesColor, buffer+5, 3);
      RichString_appendn(str, processColor, buffer+8, 4);
   } else {
      xSnprintf(buffer, 13, "%11llu ", number);
      RichString_appendn(str, largeNumberColor, buffer, 2);
      RichString_appendn(str, processMegabytesColor, buffer+2, 3);
      RichString_appendn(str, processColor, buffer+5, 3);
      RichString_appendn(str, processShadowColor, buffer+8, 4);
   }
}

void Process_printTime(RichString* str, unsigned long long totalHundredths) {
   unsigned long long totalSeconds = totalHundredths / 100;

   unsigned long long hours = totalSeconds / 3600;
   int minutes = (totalSeconds / 60) % 60;
   int seconds = totalSeconds % 60;
   int hundredths = totalHundredths - (totalSeconds * 100);
   char buffer[11];
   if (hours >= 100) {
      xSnprintf(buffer, 10, "%7lluh ", hours);
      RichString_append(str, CRT_colors[LARGE_NUMBER], buffer);
   } else {
      if (hours) {
         xSnprintf(buffer, 10, "%2lluh", hours);
         RichString_append(str, CRT_colors[LARGE_NUMBER], buffer);
         xSnprintf(buffer, 10, "%02d:%02d ", minutes, seconds);
      } else {
         xSnprintf(buffer, 10, "%2d:%02d.%02d ", minutes, seconds, hundredths);
      }
      RichString_append(str, CRT_colors[DEFAULT_COLOR], buffer);
   }
}

static inline void Process_writeCommand(Process* this, int attr, int baseattr, RichString* str) {
   int start = RichString_size(str), finish = 0;
   char* comm = this->comm;

   if (this->settings->highlightBaseName || !this->settings->showProgramPath) {
      int i, basename = 0;
      for (i = 0; i < this->basenameOffset; i++) {
         if (comm[i] == '/') {
            basename = i + 1;
         } else if (comm[i] == ':') {
            finish = i + 1;
            break;
         }
      }
      if (!finish) {
         if (this->settings->showProgramPath)
            start += basename;
         else
            comm += basename;
         finish = this->basenameOffset - basename;
      }
      finish += start - 1;
   }

   RichString_append(str, attr, comm);

   if (this->settings->highlightBaseName)
      RichString_setAttrn(str, baseattr, start, finish);
}

void Process_outputRate(RichString* str, char* buffer, int n, double rate, int coloring) {
   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int processMegabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int processColor = CRT_colors[PROCESS];
   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      processMegabytesColor = CRT_colors[PROCESS];
   }
   if (rate == -1) {
      int len = snprintf(buffer, n, "    no perm ");
      RichString_appendn(str, CRT_colors[PROCESS_SHADOW], buffer, len);
   } else if (rate < ONE_K) {
      int len = snprintf(buffer, n, "%7.2f B/s ", rate);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_M) {
      int len = snprintf(buffer, n, "%7.2f K/s ", rate / ONE_K);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_G) {
      int len = snprintf(buffer, n, "%7.2f M/s ", rate / ONE_M);
      RichString_appendn(str, processMegabytesColor, buffer, len);
   } else if (rate < ONE_T) {
      int len = snprintf(buffer, n, "%7.2f G/s ", rate / ONE_G);
      RichString_appendn(str, largeNumberColor, buffer, len);
   } else {
      int len = snprintf(buffer, n, "%7.2f T/s ", rate / ONE_T);
      RichString_appendn(str, largeNumberColor, buffer, len);
   }
}

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int baseattr = CRT_colors[PROCESS_BASENAME];
   int n = sizeof(buffer) - 1;
   bool coloring = this->settings->highlightMegabytes;

   switch (field) {
   case PERCENT_CPU: {
      if (this->percent_cpu > 999.9) {
         xSnprintf(buffer, n, "%4u ", (unsigned int)this->percent_cpu);
      } else if (this->percent_cpu > 99.9) {
         xSnprintf(buffer, n, "%3u. ", (unsigned int)this->percent_cpu);
      } else {
         xSnprintf(buffer, n, "%4.1f ", this->percent_cpu);
      }
      break;
   }
   case PERCENT_MEM: {
      if (this->percent_mem > 99.9) {
         xSnprintf(buffer, n, "100. ");
      } else {
         xSnprintf(buffer, n, "%4.1f ", this->percent_mem);
      }
      break;
   }
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
            if (indent & (1U << i))
               maxIndent = i+1;
          for (int i = 0; i < maxIndent - 1; i++) {
            int written, ret;
            if (indent & (1 << i))
               ret = snprintf(buf, n, "%s  ", CRT_treeStr[TREE_STR_VERT]);
            else
               ret = snprintf(buf, n, "   ");
            if (ret < 0 || ret >= n) {
               written = n;
            } else {
               written = ret;
            }
            buf += written;
            n -= written;
         }
         const char* draw = CRT_treeStr[lastItem ? (this->settings->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
         xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
         RichString_append(str, CRT_colors[PROCESS_TREE], buffer);
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }
   }
   case MAJFLT: Process_colorNumber(str, this->majflt, coloring); return;
   case MINFLT: Process_colorNumber(str, this->minflt, coloring); return;
   case M_RESIDENT: Process_humanNumber(str, this->m_resident * PAGE_SIZE_KB, coloring); return;
   case M_SIZE: Process_humanNumber(str, this->m_size * PAGE_SIZE_KB, coloring); return;
   case NICE: {
      xSnprintf(buffer, n, "%3ld ", this->nice);
      attr = this->nice < 0 ? CRT_colors[PROCESS_HIGH_PRIORITY]
           : this->nice > 0 ? CRT_colors[PROCESS_LOW_PRIORITY]
           : attr;
      break;
   }
   case NLWP: xSnprintf(buffer, n, "%4ld ", this->nlwp); break;
   case PGRP: xSnprintf(buffer, n, Process_pidFormat, this->pgrp); break;
   case PID: xSnprintf(buffer, n, Process_pidFormat, this->pid); break;
   case PPID: xSnprintf(buffer, n, Process_pidFormat, this->ppid); break;
   case PRIORITY: {
      if(this->priority <= -100)
         xSnprintf(buffer, n, " RT ");
      else
         xSnprintf(buffer, n, "%3ld ", this->priority);
      break;
   }
   case PROCESSOR: xSnprintf(buffer, n, "%3d ", Settings_cpuId(this->settings, this->processor)); break;
   case SESSION: xSnprintf(buffer, n, Process_pidFormat, this->session); break;
   case STARTTIME: xSnprintf(buffer, n, "%s", this->starttime_show); break;
   case STATE: {
      xSnprintf(buffer, n, "%c ", this->state);
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
   case ST_UID: xSnprintf(buffer, n, "%5d ", this->st_uid); break;
   case TIME: Process_printTime(str, this->time); return;
   case TGID: xSnprintf(buffer, n, Process_pidFormat, this->tgid); break;
   case TPGID: xSnprintf(buffer, n, Process_pidFormat, this->tpgid); break;
   case TTY_NR: xSnprintf(buffer, n, "%3u:%3u ", major(this->tty_nr), minor(this->tty_nr)); break;
   case USER: {
      if (Process_getuid != (int) this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];
      if (this->user) {
         xSnprintf(buffer, n, "%-9s ", this->user);
      } else {
         xSnprintf(buffer, n, "%-9d ", this->st_uid);
      }
      if (buffer[9] != '\0') {
         buffer[9] = ' ';
         buffer[10] = '\0';
      }
      break;
   }
   default:
      xSnprintf(buffer, n, "- ");
   }
   RichString_append(str, attr, buffer);
}

void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->settings->fields;
   RichString_prune(out);
   for (int i = 0; fields[i]; i++)
      As_Process(this)->writeField(this, out, fields[i]);
   if (this->settings->shadowOtherUsers && (int)this->st_uid != Process_getuid)
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   assert(out->chlen > 0);
}

void Process_done(Process* this) {
   assert (this != NULL);
   free(this->comm);
}

ProcessClass Process_class = {
   .super = {
      .extends = Class(Object),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = Process_writeField,
};

void Process_init(Process* this, struct Settings_* settings) {
   this->settings = settings;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->updated = false;
   this->basenameOffset = -1;
   if (Process_getuid == -1) Process_getuid = getuid();
}

void Process_toggleTag(Process* this) {
   this->tag = this->tag == true ? false : true;
}

bool Process_setPriority(Process* this, int priority) {
   CRT_dropPrivileges();
   int old_prio = getpriority(PRIO_PROCESS, this->pid);
   int err = setpriority(PRIO_PROCESS, this->pid, priority);
   CRT_restorePrivileges();
   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, this->pid)) {
      this->nice = priority;
   }
   return (err == 0);
}

bool Process_changePriorityBy(Process* this, Arg delta) {
   return Process_setPriority(this, this->nice + delta.i);
}

bool Process_sendSignal(Process* this, Arg sgn) {
   CRT_dropPrivileges();
   bool ok = (kill(this->pid, sgn.i) == 0);
   CRT_restorePrivileges();
   return ok;
}

long Process_pidCompare(const void* v1, const void* v2) {
   Process* p1 = (Process*)v1;
   Process* p2 = (Process*)v2;
   return (p1->pid - p2->pid);
}

long Process_compare(const void* v1, const void* v2) {
   Process *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (Process*)v1;
      p2 = (Process*)v2;
   } else {
      p2 = (Process*)v1;
      p1 = (Process*)v2;
   }
   switch (settings->sortKey) {
   case PERCENT_CPU:
      return (p2->percent_cpu > p1->percent_cpu ? 1 : -1);
   case PERCENT_MEM:
      return (p2->m_resident - p1->m_resident);
   case COMM:
      return strcmp(p1->comm, p2->comm);
   case MAJFLT:
      return (p2->majflt - p1->majflt);
   case MINFLT:
      return (p2->minflt - p1->minflt);
   case M_RESIDENT:
      return (p2->m_resident - p1->m_resident);
   case M_SIZE:
      return (p2->m_size - p1->m_size);
   case NICE:
      return (p1->nice - p2->nice);
   case NLWP:
      return (p1->nlwp - p2->nlwp);
   case PGRP:
      return (p1->pgrp - p2->pgrp);
   case PID:
      return (p1->pid - p2->pid);
   case PPID:
      return (p1->ppid - p2->ppid);
   case PRIORITY:
      return (p1->priority - p2->priority);
   case PROCESSOR:
      return (p1->processor - p2->processor);
   case SESSION:
      return (p1->session - p2->session);
   case STARTTIME: {
      if (p1->starttime_ctime == p2->starttime_ctime)
         return (p1->pid - p2->pid);
      else
         return (p1->starttime_ctime - p2->starttime_ctime);
   }
   case STATE:
      return (Process_sortState(p1->state) - Process_sortState(p2->state));
   case ST_UID:
      return (p1->st_uid - p2->st_uid);
   case TIME:
      return ((p2->time) - (p1->time));
   case TGID:
      return (p1->tgid - p2->tgid);
   case TPGID:
      return (p1->tpgid - p2->tpgid);
   case TTY_NR:
      return (p1->tty_nr - p2->tty_nr);
   case USER:
      return strcmp(p1->user ? p1->user : "", p2->user ? p2->user : "");
   default:
      return (p1->pid - p2->pid);
   }
}

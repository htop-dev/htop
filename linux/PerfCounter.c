/*
 * This file is based on tiptop.
 * by Erven ROHOU
 * Copyright (c) 2011, 2012, 2014 Inria
 * License: GNU General Public License version 2.
 */
 
#include "PerfCounter.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "XAlloc.h"

/*{

#include <config.h>
#include <sys/types.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <inttypes.h>
#include <stdbool.h>
// The sys_perf_counter_open syscall and header files changed names
// between Linux 2.6.31 and 2.6.32. Do the mangling here.
#ifdef HAVE_LINUX_PERF_COUNTER_H
#include <linux/perf_counter.h>
#define STRUCT_NAME perf_counter_attr
#define SYSCALL_NUM __NR_perf_counter_open

#elif HAVE_LINUX_PERF_EVENT_H
#include <linux/perf_event.h>
#define STRUCT_NAME perf_event_attr
#define SYSCALL_NUM __NR_perf_event_open

#else
#error Sorry, performance counters not supported on this system.
#endif

typedef struct PerfCounter_ {
   struct STRUCT_NAME events;
   pid_t pid;
   int fd;
   uint64_t prevValue;
   uint64_t value;
} PerfCounter;

#define PerfCounter_delta(pc_) ((pc_)->value - (pc_)->prevValue)

}*/

int PerfCounter_openFds = 0;
static int PerfCounter_fdLimit = -1;

static void PerfCounter_initFdLimit() {
   char name[100] = { 0 };  /* needs to fit the name /proc/xxxx/limits */
   snprintf(name, sizeof(name) - 1, "/proc/%d/limits", getpid());
   FILE* f = fopen(name, "r");
   if (f) {
     char line[100];
     while (fgets(line, 100, f)) {
       int n = sscanf(line, "Max open files %d", &PerfCounter_fdLimit);
       if (n) {
         break;
       }
     }
     fclose(f);
   }

   PerfCounter_fdLimit -= 20; /* keep some slack */
   if (PerfCounter_fdLimit == 0) {  /* something went wrong */
      PerfCounter_fdLimit = 200;  /* reasonable default? */
   }
}

static long perf_event_open(struct STRUCT_NAME *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
   int ret = syscall(SYSCALL_NUM, hw_event, pid, cpu, group_fd, flags);
   #if defined(__x86_64__) || defined(__i386__)
   if (ret < 0 && ret > -4096) {
      errno = -ret;
      ret = -1;
   }
   #endif
   return ret;
}

PerfCounter* PerfCounter_new(pid_t pid, uint32_t type, uint64_t config) {
   if (PerfCounter_fdLimit == -1) {
      PerfCounter_initFdLimit();
   }
   PerfCounter* this = xCalloc(sizeof(PerfCounter), 1);
   this->pid = pid;
   this->events.disabled = 0;
   this->events.pinned = 1;
   this->events.exclude_hv = 1;
   this->events.exclude_kernel = 1;
   this->events.type = type;
   this->events.config = config;
   if (PerfCounter_openFds < PerfCounter_fdLimit) {
      this->fd = perf_event_open(&this->events, pid, -1, -1, 0);
   } else {
      this->fd = -1;
   }
   if (this->fd != -1) {
      PerfCounter_openFds++;
   }
   return this;
}

void PerfCounter_delete(PerfCounter* this) {
   if (!this) {
      return;
   }
   if (this->fd != -1) {
      PerfCounter_openFds--;
   }
   close(this->fd);
   free(this);
}

bool PerfCounter_read(PerfCounter* this) {
   if (this->fd == -1) {
      return false;
   }
   uint64_t value;
   int r = read(this->fd, &value, sizeof(value));
   if (r != sizeof(value)) {
      close(this->fd);
      this->fd = perf_event_open(&this->events, this->pid, -1, -1, 0);
      return false;
   }
   this->prevValue = this->value;
   this->value = value;
   return true;
}

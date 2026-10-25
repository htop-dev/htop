#ifndef HEADER_ProcMonitorEvent
#define HEADER_ProcMonitorEvent
/*
htop - linux/ProcMonitorEvent.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Record and map layouts shared between the eBPF program
linux/ProcMonitor.bpf.c and its userspace consumer linux/ProcMonitor.c.
Only fixed-width UAPI types are used, so both compilers agree on the layout.
*/

#include <stddef.h>
#include <linux/types.h>


#define PROCMONITOR_COMM_LEN 16
#define PROCMONITOR_ARGS_LEN 512
#define PROCMONITOR_FILENAME_LEN 256

typedef enum ProcMonitorEventType_ {
   PROCMONITOR_EVENT_FORK = 1,         /* new process */
   PROCMONITOR_EVENT_EXEC = 2,         /* process executed a new program */
   PROCMONITOR_EVENT_EXIT = 3,         /* last thread of a process ended */
   PROCMONITOR_EVENT_THREAD_START = 4, /* new thread in an existing process */
   PROCMONITOR_EVENT_THREAD_EXIT = 5,  /* thread ended, its process lives on */
} ProcMonitorEventType;

/* Every record carries the identity fields, so a process can be
 * reconstructed from whichever of its events made it into the ring buffer. */
typedef struct ProcMonitorEvent_ {
   __u32 type;                /* ProcMonitorEventType */
   __u32 pid;                 /* thread group id */
   __u32 tid;                 /* id of the thread the event is about */
   __u32 ppid;                /* thread group id of the real parent */
   __u32 uid;                 /* effective user id (owner of /proc/<pid>) */
   __u32 flags;               /* task flags (PF_*) */
   __s32 exitCode;            /* wait(2) style status; EXIT and THREAD_EXIT only */
   __u32 reserved;
   __u64 startBootNs;         /* start time of the thread in ns since boot (CLOCK_BOOTTIME) */
   __u64 cpuNs;               /* CPU time consumed: all threads for EXIT, the thread for THREAD_EXIT */
   char comm[PROCMONITOR_COMM_LEN];

   /* The members below are only transferred for EXEC records */
   __u32 argsLen;             /* valid bytes in args */
   __u32 reserved2;
   char filename[PROCMONITOR_FILENAME_LEN]; /* path handed to execve(2) */
   char args[PROCMONITOR_ARGS_LEN];         /* NUL separated argv, possibly truncated */
} ProcMonitorEvent;

/* Size of a record without the EXEC only members */
#define PROCMONITOR_EVENT_BASE_SIZE (offsetof(ProcMonitorEvent, argsLen))

/* Single entry of the procmon_config array map, written by userspace */
typedef struct ProcMonitorConfig_ {
   __u32 traceThreads;        /* also report THREAD_START and THREAD_EXIT */
} ProcMonitorConfig;

#endif /* HEADER_ProcMonitorEvent */

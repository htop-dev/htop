#ifndef HEADER_FreeBSDProcess
#define HEADER_FreeBSDProcess
/*
htop - FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/time.h>

#include "Object.h"
#include "Process.h"
#include "Machine.h"

typedef enum {
   SCHEDCLASS_UNKNOWN = 0,

   SCHEDCLASS_INTR_THREAD, /* interrupt thread */
   SCHEDCLASS_REALTIME,
   SCHEDCLASS_TIMESHARE, /* Regular scheduling */
   SCHEDCLASS_IDLE,

   MAX_SCHEDCLASS,
} FreeBSDSchedClass;

typedef struct FreeBSDProcess_ {
   Process super;
   int   jid;
   char* jname;
   char* emul;
   FreeBSDSchedClass sched_class;

   unsigned long long io_last_inblock;      /* cumulative block read ops at last sample */
   unsigned long long io_last_oublock;      /* cumulative block write ops at last sample */
   unsigned long long io_last_scan_time_ms; /* time of last I/O sample, ms since the Epoch */
   struct timeval io_process_start;         /* start time of the sampled process, to detect PID reuse */
   double io_read_ops;                      /* block read operations per second */
   double io_write_ops;                     /* block write operations per second */
} FreeBSDProcess;

extern const ProcessClass FreeBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* FreeBSDProcess_new(const Machine* host);

void Process_delete(Object* cast);

#endif

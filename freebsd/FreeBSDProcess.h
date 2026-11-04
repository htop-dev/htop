#ifndef HEADER_FreeBSDProcess
#define HEADER_FreeBSDProcess
/*
htop - FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

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

   /* Block input operations (ru_inblock) */
   uint64_t io_read_ops;

   /* Block output operations (ru_oublock) */
   uint64_t io_write_ops;

   /* Point in time of last io scan (monotonic milliseconds) */
   uint64_t io_last_scan_time_ms;

   /* Block input operations per second */
   double io_rate_read_ops;

   /* Block output operations per second */
   double io_rate_write_ops;
} FreeBSDProcess;

extern const ProcessClass FreeBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* FreeBSDProcess_new(const Machine* host);

void Process_delete(Object* cast);

#endif

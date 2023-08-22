#ifndef HEADER_RowField
#define HEADER_RowField
/*
htop - RowField.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessField.h" // platform-specific fields reserved for processes


typedef enum ReservedFields_ {
   NULL_FIELD = 0,
   PID = 1,
   COMM = 2,
   STATE = 3,
   PPID = 4,
   PGRP = 5,
   SESSION = 6,
   TTY = 7,
   TPGID = 8,
   MINFLT = 10,
   MAJFLT = 12,
   PRIORITY = 18,
   NICE = 19,
   STARTTIME = 21,
   PROCESSOR = 38,
   M_VIRT = 39,
   M_RESIDENT = 40,
   ST_UID = 46,
   PERCENT_CPU = 47,
   PERCENT_MEM = 48,
   USER = 49,
   TIME = 50,
   NLWP = 51,
   TGID = 52,
   PERCENT_NORM_CPU = 53,
   ELAPSED = 54,
   SCHEDULERPOLICY = 55,
   PROC_COMM = 124,
   PROC_EXE = 125,
   CWD = 126,

   /* Platform specific fields, defined in ${platform}/ProcessField.h */
   PLATFORM_PROCESS_FIELDS

   /* Do not add new fields after this entry (dynamic entries follow) */
   LAST_RESERVED_FIELD
} ReservedFields;

/* Follow ReservedField entries with dynamic fields defined at runtime */
#define ROW_DYNAMIC_FIELDS LAST_RESERVED_FIELD
typedef int32_t RowField;

#endif

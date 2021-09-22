#ifndef HEADER_SolarisProcessField
#define HEADER_SolarisProcessField
/*
htop - solaris/ProcessField.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


#define PLATFORM_PROCESS_FIELDS  \
   ZONEID = 100,                 \
   ZONE  = 101,                  \
   PROJID = 102,                 \
   TASKID = 103,                 \
   POOLID = 104,                 \
   CONTID = 105,                 \
   LWPID = 106,                  \
                                 \
   DUMMY_BUMP_FIELD = CWD,       \
   // End of list


#endif /* HEADER_SolarisProcessField */

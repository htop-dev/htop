#ifndef HEADER_CygwinProcessField
#define HEADER_CygwinProcessField
/*
htop - cygwin/ProcessField.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


#define PLATFORM_PROCESS_FIELDS  \
   CMINFLT = 11,                 \
   CMAJFLT = 13,                 \
   UTIME = 14,                   \
   STIME = 15,                   \
   CUTIME = 16,                  \
   CSTIME = 17,                  \
   M_SHARE = 41,                 \
   M_TRS = 42,                   \
   M_DRS = 43,                   \
   M_LRS = 44,                   \
   WINPID = 100,                 \
                                 \
   DUMMY_BUMP_FIELD = CWD,       \
   // End of list


#endif /* HEADER_CygwinProcessField */

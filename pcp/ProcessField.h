#ifndef HEADER_PCPProcessField
#define HEADER_PCPProcessField
/*
htop - pcp/ProcessField.h
(C) 2014 Hisham H. Muhammad
(C) 2021 htop dev team
(C) 2020-2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
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
   M_DT = 45,                    \
   CTID = 100,                   \
   RCHAR = 103,                  \
   WCHAR = 104,                  \
   SYSCR = 105,                  \
   SYSCW = 106,                  \
   RBYTES = 107,                 \
   WBYTES = 108,                 \
   CNCLWB = 109,                 \
   IO_READ_RATE = 110,           \
   IO_WRITE_RATE = 111,          \
   IO_RATE = 112,                \
   CGROUP = 113,                 \
   OOM = 114,                    \
   PERCENT_CPU_DELAY = 116,      \
   PERCENT_IO_DELAY = 117,       \
   PERCENT_SWAP_DELAY = 118,     \
   M_PSS = 119,                  \
   M_SWAP = 120,                 \
   M_PSSWP = 121,                \
   CTXT = 122,                   \
   SECATTR = 123,                \
                                 \
   DUMMY_BUMP_FIELD = CWD,       \
   // End of list


#endif /* HEADER_PCPProcessField */

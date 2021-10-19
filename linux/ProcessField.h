#ifndef HEADER_LinuxProcessField
#define HEADER_LinuxProcessField
/*
htop - linux/ProcessField.h
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
   CTID = 100,                   \
   VPID = 101,                   \
   VXID = 102,                   \
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
   IO_PRIORITY = 115,            \
   PERCENT_CPU_DELAY = 116,      \
   PERCENT_IO_DELAY = 117,       \
   PERCENT_SWAP_DELAY = 118,     \
   M_PSS = 119,                  \
   M_SWAP = 120,                 \
   M_PSSWP = 121,                \
   CTXT = 122,                   \
   SECATTR = 123,                \
   AUTOGROUP_ID = 127,           \
   AUTOGROUP_NICE = 128,         \
   CCGROUP = 129,                \
   // End of list


#endif /* HEADER_LinuxProcessField */

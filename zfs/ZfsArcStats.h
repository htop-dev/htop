#ifndef HEADER_ZfsArcStats
#define HEADER_ZfsArcStats
/*
htop - ZfsArcStats.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

typedef struct ZfsArcStats_ {
   int enabled;
   int isCompressed;
   unsigned long long int max;
   unsigned long long int size;
   unsigned long long int MFU;
   unsigned long long int MRU;
   unsigned long long int anon;
   unsigned long long int header;
   unsigned long long int other;
   unsigned long long int compressed;
   unsigned long long int uncompressed;
} ZfsArcStats;

#endif

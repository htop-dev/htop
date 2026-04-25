#ifndef HEADER_gettime
#define HEADER_gettime
/*
htop - generic/gettime.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>
#include <time.h>


void Generic_gettime_realtime(struct timespec* tsp, uint64_t* msec);

void Generic_gettime_monotonic(uint64_t* msec);

static inline int timespec_cmp(const struct timespec* a, const struct timespec* b) {
   if (a->tv_sec != b->tv_sec)
      return a->tv_sec < b->tv_sec ? -1 : 1;
   if (a->tv_nsec != b->tv_nsec)
      return a->tv_nsec < b->tv_nsec ? -1 : 1;
   return 0;
}

static inline void timespec_add(const struct timespec* a, const struct timespec* b, struct timespec* result) {
   result->tv_sec = a->tv_sec + b->tv_sec;
   result->tv_nsec = a->tv_nsec + b->tv_nsec;
   if (result->tv_nsec >= 1000000000L) {
      result->tv_sec++;
      result->tv_nsec -= 1000000000L;
   }
}

#endif

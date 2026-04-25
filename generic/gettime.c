/*
htop - generic/gettime.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"  // IWYU pragma: keep

#include "generic/gettime.h"

#include <string.h>

#if !defined(HAVE_CLOCK_GETTIME)
#include <sys/time.h>
#endif


void Generic_gettime_realtime(struct timespec* tsp, uint64_t* msec) {

#if defined(HAVE_CLOCK_GETTIME)

   if (clock_gettime(CLOCK_REALTIME, tsp) == 0) {
      *msec = ((uint64_t)tsp->tv_sec * 1000) + ((uint64_t)tsp->tv_nsec / 1000000);
   } else {
      memset(tsp, 0, sizeof(*tsp));
      *msec = 0;
   }

#else /* lower resolution gettimeofday(2) fallback */

   struct timeval tv;
   if (gettimeofday(&tv, NULL) == 0) {
      tsp->tv_sec = tv.tv_sec;
      tsp->tv_nsec = (long)tv.tv_usec * 1000L;
      *msec = ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
   } else {
      memset(tsp, 0, sizeof(*tsp));
      *msec = 0;
   }

#endif
}

void Generic_gettime_monotonic(uint64_t* msec) {
#if defined(HAVE_CLOCK_GETTIME)

   struct timespec ts;
   if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
      *msec = ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
   else
      *msec = 0;

#else /* lower resolution gettimeofday() fallback */

   struct timeval tv;
   if (gettimeofday(&tv, NULL) == 0)
      *msec = ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
   else
      *msec = 0;

#endif
}

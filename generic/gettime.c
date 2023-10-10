/*
htop - generic/gettime.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"  // IWYU pragma: keep

#include "generic/gettime.h"

#include <string.h>
#include <time.h>


void Generic_gettime_realtime(struct timeval* tvp, uint64_t* msec) {

#if defined(HAVE_CLOCK_GETTIME)

   struct timespec ts;
   if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
      tvp->tv_sec = ts.tv_sec;
      tvp->tv_usec = ts.tv_nsec / 1000;
      *msec = ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
   } else {
      memset(tvp, 0, sizeof(struct timeval));
      *msec = 0;
   }

#else /* lower resolution gettimeofday(2) is always available */

   struct timeval tv;
   if (gettimeofday(&tv, NULL) == 0) {
      *tvp = tv; /* struct copy */
      *msec = ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
   } else {
      memset(tvp, 0, sizeof(struct timeval));
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

#else /* lower resolution gettimeofday() should be always available */

   struct timeval tv;
   if (gettimeofday(&tv, NULL) == 0)
      *msec = ((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000);
   else
      *msec = 0;

#endif
}

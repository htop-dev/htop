#ifndef HEADER_gettime
#define HEADER_gettime
/*
htop - generic/gettime.h
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>


void Generic_gettime_realtime(struct timeval* ts, uint64_t* msec);

void Generic_gettime_monotonic(uint64_t* msec);

#endif

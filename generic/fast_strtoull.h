#ifndef HEADER_fast_strtoull
#define HEADER_fast_strtoull
/*
htop - generic/fast_strtoull.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>


uint64_t fast_strtoull_dec(char** str, int maxlen);

uint64_t fast_strtoull_hex(char** str, int maxlen);

#endif /* HEADER_fast_strtoull */

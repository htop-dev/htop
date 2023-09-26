#ifndef HEADER_fdopenat
#define HEADER_fdopenat
/*
htop - generic/fdopenat.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdio.h>

#include "Compat.h"


FILE* fopenat(openat_arg_t openatArg, const char* pathname, const char* mode);

#endif /* HEADER_fdopenat */

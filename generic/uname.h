#ifndef HEADER_uname
#define HEADER_uname
/*
htop - generic/uname.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

typedef void (*Platform_FetchReleaseFunction)(char* buffer, size_t length);

const char* Generic_unameRelease(Platform_FetchReleaseFunction fetchRelease);

const char* Generic_uname(void);

#endif

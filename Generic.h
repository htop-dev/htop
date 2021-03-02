#ifndef HEADER_Generic
#define HEADER_Generic
/*
htop - Generic.h
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

void Generic_Hostname(char* buffer, size_t size);

char* Generic_OSRelease(void);

#endif

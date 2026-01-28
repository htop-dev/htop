#ifndef HEADER_sort
#define HEADER_sort
/*
htop - generic/sort.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "Object.h"


void Generic_sort(void* array, size_t len, size_t size, Object_Compare compare, void* context);

#endif

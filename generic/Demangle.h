#ifndef HEADER_Demangle
#define HEADER_Demangle
/*
htop - generic/Demangle.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"


#ifdef HAVE_DEMANGLING
ATTR_NONNULL ATTR_MALLOC
char* Generic_Demangle(const char* mangled);
#else
ATTR_NONNULL
static inline char* Generic_Demangle(const char* mangled) {
   (void)mangled;
   return NULL;
}
#endif

#endif

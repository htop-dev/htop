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
char* Demangle_demangle(const char* mangled);
#else
ATTR_NONNULL
static inline char* Demangle_demangle(const char* mangled) {
   (void)mangled;
   return NULL;
}
#endif /* HAVE_DEMANGLING */

#endif

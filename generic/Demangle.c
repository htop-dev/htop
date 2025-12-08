/*
htop - generic/Demangle.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/Demangle.h"

#include <stdint.h> // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep

#ifdef HAVE_DEMANGLE_H
# if !defined(HAVE_DECL_BASENAME)
// Suppress libiberty's own declaration of basename().
// It's a pity that we need this workaround as libiberty developers
// refuse fix their headers and export an unwanted interface to us.
// htop doesn't use basename() API. (The POSIX version is flawed by
// design; libiberty's ships with GNU version of basename() that's
// incompatible with POSIX.)
// <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122729>
#  define HAVE_DECL_BASENAME 1
# endif
#include <demangle.h>
#endif


#ifdef HAVE_DEMANGLING
char* Generic_Demangle(const char* mangled) {
# if defined(HAVE_LIBIBERTY_CPLUS_DEMANGLE)
   int options = DMGL_PARAMS | DMGL_AUTO;
   return cplus_demangle(mangled, options);
# elif defined(HAVE_LIBDEMANGLE_CPLUS_DEMANGLE)
   // The cplus_demangle() API from libdemangle is flawed. It does not
   // provide us the required size of the buffer to store the demangled
   // name, and it leaves the buffer content undefined if the specified
   // buffer size is not big enough.
   static size_t allocSize = 8;

   char* buf;

   while (!!(buf = malloc(allocSize))) {
      int ret = cplus_demangle(mangled, buf, allocSize);
      if (ret == 0)
         break;

      free(buf);
      buf = NULL;

      if (ret != DEMANGLE_ESPACE || allocSize > SIZE_MAX / 2)
         break;

      allocSize *= 2;
   }

   return buf;
# endif

   // Unimplemented
   (void)mangled;
   return NULL;
}
#endif

/*
htop - generic/Demangle.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/Demangle.h"

#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h> // IWYU pragma: keep
#include <stdint.h> // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

#include "generic/ProvideDemangle.h"


#ifdef HAVE_DEMANGLING
char* Demangle_demangle(const char* mangled) {
# if defined(HAVE_LIBIBERTY_CPLUS_DEMANGLE)
   int options = DMGL_PARAMS | DMGL_AUTO;
   return cplus_demangle(mangled, options);
# elif defined(HAVE_LIBDEMANGLE_CPLUS_DEMANGLE)
   // The cplus_demangle() API from libdemangle is flawed. It does not
   // provide us the required size of the buffer to store the demangled
   // name, and it leaves the buffer content undefined if the specified
   // buffer size is not big enough.

   // No crash on allocation failure. This is for safety against
   // incredibly long demangled names in untrustable programs.
   static size_t bufSize = 128;
   static char* buf;

   while (true) {
      if (!buf) {
         buf = malloc(bufSize);
         if (!buf)
            return NULL;
      }

      int res = cplus_demangle(mangled, buf, bufSize);
      if (res == 0)
         break;
      if (res != DEMANGLE_ESPACE || bufSize > SIZE_MAX / 2)
         return NULL;

      bufSize *= 2;
      free(buf);
      buf = NULL;
   }

   return strdup(buf);
# else
   (void)mangled;
   return NULL;
# endif
}
#endif /* HAVE_DEMANGLING */

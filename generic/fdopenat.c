/*
htop - generic/fdopenat.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "generic/fdopenat.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "Compat.h"
#include "XUtils.h"


FILE* fopenat(openat_arg_t openatArg, const char* pathname, const char* mode) {
   assert(String_eq(mode, "r")); /* only currently supported mode */

   int fd = Compat_openat(openatArg, pathname, O_RDONLY);
   if (fd < 0)
      return NULL;

   FILE* stream = fdopen(fd, mode);
   if (!stream)
      close(fd);

   return stream;
}

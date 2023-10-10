/*
htop - generic/uname.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"  // IWYU pragma: keep

#include "generic/uname.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "XUtils.h"

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif


#ifndef OSRELEASEFILE
#define OSRELEASEFILE "/etc/os-release"
#endif

static void parseOSRelease(char* buffer, size_t bufferLen) {
   FILE* stream = fopen(OSRELEASEFILE, "r");
   if (!stream) {
      xSnprintf(buffer, bufferLen, "No OS Release");
      return;
   }

   char name[64] = {'\0'};
   char version[64] = {'\0'};
   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), stream)) {
      if (String_startsWith(lineBuffer, "PRETTY_NAME=\"")) {
         const char* start = lineBuffer + strlen("PRETTY_NAME=\"");
         const char* stop = strrchr(lineBuffer, '"');
         if (!stop || stop <= start)
            continue;
         String_safeStrncpy(buffer, start, MINIMUM(bufferLen, (size_t)(stop - start + 1)));
         fclose(stream);
         return;
      }
      if (String_startsWith(lineBuffer, "NAME=\"")) {
         const char* start = lineBuffer + strlen("NAME=\"");
         const char* stop = strrchr(lineBuffer, '"');
         if (!stop || stop <= start)
            continue;
         String_safeStrncpy(name, start, MINIMUM(sizeof(name), (size_t)(stop - start + 1)));
         continue;
      }
      if (String_startsWith(lineBuffer, "VERSION=\"")) {
         const char* start = lineBuffer + strlen("VERSION=\"");
         const char* stop = strrchr(lineBuffer, '"');
         if (!stop || stop <= start)
            continue;
         String_safeStrncpy(version, start, MINIMUM(sizeof(version), (size_t)(stop - start + 1)));
         continue;
      }
   }
   fclose(stream);

   snprintf(buffer, bufferLen, "%s%s%s", name[0] ? name : "", name[0] && version[0] ? " " : "", version);
}

char* Generic_uname(void) {
   static char savedString[
      /* uname structure fields - manpages recommend sizeof */
      sizeof(((struct utsname*)0)->sysname) +
      sizeof(((struct utsname*)0)->release) +
      sizeof(((struct utsname*)0)->machine) +
      16/*markup*/ +
      128/*distro*/] = {'\0'};
   static bool loaded_data = false;

   if (!loaded_data) {
      struct utsname uname_info;
      int uname_result = uname(&uname_info);

      char distro[128];
      parseOSRelease(distro, sizeof(distro));

      if (uname_result == 0) {
         size_t written = xSnprintf(savedString, sizeof(savedString), "%s %s [%s]", uname_info.sysname, uname_info.release, uname_info.machine);
         if (!String_contains_i(savedString, distro, false) && sizeof(savedString) > written)
            snprintf(savedString + written, sizeof(savedString) - written, " @ %s", distro);
      } else {
         snprintf(savedString, sizeof(savedString), "%s", distro);
      }

      loaded_data = true;
   }

   return savedString;
}

/*
htop - UsersTable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "UsersTable.h"

#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "XUtils.h"


UsersTable* UsersTable_new(void) {
   UsersTable* this;
   this = xMalloc(sizeof(UsersTable));
   this->users = Hashtable_new(10, true);
   return this;
}

void UsersTable_delete(UsersTable* this) {
   Hashtable_delete(this->users);
   free(this);
}

char* UsersTable_getRef(UsersTable* this, unsigned int uid) {
   char* name = Hashtable_get(this->users, uid);

   if (name == NULL) {

/* ==============================================================
 * STATIC BUILD PATH (Crash Prevention Logic)
 * This block is compiled ONLY when --enable-static is detected
 * by configure.ac, which defines HTOP_STATIC.
 * ============================================================== */
#ifdef HTOP_STATIC
      /* 1. Try resolving standard users with getpwuid (safe range < 65536)
       * Standard local users are safe to query directly even in static builds.
       */
     if (uid < 65536) {
         const struct passwd* userData = getpwuid(uid);
         if (userData != NULL) {
            name = xStrdup(userData->pw_name);
         }
      }

      /* 2. If not found (or high UID), try resolving via "getent" command.
       * This "Out-of-Process" lookup avoids crashing htop by isolating the NSS lookup
       * from the static binary's memory space. This is critical for Systemd dynamic users.
       */
      if (name == NULL) {
         char command[64];
         // Construct command: getent passwd <uid>
         snprintf(command, sizeof(command), "getent passwd %u", uid);

         FILE* fp = popen(command, "r");
         if (fp) {
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
               // getent output format: name:password:uid...
               // We only need the part before the first colon
               char* colon = strchr(buffer, ':');
               if (colon) {
                  *colon = '\0'; // Truncate string at the first colon
                  name = xStrdup(buffer);
               }
            }
            pclose(fp);
         }
      }

      /* 3. If still not found, fallback to displaying the UID number */
      if (name == NULL) {
         char buf[32];
         xSnprintf(buf, sizeof(buf), "%u", uid);
         name = xStrdup(buf);
      }

/* ==============================================================
 * DYNAMIC BUILD PATH (Standard Logic)
 * Default behavior for standard dynamic builds. Zero performance overhead.
 * ============================================================== */
#else
     const struct passwd* userData = getpwuid(uid);
      if (userData != NULL) {
         name = xStrdup(userData->pw_name);
      }
#endif

      /* Common caching logic for both paths */
      if (name != NULL) {
         Hashtable_put(this->users, uid, name);
      }
   }
   return name;
}

inline void UsersTable_foreach(UsersTable* this, Hashtable_PairFunction f, void* userData) {
   Hashtable_foreach(this->users, f, userData);
}

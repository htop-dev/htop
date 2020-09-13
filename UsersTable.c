/*
htop - UsersTable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UsersTable.h"
#include "XAlloc.h"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>


UsersTable* UsersTable_new() {
   UsersTable* this;
   this = xMalloc(sizeof(UsersTable));
   this->users = Hashtable_new(20, true);
   return this;
}

void UsersTable_delete(UsersTable* this) {
   Hashtable_delete(this->users);
   free(this);
}

char* UsersTable_getRef(UsersTable* this, unsigned int uid) {
   char* name = (char*) (Hashtable_get(this->users, uid));
   if (name == NULL) {
      struct passwd userData = { 0 };
      struct passwd *result = &userData;
      size_t bufsize = 16384;
      char *buf = xMalloc(bufsize);

      if (0 != getpwuid_r(uid, &userData, buf, bufsize, &result)) {
         result = NULL;
      }

      if (result != NULL) {
         name = xStrdup(result->pw_name);
         Hashtable_put(this->users, uid, name);
      }

      free(buf);
   }

   return name;
}

inline void UsersTable_foreach(UsersTable* this, Hashtable_PairFunction f, void* userData) {
   Hashtable_foreach(this->users, f, userData);
}

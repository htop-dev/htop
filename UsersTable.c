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


UsersTable* UsersTable_new() {
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
      const struct passwd* userData = getpwuid(uid);
      if (userData != NULL) {
         name = xStrdup(userData->pw_name);
         Hashtable_put(this->users, uid, name);
      }
   }
   return name;
}

inline void UsersTable_foreach(UsersTable* this, Hashtable_PairFunction f, void* userData) {
   Hashtable_foreach(this->users, f, userData);
}

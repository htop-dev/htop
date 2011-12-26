/*
htop - UsersTable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UsersTable.h"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>

/*{
#include "Hashtable.h"

typedef struct UsersTable_ {
   Hashtable* users;
} UsersTable;
}*/

UsersTable* UsersTable_new() {
   UsersTable* this;
   this = malloc(sizeof(UsersTable));
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
      struct passwd* userData = getpwuid(uid);
      if (userData != NULL) {
         name = strdup(userData->pw_name);
         Hashtable_put(this->users, uid, name);
      }
   }
   return name;
}

inline void UsersTable_foreach(UsersTable* this, Hashtable_PairFunction f, void* userData) {
   Hashtable_foreach(this->users, f, userData);
}

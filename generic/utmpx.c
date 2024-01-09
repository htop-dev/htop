/*
htop - generic/utmpx.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"  // IWYU pragma: keep

#include "generic/utmpx.h"

#include "Macros.h"
#include "XUtils.h"

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif


int Generic_utmpx_get_users(void) {
#ifdef HAVE_UTMPX_H
   struct utmpx *ut;
   int ret = 0;
#ifdef HAVE_STRUCT_UTMPX_UT_SESSION
   size_t sessions_cap = 32, sessions_size = 0;
   long* sessions = xMallocArray(sessions_cap, sizeof(*sessions));
#endif /* HAVE_STRUCT_UTMPX_UT_SESSION */

   setutxent();
   while ((ut = getutxent())) {
      if (ut->ut_type == USER_PROCESS) {
#ifdef HAVE_STRUCT_UTMPX_UT_SESSION
         bool found = false;
         for (size_t i = 0; i < sessions_size; i++) {
            if (sessions[i] == ut->ut_session) {
               found = true;
               break;
            }
         }
         if (!found) {
            ret++;

            if (sessions_size +1 >= sessions_cap) {
               sessions_cap *= 2;
               xReallocArray(sessions, sessions_cap, sizeof(*sessions));
            }
            sessions[sessions_size++] = ut->ut_session;
         }
#else
         ret++;
#endif /* HAVE_STRUCT_UTMPX_UT_SESSION */
      }
   }
   endutxent();

#ifdef HAVE_STRUCT_UTMPX_UT_SESSION
   free(sessions);
#endif /* HAVE_STRUCT_UTMPX_UT_SESSION */

   return ret;
#else
   return -1;
#endif /* HAVE_UTMPX_H */
}

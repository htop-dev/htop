/*
htop - UserSessionsMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "UserSessionsMeter.h"

#include <dlfcn.h>


#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "RichString.h"
#include "XUtils.h"
#include "generic/utmpx.h"

#if defined(BUILD_STATIC) && defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-login.h>
#endif

#ifndef BUILD_STATIC
#include <dlfcn.h>
#include "linux/Platform.h"
#endif


static int users = 0;

#ifdef BUILD_STATIC
#define sym_sd_get_sessions sd_get_sessions
#else

# ifdef HTOP_LINUX
static int (*sym_sd_get_sessions)(char***);
static void* dlopenHandle;
static size_t activeHandles;
# endif /* HTOP_LINUX */

#endif /* BUILD_STATIC */


static void UserSessionsMeter_init(ATTR_UNUSED Meter* this) {
#if !defined(BUILD_STATIC) && defined(HTOP_LINUX)
   assert(activeHandles != SIZE_MAX);
   assert((dlopenHandle != NULL) == (activeHandles > 0));

   if (activeHandles > 0) {
      activeHandles++;
      return;
   }

   dlopenHandle = Platform_load_libsystemd();
   if (!dlopenHandle)
      return;

   /* Clear any errors */
   dlerror();

   #define resolve(symbolname) do {                                      \
      *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);  \
      if (!sym_##symbolname || dlerror() != NULL)                        \
         goto dlfailure;                                                 \
   } while(0)

   resolve(sd_get_sessions);

   #undef resolve

   activeHandles++;
   return;

dlfailure:
   Platform_close_libsystemd();
   dlopenHandle = NULL;
#endif /* !BUILD_STATIC && HTOP_LINUX */
}

static void UserSessionsMeter_done(ATTR_UNUSED Meter* this) {
#if !defined(BUILD_STATIC) && defined(HTOP_LINUX)
   if (activeHandles > 0) {
      activeHandles--;
      if (activeHandles == 0) {
         Platform_close_libsystemd();
         dlopenHandle = NULL;
      }
   }
#endif /* !BUILD_STATIC && HTOP_LINUX */
}

#if (!defined(BUILD_STATIC) && defined(HTOP_LINUX)) || defined(HAVE_LIBSYSTEMD)
static int update_via_sd_login(void) {
#ifndef BUILD_STATIC
   if (!dlopenHandle)
      return -1;
#endif /* !BUILD_STATIC */

   return sym_sd_get_sessions(NULL);
}
#endif /* (!BUILD_STATIC && HTOP_LINUX) || HAVE_LIBSYSTEMD */

static void UserSessionsMeter_updateValues(Meter* this) {
   int ret;
#if (!defined(BUILD_STATIC) && defined(HTOP_LINUX)) || defined(HAVE_LIBSYSTEMD)
   ret = update_via_sd_login();
   if (ret < 0)
      ret = Generic_utmpx_get_users();
#else
   ret = Generic_utmpx_get_users();
#endif /* (!BUILD_STATIC && HTOP_LINUX) || HAVE_LIBSYSTEMD */

   users = ret;

   if (users >= 0) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%d", ret);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }
}

static void UserSessionsMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[16];

   if (users >= 0) {
      int len = xSnprintf(buffer, sizeof(buffer), "%d", users);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
   } else {
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
   }
}

static const int UserSessionsMeter_attributes[] = {
   METER_VALUE
};

const MeterClass UserSessionsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = UserSessionsMeter_display
   },
   .updateValues = UserSessionsMeter_updateValues,
   .init = UserSessionsMeter_init,
   .done = UserSessionsMeter_done,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = UserSessionsMeter_attributes,
   .name = "Users",
   .uiName = "User Sessions",
   .description = "Number of currently active user sessions",
   .caption = "User sessions: ",
};

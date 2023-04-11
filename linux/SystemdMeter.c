/*
htop - SystemdMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/SystemdMeter.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"

#if defined(BUILD_STATIC) && defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#endif


#ifdef BUILD_STATIC

#define sym_sd_bus_open_system sd_bus_open_system
#define sym_sd_bus_get_property_string sd_bus_get_property_string
#define sym_sd_bus_get_property_trivial sd_bus_get_property_trivial
#define sym_sd_bus_unref sd_bus_unref

#else

typedef void sd_bus;
typedef void sd_bus_error;
static int (*sym_sd_bus_open_system)(sd_bus**);
static int (*sym_sd_bus_open_user)(sd_bus**);
static int (*sym_sd_bus_get_property_string)(sd_bus*, const char*, const char*, const char*, const char*, sd_bus_error*, char**);
static int (*sym_sd_bus_get_property_trivial)(sd_bus*, const char*, const char*, const char*, const char*, sd_bus_error*, char, void*);
static sd_bus* (*sym_sd_bus_unref)(sd_bus*);
static void* dlopenHandle = NULL;

#endif /* BUILD_STATIC */


#define INVALID_VALUE ((unsigned int)-1)

typedef struct SystemdMeterContext {
#if !defined(BUILD_STATIC) || defined(HAVE_LIBSYSTEMD)
   sd_bus* bus;
#endif /* !BUILD_STATIC || HAVE_LIBSYSTEMD */
   char* systemState;
   unsigned int nFailedUnits;
   unsigned int nInstalledJobs;
   unsigned int nNames;
   unsigned int nJobs;
} SystemdMeterContext_t;

static SystemdMeterContext_t ctx_system;
static SystemdMeterContext_t ctx_user;

static void SystemdMeter_done(ATTR_UNUSED Meter* this) {
   SystemdMeterContext_t* ctx = String_eq(Meter_name(this), "SystemdUser") ? &ctx_user : &ctx_system;

   free(ctx->systemState);
   ctx->systemState = NULL;

#ifdef BUILD_STATIC
# ifdef HAVE_LIBSYSTEMD
   if (ctx->bus) {
      sym_sd_bus_unref(ctx->bus);
   }
   ctx->bus = NULL;
# endif /* HAVE_LIBSYSTEMD */
#else /* BUILD_STATIC */
   if (ctx->bus && dlopenHandle) {
      sym_sd_bus_unref(ctx->bus);
   }
   ctx->bus = NULL;

   if (!ctx_system.systemState && !ctx_user.systemState && dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
#endif /* BUILD_STATIC */
}

#if !defined(BUILD_STATIC) || defined(HAVE_LIBSYSTEMD)
static int updateViaLib(bool user) {
   SystemdMeterContext_t* ctx = user ? &ctx_user : &ctx_system;
#ifndef BUILD_STATIC
   if (!dlopenHandle) {
      dlopenHandle = dlopen("libsystemd.so.0", RTLD_LAZY);
      if (!dlopenHandle)
         goto dlfailure;

      /* Clear any errors */
      dlerror();

      #define resolve(symbolname) do {                                      \
         *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);  \
         if (!sym_##symbolname || dlerror() != NULL)                        \
            goto dlfailure;                                                 \
      } while(0)

      resolve(sd_bus_open_system);
      resolve(sd_bus_open_user);
      resolve(sd_bus_get_property_string);
      resolve(sd_bus_get_property_trivial);
      resolve(sd_bus_unref);

      #undef resolve
   }
#endif /* !BUILD_STATIC */

   int r;
   /* Connect to the system bus */
   if (!ctx->bus) {
      if (user) {
         r = sym_sd_bus_open_user(&ctx->bus);
      } else {
         r = sym_sd_bus_open_system(&ctx->bus);
      }
      if (r < 0)
         goto busfailure;
   }

   static const char* const busServiceName = "org.freedesktop.systemd1";
   static const char* const busObjectPath = "/org/freedesktop/systemd1";
   static const char* const busInterfaceName = "org.freedesktop.systemd1.Manager";

   r = sym_sd_bus_get_property_string(ctx->bus,
                                      busServiceName,        /* service to contact */
                                      busObjectPath,         /* object path */
                                      busInterfaceName,      /* interface name */
                                      "SystemState",         /* property name */
                                      NULL,                  /* object to return error in */
                                      &ctx->systemState);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(ctx->bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NFailedUnits",       /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &ctx->nFailedUnits);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(ctx->bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NInstalledJobs",     /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &ctx->nInstalledJobs);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(ctx->bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NNames",             /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &ctx->nNames);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(ctx->bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NJobs",              /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &ctx->nJobs);
   if (r < 0)
      goto busfailure;

   /* success */
   return 0;

busfailure:
   sym_sd_bus_unref(ctx->bus);
   ctx->bus = NULL;
   return -2;

#ifndef BUILD_STATIC
dlfailure:
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
   return -1;
#endif /* !BUILD_STATIC */
}
#endif /* !BUILD_STATIC || HAVE_LIBSYSTEMD */

static void updateViaExec(bool user) {
   SystemdMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

   if (Settings_isReadonly())
      return;

   int fdpair[2];
   if (pipe(fdpair) < 0)
      return;

   pid_t child = fork();
   if (child < 0) {
      close(fdpair[1]);
      close(fdpair[0]);
      return;
   }

   if (child == 0) {
      close(fdpair[0]);
      dup2(fdpair[1], STDOUT_FILENO);
      close(fdpair[1]);
      int fdnull = open("/dev/null", O_WRONLY);
      if (fdnull < 0)
         exit(1);
      dup2(fdnull, STDERR_FILENO);
      close(fdnull);
      // Use of NULL in variadic functions must have a pointer cast.
      // The NULL constant is not required by standard to have a pointer type.
      execlp(
         "systemctl",
         "systemctl",
         "show",
         user ? "--user" : "--system",
         "--property=SystemState",
         "--property=NFailedUnits",
         "--property=NNames",
         "--property=NJobs",
         "--property=NInstalledJobs",
         (char*)NULL);
      exit(127);
   }
   close(fdpair[1]);

   int wstatus;
   if (waitpid(child, &wstatus, 0) < 0 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
      close(fdpair[0]);
      return;
   }

   FILE* commandOutput = fdopen(fdpair[0], "r");
   if (!commandOutput) {
      close(fdpair[0]);
      return;
   }

   char lineBuffer[128];
   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      if (String_startsWith(lineBuffer, "SystemState=")) {
         char* newline = strchr(lineBuffer + strlen("SystemState="), '\n');
         if (newline) {
            *newline = '\0';
         }
         free_and_xStrdup(&ctx->systemState, lineBuffer + strlen("SystemState="));
      } else if (String_startsWith(lineBuffer, "NFailedUnits=")) {
         ctx->nFailedUnits = strtoul(lineBuffer + strlen("NFailedUnits="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NNames=")) {
         ctx->nNames = strtoul(lineBuffer + strlen("NNames="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NJobs=")) {
         ctx->nJobs = strtoul(lineBuffer + strlen("NJobs="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NInstalledJobs=")) {
         ctx->nInstalledJobs = strtoul(lineBuffer + strlen("NInstalledJobs="), NULL, 10);
      }
   }

   fclose(commandOutput);
}

static void SystemdMeter_updateValues(Meter* this) {
   bool user = String_eq(Meter_name(this), "SystemdUser");
   SystemdMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

   free(ctx->systemState);
   ctx->systemState = NULL;
   ctx->nFailedUnits = ctx->nInstalledJobs = ctx->nNames = ctx->nJobs = INVALID_VALUE;

#if !defined(BUILD_STATIC) || defined(HAVE_LIBSYSTEMD)
   if (updateViaLib(user) < 0)
      updateViaExec(user);
#else
   updateViaExec(user);
#endif /* !BUILD_STATIC || HAVE_LIBSYSTEMD */

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", ctx->systemState ? ctx->systemState : "???");
}

static int zeroDigitColor(unsigned int value) {
   switch (value) {
   case 0:
      return CRT_colors[METER_VALUE];
   case INVALID_VALUE:
      return CRT_colors[METER_VALUE_ERROR];
   default:
      return CRT_colors[METER_VALUE_NOTICE];
   }
}

static int valueDigitColor(unsigned int value) {
   switch (value) {
      case 0:
         return CRT_colors[METER_VALUE_NOTICE];
      case INVALID_VALUE:
         return CRT_colors[METER_VALUE_ERROR];
      default:
         return CRT_colors[METER_VALUE];
   }
}


static void _SystemdMeter_display(ATTR_UNUSED const Object* cast, RichString* out, SystemdMeterContext_t* ctx) {
   char buffer[16];
   int len;
   int color = METER_VALUE_ERROR;

   if (ctx->systemState) {
      color = String_eq(ctx->systemState, "running") ? METER_VALUE_OK :
              String_eq(ctx->systemState, "degraded") ? METER_VALUE_ERROR : METER_VALUE_WARN;
   }
   RichString_writeAscii(out, CRT_colors[color], ctx->systemState ? ctx->systemState : "N/A");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");

   if (ctx->nFailedUnits == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
      len = 1;
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%u", ctx->nFailedUnits);
   }
   RichString_appendnAscii(out, zeroDigitColor(ctx->nFailedUnits), buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");

   if (ctx->nNames == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
      len = 1;
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%u", ctx->nNames);
   }
   RichString_appendnAscii(out, valueDigitColor(ctx->nNames), buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " failed) (");

   if (ctx->nJobs == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
      len = 1;
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%u", ctx->nJobs);
   }
   RichString_appendnAscii(out, zeroDigitColor(ctx->nJobs), buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");

   if (ctx->nInstalledJobs == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
      len = 1;
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%u", ctx->nInstalledJobs);
   }
   RichString_appendnAscii(out, valueDigitColor(ctx->nInstalledJobs), buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " jobs)");
}

static void SystemdMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   _SystemdMeter_display(cast, out, &ctx_system);
}

static void SystemdUserMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   _SystemdMeter_display(cast, out, &ctx_user);
}

static const int SystemdMeter_attributes[] = {
   METER_VALUE
};

const MeterClass SystemdMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SystemdMeter_display
   },
   .updateValues = SystemdMeter_updateValues,
   .done = SystemdMeter_done,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SystemdMeter_attributes,
   .name = "Systemd",
   .uiName = "Systemd state",
   .description = "Systemd system state and unit overview",
   .caption = "Systemd: ",
};

const MeterClass SystemdUserMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SystemdUserMeter_display
   },
   .updateValues = SystemdMeter_updateValues,
   .done = SystemdMeter_done,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SystemdMeter_attributes,
   .name = "SystemdUser",
   .uiName = "Systemd user state",
   .description = "Systemd user state and unit overview",
   .caption = "Systemd User: ",
};

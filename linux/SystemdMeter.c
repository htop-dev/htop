/*
htop - SystemdMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "SystemdMeter.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "RichString.h"
#include "XUtils.h"


#define INVALID_VALUE ((unsigned int)-1)

typedef void sd_bus;
typedef void sd_bus_error;
static int (*sym_sd_bus_open_system)(sd_bus**);
static int (*sym_sd_bus_get_property_string)(sd_bus*, const char*, const char*, const char*, const char*, sd_bus_error*, char**);
static int (*sym_sd_bus_get_property_trivial)(sd_bus*, const char*, const char*, const char*, const char*, sd_bus_error*, char, void*);
static sd_bus* (*sym_sd_bus_unref)(sd_bus*);

static char* systemState = NULL;
static unsigned int nFailedUnits = INVALID_VALUE;
static unsigned int nInstalledJobs = INVALID_VALUE;
static unsigned int nNames = INVALID_VALUE;
static unsigned int nJobs = INVALID_VALUE;
static void* dlopenHandle = NULL;
static sd_bus* bus = NULL;

static void SystemdMeter_done(ATTR_UNUSED Meter* this) {
   free(systemState);
   systemState = NULL;

   if (bus && dlopenHandle) {
      sym_sd_bus_unref(bus);
   }
   bus = NULL;

   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
}

static int updateViaLib(void) {
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
      resolve(sd_bus_get_property_string);
      resolve(sd_bus_get_property_trivial);
      resolve(sd_bus_unref);

      #undef resolve
   }

   int r;

   /* Connect to the system bus */
   if (!bus) {
      r = sym_sd_bus_open_system(&bus);
      if (r < 0)
         goto busfailure;
   }

   static const char* const busServiceName = "org.freedesktop.systemd1";
   static const char* const busObjectPath = "/org/freedesktop/systemd1";
   static const char* const busInterfaceName = "org.freedesktop.systemd1.Manager";

   r = sym_sd_bus_get_property_string(bus,
                                      busServiceName,        /* service to contact */
                                      busObjectPath,         /* object path */
                                      busInterfaceName,      /* interface name */
                                      "SystemState",         /* property name */
                                      NULL,                  /* object to return error in */
                                      &systemState);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NFailedUnits",       /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &nFailedUnits);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NInstalledJobs",     /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &nInstalledJobs);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NNames",             /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &nNames);
   if (r < 0)
      goto busfailure;

   r = sym_sd_bus_get_property_trivial(bus,
                                       busServiceName,       /* service to contact */
                                       busObjectPath,        /* object path */
                                       busInterfaceName,     /* interface name */
                                       "NJobs",              /* property name */
                                       NULL,                 /* object to return error in */
                                       'u',                  /* property type */
                                       &nJobs);
   if (r < 0)
      goto busfailure;

   /* success */
   return 0;

busfailure:
   sym_sd_bus_unref(bus);
   bus = NULL;
   return -2;

dlfailure:
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
   return -1;
}

static void updateViaExec(void) {
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
      execl("/bin/systemctl",
            "/bin/systemctl",
            "show",
            "--property=SystemState",
            "--property=NFailedUnits",
            "--property=NNames",
            "--property=NJobs",
            "--property=NInstalledJobs",
            NULL);
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
         free(systemState);
         systemState = xStrdup(lineBuffer + strlen("SystemState="));
      } else if (String_startsWith(lineBuffer, "NFailedUnits=")) {
         nFailedUnits = strtoul(lineBuffer + strlen("NFailedUnits="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NNames=")) {
         nNames = strtoul(lineBuffer + strlen("NNames="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NJobs=")) {
         nJobs = strtoul(lineBuffer + strlen("NJobs="), NULL, 10);
      } else if (String_startsWith(lineBuffer, "NInstalledJobs=")) {
         nInstalledJobs = strtoul(lineBuffer + strlen("NInstalledJobs="), NULL, 10);
      }
   }

   fclose(commandOutput);
}

static void SystemdMeter_updateValues(ATTR_UNUSED Meter* this, char* buffer, size_t size) {
   free(systemState);
   systemState = NULL;
   nFailedUnits = nInstalledJobs = nNames = nJobs = INVALID_VALUE;

   if (updateViaLib() < 0)
      updateViaExec();

   xSnprintf(buffer, size, "%s", systemState ? systemState : "???");
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


static void SystemdMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[16];

   int color = (systemState && 0 == strcmp(systemState, "running")) ? METER_VALUE_OK : METER_VALUE_ERROR;
   RichString_write(out, CRT_colors[color], systemState ? systemState : "???");

   RichString_append(out, CRT_colors[METER_TEXT], " (");

   if (nFailedUnits == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", nFailedUnits);
   }
   RichString_append(out, zeroDigitColor(nFailedUnits), buffer);

   RichString_append(out, CRT_colors[METER_TEXT], "/");

   if (nNames == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", nNames);
   }
   RichString_append(out, valueDigitColor(nNames), buffer);

   RichString_append(out, CRT_colors[METER_TEXT], " failed) (");

   if (nJobs == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", nJobs);
   }
   RichString_append(out, zeroDigitColor(nJobs), buffer);

   RichString_append(out, CRT_colors[METER_TEXT], "/");

   if (nInstalledJobs == INVALID_VALUE) {
      buffer[0] = '?';
      buffer[1] = '\0';
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", nInstalledJobs);
   }
   RichString_append(out, valueDigitColor(nInstalledJobs), buffer);

   RichString_append(out, CRT_colors[METER_TEXT], " jobs)");
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

/*
htop - OpenRCMeter.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/OpenRCMeter.h"

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

#define INVALID_VALUE ((size_t)-1)

typedef struct OpenRCMeterContext {
   char* runlevel;
   size_t services_stopped;
   size_t services_started;
} OpenRCMeterContext_t;

static OpenRCMeterContext_t ctx_system;
static OpenRCMeterContext_t ctx_user;

static void OpenRCMeter_done(ATTR_UNUSED Meter* this) {
   OpenRCMeterContext_t* ctx = String_eq(Meter_name(this), "OpenRCUser") ? &ctx_user : &ctx_system;

   free(ctx->runlevel);
   ctx->runlevel = NULL;
}

ATTR_NONNULL_N(3) ATTR_ACCESS2_W(3)
static int OpenRCMeter_execRcStatus(bool user, bool full, pid_t* childPid) {
   if (user) {
      const char* xdg = getenv("XDG_RUNTIME_DIR");
      if (!xdg || !*xdg)
         return -1;
   }

   int fdpair[2] = {-1, -1};
   if (pipe(fdpair) < 0)
      return -1;

   pid_t child = fork();
   if (child < 0) {
      close(fdpair[1]);
      close(fdpair[0]);
      return -1;
   }

   if (child == 0) {
      close(fdpair[0]);
      dup2(fdpair[1], STDOUT_FILENO);
      close(fdpair[1]);
      int fdnull = open("/dev/null", O_WRONLY);
      if (fdnull < 0)
         _exit(1);
      dup2(fdnull, STDERR_FILENO);
      close(fdnull);

      if (user) {
         if (full) {
            execlp("rc-status", "rc-status", "-C", "--user", "-f", "ini", "-a", (char*)NULL);
         } else {
            execlp("rc-status", "rc-status", "-C", "--user", "-r", (char*)NULL);
         }
      } else {
         if (full) {
            execlp("rc-status", "rc-status", "-C", "-f", "ini", "-a", (char*)NULL);
         } else {
            execlp("rc-status", "rc-status", "-C", "-r", (char*)NULL);
         }
      }
      _exit(127);
   }

   *childPid = child;

   close(fdpair[1]);
   return fdpair[0];
}

static void OpenRCMeter_updateViaExec(bool user) {
   OpenRCMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

   ctx->services_started = INVALID_VALUE;
   ctx->services_stopped = INVALID_VALUE;

   if (Settings_isReadonly())
      return;

   char lineBuffer[1024];

   pid_t child;
   int fd = OpenRCMeter_execRcStatus(user, false, &child);
   if (fd < 0)
      return;

   FILE* commandOutput = fdopen(fd, "r");
   if (!commandOutput) {
      close(fd);
      xWaitpid(child, NULL, 0, false);
      return;
   }

   if (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      char* newline = strchr(lineBuffer, '\n');
      if (newline)
         *newline = '\0';

      free_and_xStrdup(&ctx->runlevel, lineBuffer);
   }
   fclose(commandOutput);

   int wstatus;
   if (xWaitpid(child, &wstatus, 0, false) < 0 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
      return;

   fd = OpenRCMeter_execRcStatus(user, true, &child);
   if (fd < 0)
      return;

   commandOutput = fdopen(fd, "r");
   if (!commandOutput) {
      close(fd);
      xWaitpid(child, NULL, 0, false);
      return;
   }

   ctx->services_started = 0;
   ctx->services_stopped = 0;

   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      char* equals = strchr(lineBuffer, '=');
      if (!equals)
         continue;

      char* status = equals + 1;
      while (*status == ' ' || *status == '\t')
         status++;

      char* newline = strchr(status, '\n');
      if (newline)
         *newline = '\0';

      if (strstr(status, "started")) {
         ctx->services_started++;
      } else if (strstr(status, "stopped")) {
         ctx->services_stopped++;
      }
   }

   fclose(commandOutput);

   if (xWaitpid(child, &wstatus, 0, false) < 0 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
      ctx->services_started = INVALID_VALUE;
      ctx->services_stopped = INVALID_VALUE;
   }
}

static void OpenRCMeter_updateValues(Meter* this) {
   bool user = String_eq(Meter_name(this), "OpenRCUser");
   OpenRCMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

   free(ctx->runlevel);
   ctx->runlevel = NULL;
   ctx->services_stopped = INVALID_VALUE;
   ctx->services_started = INVALID_VALUE;

   OpenRCMeter_updateViaExec(user);

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", ctx->runlevel ? ctx->runlevel : "???");
}

static void OpenRCMeter_display(ATTR_UNUSED const Object* cast, RichString* out, OpenRCMeterContext_t* ctx) {
   char buffer[32];

   RichString_writeAscii(out, CRT_colors[METER_TEXT], "Runlevel: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], ctx->runlevel ? ctx->runlevel : "N/A");

   if (ctx->services_started == INVALID_VALUE && ctx->services_stopped == INVALID_VALUE) {
      return;
   }

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");

   if (ctx->services_started == INVALID_VALUE) {
      xSnprintf(buffer, sizeof(buffer), "?");
   } else {
      xSnprintf(buffer, sizeof(buffer), "%zu", ctx->services_started);
   }
   RichString_appendAscii(out, CRT_colors[METER_VALUE_OK], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " started, ");

   if (ctx->services_stopped == INVALID_VALUE) {
      xSnprintf(buffer, sizeof(buffer), "?");
   } else {
      xSnprintf(buffer, sizeof(buffer), "%zu", ctx->services_stopped);
   }
   RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " stopped)");
}

static void OpenRCMeter_display_system(ATTR_UNUSED const Object* cast, RichString* out) {
   OpenRCMeter_display(cast, out, &ctx_system);
}

static void OpenRCMeter_display_user(ATTR_UNUSED const Object* cast, RichString* out) {
   OpenRCMeter_display(cast, out, &ctx_user);
}

static const int OpenRCMeter_attributes[] = {
   METER_VALUE
};

const MeterClass OpenRCMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = OpenRCMeter_display_system,
   },
   .updateValues = OpenRCMeter_updateValues,
   .done = OpenRCMeter_done,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = OpenRCMeter_attributes,
   .name = "OpenRC",
   .uiName = "OpenRC state",
   .description = "OpenRC system state and service overview",
   .caption = "OpenRC: ",
};

const MeterClass OpenRCUserMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = OpenRCMeter_display_user,
   },
   .updateValues = OpenRCMeter_updateValues,
   .done = OpenRCMeter_done,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = OpenRCMeter_attributes,
   .name = "OpenRCUser",
   .uiName = "OpenRC user state",
   .description = "OpenRC user state and service overview",
   .caption = "OpenRC User: ",
};

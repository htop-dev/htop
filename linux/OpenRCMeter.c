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

#define INVALID_VALUE ((unsigned int)-1)

typedef struct OpenRCMeterContext {
   char* runlevel;
   unsigned int services_stopped;
   unsigned int services_started;
} OpenRCMeterContext_t;

static OpenRCMeterContext_t ctx_system;
static OpenRCMeterContext_t ctx_user;

static void OpenRCMeter_done(ATTR_UNUSED Meter* this) {
   OpenRCMeterContext_t* ctx = String_eq(Meter_name(this), "OpenRCUser") ? &ctx_user : &ctx_system;

   free(ctx->runlevel);
   ctx->runlevel = NULL;
}

static void updateViaExec(bool user) {
   OpenRCMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

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
      if (user) {
         execlp("rc-status", "rc-status", "--user", "-a", (char*)NULL);
      } else {
         execlp("rc-status", "rc-status", "-a", (char*)NULL);
      }
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

   ctx->services_started = 0;
   ctx->services_stopped = 0;

   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      if (String_startsWith(lineBuffer, "Runlevel: ")) {
         char* newline = strchr(lineBuffer + strlen("Runlevel: "), '\n');
         if (newline) {
            *newline = '\0';
         }
         free_and_xStrdup(&ctx->runlevel, lineBuffer + strlen("Runlevel: "));
      } else {
         if (strstr(lineBuffer, "[") && strstr(lineBuffer, "started") && strstr(lineBuffer, "]")) {
            ctx->services_started++;
         } else if (strstr(lineBuffer, "[") && strstr(lineBuffer, "stopped") && strstr(lineBuffer, "]")) {
            ctx->services_stopped++;
         }
      }
   }

   fclose(commandOutput);
}

static void OpenRCMeter_updateValues(Meter* this) {
   bool user = String_eq(Meter_name(this), "OpenRCUser");
   OpenRCMeterContext_t* ctx = user ? &ctx_user : &ctx_system;

   free(ctx->runlevel);
   ctx->runlevel = NULL;
   ctx->services_stopped = INVALID_VALUE;
   ctx->services_started = INVALID_VALUE;

   updateViaExec(user);

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", ctx->runlevel ? ctx->runlevel : "???");
}

static void OpenRCMeter_display(ATTR_UNUSED const Object* cast, RichString* out, OpenRCMeterContext_t* ctx) {
   char buffer[32];

   RichString_writeAscii(out, CRT_colors[METER_TEXT], "Runlevel: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], ctx->runlevel ? ctx->runlevel : "N/A");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");

   if (ctx->services_started == INVALID_VALUE) {
      xSnprintf(buffer, sizeof(buffer), "?");
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", ctx->services_started);
   }
   RichString_appendAscii(out, CRT_colors[METER_VALUE_OK], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " started, ");

   if (ctx->services_stopped == INVALID_VALUE) {
      xSnprintf(buffer, sizeof(buffer), "?");
   } else {
      xSnprintf(buffer, sizeof(buffer), "%u", ctx->services_stopped);
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

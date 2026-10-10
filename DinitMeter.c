/*
htop - DinitMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DinitMeter.h"

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

typedef struct DinitMeterContext {
   size_t servicesStarted;
   size_t servicesStopped;
   size_t servicesFailed;
} DinitMeterContext_t;

static DinitMeterContext_t ctx_dinit;

/*
 * `dinitctl list` outputs lines like:
 *   [{+}     ] boot
 *   [{ }<<   ] starting
 *   [   >>{ }] stopping
 *   [     {-}] stopped
 *   [     {X}] bad (failed)
 *
 * The 9-character bracketed state field:
 *   line[2] == '+' indicates started (or 's' for skipped)
 *   line[7] == 'X' indicates failed to start / bad exit
 *   otherwise indicates stopped / transitioning
 */
static void DinitMeter_updateValues(Meter* this) {
   ctx_dinit.servicesStarted = INVALID_VALUE;
   ctx_dinit.servicesStopped = INVALID_VALUE;
   ctx_dinit.servicesFailed = INVALID_VALUE;

   if (Settings_isReadonly())
      goto done;

   /* Check if dinit is present on the system */
   if (access("/sbin/dinit", F_OK) != 0 &&
       access("/usr/sbin/dinit", F_OK) != 0 &&
       access("/usr/bin/dinit", F_OK) != 0 &&
       access("/usr/local/sbin/dinit", F_OK) != 0 &&
       access("/usr/local/bin/dinit", F_OK) != 0 &&
       access("/dev/dinitctl", F_OK) != 0 &&
       access("/run/dinitctl", F_OK) != 0)
      goto done;

   int fdpair[2] = {-1, -1};
   if (pipe(fdpair) < 0)
      goto done;

   pid_t child = fork();
   if (child < 0) {
      close(fdpair[1]);
      close(fdpair[0]);
      goto done;
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
      execlp("dinitctl", "dinitctl", "list", (char*)NULL);
      _exit(127);
   }

   close(fdpair[1]);

   FILE* commandOutput = fdopen(fdpair[0], "r");
   if (!commandOutput) {
      close(fdpair[0]);
      xWaitpid(child, NULL, 0, false);
      goto done;
   }

   size_t started = 0;
   size_t stopped = 0;
   size_t failed = 0;
   bool hasOutput = false;

   char lineBuffer[512];
   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      size_t len = strlen(lineBuffer);
      if (len < 10 || lineBuffer[0] != '[' || lineBuffer[9] != ']')
         continue;

      hasOutput = true;
      if (lineBuffer[2] == '+') {
         started++;
      } else if (lineBuffer[7] == 'X') {
         failed++;
      } else {
         stopped++;
      }
   }
   fclose(commandOutput);

   int wstatus;
   if (xWaitpid(child, &wstatus, 0, false) >= 0 && WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0 && hasOutput) {
      ctx_dinit.servicesStarted = started;
      ctx_dinit.servicesStopped = stopped;
      ctx_dinit.servicesFailed = failed;
   }

done:
   if (ctx_dinit.servicesStarted != INVALID_VALUE) {
      if (ctx_dinit.servicesFailed > 0) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
            "%zu started, %zu stopped, %zu failed",
            ctx_dinit.servicesStarted, ctx_dinit.servicesStopped, ctx_dinit.servicesFailed);
      } else {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
            "%zu started, %zu stopped",
            ctx_dinit.servicesStarted, ctx_dinit.servicesStopped);
      }
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }
}

static void DinitMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[32];

   if (ctx_dinit.servicesStarted == INVALID_VALUE) {
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "N/A");
      return;
   }

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_dinit.servicesStarted);
   RichString_writeAscii(out, CRT_colors[METER_VALUE_OK], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " started, ");

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_dinit.servicesStopped);
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " stopped");

   if (ctx_dinit.servicesFailed > 0) {
      xSnprintf(buffer, sizeof(buffer), ", %zu failed", ctx_dinit.servicesFailed);
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], buffer);
   }
}

static const int DinitMeter_attributes[] = {
   METER_VALUE
};

const MeterClass DinitMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DinitMeter_display,
   },
   .updateValues = DinitMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = DinitMeter_attributes,
   .name = "Dinit",
   .uiName = "dinit services",
   .description = "dinit service manager state overview",
   .caption = "dinit: ",
};

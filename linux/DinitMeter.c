/*
htop - linux/DinitMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/DinitMeter.h"

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
 *   [[+]   ] boot
 *   [[ ]   ] sshd
 *   [{  }  ] network
 *
 * Column 1 (inside first brackets): '+' = started, ' ' = stopped, '-' = stopping,
 * '{' = starting, '}' = stopping, '!' = failed.
 *
 * We use the machine-readable `dinitctl --list` format:
 *   started boot
 *   stopped sshd
 *   failed myservice
 */
static void DinitMeter_updateValues(Meter* this) {
   ctx_dinit.servicesStarted = INVALID_VALUE;
   ctx_dinit.servicesStopped = INVALID_VALUE;
   ctx_dinit.servicesFailed = INVALID_VALUE;

   if (Settings_isReadonly())
      goto done;

   /* Check dinit is present */
   if (access("/sbin/dinit", F_OK) != 0 &&
       access("/usr/sbin/dinit", F_OK) != 0 &&
       access("/usr/bin/dinit", F_OK) != 0)
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

   ctx_dinit.servicesStarted = 0;
   ctx_dinit.servicesStopped = 0;
   ctx_dinit.servicesFailed = 0;

   /*
    * Parse the human-readable dinitctl list output.
    * Lines look like:
    *   [[+]   ] servicename
    *   [[ ]   ] servicename
    *   [[!]   ] servicename   <- failed
    *
    * Character at index 2 is the state indicator:
    *   '+' = started, ' ' = stopped, '-' = stopping/starting,
    *   '{' = starting, '}' = stopping, '!' = failed/error
    */
   char lineBuffer[512];
   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      /* Skip lines that don't start with '[' */
      if (lineBuffer[0] != '[')
         continue;

      char stateChar = lineBuffer[2];
      if (stateChar == '+') {
         ctx_dinit.servicesStarted++;
      } else if (stateChar == '!') {
         ctx_dinit.servicesFailed++;
         ctx_dinit.servicesStopped++;
      } else {
         ctx_dinit.servicesStopped++;
      }
   }
   fclose(commandOutput);
   xWaitpid(child, NULL, 0, false);

done:
   if (ctx_dinit.servicesStarted != INVALID_VALUE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
         "%zu started, %zu failed",
         ctx_dinit.servicesStarted, ctx_dinit.servicesFailed);
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

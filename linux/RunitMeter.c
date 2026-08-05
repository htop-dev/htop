/*
htop - linux/RunitMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/RunitMeter.h"

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

typedef struct RunitMeterContext {
   size_t servicesUp;
   size_t servicesDown;
   size_t servicesFinish;
} RunitMeterContext_t;

static RunitMeterContext_t ctx_runit;

/*
 * sv(8) outputs lines like:
 *   run: sshd: (pid 1234) 3600s
 *   down: cron: 42s, normally up
 *   finish: foo: ...
 *
 * We run: sv status /var/service/[name...]
 * The exit status is non-zero when any service is down, so we ignore it.
 */
static void RunitMeter_updateValues(Meter* this) {
   ctx_runit.servicesUp = INVALID_VALUE;
   ctx_runit.servicesDown = INVALID_VALUE;
   ctx_runit.servicesFinish = INVALID_VALUE;

   if (Settings_isReadonly())
      goto done;

   /* Check that the runit service directory exists */
   if (access("/var/service", F_OK) != 0 && access("/service", F_OK) != 0)
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
      /* Try /var/service first, fall back to /service */
      if (access("/var/service", F_OK) == 0) {
         execlp("sv", "sv", "status", "/var/service/.", (char*)NULL);
      } else {
         execlp("sv", "sv", "status", "/service/.", (char*)NULL);
      }
      _exit(127);
   }

   close(fdpair[1]);

   FILE* commandOutput = fdopen(fdpair[0], "r");
   if (!commandOutput) {
      close(fdpair[0]);
      xWaitpid(child, NULL, 0, false);
      goto done;
   }

   ctx_runit.servicesUp = 0;
   ctx_runit.servicesDown = 0;
   ctx_runit.servicesFinish = 0;

   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), commandOutput)) {
      if (String_startsWith(lineBuffer, "run: ")) {
         ctx_runit.servicesUp++;
      } else if (String_startsWith(lineBuffer, "down: ")) {
         ctx_runit.servicesDown++;
      } else if (String_startsWith(lineBuffer, "finish: ")) {
         ctx_runit.servicesFinish++;
      }
   }
   fclose(commandOutput);
   xWaitpid(child, NULL, 0, false);

done:
   if (ctx_runit.servicesUp != INVALID_VALUE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
         "%zu up, %zu down", ctx_runit.servicesUp, ctx_runit.servicesDown);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }
}

static void RunitMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[32];

   if (ctx_runit.servicesUp == INVALID_VALUE) {
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "N/A");
      return;
   }

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_runit.servicesUp);
   RichString_writeAscii(out, CRT_colors[METER_VALUE_OK], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " up, ");

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_runit.servicesDown);
   int downColor = (ctx_runit.servicesDown > 0) ? METER_VALUE_ERROR : METER_VALUE;
   RichString_appendAscii(out, CRT_colors[downColor], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " down");

   if (ctx_runit.servicesFinish > 0) {
      xSnprintf(buffer, sizeof(buffer), ", %zu finishing", ctx_runit.servicesFinish);
      RichString_appendAscii(out, CRT_colors[METER_VALUE_NOTICE], buffer);
   }
}

static const int RunitMeter_attributes[] = {
   METER_VALUE
};

const MeterClass RunitMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = RunitMeter_display,
   },
   .updateValues = RunitMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = RunitMeter_attributes,
   .name = "Runit",
   .uiName = "runit services",
   .description = "runit service supervisor state overview",
   .caption = "runit: ",
};

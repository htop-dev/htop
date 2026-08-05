/*
htop - linux/SysVInitMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/SysVInitMeter.h"

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

typedef struct SysVInitMeterContext {
   char runlevel;       /* current runlevel character, '\0' if unknown */
   char prevRunlevel;   /* previous runlevel character, 'N' means none */
} SysVInitMeterContext_t;

static SysVInitMeterContext_t ctx_sysvInit;

static void SysVInitMeter_updateValues(Meter* this) {
   ctx_sysvInit.runlevel = '\0';
   ctx_sysvInit.prevRunlevel = 'N';

   if (Settings_isReadonly())
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
      execlp("runlevel", "runlevel", (char*)NULL);
      _exit(127);
   }

   close(fdpair[1]);

   int wstatus;
   if (xWaitpid(child, &wstatus, 0, false) < 0 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
      close(fdpair[0]);
      goto done;
   }

   FILE* commandOutput = fdopen(fdpair[0], "r");
   if (!commandOutput) {
      close(fdpair[0]);
      goto done;
   }

   /* runlevel outputs "PREV CURRENT\n", e.g. "N 3" or "3 5" */
   char prev = '\0', cur = '\0';
   if (fscanf(commandOutput, "%c %c", &prev, &cur) == 2) {
      ctx_sysvInit.prevRunlevel = prev;
      ctx_sysvInit.runlevel = cur;
   }
   fclose(commandOutput);

done:
   if (ctx_sysvInit.runlevel != '\0') {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%c", ctx_sysvInit.runlevel);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "???");
   }
}

static void SysVInitMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   RichString_writeAscii(out, CRT_colors[METER_TEXT], "Runlevel: ");

   if (ctx_sysvInit.runlevel != '\0') {
      char buf[4];
      xSnprintf(buf, sizeof(buf), "%c", ctx_sysvInit.runlevel);
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buf);
   } else {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "N/A");
   }

   if (ctx_sysvInit.prevRunlevel != '\0' && ctx_sysvInit.prevRunlevel != 'N') {
      char buf[16];
      xSnprintf(buf, sizeof(buf), " (prev: %c)", ctx_sysvInit.prevRunlevel);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], buf);
   }
}

static const int SysVInitMeter_attributes[] = {
   METER_VALUE
};

const MeterClass SysVInitMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SysVInitMeter_display,
   },
   .updateValues = SysVInitMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = SysVInitMeter_attributes,
   .name = "SysVInit",
   .uiName = "SysV Init runlevel",
   .description = "SysV Init current runlevel",
   .caption = "SysVInit: ",
};

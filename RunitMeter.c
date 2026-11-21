/*
htop - RunitMeter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RunitMeter.h"

#include <dirent.h>
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

/* Common runit scan directories, tried in order */
static const char* const runitScanDirs[] = {
   "/var/service",
   "/service",
   "/etc/service",
   "/var/run/runit/runsvdir/current",
   NULL,
};

static const char* RunitMeter_findScanDir(void) {
   const char* svdir = getenv("SVDIR");
   if (svdir && *svdir && access(svdir, F_OK) == 0)
      return svdir;

   for (int i = 0; runitScanDirs[i]; i++) {
      if (access(runitScanDirs[i], F_OK) == 0)
         return runitScanDirs[i];
   }
   return NULL;
}

static int RunitMeter_queryService(const char* servicePath, char* buffer, int bufferSize) {
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
      execlp("sv", "sv", "status", servicePath, (char*)NULL);
      _exit(127);
   }

   close(fdpair[1]);

   FILE* fp = fdopen(fdpair[0], "r");
   if (!fp) {
      close(fdpair[0]);
      xWaitpid(child, NULL, 0, false);
      return -1;
   }

   buffer[0] = '\0';
   if (!fgets(buffer, bufferSize, fp))
      buffer[0] = '\0';

   fclose(fp);
   xWaitpid(child, NULL, 0, false);
   return 0;
}

/*
 * sv(8) outputs lines like:
 *   run: sshd: (pid 1234) 3600s
 *   down: cron: 42s, normally up
 *   finish: foo: ...
 *
 * We iterate service directories and run sv status for each.
 */
static void RunitMeter_updateValues(Meter* this) {
   ctx_runit.servicesUp = INVALID_VALUE;
   ctx_runit.servicesDown = INVALID_VALUE;
   ctx_runit.servicesFinish = INVALID_VALUE;

   if (Settings_isReadonly())
      goto done;

   const char* scanDir = RunitMeter_findScanDir();
   if (!scanDir)
      goto done;

   DIR* dir = opendir(scanDir);
   if (!dir)
      goto done;

   size_t servicesUp = 0;
   size_t servicesDown = 0;
   size_t servicesFinish = 0;
   bool foundAny = false;

   struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] == '.')
         continue;

      char servicePath[512];
      xSnprintf(servicePath, sizeof(servicePath), "%s/%s", scanDir, entry->d_name);

      char supervisePath[540];
      xSnprintf(supervisePath, sizeof(supervisePath), "%s/supervise", servicePath);
      if (access(supervisePath, F_OK) != 0)
         continue;

      char lineBuffer[256];
      if (RunitMeter_queryService(servicePath, lineBuffer, sizeof(lineBuffer)) < 0)
         continue;

      if (String_startsWith(lineBuffer, "run: ")) {
         servicesUp++;
         foundAny = true;
      } else if (String_startsWith(lineBuffer, "down: ")) {
         servicesDown++;
         foundAny = true;
      } else if (String_startsWith(lineBuffer, "finish: ")) {
         servicesFinish++;
         foundAny = true;
      }
   }
   closedir(dir);

   if (foundAny) {
      ctx_runit.servicesUp = servicesUp;
      ctx_runit.servicesDown = servicesDown;
      ctx_runit.servicesFinish = servicesFinish;
   }

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

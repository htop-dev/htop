/*
htop - S6Meter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "S6Meter.h"

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

/* Common s6 scan directories, tried in order */
static const char* const s6ScanDirs[] = {
   "/run/s6/services",
   "/run/s6-rc/services",
   "/var/run/s6/services",
   "/var/run/s6-rc/services",
   "/etc/s6/services",
   NULL,
};

typedef struct S6MeterContext {
   size_t servicesUp;
   size_t servicesDown;
} S6MeterContext_t;

static S6MeterContext_t ctx_s6;

static const char* S6Meter_findScanDir(void) {
   const char* envDir = getenv("S6_SERVICE_DIR");
   if (envDir && *envDir && access(envDir, F_OK) == 0)
      return envDir;

   for (int i = 0; s6ScanDirs[i]; i++) {
      if (access(s6ScanDirs[i], F_OK) == 0)
         return s6ScanDirs[i];
   }
   return NULL;
}

static int S6Meter_queryService(const char* servicePath, char* buffer, int bufferSize) {
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
      execlp("s6-svstat", "s6-svstat", servicePath, (char*)NULL);
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

   int wstatus;
   if (xWaitpid(child, &wstatus, 0, false) < 0 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
      return -1;

   return 0;
}

/*
 * s6-svstat outputs one line per invocation:
 *   up (pid 1234) 3600 seconds
 *   down 42 seconds, normally up, ready 42 seconds
 *
 * We iterate service directories and run s6-svstat for each.
 */
static void S6Meter_updateValues(Meter* this) {
   ctx_s6.servicesUp = INVALID_VALUE;
   ctx_s6.servicesDown = INVALID_VALUE;

   if (Settings_isReadonly())
      goto done;

   const char* scanDir = S6Meter_findScanDir();
   if (!scanDir)
      goto done;

   DIR* dir = opendir(scanDir);
   if (!dir)
      goto done;

   size_t servicesUp = 0;
   size_t servicesDown = 0;
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
      if (S6Meter_queryService(servicePath, lineBuffer, sizeof(lineBuffer)) < 0)
         continue;

      if (String_startsWith(lineBuffer, "up")) {
         servicesUp++;
         foundAny = true;
      } else if (String_startsWith(lineBuffer, "down")) {
         servicesDown++;
         foundAny = true;
      }
   }
   closedir(dir);

   if (foundAny) {
      ctx_s6.servicesUp = servicesUp;
      ctx_s6.servicesDown = servicesDown;
   }

done:
   if (ctx_s6.servicesUp != INVALID_VALUE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
         "%zu up, %zu down", ctx_s6.servicesUp, ctx_s6.servicesDown);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }
}

static void S6Meter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[32];

   if (ctx_s6.servicesUp == INVALID_VALUE) {
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "N/A");
      return;
   }

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_s6.servicesUp);
   RichString_writeAscii(out, CRT_colors[METER_VALUE_OK], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " up, ");

   xSnprintf(buffer, sizeof(buffer), "%zu", ctx_s6.servicesDown);
   int downColor = (ctx_s6.servicesDown > 0) ? METER_VALUE_ERROR : METER_VALUE;
   RichString_appendAscii(out, CRT_colors[downColor], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " down");
}

static const int S6Meter_attributes[] = {
   METER_VALUE
};

const MeterClass S6Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = S6Meter_display,
   },
   .updateValues = S6Meter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = S6Meter_attributes,
   .name = "S6",
   .uiName = "s6 services",
   .description = "s6 service supervisor state overview",
   .caption = "s6: ",
};

/*
htop - linux/S6Meter.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/S6Meter.h"

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
   "/etc/s6/services",
   NULL,
};

typedef struct S6MeterContext {
   size_t servicesUp;
   size_t servicesDown;
} S6MeterContext_t;

static S6MeterContext_t ctx_s6;

static const char* S6Meter_findScanDir(void) {
   for (int i = 0; s6ScanDirs[i]; i++) {
      if (access(s6ScanDirs[i], F_OK) == 0)
         return s6ScanDirs[i];
   }
   return NULL;
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

   ctx_s6.servicesUp = 0;
   ctx_s6.servicesDown = 0;

   struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] == '.')
         continue;

      /* Build path: scanDir + "/" + entry->d_name + "/supervise/stat" */
      char statPath[512];
      xSnprintf(statPath, sizeof(statPath), "%s/%s/supervise/stat", scanDir, entry->d_name);

      FILE* statFile = fopen(statPath, "r");
      if (!statFile)
         continue;

      char line[64];
      if (fgets(line, sizeof(line), statFile)) {
         if (String_startsWith(line, "up")) {
            ctx_s6.servicesUp++;
         } else {
            ctx_s6.servicesDown++;
         }
      }
      fclose(statFile);
   }
   closedir(dir);

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

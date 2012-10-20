/*
htop - Settings.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"

#include "String.h"
#include "Vector.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_DELAY 15

/*{
#include "ProcessList.h"
#include "Header.h"
#include <stdbool.h>

typedef struct Settings_ {
   char* userSettings;
   ProcessList* pl;
   Header* header;
   int colorScheme;
   bool changed;
   int delay;
} Settings;

}*/

void Settings_delete(Settings* this) {
   free(this->userSettings);
   free(this);
}

static void Settings_readMeters(Settings* this, char* line, HeaderSide side) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   for (int i = 0; ids[i]; i++) {
      Header_createMeter(this->header, ids[i], side);
   }
   String_freeArray(ids);
}

static void Settings_readMeterModes(Settings* this, char* line, HeaderSide side) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   for (int i = 0; ids[i]; i++) {
      int mode = atoi(ids[i]);
      Header_setMode(this->header, i, mode, side);
   }
   String_freeArray(ids);
}

static bool Settings_read(Settings* this, char* fileName, int cpuCount) {
   FILE* fd = fopen(fileName, "r");
   if (!fd)
      return false;
   
   const int maxLine = 2048;
   char buffer[maxLine];
   bool readMeters = false;
   while (fgets(buffer, maxLine, fd)) {
      int nOptions;
      char** option = String_split(buffer, '=', &nOptions);
      if (nOptions < 2) {
         String_freeArray(option);
         continue;
      }
      if (String_eq(option[0], "fields")) {
         char* trim = String_trim(option[1]);
         int nIds;
         char** ids = String_split(trim, ' ', &nIds);
         free(trim);
         int i, j;
         for (j = 0, i = 0; i < LAST_PROCESSFIELD && ids[i]; i++) {
            // This "+1" is for compatibility with the older enum format.
            int id = atoi(ids[i]) + 1;
            if (id > 0 && id < LAST_PROCESSFIELD) {
               this->pl->fields[j] = id;
               j++;
            }
         }
         this->pl->fields[j] = (ProcessField) NULL;
         String_freeArray(ids);
      } else if (String_eq(option[0], "sort_key")) {
         // This "+1" is for compatibility with the older enum format.
         this->pl->sortKey = atoi(option[1]) + 1;
      } else if (String_eq(option[0], "sort_direction")) {
         this->pl->direction = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view")) {
         this->pl->treeView = atoi(option[1]);
      } else if (String_eq(option[0], "hide_threads")) {
         this->pl->hideThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_kernel_threads")) {
         this->pl->hideKernelThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_userland_threads")) {
         this->pl->hideUserlandThreads = atoi(option[1]);
      } else if (String_eq(option[0], "shadow_other_users")) {
         this->pl->shadowOtherUsers = atoi(option[1]);
      } else if (String_eq(option[0], "show_thread_names")) {
         this->pl->showThreadNames = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_base_name")) {
         this->pl->highlightBaseName = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_megabytes")) {
         this->pl->highlightMegabytes = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_threads")) {
         this->pl->highlightThreads = atoi(option[1]);
      } else if (String_eq(option[0], "header_margin")) {
         this->header->margin = atoi(option[1]);
      } else if (String_eq(option[0], "expand_system_time")) {
         // Compatibility option.
         this->pl->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "detailed_cpu_time")) {
         this->pl->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "cpu_count_from_zero")) {
         this->pl->countCPUsFromZero = atoi(option[1]);
      } else if (String_eq(option[0], "update_process_names")) {
         this->pl->updateProcessNames = atoi(option[1]);
      } else if (String_eq(option[0], "delay")) {
         this->delay = atoi(option[1]);
      } else if (String_eq(option[0], "color_scheme")) {
         this->colorScheme = atoi(option[1]);
         if (this->colorScheme < 0) this->colorScheme = 0;
         if (this->colorScheme > 5) this->colorScheme = 5;
      } else if (String_eq(option[0], "left_meters")) {
         Settings_readMeters(this, option[1], LEFT_HEADER);
         readMeters = true;
      } else if (String_eq(option[0], "right_meters")) {
         Settings_readMeters(this, option[1], RIGHT_HEADER);
         readMeters = true;
      } else if (String_eq(option[0], "left_meter_modes")) {
         Settings_readMeterModes(this, option[1], LEFT_HEADER);
         readMeters = true;
      } else if (String_eq(option[0], "right_meter_modes")) {
         Settings_readMeterModes(this, option[1], RIGHT_HEADER);
         readMeters = true;
      }
      String_freeArray(option);
   }
   fclose(fd);
   if (!readMeters) {
      Header_defaultMeters(this->header, cpuCount);
   }
   return true;
}

bool Settings_write(Settings* this) {
   // TODO: implement File object and make
   // file I/O object-oriented.
   FILE* fd;
   fd = fopen(this->userSettings, "w");
   if (fd == NULL) {
      return false;
   }
   fprintf(fd, "# Beware! This file is rewritten by htop when settings are changed in the interface.\n");
   fprintf(fd, "# The parser is also very primitive, and not human-friendly.\n");
   fprintf(fd, "fields=");
   for (int i = 0; this->pl->fields[i]; i++) {
      // This "-1" is for compatibility with the older enum format.
      fprintf(fd, "%d ", (int) this->pl->fields[i]-1);
   }
   fprintf(fd, "\n");
   // This "-1" is for compatibility with the older enum format.
   fprintf(fd, "sort_key=%d\n", (int) this->pl->sortKey-1);
   fprintf(fd, "sort_direction=%d\n", (int) this->pl->direction);
   fprintf(fd, "hide_threads=%d\n", (int) this->pl->hideThreads);
   fprintf(fd, "hide_kernel_threads=%d\n", (int) this->pl->hideKernelThreads);
   fprintf(fd, "hide_userland_threads=%d\n", (int) this->pl->hideUserlandThreads);
   fprintf(fd, "shadow_other_users=%d\n", (int) this->pl->shadowOtherUsers);
   fprintf(fd, "show_thread_names=%d\n", (int) this->pl->showThreadNames);
   fprintf(fd, "highlight_base_name=%d\n", (int) this->pl->highlightBaseName);
   fprintf(fd, "highlight_megabytes=%d\n", (int) this->pl->highlightMegabytes);
   fprintf(fd, "highlight_threads=%d\n", (int) this->pl->highlightThreads);
   fprintf(fd, "tree_view=%d\n", (int) this->pl->treeView);
   fprintf(fd, "header_margin=%d\n", (int) this->header->margin);
   fprintf(fd, "detailed_cpu_time=%d\n", (int) this->pl->detailedCPUTime);
   fprintf(fd, "cpu_count_from_zero=%d\n", (int) this->pl->countCPUsFromZero);
   fprintf(fd, "update_process_names=%d\n", (int) this->pl->updateProcessNames);
   fprintf(fd, "color_scheme=%d\n", (int) this->colorScheme);
   fprintf(fd, "delay=%d\n", (int) this->delay);
   fprintf(fd, "left_meters=");
   for (int i = 0; i < Header_size(this->header, LEFT_HEADER); i++) {
      char* name = Header_readMeterName(this->header, i, LEFT_HEADER);
      fprintf(fd, "%s ", name);
      free(name);
   }
   fprintf(fd, "\n");
   fprintf(fd, "left_meter_modes=");
   for (int i = 0; i < Header_size(this->header, LEFT_HEADER); i++)
      fprintf(fd, "%d ", Header_readMeterMode(this->header, i, LEFT_HEADER));
   fprintf(fd, "\n");
   fprintf(fd, "right_meters=");
   for (int i = 0; i < Header_size(this->header, RIGHT_HEADER); i++) {
      char* name = Header_readMeterName(this->header, i, RIGHT_HEADER);
      fprintf(fd, "%s ", name);
      free(name);
   }
   fprintf(fd, "\n");
   fprintf(fd, "right_meter_modes=");
   for (int i = 0; i < Header_size(this->header, RIGHT_HEADER); i++)
      fprintf(fd, "%d ", Header_readMeterMode(this->header, i, RIGHT_HEADER));
   fprintf(fd, "\n");
   fclose(fd);
   return true;
}

Settings* Settings_new(ProcessList* pl, Header* header, int cpuCount) {
   Settings* this = malloc(sizeof(Settings));
   this->pl = pl;
   this->header = header;
   char* legacyDotfile = NULL;
   char* rcfile = getenv("HTOPRC");
   if (rcfile) {
      this->userSettings = strdup(rcfile);
   } else {
      const char* home = getenv("HOME");
      if (!home) home = "";
      const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
      char* configDir = NULL;
      char* htopDir = NULL;
      if (xdgConfigHome) {
         this->userSettings = String_cat(xdgConfigHome, "/htop/htoprc");
         configDir = strdup(xdgConfigHome);
         htopDir = String_cat(xdgConfigHome, "/htop");
      } else {
         this->userSettings = String_cat(home, "/.config/htop/htoprc");
         configDir = String_cat(home, "/.config");
         htopDir = String_cat(home, "/.config/htop");
      }
      legacyDotfile = String_cat(home, "/.htoprc");
      mkdir(configDir, 0700);
      mkdir(htopDir, 0700);
      free(htopDir);
      free(configDir);
      struct stat st;
      lstat(legacyDotfile, &st);
      if (access(legacyDotfile, R_OK) != 0 || S_ISLNK(st.st_mode)) {
         free(legacyDotfile);
         legacyDotfile = NULL;
      }
   }
   this->colorScheme = 0;
   this->changed = false;
   this->delay = DEFAULT_DELAY;
   bool ok = Settings_read(this, legacyDotfile ? legacyDotfile : this->userSettings, cpuCount);
   if (ok) {
      if (legacyDotfile) {
         // Transition to new location and delete old configuration file
         if (Settings_write(this))
            unlink(legacyDotfile);
         free(legacyDotfile);
      }
   } else {
      this->changed = true;
      // TODO: how to get SYSCONFDIR correctly through Autoconf?
      char* systemSettings = String_cat(SYSCONFDIR, "/htoprc");
      ok = Settings_read(this, systemSettings, cpuCount);
      free(systemSettings);
      if (!ok) {
         Header_defaultMeters(this->header, cpuCount);
         pl->hideKernelThreads = true;
         pl->highlightMegabytes = true;
         pl->highlightThreads = false;
      }
   }
   return this;
}

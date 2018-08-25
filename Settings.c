/*
htop - Settings.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include "Platform.h"

#include "StringUtils.h"
#include "Vector.h"
#include "CRT.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_DELAY 15

/*{
#include "Process.h"
#include <stdbool.h>

typedef struct {
   const char* name;
   const char* columns;
   const char* sortKey;
} ScreenDefaults;

typedef struct {
   int len;
   char** names;
   int* modes;
} MeterColumnSettings;

typedef struct {
   char* name;
   ProcessField* fields;
   int flags;
   int direction;
   ProcessField sortKey;
   bool treeView;
} ScreenSettings;

typedef struct Settings_ {
   char* filename;
   
   MeterColumnSettings meterColumns[2];

   ScreenSettings** screens;
   unsigned int nScreens;
   unsigned int ssIndex;
   ScreenSettings* ss;

   int colorScheme;
   int delay;

   int cpuCount;

   bool countCPUsFromZero;
   bool detailedCPUTime;
   bool showProgramPath;
   bool hideThreads;
   bool shadowOtherUsers;
   bool showThreadNames;
   bool hideKernelThreads;
   bool hideUserlandThreads;
   bool highlightBaseName;
   bool highlightMegabytes;
   bool highlightThreads;
   bool updateProcessNames;
   bool accountGuestInCPUMeter;
   bool headerMargin;
   bool screenTabs;

   bool changed;
} Settings;

#ifndef Settings_cpuId
#define Settings_cpuId(settings, cpu) ((settings)->countCPUsFromZero ? (cpu) : (cpu)+1)
#endif

}*/

static void writeList(FILE* fd, char** list, int len) {
   const char* sep = "";
   for (int i = 0; i < len; i++) {
      fprintf(fd, "%s%s", sep, list[i]);
      sep = " ";
   }
   fprintf(fd, "\n");
}

/*

static char** readQuotedList(char* line) {
   int n = 0;
   char** list = xCalloc(sizeof(char*), 1);
   int start = 0;
   for (;;) {
      while (line[start] && line[start] == ' ') {
         start++;
      }
      if (line[start] != '"') {
         break;
      }
      start++;
      int close = start;
      while (line[close] && line[close] != '"') {
         close++;
      }
      int len = close - start;
      char* item = xMalloc(len + 1);
      strncpy(item, line + start, len);
      item[len] = '\0';
      list[n] = item;
      n++;
      list = xRealloc(list, sizeof(char*) * (n + 1));
      start = close + 1;
   }
   list[n] = NULL;
   return list;
}

static void writeQuotedList(FILE* fd, char** list) {
   const char* sep = "";
   for (int i = 0; list[i]; i++) {
      fprintf(fd, "%s\"%s\"", sep, list[i]);
      sep = " ";
   }
   fprintf(fd, "\n");
}

*/

void Settings_delete(Settings* this) {
   free(this->filename);
   for (unsigned int i = 0; i < (sizeof(this->meterColumns)/sizeof(MeterColumnSettings)); i++) {
      String_freeArray(this->meterColumns[i].names);
      free(this->meterColumns[i].modes);
   }
   if (this->screens) {
      for (unsigned int i = 0; this->screens[i]; i++) {
         free(this->screens[i]->name);
         free(this->screens[i]->fields);
      }
      free(this->screens);
   }
   free(this);
}

static void Settings_readMeters(Settings* this, char* line, int side) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   this->meterColumns[side].names = ids;
}

static void Settings_readMeterModes(Settings* this, char* line, int side) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   int len = 0;
   for (int i = 0; ids[i]; i++) {
      len++;
   }
   this->meterColumns[side].len = len;
   int* modes = xCalloc(len, sizeof(int));
   for (int i = 0; i < len; i++) {
      modes[i] = atoi(ids[i]);
   }
   String_freeArray(ids);
   this->meterColumns[side].modes = modes;
}

static void Settings_defaultMeters(Settings* this) {
   int sizes[] = { 3, 3 };
   if (this->cpuCount > 4) {
      sizes[1]++;
   }
   for (int i = 0; i < 2; i++) {
      this->meterColumns[i].names = xCalloc(sizes[i] + 1, sizeof(char*));
      this->meterColumns[i].modes = xCalloc(sizes[i], sizeof(int));
      this->meterColumns[i].len = sizes[i];
   }
   
   int r = 0;
   if (this->cpuCount > 8) {
      this->meterColumns[0].names[0] = xStrdup("LeftCPUs2");
      this->meterColumns[0].modes[0] = BAR_METERMODE;
      this->meterColumns[1].names[r] = xStrdup("RightCPUs2");
      this->meterColumns[1].modes[r++] = BAR_METERMODE;
   } else if (this->cpuCount > 4) {
      this->meterColumns[0].names[0] = xStrdup("LeftCPUs");
      this->meterColumns[0].modes[0] = BAR_METERMODE;
      this->meterColumns[1].names[r] = xStrdup("RightCPUs");
      this->meterColumns[1].modes[r++] = BAR_METERMODE;
   } else {
      this->meterColumns[0].names[0] = xStrdup("AllCPUs");
      this->meterColumns[0].modes[0] = BAR_METERMODE;
   }
   this->meterColumns[0].names[1] = xStrdup("Memory");
   this->meterColumns[0].modes[1] = BAR_METERMODE;
   this->meterColumns[0].names[2] = xStrdup("Swap");
   this->meterColumns[0].modes[2] = BAR_METERMODE;
   
   this->meterColumns[1].names[r] = xStrdup("Tasks");
   this->meterColumns[1].modes[r++] = TEXT_METERMODE;
   this->meterColumns[1].names[r] = xStrdup("LoadAverage");
   this->meterColumns[1].modes[r++] = TEXT_METERMODE;
   this->meterColumns[1].names[r] = xStrdup("Uptime");
   this->meterColumns[1].modes[r++] = TEXT_METERMODE;
}

static const char* toFieldName(int i) {
   if (i < 0 || i > LAST_PROCESSFIELD) {
      return "";
   }
   return Process_fields[i].name;
}

static int toFieldIndex(const char* str) {
   if (isdigit(str[0])) {
      // This "+1" is for compatibility with the older enum format.
      int id = atoi(str) + 1;
      if (id < Platform_numberOfFields && toFieldName(id)) {
         return id;
      }
   } else {
      for (int p = 1; p < LAST_PROCESSFIELD; p++) {
         const char* pName = toFieldName(p);
         if (pName && strcmp(pName, str) == 0) {
            return p;
         }
      }
   }
   return -1;
}

static void readFields(ProcessField* fields, int* flags, const char* line) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   int i, j;
   *flags = 0;
   for (j = 0, i = 0; i < Platform_numberOfFields && ids[i]; i++) {
      int idx = toFieldIndex(ids[i]);
      if (idx != -1) {
         fields[j] = idx;
         *flags |= Process_fields[idx].flags;
         j++;
      }
   }
   fields[j] = NULL_PROCESSFIELD;
   String_freeArray(ids);
}

ScreenSettings* Settings_newScreen(Settings* this, const char* name, const char* line) {
   ScreenSettings* ss = xCalloc(sizeof(ScreenSettings), 1);
   ss->name = xStrdup(name);
   ss->fields = xCalloc(Platform_numberOfFields+1, sizeof(ProcessField));
   ss->flags = 0;
   ss->direction = 1;
   ss->treeView = 0;
   readFields(ss->fields, &(ss->flags), line);
   ss->sortKey = ss->fields[0];
   this->screens[this->nScreens] = ss;
   this->nScreens++;
   this->screens = xRealloc(this->screens, sizeof(ScreenSettings*) * (this->nScreens + 1));
   this->screens[this->nScreens] = NULL;
   return ss;
}

static void Settings_defaultScreens(Settings* this) {
   for (unsigned int i = 0; i < Platform_numberOfDefaultScreens; i++) {
      ScreenDefaults* defaults = &Platform_defaultScreens[i];
      Settings_newScreen(this, defaults->name, defaults->columns);
      this->screens[i]->sortKey = toFieldIndex(defaults->sortKey);
   }
}

static bool Settings_read(Settings* this, const char* fileName) {
   FILE* fd;
   
   CRT_dropPrivileges();
   fd = fopen(fileName, "r");
   CRT_restorePrivileges();
   if (!fd)
      return false;
   
   bool didReadMeters = false;
   bool didReadFields = false;
   ProcessField* legacyFields = xCalloc(Platform_numberOfFields+1, sizeof(ProcessField));
   int legacyFlags;
   bool legacyFieldsRead = false;
   for (;;) {
      char* line = String_readLine(fd);
      if (!line) {
         break;
      }
      int nOptions;
      char** option = String_split(line, '=', &nOptions);
      free (line);
      if (nOptions < 2) {
         String_freeArray(option);
         continue;
      }
      if (String_eq(option[0], "fields")) {
         readFields(legacyFields, &legacyFlags, option[1]);
         legacyFieldsRead = true;
      } else if (String_eq(option[0], "hide_threads")) {
         this->hideThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_kernel_threads")) {
         this->hideKernelThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_userland_threads")) {
         this->hideUserlandThreads = atoi(option[1]);
      } else if (String_eq(option[0], "shadow_other_users")) {
         this->shadowOtherUsers = atoi(option[1]);
      } else if (String_eq(option[0], "show_thread_names")) {
         this->showThreadNames = atoi(option[1]);
      } else if (String_eq(option[0], "show_program_path")) {
         this->showProgramPath = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_base_name")) {
         this->highlightBaseName = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_megabytes")) {
         this->highlightMegabytes = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_threads")) {
         this->highlightThreads = atoi(option[1]);
      } else if (String_eq(option[0], "header_margin")) {
         this->headerMargin = atoi(option[1]);
      } else if (String_eq(option[0], "screen_tabs")) {
         this->screenTabs = atoi(option[1]);
      } else if (String_eq(option[0], "expand_system_time")) {
         // Compatibility option.
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "detailed_cpu_time")) {
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "cpu_count_from_zero")) {
         this->countCPUsFromZero = atoi(option[1]);
      } else if (String_eq(option[0], "update_process_names")) {
         this->updateProcessNames = atoi(option[1]);
      } else if (String_eq(option[0], "account_guest_in_cpu_meter")) {
         this->accountGuestInCPUMeter = atoi(option[1]);
      } else if (String_eq(option[0], "delay")) {
         this->delay = atoi(option[1]);
      } else if (String_eq(option[0], "color_scheme")) {
         this->colorScheme = atoi(option[1]);
         if (this->colorScheme < 0 || this->colorScheme >= LAST_COLORSCHEME) this->colorScheme = 0;
      } else if (String_eq(option[0], "left_meters")) {
         Settings_readMeters(this, option[1], 0);
         didReadMeters = true;
      } else if (String_eq(option[0], "right_meters")) {
         Settings_readMeters(this, option[1], 1);
         didReadMeters = true;
      } else if (String_eq(option[0], "left_meter_modes")) {
         Settings_readMeterModes(this, option[1], 0);
         didReadMeters = true;
      } else if (String_eq(option[0], "right_meter_modes")) {
         Settings_readMeterModes(this, option[1], 1);
         didReadMeters = true;
      } else if (strncmp(option[0], "screen:", 7) == 0) {
         Settings_newScreen(this, option[0] + 7, option[1]);
         didReadFields = true;
      } else if (String_eq(option[0], ".tree_view")) {
         if (this->nScreens > 0) {
            this->screens[this->nScreens - 1]->treeView = atoi(option[1]);
         }
      } else if (String_eq(option[0], ".sort_direction")) {
         if (this->nScreens > 0) {
            this->screens[this->nScreens - 1]->direction = atoi(option[1]);
         }
      } else if (String_eq(option[0], ".sort_key")) {
         if (this->nScreens > 0) {
            this->screens[this->nScreens - 1]->sortKey = toFieldIndex(option[1]);
         }
      }
      String_freeArray(option);
   }
   fclose(fd);
   if (this->nScreens == 0) {
      Settings_defaultScreens(this);
      if (legacyFieldsRead) {
         didReadFields = true;
         free(this->screens[0]->fields);
         this->screens[0]->fields = legacyFields;
         this->screens[0]->flags = legacyFlags;
      }
   }
   if (!didReadMeters) {
      Settings_defaultMeters(this);
   }
   return didReadFields;
}

static void writeFields(FILE* fd, ProcessField* fields, bool byName) {
   const char* sep = "";
   for (int i = 0; fields[i]; i++) {
      if (byName) {
         fprintf(fd, "%s%s", sep, toFieldName(fields[i]));
      } else {
         // This " - 1" is for compatibility with the older enum format.
         fprintf(fd, "%s%d", sep, (int) fields[i] - 1);
      }
      sep = " ";
   }
   fprintf(fd, "\n");
}

static void writeMeters(Settings* this, FILE* fd, int side) {
   writeList(fd, this->meterColumns[side].names, this->meterColumns[side].len);
}

static void writeMeterModes(Settings* this, FILE* fd, int side) {
   const char* sep = "";
   for (int i = 0; i < this->meterColumns[side].len; i++) {
      fprintf(fd, "%s%d", sep, this->meterColumns[side].modes[i]);
      sep = " ";
   }
   fprintf(fd, "\n");
}

bool Settings_write(Settings* this) {
   FILE* fd;

   CRT_dropPrivileges();
   fd = fopen(this->filename, "w");
   CRT_restorePrivileges();

   if (fd == NULL) {
      return false;
   }
   fprintf(fd, "# Beware! This file is rewritten by htop when settings are changed in the interface.\n");
   fprintf(fd, "# The parser is also very primitive, and not human-friendly.\n");
   fprintf(fd, "fields="); writeFields(fd, this->screens[0]->fields, false);
   fprintf(fd, "hide_threads=%d\n", (int) this->hideThreads);
   fprintf(fd, "hide_kernel_threads=%d\n", (int) this->hideKernelThreads);
   fprintf(fd, "hide_userland_threads=%d\n", (int) this->hideUserlandThreads);
   fprintf(fd, "shadow_other_users=%d\n", (int) this->shadowOtherUsers);
   fprintf(fd, "show_thread_names=%d\n", (int) this->showThreadNames);
   fprintf(fd, "show_program_path=%d\n", (int) this->showProgramPath);
   fprintf(fd, "highlight_base_name=%d\n", (int) this->highlightBaseName);
   fprintf(fd, "highlight_megabytes=%d\n", (int) this->highlightMegabytes);
   fprintf(fd, "highlight_threads=%d\n", (int) this->highlightThreads);
   fprintf(fd, "header_margin=%d\n", (int) this->headerMargin);
   fprintf(fd, "screen_tabs=%d\n", (int) this->screenTabs);
   fprintf(fd, "detailed_cpu_time=%d\n", (int) this->detailedCPUTime);
   fprintf(fd, "cpu_count_from_zero=%d\n", (int) this->countCPUsFromZero);
   fprintf(fd, "update_process_names=%d\n", (int) this->updateProcessNames);
   fprintf(fd, "account_guest_in_cpu_meter=%d\n", (int) this->accountGuestInCPUMeter);
   fprintf(fd, "color_scheme=%d\n", (int) this->colorScheme);
   fprintf(fd, "delay=%d\n", (int) this->delay);
   fprintf(fd, "left_meters="); writeMeters(this, fd, 0);
   fprintf(fd, "left_meter_modes="); writeMeterModes(this, fd, 0);
   fprintf(fd, "right_meters="); writeMeters(this, fd, 1);
   fprintf(fd, "right_meter_modes="); writeMeterModes(this, fd, 1);

   // Legacy compatibility with older versions of htop
   fprintf(fd, "tree_view=%d\n", (int) this->screens[0]->treeView);
   // This "-1" is for compatibility with the older enum format.
   fprintf(fd, "sort_key=%d\n", (int) this->screens[0]->sortKey-1);
   fprintf(fd, "sort_direction=%d\n", (int) this->screens[0]->direction);

   if (this->screens && this->screens[0]) {
      for (unsigned int i = 0; i < this->nScreens; i++) {
         ScreenSettings* ss = this->screens[i];
         fprintf(fd, "screen:%s=", ss->name);
         writeFields(fd, ss->fields, true);
         fprintf(fd, ".tree_view=%d\n", (int) ss->treeView);
         fprintf(fd, ".sort_key=%s\n", toFieldName(ss->sortKey));
         fprintf(fd, ".sort_direction=%d\n", (int) ss->direction);
      }
   }
   fclose(fd);
   return true;
}

Settings* Settings_new(int cpuCount) {
  
   Settings* this = xCalloc(1, sizeof(Settings));

   this->hideThreads = false;
   this->shadowOtherUsers = false;
   this->showThreadNames = false;
   this->hideKernelThreads = false;
   this->hideUserlandThreads = false;
   this->highlightBaseName = false;
   this->highlightMegabytes = false;
   this->detailedCPUTime = false;
   this->countCPUsFromZero = false;
   this->updateProcessNames = false;
   this->cpuCount = cpuCount;
   this->showProgramPath = true;
   this->highlightThreads = true;
   
   this->screens = xCalloc(sizeof(ScreenSettings*), 1);
   this->nScreens = 0;

   char* legacyDotfile = NULL;
   char* rcfile = getenv("HTOPRC");
   if (rcfile) {
      this->filename = xStrdup(rcfile);
   } else {
      const char* home = getenv("HOME");
      if (!home) home = "";
      const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
      char* configDir = NULL;
      char* htopDir = NULL;
      if (xdgConfigHome) {
         this->filename = String_cat(xdgConfigHome, "/htop/htoprc");
         configDir = xStrdup(xdgConfigHome);
         htopDir = String_cat(xdgConfigHome, "/htop");
      } else {
         this->filename = String_cat(home, "/.config/htop/htoprc");
         configDir = String_cat(home, "/.config");
         htopDir = String_cat(home, "/.config/htop");
      }
      legacyDotfile = String_cat(home, "/.htoprc");
      
      CRT_dropPrivileges();
      (void) mkdir(configDir, 0700);
      (void) mkdir(htopDir, 0700);
      free(htopDir);
      free(configDir);
      struct stat st;
      int err = lstat(legacyDotfile, &st);
      if (err || S_ISLNK(st.st_mode)) {
         free(legacyDotfile);
         legacyDotfile = NULL;
      }
      CRT_restorePrivileges();
   }
   this->colorScheme = 0;
   this->changed = false;
   this->delay = DEFAULT_DELAY;
   bool ok = false;
   if (legacyDotfile) {
      ok = Settings_read(this, legacyDotfile);
      if (ok) {
         // Transition to new location and delete old configuration file
         if (Settings_write(this))
            unlink(legacyDotfile);
      }
      free(legacyDotfile);
   }
   if (!ok) {
      ok = Settings_read(this, this->filename);
   }
   if (!ok) {
      this->changed = true;
      // TODO: how to get SYSCONFDIR correctly through Autoconf?
      char* systemSettings = String_cat(SYSCONFDIR, "/htoprc");
      ok = Settings_read(this, systemSettings);
      free(systemSettings);
   }
   if (!ok) {
      Settings_defaultMeters(this);
      Settings_defaultScreens(this);
      this->hideKernelThreads = true;
      this->highlightMegabytes = true;
      this->highlightThreads = true;
      this->headerMargin = true;
      this->screenTabs = true;
   }

   this->ssIndex = 0;
   this->ss = this->screens[this->ssIndex];

   return this;
}

void ScreenSettings_invertSortOrder(ScreenSettings* this) {
   if (this->direction == 1)
      this->direction = -1;
   else
      this->direction = 1;
}

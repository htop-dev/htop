/*
htop - Settings.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Macros.h"
#include "Meter.h"
#include "Platform.h"
#include "XUtils.h"


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
   for (unsigned int i = 0; i < HeaderLayout_getColumns(this->hLayout); i++) {
      String_freeArray(this->hColumns[i].names);
      free(this->hColumns[i].modes);
   }
   free(this->hColumns);
   if (this->screens) {
      for (unsigned int i = 0; this->screens[i]; i++) {
         ScreenSettings_delete(this->screens[i]);
      }
      free(this->screens);
   }
   free(this);
}

static void Settings_readMeters(Settings* this, const char* line, unsigned int column) {
   char* trim = String_trim(line);
   char** ids = String_split(trim, ' ', NULL);
   free(trim);
   column = MINIMUM(column, HeaderLayout_getColumns(this->hLayout) - 1);
   this->hColumns[column].names = ids;
}

static void Settings_readMeterModes(Settings* this, const char* line, unsigned int column) {
   char* trim = String_trim(line);
   char** ids = String_split(trim, ' ', NULL);
   free(trim);
   int len = 0;
   for (int i = 0; ids[i]; i++) {
      len++;
   }
   column = MINIMUM(column, HeaderLayout_getColumns(this->hLayout) - 1);
   this->hColumns[column].len = len;
   int* modes = len ? xCalloc(len, sizeof(int)) : NULL;
   for (int i = 0; i < len; i++) {
      modes[i] = atoi(ids[i]);
   }
   String_freeArray(ids);
   this->hColumns[column].modes = modes;
}

static bool Settings_validateMeters(Settings* this) {
   const size_t colCount = HeaderLayout_getColumns(this->hLayout);

   bool anyMeter = false;

   for (size_t column = 0; column < colCount; column++) {
      char** names = this->hColumns[column].names;
      const int* modes = this->hColumns[column].modes;
      const size_t len = this->hColumns[column].len;

      if (!len)
         continue;

      if (!names || !modes)
         return false;

      anyMeter |= !!len;

      // Check for each mode there is an entry with a non-NULL name
      for (size_t meterIdx = 0; meterIdx < len; meterIdx++)
         if (!names[meterIdx])
            return false;

      if (names[len])
         return false;
   }

   return anyMeter;
}

static void Settings_defaultMeters(Settings* this, unsigned int initialCpuCount) {
   int sizes[] = { 3, 3 };

   if (initialCpuCount > 4 && initialCpuCount <= 128) {
      sizes[1]++;
   }

   // Release any previously allocated memory
   for (size_t i = 0; i < HeaderLayout_getColumns(this->hLayout); i++) {
      String_freeArray(this->hColumns[i].names);
      free(this->hColumns[i].modes);
   }
   free(this->hColumns);

   this->hLayout = HF_TWO_50_50;
   this->hColumns = xCalloc(HeaderLayout_getColumns(this->hLayout), sizeof(MeterColumnSetting));
   for (size_t i = 0; i < 2; i++) {
      this->hColumns[i].names = xCalloc(sizes[i] + 1, sizeof(char*));
      this->hColumns[i].modes = xCalloc(sizes[i], sizeof(int));
      this->hColumns[i].len = sizes[i];
   }

   int r = 0;

   if (initialCpuCount > 128) {
      // Just show the average, ricers need to config for impressive screenshots
      this->hColumns[0].names[0] = xStrdup("CPU");
      this->hColumns[0].modes[0] = BAR_METERMODE;
   } else if (initialCpuCount > 32) {
      this->hColumns[0].names[0] = xStrdup("LeftCPUs8");
      this->hColumns[0].modes[0] = BAR_METERMODE;
      this->hColumns[1].names[r] = xStrdup("RightCPUs8");
      this->hColumns[1].modes[r++] = BAR_METERMODE;
   } else if (initialCpuCount > 16) {
      this->hColumns[0].names[0] = xStrdup("LeftCPUs4");
      this->hColumns[0].modes[0] = BAR_METERMODE;
      this->hColumns[1].names[r] = xStrdup("RightCPUs4");
      this->hColumns[1].modes[r++] = BAR_METERMODE;
   } else if (initialCpuCount > 8) {
      this->hColumns[0].names[0] = xStrdup("LeftCPUs2");
      this->hColumns[0].modes[0] = BAR_METERMODE;
      this->hColumns[1].names[r] = xStrdup("RightCPUs2");
      this->hColumns[1].modes[r++] = BAR_METERMODE;
   } else if (initialCpuCount > 4) {
      this->hColumns[0].names[0] = xStrdup("LeftCPUs");
      this->hColumns[0].modes[0] = BAR_METERMODE;
      this->hColumns[1].names[r] = xStrdup("RightCPUs");
      this->hColumns[1].modes[r++] = BAR_METERMODE;
   } else {
      this->hColumns[0].names[0] = xStrdup("AllCPUs");
      this->hColumns[0].modes[0] = BAR_METERMODE;
   }
   this->hColumns[0].names[1] = xStrdup("Memory");
   this->hColumns[0].modes[1] = BAR_METERMODE;
   this->hColumns[0].names[2] = xStrdup("Swap");
   this->hColumns[0].modes[2] = BAR_METERMODE;
   this->hColumns[1].names[r] = xStrdup("Tasks");
   this->hColumns[1].modes[r++] = TEXT_METERMODE;
   this->hColumns[1].names[r] = xStrdup("LoadAverage");
   this->hColumns[1].modes[r++] = TEXT_METERMODE;
   this->hColumns[1].names[r] = xStrdup("Uptime");
   this->hColumns[1].modes[r++] = TEXT_METERMODE;
}

static const char* toFieldName(Hashtable* columns, int id) {
   if (id < 0)
      return NULL;
   if (id >= ROW_DYNAMIC_FIELDS) {
      const DynamicColumn* column = DynamicColumn_lookup(columns, id);
      return column->name;
   }
   return Process_fields[id].name;
}

static int toFieldIndex(Hashtable* columns, const char* str) {
   if (isdigit(str[0])) {
      // This "+1" is for compatibility with the older enum format.
      int id = atoi(str) + 1;
      if (toFieldName(columns, id)) {
         return id;
      }
   } else {
      // Dynamically-defined columns are always stored by-name.
      char dynamic[32] = {0};
      if (sscanf(str, "Dynamic(%30s)", dynamic)) {
         char* end;
         if ((end = strrchr(dynamic, ')')) != NULL) {
            bool success;
            unsigned int key;
            *end = '\0';
            success = DynamicColumn_search(columns, dynamic, &key) != NULL;
            *end = ')';
            if (success)
               return key;
         }
      }
      // Fallback to iterative scan of table of fields by-name.
      for (int p = 1; p < LAST_PROCESSFIELD; p++) {
         const char* pName = toFieldName(columns, p);
         if (pName && strcmp(pName, str) == 0)
            return p;
      }
   }
   return -1;
}

static void ScreenSettings_readFields(ScreenSettings* ss, Hashtable* columns, const char* line) {
   char* trim = String_trim(line);
   char** ids = String_split(trim, ' ', NULL);
   free(trim);

   /* reset default fields */
   memset(ss->fields, '\0', LAST_PROCESSFIELD * sizeof(ProcessField));

   for (size_t j = 0, i = 0; ids[i]; i++) {
      if (j >= UINT_MAX / sizeof(ProcessField))
         continue;
      if (j >= LAST_PROCESSFIELD) {
         ss->fields = xRealloc(ss->fields, (j + 1) * sizeof(ProcessField));
         memset(&ss->fields[j], 0, sizeof(ProcessField));
      }
      int id = toFieldIndex(columns, ids[i]);
      if (id >= 0)
         ss->fields[j++] = id;
      if (id > 0 && id < LAST_PROCESSFIELD)
         ss->flags |= Process_fields[id].flags;
   }
   String_freeArray(ids);
}

ScreenSettings* Settings_newScreen(Settings* this, const ScreenDefaults* defaults) {
   int sortKey = defaults->sortKey ? toFieldIndex(this->dynamicColumns, defaults->sortKey) : PID;
   int sortDesc = (sortKey >= 0 && sortKey < LAST_PROCESSFIELD) ? Process_fields[sortKey].defaultSortDesc : 1;

   ScreenSettings* ss = xMalloc(sizeof(ScreenSettings));
   *ss = (ScreenSettings) {
      .name = xStrdup(defaults->name),
      .fields = xCalloc(LAST_PROCESSFIELD, sizeof(ProcessField)),
      .flags = 0,
      .direction = sortDesc ? -1 : 1,
      .treeDirection = 1,
      .sortKey = sortKey,
      .treeSortKey = PID,
      .treeView = false,
      .treeViewAlwaysByID = false,
      .allBranchesCollapsed = false,
   };

   ScreenSettings_readFields(ss, this->dynamicColumns, defaults->columns);
   this->screens[this->nScreens] = ss;
   this->nScreens++;
   this->screens = xRealloc(this->screens, sizeof(ScreenSettings*) * (this->nScreens + 1));
   this->screens[this->nScreens] = NULL;
   return ss;
}

void ScreenSettings_delete(ScreenSettings* this) {
   free(this->name);
   free(this->fields);
   free(this);
}

static ScreenSettings* Settings_defaultScreens(Settings* this) {
   if (this->nScreens)
      return this->screens[0];
   for (unsigned int i = 0; i < Platform_numberOfDefaultScreens; i++) {
      const ScreenDefaults* defaults = &Platform_defaultScreens[i];
      Settings_newScreen(this, defaults);
   }
   return this->screens[0];
}

static bool Settings_read(Settings* this, const char* fileName, unsigned int initialCpuCount) {
   FILE* fd = fopen(fileName, "r");
   if (!fd)
      return false;

   ScreenSettings* screen = NULL;
   bool didReadMeters = false;
   bool didReadAny = false;
   for (;;) {
      char* line = String_readLine(fd);
      if (!line) {
         break;
      }
      didReadAny = true;
      size_t nOptions;
      char** option = String_split(line, '=', &nOptions);
      free (line);
      if (nOptions < 2) {
         String_freeArray(option);
         continue;
      }
      if (String_eq(option[0], "config_reader_min_version")) {
         this->config_version = atoi(option[1]);
         if (this->config_version > CONFIG_READER_MIN_VERSION) {
            // the version of the config file on disk is newer than what we can read
            fprintf(stderr, "WARNING: %s specifies configuration format\n", fileName);
            fprintf(stderr, "         version v%d, but this %s binary only supports up to version v%d.\n", this->config_version, PACKAGE, CONFIG_READER_MIN_VERSION);
            fprintf(stderr, "         The configuration file will be downgraded to v%d when %s exits.\n", CONFIG_READER_MIN_VERSION, PACKAGE);
            String_freeArray(option);
            fclose(fd);
            return false;
         }
      } else if (String_eq(option[0], "fields") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         ScreenSettings_readFields(screen, this->dynamicColumns, option[1]);
      } else if (String_eq(option[0], "sort_key") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         // This "+1" is for compatibility with the older enum format.
         screen = Settings_defaultScreens(this);
         screen->sortKey = atoi(option[1]) + 1;
      } else if (String_eq(option[0], "tree_sort_key") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         // This "+1" is for compatibility with the older enum format.
         screen = Settings_defaultScreens(this);
         screen->treeSortKey = atoi(option[1]) + 1;
      } else if (String_eq(option[0], "sort_direction") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         screen->direction = atoi(option[1]);
      } else if (String_eq(option[0], "tree_sort_direction") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         screen->treeDirection = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         screen->treeView = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view_always_by_pid") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         screen->treeViewAlwaysByID = atoi(option[1]);
      } else if (String_eq(option[0], "all_branches_collapsed") && this->config_version <= 2) {
         // old (no screen) naming also supported for backwards compatibility
         screen = Settings_defaultScreens(this);
         screen->allBranchesCollapsed = atoi(option[1]);
      } else if (String_eq(option[0], "hide_kernel_threads")) {
         this->hideKernelThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_userland_threads")) {
         this->hideUserlandThreads = atoi(option[1]);
      } else if (String_eq(option[0], "hide_running_in_container")) {
         this->hideRunningInContainer = atoi(option[1]);
      } else if (String_eq(option[0], "shadow_other_users")) {
         this->shadowOtherUsers = atoi(option[1]);
      } else if (String_eq(option[0], "show_thread_names")) {
         this->showThreadNames = atoi(option[1]);
      } else if (String_eq(option[0], "show_program_path")) {
         this->showProgramPath = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_base_name")) {
         this->highlightBaseName = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_deleted_exe")) {
         this->highlightDeletedExe = atoi(option[1]);
      } else if (String_eq(option[0], "shadow_distribution_path_prefix")) {
         this->shadowDistPathPrefix = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_megabytes")) {
         this->highlightMegabytes = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_threads")) {
         this->highlightThreads = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_changes")) {
         this->highlightChanges = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_changes_delay_secs")) {
         this->highlightDelaySecs = CLAMP(atoi(option[1]), 1, 24 * 60 * 60);
      } else if (String_eq(option[0], "find_comm_in_cmdline")) {
         this->findCommInCmdline = atoi(option[1]);
      } else if (String_eq(option[0], "strip_exe_from_cmdline")) {
         this->stripExeFromCmdline = atoi(option[1]);
      } else if (String_eq(option[0], "show_merged_command")) {
         this->showMergedCommand = atoi(option[1]);
      } else if (String_eq(option[0], "header_margin")) {
         this->headerMargin = atoi(option[1]);
      } else if (String_eq(option[0], "screen_tabs")) {
         this->screenTabs = atoi(option[1]);
      } else if (String_eq(option[0], "expand_system_time")) {
         // Compatibility option.
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "detailed_cpu_time")) {
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "cpu_count_from_one")) {
         this->countCPUsFromOne = atoi(option[1]);
      } else if (String_eq(option[0], "cpu_count_from_zero")) {
         // old (inverted) naming also supported for backwards compatibility
         this->countCPUsFromOne = !atoi(option[1]);
      } else if (String_eq(option[0], "show_cpu_usage")) {
         this->showCPUUsage = atoi(option[1]);
      } else if (String_eq(option[0], "show_cpu_frequency")) {
         this->showCPUFrequency = atoi(option[1]);
      #ifdef BUILD_WITH_CPU_TEMP
      } else if (String_eq(option[0], "show_cpu_temperature")) {
         this->showCPUTemperature = atoi(option[1]);
      } else if (String_eq(option[0], "degree_fahrenheit")) {
         this->degreeFahrenheit = atoi(option[1]);
      #endif
      } else if (String_eq(option[0], "update_process_names")) {
         this->updateProcessNames = atoi(option[1]);
      } else if (String_eq(option[0], "account_guest_in_cpu_meter")) {
         this->accountGuestInCPUMeter = atoi(option[1]);
      } else if (String_eq(option[0], "delay")) {
         this->delay = CLAMP(atoi(option[1]), 1, 255);
      } else if (String_eq(option[0], "color_scheme")) {
         this->colorScheme = atoi(option[1]);
         if (this->colorScheme < 0 || this->colorScheme >= LAST_COLORSCHEME) {
            this->colorScheme = 0;
         }
      #ifdef HAVE_GETMOUSE
      } else if (String_eq(option[0], "enable_mouse")) {
         this->enableMouse = atoi(option[1]);
      #endif
      } else if (String_eq(option[0], "header_layout")) {
         this->hLayout = isdigit((unsigned char)option[1][0]) ? ((HeaderLayout) atoi(option[1])) : HeaderLayout_fromName(option[1]);
         if (this->hLayout < 0 || this->hLayout >= LAST_HEADER_LAYOUT)
            this->hLayout = HF_TWO_50_50;
         free(this->hColumns);
         this->hColumns = xCalloc(HeaderLayout_getColumns(this->hLayout), sizeof(MeterColumnSetting));
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
      } else if (String_startsWith(option[0], "column_meters_")) {
         Settings_readMeters(this, option[1], atoi(option[0] + strlen("column_meters_")));
         didReadMeters = true;
      } else if (String_startsWith(option[0], "column_meter_modes_")) {
         Settings_readMeterModes(this, option[1], atoi(option[0] + strlen("column_meter_modes_")));
         didReadMeters = true;
      } else if (String_eq(option[0], "hide_function_bar")) {
         this->hideFunctionBar = atoi(option[1]);
      #ifdef HAVE_LIBHWLOC
      } else if (String_eq(option[0], "topology_affinity")) {
         this->topologyAffinity = !!atoi(option[1]);
      #endif
      } else if (strncmp(option[0], "screen:", 7) == 0) {
         screen = Settings_newScreen(this, &(const ScreenDefaults) { .name = option[0] + 7, .columns = option[1] });
      } else if (String_eq(option[0], ".sort_key")) {
         if (screen) {
            int key = toFieldIndex(this->dynamicColumns, option[1]);
            screen->sortKey = key > 0 ? key : PID;
         }
      } else if (String_eq(option[0], ".tree_sort_key")) {
         if (screen) {
            int key = toFieldIndex(this->dynamicColumns, option[1]);
            screen->treeSortKey = key > 0 ? key : PID;
         }
      } else if (String_eq(option[0], ".sort_direction")) {
         if (screen)
            screen->direction = atoi(option[1]);
      } else if (String_eq(option[0], ".tree_sort_direction")) {
         if (screen)
            screen->treeDirection = atoi(option[1]);
      } else if (String_eq(option[0], ".tree_view")) {
         if (screen)
            screen->treeView = atoi(option[1]);
      } else if (String_eq(option[0], ".tree_view_always_by_pid")) {
         if (screen)
            screen->treeViewAlwaysByID = atoi(option[1]);
      } else if (String_eq(option[0], ".all_branches_collapsed")) {
         if (screen)
            screen->allBranchesCollapsed = atoi(option[1]);
      }
      String_freeArray(option);
   }
   fclose(fd);
   if (!didReadMeters || !Settings_validateMeters(this))
      Settings_defaultMeters(this, initialCpuCount);
   if (!this->nScreens)
      Settings_defaultScreens(this);
   return didReadAny;
}

static void writeFields(FILE* fd, const ProcessField* fields, Hashtable* columns, bool byName, char separator) {
   const char* sep = "";
   for (unsigned int i = 0; fields[i]; i++) {
      if (fields[i] < LAST_PROCESSFIELD && byName) {
         const char* pName = toFieldName(columns, fields[i]);
         fprintf(fd, "%s%s", sep, pName);
      } else if (fields[i] >= LAST_PROCESSFIELD && byName) {
         const char* pName = toFieldName(columns, fields[i]);
         fprintf(fd, " Dynamic(%s)", pName);
      } else {
         // This "-1" is for compatibility with the older enum format.
         fprintf(fd, "%s%d", sep, (int) fields[i] - 1);
      }
      sep = " ";
   }
   fputc(separator, fd);
}

static void writeList(FILE* fd, char** list, int len, char separator) {
   const char* sep = "";
   for (int i = 0; i < len; i++) {
      fprintf(fd, "%s%s", sep, list[i]);
      sep = " ";
   }
   fputc(separator, fd);
}

static void writeMeters(const Settings* this, FILE* fd, char separator, unsigned int column) {
   writeList(fd, this->hColumns[column].names, this->hColumns[column].len, separator);
}

static void writeMeterModes(const Settings* this, FILE* fd, char separator, unsigned int column) {
   const char* sep = "";
   for (size_t i = 0; i < this->hColumns[column].len; i++) {
      fprintf(fd, "%s%d", sep, this->hColumns[column].modes[i]);
      sep = " ";
   }
   fputc(separator, fd);
}

int Settings_write(const Settings* this, bool onCrash) {
   FILE* fd;
   char separator;
   if (onCrash) {
      fd = stderr;
      separator = ';';
   } else {
      fd = fopen(this->filename, "w");
      if (fd == NULL)
         return -errno;
      separator = '\n';
   }

   #define printSettingInteger(setting_, value_) \
      fprintf(fd, setting_ "=%d%c", (int) (value_), separator)
   #define printSettingString(setting_, value_) \
      fprintf(fd, setting_ "=%s%c", value_, separator)

   if (!onCrash) {
      fprintf(fd, "# Beware! This file is rewritten by htop when settings are changed in the interface.\n");
      fprintf(fd, "# The parser is also very primitive, and not human-friendly.\n");
   }
   printSettingString("htop_version", VERSION);
   printSettingInteger("config_reader_min_version", CONFIG_READER_MIN_VERSION);
   fprintf(fd, "fields="); writeFields(fd, this->screens[0]->fields, this->dynamicColumns, false, separator);
   printSettingInteger("hide_kernel_threads", this->hideKernelThreads);
   printSettingInteger("hide_userland_threads", this->hideUserlandThreads);
   printSettingInteger("hide_running_in_container", this->hideRunningInContainer);
   printSettingInteger("shadow_other_users", this->shadowOtherUsers);
   printSettingInteger("show_thread_names", this->showThreadNames);
   printSettingInteger("show_program_path", this->showProgramPath);
   printSettingInteger("highlight_base_name", this->highlightBaseName);
   printSettingInteger("highlight_deleted_exe", this->highlightDeletedExe);
   printSettingInteger("shadow_distribution_path_prefix", this->shadowDistPathPrefix);
   printSettingInteger("highlight_megabytes", this->highlightMegabytes);
   printSettingInteger("highlight_threads", this->highlightThreads);
   printSettingInteger("highlight_changes", this->highlightChanges);
   printSettingInteger("highlight_changes_delay_secs", this->highlightDelaySecs);
   printSettingInteger("find_comm_in_cmdline", this->findCommInCmdline);
   printSettingInteger("strip_exe_from_cmdline", this->stripExeFromCmdline);
   printSettingInteger("show_merged_command", this->showMergedCommand);
   printSettingInteger("header_margin", this->headerMargin);
   printSettingInteger("screen_tabs", this->screenTabs);
   printSettingInteger("detailed_cpu_time", this->detailedCPUTime);
   printSettingInteger("cpu_count_from_one", this->countCPUsFromOne);
   printSettingInteger("show_cpu_usage", this->showCPUUsage);
   printSettingInteger("show_cpu_frequency", this->showCPUFrequency);
   #ifdef BUILD_WITH_CPU_TEMP
   printSettingInteger("show_cpu_temperature", this->showCPUTemperature);
   printSettingInteger("degree_fahrenheit", this->degreeFahrenheit);
   #endif
   printSettingInteger("update_process_names", this->updateProcessNames);
   printSettingInteger("account_guest_in_cpu_meter", this->accountGuestInCPUMeter);
   printSettingInteger("color_scheme", this->colorScheme);
   #ifdef HAVE_GETMOUSE
   printSettingInteger("enable_mouse", this->enableMouse);
   #endif
   printSettingInteger("delay", (int) this->delay);
   printSettingInteger("hide_function_bar", (int) this->hideFunctionBar);
   #ifdef HAVE_LIBHWLOC
   printSettingInteger("topology_affinity", this->topologyAffinity);
   #endif

   printSettingString("header_layout", HeaderLayout_getName(this->hLayout));
   for (unsigned int i = 0; i < HeaderLayout_getColumns(this->hLayout); i++) {
      fprintf(fd, "column_meters_%u=", i);
      writeMeters(this, fd, separator, i);
      fprintf(fd, "column_meter_modes_%u=", i);
      writeMeterModes(this, fd, separator, i);
   }

   // Legacy compatibility with older versions of htop
   printSettingInteger("tree_view", this->screens[0]->treeView);
   // This "-1" is for compatibility with the older enum format.
   printSettingInteger("sort_key", this->screens[0]->sortKey - 1);
   printSettingInteger("tree_sort_key", this->screens[0]->treeSortKey - 1);
   printSettingInteger("sort_direction", this->screens[0]->direction);
   printSettingInteger("tree_sort_direction", this->screens[0]->treeDirection);
   printSettingInteger("tree_view_always_by_pid", this->screens[0]->treeViewAlwaysByID);
   printSettingInteger("all_branches_collapsed", this->screens[0]->allBranchesCollapsed);

   for (unsigned int i = 0; i < this->nScreens; i++) {
      ScreenSettings* ss = this->screens[i];
      fprintf(fd, "screen:%s=", ss->name);
      writeFields(fd, ss->fields, this->dynamicColumns, true, separator);
      printSettingString(".sort_key", toFieldName(this->dynamicColumns, ss->sortKey));
      printSettingString(".tree_sort_key", toFieldName(this->dynamicColumns, ss->treeSortKey));
      printSettingInteger(".tree_view", ss->treeView);
      printSettingInteger(".tree_view_always_by_pid", ss->treeViewAlwaysByID);
      printSettingInteger(".sort_direction", ss->direction);
      printSettingInteger(".tree_sort_direction", ss->treeDirection);
      printSettingInteger(".all_branches_collapsed", ss->allBranchesCollapsed);
   }

   #undef printSettingString
   #undef printSettingInteger

   if (onCrash)
      return 0;

   int r = 0;

   if (ferror(fd) != 0)
      r = (errno != 0) ? -errno : -EBADF;

   if (fclose(fd) != 0)
      r = r ? r : -errno;

   return r;
}

Settings* Settings_new(unsigned int initialCpuCount, Hashtable* dynamicMeters, Hashtable* dynamicColumns) {
   Settings* this = xCalloc(1, sizeof(Settings));

   this->dynamicColumns = dynamicColumns;
   this->dynamicMeters = dynamicMeters;
   this->hLayout = HF_TWO_50_50;
   this->hColumns = xCalloc(HeaderLayout_getColumns(this->hLayout), sizeof(MeterColumnSetting));

   this->shadowOtherUsers = false;
   this->showThreadNames = false;
   this->hideKernelThreads = true;
   this->hideUserlandThreads = false;
   this->hideRunningInContainer = false;
   this->highlightBaseName = false;
   this->highlightDeletedExe = true;
   this->shadowDistPathPrefix = false;
   this->highlightMegabytes = true;
   this->detailedCPUTime = false;
   this->countCPUsFromOne = false;
   this->showCPUUsage = true;
   this->showCPUFrequency = false;
   #ifdef BUILD_WITH_CPU_TEMP
   this->showCPUTemperature = false;
   this->degreeFahrenheit = false;
   #endif
   this->updateProcessNames = false;
   this->showProgramPath = true;
   this->highlightThreads = true;
   this->highlightChanges = false;
   this->highlightDelaySecs = DEFAULT_HIGHLIGHT_SECS;
   this->findCommInCmdline = true;
   this->stripExeFromCmdline = true;
   this->showMergedCommand = false;
   this->hideFunctionBar = 0;
   this->headerMargin = true;
   #ifdef HAVE_LIBHWLOC
   this->topologyAffinity = false;
   #endif

   this->screens = xCalloc(Platform_numberOfDefaultScreens * sizeof(ScreenSettings*), 1);
   this->nScreens = 0;

   char* legacyDotfile = NULL;
   const char* rcfile = getenv("HTOPRC");
   if (rcfile) {
      this->filename = xStrdup(rcfile);
   } else {
      const char* home = getenv("HOME");
      if (!home) {
         const struct passwd* pw = getpwuid(getuid());
         home = pw ? pw->pw_dir : "";
      }
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
   }
   this->colorScheme = 0;
#ifdef HAVE_GETMOUSE
   this->enableMouse = true;
#endif
   this->changed = false;
   this->delay = DEFAULT_DELAY;
   bool ok = false;
   if (legacyDotfile) {
      ok = Settings_read(this, legacyDotfile, initialCpuCount);
      if (ok) {
         // Transition to new location and delete old configuration file
         if (Settings_write(this, false) == 0) {
            unlink(legacyDotfile);
         }
      }
      free(legacyDotfile);
   }
   if (!ok) {
      ok = Settings_read(this, this->filename, initialCpuCount);
   }
   if (!ok) {
      this->screenTabs = true;
      this->changed = true;
      ok = Settings_read(this, SYSCONFDIR "/htoprc", initialCpuCount);
   }
   if (!ok) {
      Settings_defaultMeters(this, initialCpuCount);
      Settings_defaultScreens(this);
   }

   this->ssIndex = 0;
   this->ss = this->screens[this->ssIndex];

   this->lastUpdate = 1;

   return this;
}

void ScreenSettings_invertSortOrder(ScreenSettings* this) {
   int* attr = (this->treeView) ? &(this->treeDirection) : &(this->direction);
   *attr = (*attr == 1) ? -1 : 1;
}

void ScreenSettings_setSortKey(ScreenSettings* this, ProcessField sortKey) {
   if (this->treeViewAlwaysByID || !this->treeView) {
      this->sortKey = sortKey;
      this->direction = (Process_fields[sortKey].defaultSortDesc) ? -1 : 1;
      this->treeView = false;
   } else {
      this->treeSortKey = sortKey;
      this->treeDirection = (Process_fields[sortKey].defaultSortDesc) ? -1 : 1;
   }
}

static bool readonly = false;

void Settings_enableReadonly(void) {
   readonly = true;
}

bool Settings_isReadonly(void) {
   return readonly;
}

void Settings_setHeaderLayout(Settings* this, HeaderLayout hLayout) {
   unsigned int oldColumns = HeaderLayout_getColumns(this->hLayout);
   unsigned int newColumns = HeaderLayout_getColumns(hLayout);

   if (newColumns > oldColumns) {
      this->hColumns = xReallocArray(this->hColumns, newColumns, sizeof(MeterColumnSetting));
      memset(this->hColumns + oldColumns, 0, (newColumns - oldColumns) * sizeof(MeterColumnSetting));
   } else if (newColumns < oldColumns) {
      for (unsigned int i = newColumns; i < oldColumns; i++) {
         if (this->hColumns[i].names) {
            for (size_t j = 0; j < this->hColumns[i].len; j++)
               free(this->hColumns[i].names[j]);
            free(this->hColumns[i].names);
         }
         free(this->hColumns[i].modes);
      }
      this->hColumns = xReallocArray(this->hColumns, newColumns, sizeof(MeterColumnSetting));
   }

   this->hLayout = hLayout;
   this->changed = true;
}

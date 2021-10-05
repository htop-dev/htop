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


void Settings_delete(Settings* this) {
   free(this->filename);
   free(this->fields);
   for (unsigned int i = 0; i < HeaderLayout_getColumns(this->hLayout); i++) {
      String_freeArray(this->hColumns[i].names);
      free(this->hColumns[i].modes);
   }
   free(this->hColumns);
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

   for (size_t column = 0; column < colCount; column++) {
      char** names = this->hColumns[column].names;
      const int* modes = this->hColumns[column].modes;
      const size_t len = this->hColumns[column].len;

      if (!names || !modes || !len)
         return false;

      // Check for each mode there is an entry with a non-NULL name
      for (size_t meterIdx = 0; meterIdx < len; meterIdx++)
         if (!names[meterIdx])
            return false;

      if (names[len])
         return false;
   }

   return true;
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

static void Settings_readFields(Settings* settings, const char* line) {
   char* trim = String_trim(line);
   char** ids = String_split(trim, ' ', NULL);
   free(trim);

   settings->flags = 0;

   unsigned int i, j;
   for (j = 0, i = 0; ids[i]; i++) {
      if (j >= UINT_MAX / sizeof(ProcessField))
         continue;
      if (j >= LAST_PROCESSFIELD) {
         settings->fields = xRealloc(settings->fields, j * sizeof(ProcessField));
         memset(&settings->fields[j], 0, sizeof(ProcessField));
      }

      // Dynamically-defined columns are always stored by-name.
      char dynamic[32] = {0};
      if (sscanf(ids[i], "Dynamic(%30s)", dynamic)) {
         char* end;
         if ((end = strrchr(dynamic, ')')) == NULL)
            continue;
         *end = '\0';
         unsigned int key;
         if (!DynamicColumn_search(settings->dynamicColumns, dynamic, &key))
            continue;
         settings->fields[j++] = key;
         continue;
      }
      // This "+1" is for compatibility with the older enum format.
      int id = atoi(ids[i]) + 1;
      if (id > 0 && id < LAST_PROCESSFIELD && Process_fields[id].name) {
         settings->flags |= Process_fields[id].flags;
         settings->fields[j++] = id;
      }
   }
   settings->fields[j] = NULL_PROCESSFIELD;
   String_freeArray(ids);
}

static bool Settings_read(Settings* this, const char* fileName, unsigned int initialCpuCount) {
   FILE* fd = fopen(fileName, "r");
   if (!fd)
      return false;

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
      } else if (String_eq(option[0], "fields")) {
         Settings_readFields(this, option[1]);
      } else if (String_eq(option[0], "sort_key")) {
         // This "+1" is for compatibility with the older enum format.
         this->sortKey = atoi(option[1]) + 1;
      } else if (String_eq(option[0], "tree_sort_key")) {
         // This "+1" is for compatibility with the older enum format.
         this->treeSortKey = atoi(option[1]) + 1;
      } else if (String_eq(option[0], "sort_direction")) {
         this->direction = atoi(option[1]);
      } else if (String_eq(option[0], "tree_sort_direction")) {
         this->treeDirection = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view")) {
         this->treeView = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view_always_by_pid")) {
         this->treeViewAlwaysByPID = atoi(option[1]);
      } else if (String_eq(option[0], "all_branches_collapsed")) {
         this->allBranchesCollapsed = atoi(option[1]);
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
      } else if (String_eq(option[0], "highlight_deleted_exe")) {
         this->highlightDeletedExe = atoi(option[1]);
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
      }
      String_freeArray(option);
   }
   fclose(fd);
   if (!didReadMeters || !Settings_validateMeters(this)) {
      Settings_defaultMeters(this, initialCpuCount);
   }
   return didReadAny;
}

static void writeFields(FILE* fd, const ProcessField* fields, Hashtable* columns, const char* name, char separator) {
   fprintf(fd, "%s=", name);
   const char* sep = "";
   for (unsigned int i = 0; fields[i]; i++) {
      if (fields[i] >= LAST_PROCESSFIELD) {
         const DynamicColumn* column = DynamicColumn_lookup(columns, fields[i]);
         fprintf(fd, "%sDynamic(%s)", sep, column->name);
      } else {
         // This "-1" is for compatibility with the older enum format.
         fprintf(fd, "%s%d", sep, (int) fields[i] - 1);
      }
      sep = " ";
   }
   fputc(separator, fd);
}

static void writeMeters(const Settings* this, FILE* fd, char separator, unsigned int column) {
   const char* sep = "";
   for (size_t i = 0; i < this->hColumns[column].len; i++) {
      fprintf(fd, "%s%s", sep, this->hColumns[column].names[i]);
      sep = " ";
   }
   fputc(separator, fd);
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
   writeFields(fd, this->fields, this->dynamicColumns, "fields", separator);
   // This "-1" is for compatibility with the older enum format.
   printSettingInteger("sort_key", this->sortKey - 1);
   printSettingInteger("sort_direction", this->direction);
   printSettingInteger("tree_sort_key", this->treeSortKey - 1);
   printSettingInteger("tree_sort_direction", this->treeDirection);
   printSettingInteger("hide_kernel_threads", this->hideKernelThreads);
   printSettingInteger("hide_userland_threads", this->hideUserlandThreads);
   printSettingInteger("shadow_other_users", this->shadowOtherUsers);
   printSettingInteger("show_thread_names", this->showThreadNames);
   printSettingInteger("show_program_path", this->showProgramPath);
   printSettingInteger("highlight_base_name", this->highlightBaseName);
   printSettingInteger("highlight_deleted_exe", this->highlightDeletedExe);
   printSettingInteger("highlight_megabytes", this->highlightMegabytes);
   printSettingInteger("highlight_threads", this->highlightThreads);
   printSettingInteger("highlight_changes", this->highlightChanges);
   printSettingInteger("highlight_changes_delay_secs", this->highlightDelaySecs);
   printSettingInteger("find_comm_in_cmdline", this->findCommInCmdline);
   printSettingInteger("strip_exe_from_cmdline", this->stripExeFromCmdline);
   printSettingInteger("show_merged_command", this->showMergedCommand);
   printSettingInteger("tree_view", this->treeView);
   printSettingInteger("tree_view_always_by_pid", this->treeViewAlwaysByPID);
   printSettingInteger("all_branches_collapsed", this->allBranchesCollapsed);
   printSettingInteger("header_margin", this->headerMargin);
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

Settings* Settings_new(unsigned int initialCpuCount, Hashtable* dynamicColumns) {
   Settings* this = xCalloc(1, sizeof(Settings));

   this->dynamicColumns = dynamicColumns;
   this->hLayout = HF_TWO_50_50;
   this->hColumns = xCalloc(HeaderLayout_getColumns(this->hLayout), sizeof(MeterColumnSetting));
   this->sortKey = PERCENT_CPU;
   this->treeSortKey = PID;
   this->direction = -1;
   this->treeDirection = 1;
   this->shadowOtherUsers = false;
   this->showThreadNames = false;
   this->hideKernelThreads = true;
   this->hideUserlandThreads = false;
   this->treeView = false;
   this->allBranchesCollapsed = false;
   this->highlightBaseName = false;
   this->highlightDeletedExe = true;
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
   this->fields = xCalloc(LAST_PROCESSFIELD + 1, sizeof(ProcessField));
   // TODO: turn 'fields' into a Vector,
   // (and ProcessFields into proper objects).
   this->flags = 0;
   const ProcessField* defaults = Platform_defaultFields;
   for (int i = 0; defaults[i]; i++) {
      this->fields[i] = defaults[i];
      this->flags |= Process_fields[defaults[i]].flags;
   }

   char* legacyDotfile = NULL;
   const char* rcfile = getenv("HTOPRC");
   if (rcfile) {
      this->filename = xStrdup(rcfile);
   } else {
      const char* home = getenv("HOME");
      if (!home)
         home = "";

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
      this->changed = true;
      ok = Settings_read(this, SYSCONFDIR "/htoprc", initialCpuCount);
   }
   if (!ok) {
      Settings_defaultMeters(this, initialCpuCount);
   }
   return this;
}

void Settings_invertSortOrder(Settings* this) {
   int* attr = (this->treeView) ? &(this->treeDirection) : &(this->direction);
   *attr = (*attr == 1) ? -1 : 1;
}

void Settings_setSortKey(Settings* this, ProcessField sortKey) {
   if (this->treeViewAlwaysByPID || !this->treeView) {
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

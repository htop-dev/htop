#ifndef HEADER_Settings
#define HEADER_Settings
/*
htop - Settings.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "Hashtable.h"
#include "HeaderLayout.h"
#include "Process.h"


#define DEFAULT_DELAY 15

#define CONFIG_READER_MIN_VERSION 3

typedef struct {
   const char* name;
   const char* columns;
   const char* sortKey;
} ScreenDefaults;

typedef struct {
   size_t len;
   char** names;
   int* modes;
} MeterColumnSetting;

typedef struct {
   char* name;
   ProcessField* fields;
   uint32_t flags;
   int direction;
   int treeDirection;
   ProcessField sortKey;
   ProcessField treeSortKey;
   bool treeView;
   bool treeViewAlwaysByPID;
   bool allBranchesCollapsed;
} ScreenSettings;

typedef struct Settings_ {
   char* filename;
   int config_version;
   HeaderLayout hLayout;
   MeterColumnSetting* hColumns;
   Hashtable* dynamicColumns;

   ScreenSettings** screens;
   unsigned int nScreens;
   unsigned int ssIndex;
   ScreenSettings* ss;

   int colorScheme;
   int delay;

   bool countCPUsFromOne;
   bool detailedCPUTime;
   bool showCPUUsage;
   bool showCPUFrequency;
   #ifdef BUILD_WITH_CPU_TEMP
   bool showCPUTemperature;
   bool degreeFahrenheit;
   #endif
   bool showProgramPath;
   bool shadowOtherUsers;
   bool showThreadNames;
   bool hideKernelThreads;
   bool hideUserlandThreads;
   bool highlightBaseName;
   bool highlightDeletedExe;
   bool highlightMegabytes;
   bool highlightThreads;
   bool highlightChanges;
   int highlightDelaySecs;
   bool findCommInCmdline;
   bool stripExeFromCmdline;
   bool showMergedCommand;
   bool updateProcessNames;
   bool accountGuestInCPUMeter;
   bool headerMargin;
   bool screenTabs;
   #ifdef HAVE_GETMOUSE
   bool enableMouse;
   #endif
   int hideFunctionBar;  // 0 - off, 1 - on ESC until next input, 2 - permanently
   #ifdef HAVE_LIBHWLOC
   bool topologyAffinity;
   #endif

   bool changed;
   uint64_t lastUpdate;
} Settings;

#define Settings_cpuId(settings, cpu) ((settings)->countCPUsFromOne ? (cpu)+1 : (cpu))

static inline ProcessField ScreenSettings_getActiveSortKey(const ScreenSettings* this) {
   return (this->treeView)
          ? (this->treeViewAlwaysByPID ? PID : this->treeSortKey)
          : this->sortKey;
}

static inline int ScreenSettings_getActiveDirection(const ScreenSettings* this) {
   return this->treeView ? this->treeDirection : this->direction;
}

void Settings_delete(Settings* this);

int Settings_write(const Settings* this, bool onCrash);

Settings* Settings_new(unsigned int initialCpuCount, Hashtable* dynamicColumns);

ScreenSettings* Settings_newScreen(Settings* this, const ScreenDefaults* defaults);

void ScreenSettings_delete(ScreenSettings* this);

void ScreenSettings_invertSortOrder(ScreenSettings* this);

void ScreenSettings_setSortKey(ScreenSettings* this, ProcessField sortKey);

void Settings_enableReadonly(void);

bool Settings_isReadonly(void);

void Settings_setHeaderLayout(Settings* this, HeaderLayout hLayout);

#endif

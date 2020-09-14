#ifndef HEADER_Settings
#define HEADER_Settings
/*
htop - Settings.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define DEFAULT_DELAY 15

#include "Process.h"
#include <stdbool.h>

typedef struct {
   int len;
   char** names;
   int* modes;
} MeterColumnSettings;

typedef struct Settings_ {
   char* filename;
   MeterColumnSettings columns[2];

   ProcessField* fields;
   int flags;
   int colorScheme;
   int delay;

   int cpuCount;
   int direction;
   ProcessField sortKey;

   bool countCPUsFromZero;
   bool detailedCPUTime;
   bool showCPUUsage;
   bool showCPUFrequency;
   bool treeView;
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
   bool enableMouse;
   #ifdef HAVE_LIBHWLOC
   bool topologyAffinity;
   #endif

   bool changed;
} Settings;

#define Settings_cpuId(settings, cpu) ((settings)->countCPUsFromZero ? (cpu) : (cpu)+1)

void Settings_delete(Settings* this);

bool Settings_write(Settings* this);

Settings* Settings_new(int cpuCount);

void Settings_invertSortOrder(Settings* this);

#endif

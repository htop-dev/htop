#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceScreen.h
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "Settings.h"


typedef struct BacktracePanelPrintingHelper_ {
   size_t maxAddrLen;
   size_t maxDemangledFuncNameLen;
   size_t maxFuncNameLen;
   size_t maxFrameNumLen;
   size_t maxObjPathLen;
   size_t maxObjNameLen;
} BacktracePanelPrintingHelper;

typedef enum BacktraceScreenDisplayOptions_ {
   NO_OPTION = 0,
   DEMANGLE_NAME_FUNCTION = 1 << 0,
   SHOW_FULL_PATH_OBJECT = 1 << 1,
} BacktraceScreenDisplayOptions;

typedef struct BacktracePanel_ {
   Panel super;
   const Process* process;
   BacktracePanelPrintingHelper printingHelper;
   int displayOptions;
   const Settings* settings;
} BacktracePanel;

typedef struct BacktraceFrame_ {
   Object super;

   int index;
   size_t address;
   size_t offset;
   char* functionName;
   char* demangleFunctionName;
   bool isSignalFrame;

   char* objectPath;
   char* objectName;

   const BacktracePanel* backtracePanel;
} BacktraceFrame;

BacktracePanel* BacktracePanel_new(const Process* process, const Settings* settings);
void BacktracePanel_delete(Object* object);
BacktraceFrame* BacktraceFrame_new(const BacktracePanel *panel);
void BacktraceFrame_delete(Object* object);

extern const PanelClass BacktracePanel_class;
extern const ObjectClass BacktraceFrame_class;

#endif

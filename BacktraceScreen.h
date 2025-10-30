#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceScreen.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "Settings.h"
#include "Vector.h"

typedef struct BacktraceFrameData_ {
   Object super;

   int index;
   size_t address;
   size_t offset;
   char* functionName;
   char* demangleFunctionName;
   bool isSignalFrame;
   char* objectPath;
   char* objectName;
} BacktraceFrameData;

typedef struct BacktracePanelPrintingHelper_ {
   size_t maxAddrLen;
   size_t maxDemangledFuncNameLen;
   size_t maxFuncNameLen;
   size_t maxFrameNumLen;
   size_t maxObjPathLen;
   size_t maxObjNameLen;
} BacktracePanelPrintingHelper;

typedef struct BacktracePanel_ {
   Panel super;

   Vector* processes;
   BacktracePanelPrintingHelper printingHelper;
   int displayOptions;
   const Settings* settings;
} BacktracePanel;

typedef enum BacktracePanelRowType_ {
   BACKTRACE_PANEL_ROW_DATA_FRAME,
   BACKTRACE_PANEL_ROW_ERROR,
   BACKTRACE_PANEL_ROW_PROCESS_INFORMATION
} BacktracePanelRowType;

typedef struct BacktracePanelRow_ {
   Object super;

   int type;
   union {
      BacktraceFrameData* frame;
      char* error;
   } data;

   const int* displayOptions;
   const BacktracePanelPrintingHelper* printingHelper;
   const Process* process;
   const Settings* settings;
} BacktracePanelRow;

BacktraceFrameData* BacktraceFrameData_new(void);
void BacktraceFrameData_delete(Object* object);

BacktracePanel* BacktracePanel_new(Vector* processes, const Settings* settings);
void BacktracePanel_delete(Object* object);

BacktracePanelRow* BacktracePanelRow_new(const BacktracePanel* panel);
void BacktracePanelRow_delete(Object *object);

extern const ObjectClass BacktraceFrameData_class;

extern const PanelClass BacktracePanel_class;
extern const ObjectClass BacktracePanelRow_class;

#endif

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

   size_t address;
   size_t offset;
   char* functionName;
   char* demangleFunctionName;
   char* objectPath;
   unsigned int index;
   bool isSignalFrame;
} BacktraceFrameData;

typedef struct BacktracePanelPrintingHelper_ {
   size_t maxAddrLen;
   size_t maxFrameNumLen;
   size_t maxObjPathLen;
   size_t maxObjNameLen;
   bool hasDemangledNames;
} BacktracePanelPrintingHelper;

typedef struct BacktracePanel_ {
   Panel super;

   Vector* processes;
   BacktracePanelPrintingHelper printingHelper;
   const Settings* settings;
   int displayOptions;
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

   const BacktracePanel* panel;
   const Process* process;
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

/*
htop - BacktraceScreen.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "BacktraceScreen.h"

#if defined(HAVE_BACKTRACE_SCREEN)

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "Settings.h"
#include "Vector.h"
#include "XUtils.h"


#define MAX_HEX_ADDR_STR_LEN_32 (strlen("0x") + 8)
#define MAX_HEX_ADDR_STR_LEN_64 (strlen("0x") + 16)

typedef enum BacktracePanelOptions_ {
   OPTION_NAME_DEMANGLE,
   OPTION_NAME_RAW,
   OPTION_OBJECT_FULL_PATH,
   OPTION_OBJECT_BASENAME,
   LAST_PANEL_OPTION,
} BacktracePanelOptions;

static const char* const BacktracePanel_options[LAST_PANEL_OPTION] = {
   [OPTION_NAME_DEMANGLE] = "Demangle",
   [OPTION_NAME_RAW] = "Raw",
   [OPTION_OBJECT_FULL_PATH] = "Full Path",
   [OPTION_OBJECT_BASENAME] = "Basename",
};

typedef enum BacktraceFrameHeaders_ {
   HEADER_NUMBER_FRAME,
   HEADER_ADDRESS,
   HEADER_NAME,
   HEADER_PATH,
   LAST_PANEL_HEADER,
} BacktracePanelHeaders;

static const char* const BacktraceFrame_headerFields[LAST_PANEL_HEADER] = {
   [HEADER_NUMBER_FRAME] = "#",
   [HEADER_ADDRESS] = "ADDRESS",
   [HEADER_NAME] = "NAME",
   [HEADER_PATH] = "PATH",
};

static const char* const BacktraceScreenFunctions[] = {
   "Refresh",
#if defined(HAVE_DEMANGLING)
   BacktracePanel_options[OPTION_NAME_RAW],
#endif
   BacktracePanel_options[OPTION_OBJECT_FULL_PATH],
   "Done   ",
   NULL
};

static const char* const BacktraceScreenKeys[] = {
   "F1",
#if defined(HAVE_DEMANGLING)
   "F2",
#endif
   "F3",
   "Esc",
   NULL
};

static const int BacktraceScreenEvents[] = {
   KEY_F(1),
#if defined(HAVE_DEMANGLING)
   KEY_F(2),
#endif
   KEY_F(3),
   27,
};

typedef enum BacktraceScreenDisplayOptions_ {
   NO_OPTION = 0,
   DEMANGLE_NAME_FUNCTION = 1 << 0,
   SHOW_FULL_PATH_OBJECT = 1 << 1,
} BacktraceScreenDisplayOptions;

BacktraceFrameData* BacktraceFrameData_new(void) {
   BacktraceFrameData* this = AllocThis(BacktraceFrameData);
   this->index = -1;
   this->address = 0;
   this->offset = 0;
   this->functionName = NULL;
   this->demangleFunctionName = NULL;
   this->isSignalFrame = false;
   this->objectPath = NULL;
   this->objectName = NULL;
   return this;
}

void BacktraceFrameData_delete(Object* object) {
   BacktraceFrameData* this = (BacktraceFrameData*)object;
   free(this->functionName);
   free(this->demangleFunctionName);
   free(this->objectPath);
   free(this->objectName);
   free(this);
}

static void BacktracePanel_displayHeader(BacktracePanel* this) {
   const BacktracePanelPrintingHelper* printingHelper = &this->printingHelper;
   const int displayOptions = this->displayOptions;

   size_t maxFunctionNameLength = printingHelper->maxFuncNameLen;
   if (!!(displayOptions & DEMANGLE_NAME_FUNCTION) &&
         printingHelper->maxDemangledFuncNameLen > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFuncNameLen;
   }

   size_t maxObjLen = printingHelper->maxObjNameLen;
   if (!!(displayOptions & SHOW_FULL_PATH_OBJECT)) {
      maxObjLen = printingHelper->maxObjPathLen;
   }

   /*
    * The parameters for printf are of type int.
    * A check is needed to prevent integer overflow.
    */
   assert(printingHelper->maxFrameNumLen <= INT_MAX);
   assert(printingHelper->maxAddrLen <= INT_MAX);
   assert(printingHelper->maxDemangledFuncNameLen <= INT_MAX);
   assert(maxObjLen <= INT_MAX);
   assert(maxFunctionNameLength <= INT_MAX);

   char* line = NULL;
   xAsprintf(&line, "%*s %-*s %-*s %-*s",
      (int)printingHelper->maxFrameNumLen, BacktraceFrame_headerFields[HEADER_NUMBER_FRAME],
      (int)printingHelper->maxAddrLen, BacktraceFrame_headerFields[HEADER_ADDRESS],
      (int)maxObjLen, BacktraceFrame_headerFields[HEADER_PATH],
      (int)maxFunctionNameLength, BacktraceFrame_headerFields[HEADER_NAME]
   );

   Panel_setHeader((Panel*)this, line);
   free(line);
}

static void BacktracePanel_makePrintingHelper(const BacktracePanel* this, BacktracePanelPrintingHelper* printingHelper) {
   Vector* lines = this->super.items;
   size_t longestAddress = 0;

   for (int i = 0; i < Vector_size(lines); i++) {
      const BacktracePanelRow* row = (const BacktracePanelRow*)Vector_get(lines, i);
      if (row->type != BACKTRACE_PANEL_ROW_DATA_FRAME) {
         continue;
      }

      size_t digitOfOffsetFrame = strlen("+0x") + countDigits(row->data.frame->offset, 16);
      if (row->data.frame->demangleFunctionName) {
         size_t demangledFunctionNameLength = strlen(row->data.frame->demangleFunctionName) + digitOfOffsetFrame;
         printingHelper->maxDemangledFuncNameLen = MAXIMUM(demangledFunctionNameLength, printingHelper->maxDemangledFuncNameLen);
      }

      if (row->data.frame->functionName) {
         size_t functionNameLength = strlen(row->data.frame->functionName) + digitOfOffsetFrame;
         printingHelper->maxFuncNameLen = MAXIMUM(functionNameLength, printingHelper->maxFuncNameLen);
      }

      if (row->data.frame->objectPath) {
         size_t objectPathLength = strlen(row->data.frame->objectPath);
         printingHelper->maxObjPathLen = MAXIMUM(objectPathLength, printingHelper->maxObjPathLen);
      }

      if (row->data.frame->objectName) {
         size_t objectNameLength = strlen(row->data.frame->objectName);
         printingHelper->maxObjNameLen = MAXIMUM(objectNameLength, printingHelper->maxObjNameLen);
      }

      printingHelper->maxFrameNumLen = MAXIMUM(countDigits(row->data.frame->index, 10), printingHelper->maxFrameNumLen);

      longestAddress = MAXIMUM(row->data.frame->address, longestAddress);
   }

   size_t addressLength = MAX_HEX_ADDR_STR_LEN_32;
   if (longestAddress > UINT32_MAX) {
      addressLength = MAX_HEX_ADDR_STR_LEN_64;
   }
   printingHelper->maxAddrLen = addressLength;
}

static void BacktracePanel_populateFrames(BacktracePanel* this) {
   char* error = NULL;

   Vector* data = Vector_new(Class(BacktraceFrameData), false, VECTOR_DEFAULT_SIZE);
   for (int i = 0; i < Vector_size(this->processes); i++) {
      const Process* process = (Process*)Vector_get(this->processes, i);
      Platform_getBacktrace(Process_getPid(process), data, &error);

      if (error) {
         BacktracePanelRow* errorRow = BacktracePanelRow_new(this);
         errorRow->type = BACKTRACE_PANEL_ROW_ERROR;
         errorRow->data.error = error;

         Panel_prune((Panel*)this);
         Panel_add((Panel*)this, (Object*)errorRow);
         Vector_delete(data);
         return;
      }

      BacktracePanelRow* header = BacktracePanelRow_new(this);
      header->process = process;
      header->type = BACKTRACE_PANEL_ROW_PROCESS_INFORMATION;
      Panel_add((Panel*)this, (Object*)header);

      for (int j = 0; j < Vector_size(data); j++) {
         BacktracePanelRow* row = BacktracePanelRow_new(this);
         row->type = BACKTRACE_PANEL_ROW_DATA_FRAME;
         row->data.frame = (BacktraceFrameData*)Vector_get(data, j);
         row->process = process;

         Panel_add((Panel*)this, (Object*)row);
      }
      Vector_prune(data);
   }
   Vector_delete(data);

   BacktracePanelPrintingHelper* printingHelper = &this->printingHelper;
   BacktracePanel_makePrintingHelper(this, printingHelper);
   BacktracePanel_displayHeader(this);
}

static HandlerResult BacktracePanel_eventHandler(Panel* super, int ch) {
   BacktracePanel* this = (BacktracePanel*)super;
   int* const displayOptions = &this->displayOptions;

   HandlerResult result = IGNORED;
   switch (ch) {
   case KEY_F(1):
      Panel_prune(super);
      BacktracePanel_populateFrames(this);
      break;

#if defined(HAVE_DEMANGLING)
   case KEY_F(2):
      if (!!(*displayOptions & DEMANGLE_NAME_FUNCTION)) {
         *displayOptions &= ~DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), BacktracePanel_options[OPTION_NAME_DEMANGLE]);
      } else {
         *displayOptions |= DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), BacktracePanel_options[OPTION_NAME_RAW]);
      }
      this->super.needsRedraw = true;
      break;
#endif

   case 'p':
   case KEY_F(3):
      if (!!(*displayOptions & SHOW_FULL_PATH_OBJECT)) {
         *displayOptions &= ~SHOW_FULL_PATH_OBJECT;
         FunctionBar_setLabel(super->defaultBar, KEY_F(3), BacktracePanel_options[OPTION_OBJECT_FULL_PATH]);
      } else {
         FunctionBar_setLabel(super->defaultBar, KEY_F(3), BacktracePanel_options[OPTION_OBJECT_BASENAME]);
         *displayOptions |= SHOW_FULL_PATH_OBJECT;
      }
      this->super.needsRedraw = true;
      BacktracePanel_displayHeader(this);
      break;
   }
   return result;
}

BacktracePanel* BacktracePanel_new(Vector* processes, const Settings* settings) {
   BacktracePanel* this = AllocThis(BacktracePanel);
   this->processes = processes;

   this->printingHelper.maxAddrLen = 0;
   this->printingHelper.maxDemangledFuncNameLen = 0;
   this->printingHelper.maxFrameNumLen = 0;
   this->printingHelper.maxFuncNameLen = 0;
   this->printingHelper.maxObjNameLen = 0;
   this->printingHelper.maxObjPathLen = 0;

   this->displayOptions = DEMANGLE_NAME_FUNCTION;
   this->settings = settings;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 0, 1, Class(BacktracePanelRow), true,
      FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents)
   );

   BacktracePanel_populateFrames(this);

   if (settings->showProgramPath) {
      this->displayOptions |= SHOW_FULL_PATH_OBJECT;
      FunctionBar_setLabel(super->defaultBar, KEY_F(3), BacktracePanel_options[OPTION_OBJECT_BASENAME]);
   } else {
      this->displayOptions &= ~SHOW_FULL_PATH_OBJECT;
      FunctionBar_setLabel(super->defaultBar, KEY_F(3), BacktracePanel_options[OPTION_OBJECT_FULL_PATH]);
   }

   return this;
}

void BacktracePanel_delete(Object* object) {
   BacktracePanel* this = (BacktracePanel*)object;
   Vector_delete(this->processes);
   Panel_delete(object);
}

static void BacktracePanelRow_highlightBasename(const BacktracePanelRow* row, RichString* out, char* line, int objectPathStart) {
   assert(row);
   assert(row->type == BACKTRACE_PANEL_ROW_DATA_FRAME);
   assert(objectPathStart >= 0);

   const Process* process = row->process;

   char* procExe = process->procExe ? process->procExe + process->procExeBasenameOffset : NULL;
   if (!procExe) {
      return;
   }

   size_t endBasenameIndex = objectPathStart;
   size_t lastSlashBasenameIndex = objectPathStart;
   for (; line[endBasenameIndex] != ' '; endBasenameIndex++) {
      if (line[endBasenameIndex] == '/') {
         lastSlashBasenameIndex = endBasenameIndex + 1;
      }
   }

   size_t sizeBasename = endBasenameIndex - lastSlashBasenameIndex;
   if (strncmp(line + lastSlashBasenameIndex, procExe, sizeBasename) == 0) {
      RichString_setAttrn(out, CRT_colors[PROCESS_BASENAME], lastSlashBasenameIndex, sizeBasename);
   }
}

static void BacktracePanelRow_displayInformation(const Object* super, RichString* out) {
   const BacktracePanelRow* row = (const BacktracePanelRow*)super;
   assert(row);
   assert(row->type == BACKTRACE_PANEL_ROW_PROCESS_INFORMATION);

   const Process* process = row->process;

   char* informations = NULL;
   int colorBasename = DEFAULT_COLOR;
   int indexProcessComm = -1;
   int len = -1;
   size_t highlightLen = 0;
   size_t highlightOffset = 0;

   for (size_t i = 0; i < process->mergedCommand.highlightCount; i++) {
      const ProcessCmdlineHighlight* highlight = process->mergedCommand.highlights;
      if (!!(highlight->flags & CMDLINE_HIGHLIGHT_FLAG_BASENAME)) {
         highlightLen = highlight->length;
         highlightOffset = highlight->offset;
         break;
      }
   }

   if (highlightLen == 0) {
      highlightLen = strlen(process->mergedCommand.str);
   }

   if (Process_isThread(process)) {
      colorBasename = PROCESS_THREAD_BASENAME;
      len = xAsprintf(&informations, "Thread %d: %n%s", Process_getPid(process), &indexProcessComm, process->mergedCommand.str);
   } else {
      colorBasename = PROCESS_BASENAME;
      len = xAsprintf(&informations, "Process %d: %n%s",Process_getPid(process), &indexProcessComm, process->mergedCommand.str);
   }

   RichString_appendnAscii(out, CRT_colors[DEFAULT_COLOR] | A_BOLD, informations, len);
   if (indexProcessComm != -1) {
      RichString_setAttrn(out, CRT_colors[colorBasename] | A_BOLD, indexProcessComm + highlightOffset, highlightLen);
   }

   free(informations);
}

static void BacktracePanelRow_displayFrame(const Object* super, RichString* out) {
   const BacktracePanelRow* row = (const BacktracePanelRow*)super;
   assert(row);
   assert(row->type == BACKTRACE_PANEL_ROW_DATA_FRAME);

   const BacktracePanelPrintingHelper* printingHelper = row->printingHelper;
   const int* const displayOptions = row->displayOptions;
   const BacktraceFrameData* frame = row->data.frame;

   char* functionName = frame->functionName;
   int maxFunctionNameLength = printingHelper->maxFuncNameLen;
   if (!!(*displayOptions & DEMANGLE_NAME_FUNCTION) &&
         printingHelper->maxDemangledFuncNameLen > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFuncNameLen;
      if (frame->demangleFunctionName) {
         functionName = frame->demangleFunctionName;
      }
   }

   char* completeFunctionName = NULL;
   xAsprintf(&completeFunctionName, "%s+0x%zx", functionName, frame->offset);

   char* objectDisplayed = frame->objectName;
   size_t objectLength = printingHelper->maxObjNameLen;
   if (!!(*displayOptions & SHOW_FULL_PATH_OBJECT)) {
      objectDisplayed = frame->objectPath;
      objectLength = printingHelper->maxObjPathLen;
   }

   size_t maxAddrLen = printingHelper->maxAddrLen - strlen("0x");
   char* line = NULL;
   int objectPathStart = -1;

   /*
    * The parameters for printf are of type int.
    * A check is needed to prevent integer overflow.
    */
   assert(printingHelper->maxFrameNumLen <= INT_MAX);
   assert(maxAddrLen <= INT_MAX);
   assert(maxFunctionNameLength <= INT_MAX);
   assert(objectLength <= INT_MAX);

   int len = xAsprintf(&line, "%*d 0x%0*zx %n%-*s %-*s",
      (int)printingHelper->maxFrameNumLen, frame->index,
      (int)maxAddrLen, frame->address,
      &objectPathStart,
      (int)objectLength, objectDisplayed ? objectDisplayed : "-",
      (int)maxFunctionNameLength, completeFunctionName
   );

   int colors = CRT_colors[DEFAULT_COLOR];
   if (!objectDisplayed && row->data.frame->address == 0) {
      colors = CRT_colors[DYNAMIC_GRAY];
   }

   RichString_appendnAscii(out, colors, line, len);

   if (row->settings->highlightBaseName) {
      BacktracePanelRow_highlightBasename(row, out, line, objectPathStart);
   }

   free(completeFunctionName);
   free(line);
}

static void BacktracePanelRow_displayError(const Object* super, RichString* out) {
   const BacktracePanelRow* row = (const BacktracePanelRow*)super;
   assert(row);
   assert(row->type == BACKTRACE_PANEL_ROW_ERROR);
   assert(row->data.error);

   RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], row->data.error);
}

static void BacktracePanelRow_display(const Object* super, RichString* out) {
   const BacktracePanelRow* row = (const BacktracePanelRow*)super;
   assert(row);

   switch (row->type) {
      case BACKTRACE_PANEL_ROW_DATA_FRAME:
         BacktracePanelRow_displayFrame(super, out);
      break;
      case BACKTRACE_PANEL_ROW_PROCESS_INFORMATION:
         BacktracePanelRow_displayInformation(super, out);
      break;
      case BACKTRACE_PANEL_ROW_ERROR:
         BacktracePanelRow_displayError(super, out);
      break;
   }
}

BacktracePanelRow* BacktracePanelRow_new(const BacktracePanel* panel) {
   BacktracePanelRow* this = AllocThis(BacktracePanelRow);
   this->displayOptions = &panel->displayOptions;
   this->printingHelper = &panel->printingHelper;
   this->settings = panel->settings;
   return this;
}

void BacktracePanelRow_delete(Object* object) {
   BacktracePanelRow* this = (BacktracePanelRow*)object;
   switch (this->type) {
      case BACKTRACE_PANEL_ROW_DATA_FRAME:
         BacktraceFrameData_delete((Object *)this->data.frame);
      break;
      case BACKTRACE_PANEL_ROW_ERROR:
         free(this->data.error);
      break;

   }
   free(this);
}

const ObjectClass BacktraceFrameData_class = {
   .extends = Class(Object),
   .delete = BacktraceFrameData_delete,
};

const PanelClass BacktracePanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = BacktracePanel_delete,
   },
   .eventHandler = BacktracePanel_eventHandler,
};

const ObjectClass BacktracePanelRow_class = {
   .extends = Class(Object),
   .delete = BacktracePanelRow_delete,
   .display = BacktracePanelRow_display,
};
#endif

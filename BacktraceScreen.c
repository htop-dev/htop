/*
htop - BacktraceScreen.h
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "BacktraceScreen.h"

#if defined(HAVE_BACKTRACE)

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
#include "ListItem.h"
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


#define BACKTRACE_FRAME_HEADER_INDEX -1
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

ATTR_NONNULL static void BacktraceFrame_displayHeader(const BacktraceFrame* frame, RichString* out) {
   const BacktracePanelPrintingHelper* printingHelper = &frame->backtracePanel->printingHelper;
   const BacktraceScreenDisplayOptions displayOptions = frame->backtracePanel->displayOptions;

   int maxFunctionNameLength = printingHelper->maxFuncNameLen;
   if (!!(displayOptions & DEMANGLE_NAME_FUNCTION) &&
         printingHelper->maxDemangledFuncNameLen > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFuncNameLen;
   }

   /*
    * The parameters for printf are of type int.
    * A check is needed to prevent integer overflow.
    */
   assert(printingHelper->maxFrameNumLen <= INT_MAX);
   assert(printingHelper->maxAddrLen <= INT_MAX);
   assert(printingHelper->maxDemangledFuncNameLen <= INT_MAX);
   assert(printingHelper->maxObjPathLen <= INT_MAX);

   char* line = NULL;
   int len = xAsprintf(&line, "%*s %-*s %-*s %-*s",
      (int)printingHelper->maxFrameNumLen, BacktraceFrame_headerFields[HEADER_NUMBER_FRAME],
      (int)printingHelper->maxAddrLen, BacktraceFrame_headerFields[HEADER_ADDRESS],
      (int)maxFunctionNameLength, BacktraceFrame_headerFields[HEADER_NAME],
      (int)printingHelper->maxObjPathLen, BacktraceFrame_headerFields[HEADER_PATH]
   );

   RichString_appendnAscii(out, CRT_colors[BACKTRACE_HEADER], line, len);
   free(line);
}

static void BacktraceFrame_highlightBasename(const BacktraceFrame* frame, RichString* out, char* line, int objectPathStart) {
   assert(objectPathStart >= 0);

   char* basename = frame->backtracePanel->process->procExe ?
      frame->backtracePanel->process->procExe + frame->backtracePanel->process->procExeBasenameOffset : NULL;
   char* object = line ? line + objectPathStart : NULL;
   if (!basename || !object) {
      return;
   }

   char* lastSlash = strrchr(object, '/');
   if (lastSlash) {
      object = lastSlash + 1;
   }

   char* objectTrimmed = String_trim(object);
   if (String_eq(basename, objectTrimmed)) {
      RichString_setAttrn(out, CRT_colors[PROCESS_BASENAME], object - line, strlen(objectTrimmed));
   }

   free(objectTrimmed);
}

static void BacktraceFrame_display(const Object* super, RichString* out) {
   const BacktraceFrame* frame = (const BacktraceFrame* )super;
   assert(frame);

   if (frame->index == BACKTRACE_FRAME_HEADER_INDEX) {
      BacktraceFrame_displayHeader(frame, out);
      return;
   }

   const BacktracePanelPrintingHelper* printingHelper = &frame->backtracePanel->printingHelper;
   const BacktraceScreenDisplayOptions displayOptions = frame->backtracePanel->displayOptions;

   char* functionName = frame->functionName;
   int maxFunctionNameLength = printingHelper->maxFuncNameLen;
   if (!!(displayOptions & DEMANGLE_NAME_FUNCTION) &&
         printingHelper->maxDemangledFuncNameLen > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFuncNameLen;
      if (frame->demangleFunctionName) {
         functionName = frame->demangleFunctionName;
      }
   }

   char *completeFunctionName = NULL;
   xAsprintf(&completeFunctionName, "%s+0x%zx", functionName, frame->offset);

   char* objectDisplayed = frame->objectName;
   size_t objectLength = printingHelper->maxObjNameLen;
   if (!!(displayOptions & SHOW_FULL_PATH_OBJECT)) {
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
   int len = xAsprintf(&line, "%*d 0x%0*zx %-*s %n%-*s",
      (int)printingHelper->maxFrameNumLen, frame->index,
      (int)maxAddrLen, frame->address,
      (int)maxFunctionNameLength, completeFunctionName,
      &objectPathStart,
      (int)objectLength, objectDisplayed ? objectDisplayed : "-"
   );

   int colors = CRT_colors[DEFAULT_COLOR];
   if (!objectDisplayed && frame->address == 0) {
      colors = CRT_colors[DYNAMIC_GRAY];
   }

   RichString_appendnAscii(out, colors, line, len);

   BacktraceFrame_highlightBasename(frame, out, line, objectPathStart);

   free(completeFunctionName);
   free(line);
}

static int BacktraceFrame_compare(const void* object1, const void* object2) {
   const BacktraceFrame* frame1 = (const BacktraceFrame*)object1;
   const BacktraceFrame* frame2 = (const BacktraceFrame*)object2;
   return SPACESHIP_NULLSTR(frame1->functionName, frame2->functionName);
}

void BacktraceFrame_delete(Object* object) {
   BacktraceFrame* this = (BacktraceFrame*)object;
   free(this->functionName);
   free(this->demangleFunctionName);
   free(this->objectPath);
   free(this->objectName);
   free(this);
}

static void BacktracePanel_setError(BacktracePanel* this, const char* error) {
   assert(error);

   Panel* super = &this->super;
   Panel_prune(super);

   Vector* lines = this->super.items;
   if (lines) {
      Vector_delete(lines);
   }

   this->super.items = Vector_new(Class(ListItem), true, DEFAULT_SIZE);
   Panel_set(super, 0, (Object*)ListItem_new(error, 0));
}

static void BacktracePanel_makePrintingHelper(const BacktracePanel* this, BacktracePanelPrintingHelper* printingHelper) {
   Vector* lines = this->super.items;
   size_t longestAddress = 0;

   for (int i = 0; i < Vector_size(lines); i++) {
      const BacktraceFrame* frame = (const BacktraceFrame*)Vector_get(lines, i);
      size_t digitOfOffsetFrame = strlen("+0x") + countDigit(frame->offset, 16);
      if (frame->demangleFunctionName) {
         size_t demangledFunctionNameLength = strlen(frame->demangleFunctionName) + digitOfOffsetFrame;
         printingHelper->maxDemangledFuncNameLen = MAXIMUM(demangledFunctionNameLength, printingHelper->maxDemangledFuncNameLen);
      }

      if (frame->functionName) {
         size_t functionNameLength = strlen(frame->functionName) + digitOfOffsetFrame;
         printingHelper->maxFuncNameLen = MAXIMUM(functionNameLength, printingHelper->maxFuncNameLen);
      }

      if (frame->objectPath) {
         size_t objectPathLength = strlen(frame->objectPath);
         printingHelper->maxObjPathLen = MAXIMUM(objectPathLength, printingHelper->maxObjPathLen);
      }

      if (frame->objectName) {
         size_t objectNameLength = strlen(frame->objectName);
         printingHelper->maxObjNameLen = MAXIMUM(objectNameLength, printingHelper->maxObjNameLen);
      }

      longestAddress = MAXIMUM(frame->address, longestAddress);
   }

   size_t addressLength = MAX_HEX_ADDR_STR_LEN_32;
   if (longestAddress > UINT32_MAX) {
      addressLength = MAX_HEX_ADDR_STR_LEN_64;
   }
   printingHelper->maxAddrLen = addressLength;

   size_t maxNumberFrameDigit = countDigit(Vector_size(lines), 10);
   printingHelper->maxFrameNumLen = MAXIMUM(maxNumberFrameDigit, strlen(BacktraceFrame_headerFields[HEADER_NUMBER_FRAME]));
}

static void BacktracePanel_populateFrames(BacktracePanel* this) {
   char* error = NULL;
   Vector* lines = this->super.items;

   BacktraceFrame* header =  BacktraceFrame_new(this);
   header->index = BACKTRACE_FRAME_HEADER_INDEX;
   Vector_add(lines, (Object*)header);

   Platform_getBacktrace(lines, this, &error);
   if (error) {
      BacktracePanel_setError(this, error);
      free(error);
      return;
   }

   BacktracePanelPrintingHelper* printingHelper = &this->printingHelper;
   BacktracePanel_makePrintingHelper(this, printingHelper);
}

BacktracePanel* BacktracePanel_new(const Process* process, const Settings* settings) {
   BacktracePanel* this = AllocThis(BacktracePanel);
   this->process = process;
   this->printingHelper.maxObjPathLen = 0;
   this->printingHelper.maxFuncNameLen = 0;
   this->printingHelper.maxDemangledFuncNameLen = 0;
   this->displayOptions = DEMANGLE_NAME_FUNCTION;
   this->settings = settings;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(BacktraceFrame), true,
      FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents)
   );

   char* header = NULL;
   xAsprintf(&header, "Backtrace of '%s' (%d)", process->procComm, Process_getPid(process));
   Panel_setHeader(super, header);
   free(header);

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

BacktraceFrame* BacktraceFrame_new(const BacktracePanel *panel) {
   BacktraceFrame* this = AllocThis(BacktraceFrame);
   this->index = 0;
   this->address = 0;
   this->offset = 0;
   this->functionName = NULL;
   this->demangleFunctionName = NULL;
   this->objectPath = NULL;
   this->objectName = NULL;
   this->isSignalFrame = false;
   this->backtracePanel = panel;
   return this;
}

static HandlerResult BacktracePanel_eventHandler(Panel* super, int ch) {
   BacktracePanel* this = (BacktracePanel*)super;
   BacktraceScreenDisplayOptions* displayOptions = &this->displayOptions;

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
      break;
   }
   return result;
}

void BacktracePanel_delete(Object* object) {
   Panel_delete(object);
}

const PanelClass BacktracePanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = BacktracePanel_delete,
   },
   .eventHandler = BacktracePanel_eventHandler,
};

const ObjectClass BacktraceFrame_class = {
   .extends = Class(Object),
   .compare = BacktraceFrame_compare,
   .delete = BacktraceFrame_delete,
   .display = BacktraceFrame_display,
};
#endif

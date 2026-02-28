/*
htop - BacktraceScreen.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "BacktraceScreen.h"

#include <assert.h>
#include <limits.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "Settings.h"
#include "Vector.h"
#include "XUtils.h"
#include "generic/UnwindPtrace.h"


#if defined(HAVE_BACKTRACE_SCREEN)

static const char* const BacktraceScreenFunctions[] = {
#if defined(HAVE_DEMANGLING)
   "Mangle  ",
#endif
   "Full Path",
   "Refresh",
   "Done   ",
   NULL
};

static const char* const BacktraceScreenKeys[] = {
#if defined(HAVE_DEMANGLING)
   "F2",
#endif
   "F3",
   "F5",
   "Esc",
   NULL
};

static const int BacktraceScreenEvents[] = {
#if defined(HAVE_DEMANGLING)
   KEY_F(2),
#endif
   KEY_F(3),
   KEY_F(5),
   27,
};

typedef enum BacktraceScreenDisplayOptions_ {
   DEMANGLE_NAME_FUNCTION = 1 << 0,
   SHOW_FULL_PATH_OBJECT = 1 << 1,
} BacktraceScreenDisplayOptions;

BacktraceFrameData* BacktraceFrameData_new(void) {
   BacktraceFrameData* this = AllocThis(BacktraceFrameData);
   this->address = 0;
   this->offset = 0;
   this->functionName = NULL;
   this->demangleFunctionName = NULL;
   this->objectPath = NULL;
   this->index = 0;
   this->isSignalFrame = false;
   return this;
}

void BacktraceFrameData_delete(Object* object) {
   BacktraceFrameData* this = (BacktraceFrameData*)object;
   free(this->functionName);
   free(this->demangleFunctionName);
   free(this->objectPath);
   free(this);
}

static void BacktracePanel_displayHeader(BacktracePanel* this) {
   const BacktracePanelPrintingHelper* printingHelper = &this->printingHelper;
   const int displayOptions = this->displayOptions;

   bool showDemangledNames = (displayOptions & DEMANGLE_NAME_FUNCTION) && printingHelper->hasDemangledNames;

   bool showFullPathObject = !!(displayOptions & SHOW_FULL_PATH_OBJECT);
   size_t maxObjLen = showFullPathObject ? printingHelper->maxObjPathLen : printingHelper->maxObjNameLen;

   /*
    * The parameters for printf are of type int.
    * A check is needed to prevent integer overflow.
    */
   assert(printingHelper->maxFrameNumLen <= INT_MAX);
   assert(printingHelper->maxAddrLen <= INT_MAX - strlen("0x"));
   assert(maxObjLen <= INT_MAX);

   char* line = NULL;
   xAsprintf(&line, "%*s %-*s %-*s %s",
      (int)printingHelper->maxFrameNumLen, "#",
      (int)(printingHelper->maxAddrLen + strlen("0x")), "ADDRESS",
      (int)maxObjLen, "FILE",
      (showDemangledNames ? "NAME (demangled)" : "NAME")
   );

   Panel_setHeader((Panel*)this, line);
   free(line);
}

static const char* getBasename(const char* path) {
   char *lastSlash = strrchr(path, '/');
   return lastSlash ? lastSlash + 1 : path;
}

static void BacktracePanel_makePrintingHelper(const BacktracePanel* this, BacktracePanelPrintingHelper* printingHelper) {
   Vector* lines = this->super.items;
   unsigned int maxFrameNum = 0;
   size_t longestAddress = 0;

   printingHelper->hasDemangledNames = false;

   for (int i = 0; i < Vector_size(lines); i++) {
      const BacktracePanelRow* row = (const BacktracePanelRow*)Vector_get(lines, i);
      if (row->type != BACKTRACE_PANEL_ROW_DATA_FRAME) {
         continue;
      }

      if (row->data.frame->demangleFunctionName)
         printingHelper->hasDemangledNames = true;

      if (row->data.frame->objectPath) {
         const char* objectName = getBasename(row->data.frame->objectPath);
         size_t objectNameLength = strlen(objectName);
         size_t objectPathLength = (size_t)(objectName - row->data.frame->objectPath) + objectNameLength;

         printingHelper->maxObjNameLen = MAXIMUM(objectNameLength, printingHelper->maxObjNameLen);
         printingHelper->maxObjPathLen = MAXIMUM(objectPathLength, printingHelper->maxObjPathLen);
      }

      maxFrameNum = MAXIMUM(row->data.frame->index, maxFrameNum);

      longestAddress = MAXIMUM(row->data.frame->address, longestAddress);
   }

   printingHelper->maxFrameNumLen = MAXIMUM(countDigits(maxFrameNum, 10), printingHelper->maxFrameNumLen);
   printingHelper->maxAddrLen = MAXIMUM(countDigits(longestAddress, 16), printingHelper->maxAddrLen);
}

static void BacktracePanel_makeBacktrace(Vector* frames, pid_t pid, char** error) {
#ifdef HAVE_LIBUNWIND_PTRACE
   UnwindPtrace_makeBacktrace(frames, pid, error);
#else
   (void)frames;
   (void)pid;
   xAsprintf(error, "The backtrace screen is not implemented");
#endif
}

static void BacktracePanel_populateFrames(BacktracePanel* this) {
   char* error = NULL;

   Vector* data = Vector_new(Class(BacktraceFrameData), false, VECTOR_DEFAULT_SIZE);
   for (int i = 0; i < Vector_size(this->processes); i++) {
      const Process* process = (Process*)Vector_get(this->processes, i);
      BacktracePanel_makeBacktrace(data, Process_getPid(process), &error);

      BacktracePanelRow* header = BacktracePanelRow_new(this);
      header->process = process;
      header->type = BACKTRACE_PANEL_ROW_PROCESS_INFORMATION;
      Panel_add((Panel*)this, (Object*)header);

      if (!error) {
         for (int j = 0; j < Vector_size(data); j++) {
            BacktracePanelRow* row = BacktracePanelRow_new(this);
            row->process = process;
            row->type = BACKTRACE_PANEL_ROW_DATA_FRAME;
            row->data.frame = (BacktraceFrameData*)Vector_get(data, j);

            Panel_add((Panel*)this, (Object*)row);
         }
      } else {
         BacktracePanelRow* errorRow = BacktracePanelRow_new(this);
         errorRow->process = process;
         errorRow->type = BACKTRACE_PANEL_ROW_ERROR;
         errorRow->data.error = error;
         error = NULL;
         Panel_add((Panel*)this, (Object*)errorRow);
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
#if defined(HAVE_DEMANGLING)
   case KEY_F(2):
      *displayOptions ^= DEMANGLE_NAME_FUNCTION;

      bool showDemangledNames = !!(*displayOptions & DEMANGLE_NAME_FUNCTION);
      FunctionBar_setLabel(super->defaultBar, KEY_F(2), showDemangledNames ? "Mangle  " : "Demangle");

      this->super.needsRedraw = true;
      BacktracePanel_displayHeader(this);
      break;
#endif

   case 'p':
   case KEY_F(3):
      *displayOptions ^= SHOW_FULL_PATH_OBJECT;

      bool showFullPathObject = !!(*displayOptions & SHOW_FULL_PATH_OBJECT);
      FunctionBar_setLabel(super->defaultBar, KEY_F(3), showFullPathObject ? "Basename " : "Full Path");

      this->super.needsRedraw = true;
      BacktracePanel_displayHeader(this);
      break;

   case KEY_CTRL('L'):
   case KEY_F(5):
      Panel_prune(super);
      BacktracePanel_populateFrames(this);
      break;
   }

   return result;
}

BacktracePanel* BacktracePanel_new(Vector* processes, const Settings* settings) {
   BacktracePanel* this = AllocThis(BacktracePanel);
   this->processes = processes;

   this->printingHelper.maxAddrLen = strlen("ADDRESS") - strlen("0x");
   this->printingHelper.maxFrameNumLen = strlen("#");
   this->printingHelper.maxObjNameLen = strlen("FILE");
   this->printingHelper.maxObjPathLen = strlen("FILE");
   this->printingHelper.hasDemangledNames = false;

   this->settings = settings;
   this->displayOptions =
      DEMANGLE_NAME_FUNCTION |
      (settings->showProgramPath ? SHOW_FULL_PATH_OBJECT : 0) |
      0;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 0, 1, Class(BacktracePanelRow), true,
      FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents)
   );

   bool showFullPathObject = !!(this->displayOptions & SHOW_FULL_PATH_OBJECT);
   FunctionBar_setLabel(super->defaultBar, KEY_F(3), showFullPathObject ? "Basename " : "Full Path");

   BacktracePanel_populateFrames(this);

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
   if (!procExe)
      return;

   size_t endBasenameIndex = objectPathStart;
   size_t lastSlashBasenameIndex = objectPathStart;
   for (; line[endBasenameIndex] != 0 && line[endBasenameIndex] != ' '; endBasenameIndex++) {
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

   const char* processName = "";
   if (process->mergedCommand.str) {
      processName = process->mergedCommand.str;
      for (size_t i = 0; i < process->mergedCommand.highlightCount; i++) {
         const ProcessCmdlineHighlight* highlight = process->mergedCommand.highlights;
         if (highlight->flags & CMDLINE_HIGHLIGHT_FLAG_BASENAME) {
            highlightLen = highlight->length;
            highlightOffset = highlight->offset;
            break;
         }
      }
   } else if (process->cmdline) {
      processName = process->cmdline;
   }
   if (highlightLen == 0) {
      highlightLen = strlen(processName);
   }

   if (Process_isThread(process)) {
      colorBasename = PROCESS_THREAD_BASENAME;
      len = xAsprintf(&informations, "Thread %d: %n%s", Process_getPid(process), &indexProcessComm, processName);
   } else {
      colorBasename = PROCESS_BASENAME;
      len = xAsprintf(&informations, "Process %d: %n%s",Process_getPid(process), &indexProcessComm, processName);
   }

   RichString_appendnWide(out, CRT_colors[DEFAULT_COLOR] | A_BOLD, informations, len);
   if (indexProcessComm != -1) {
      RichString_setAttrn(out, CRT_colors[colorBasename] | A_BOLD, indexProcessComm + highlightOffset, highlightLen);
   }

   free(informations);
}

static void BacktracePanelRow_displayFrame(const Object* super, RichString* out) {
   const BacktracePanelRow* row = (const BacktracePanelRow*)super;
   assert(row);
   assert(row->type == BACKTRACE_PANEL_ROW_DATA_FRAME);

   const BacktracePanel* panel = row->panel;
   const BacktracePanelPrintingHelper* printingHelper = &panel->printingHelper;
   const int displayOptions = panel->displayOptions;

   const BacktraceFrameData* frame = row->data.frame;

   const char* functionName = "???";
   if ((displayOptions & DEMANGLE_NAME_FUNCTION) && frame->demangleFunctionName) {
      functionName = frame->demangleFunctionName;
   } else if (frame->functionName) {
      functionName = frame->functionName;
   }

   char* completeFunctionName = NULL;
   xAsprintf(&completeFunctionName, "%s+0x%zx", functionName, frame->offset);

   size_t objectLength = printingHelper->maxObjPathLen;
   const char* objectDisplayed = frame->objectPath;
   if (!(displayOptions & SHOW_FULL_PATH_OBJECT)) {
      objectLength = printingHelper->maxObjNameLen;
      if (frame->objectPath)
         objectDisplayed = getBasename(frame->objectPath);
   }

   size_t maxAddrLen = printingHelper->maxAddrLen;
   char* line = NULL;
   int objectPathStart = -1;

   /*
    * The parameters for printf are of type int.
    * A check is needed to prevent integer overflow.
    */
   assert(printingHelper->maxFrameNumLen <= INT_MAX);
   assert(maxAddrLen <= INT_MAX);
   assert(objectLength <= INT_MAX);

   int len = xAsprintf(&line, "%*u 0x%0*zx %n%-*s %s",
      (int)printingHelper->maxFrameNumLen, frame->index,
      (int)maxAddrLen, frame->address,
      &objectPathStart,
      (int)objectLength, objectDisplayed ? objectDisplayed : "-",
      completeFunctionName
   );

   int colors = frame->functionName ? CRT_colors[DEFAULT_COLOR] : CRT_colors[PROCESS_SHADOW];

   RichString_appendnAscii(out, colors, line, len);

   if (panel->settings->highlightBaseName)
      BacktracePanelRow_highlightBasename(row, out, line, objectPathStart);

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
   this->panel = panel;
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

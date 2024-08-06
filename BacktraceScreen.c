/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "BacktraceScreen.h"

#if defined(HAVE_BACKTRACE)

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <ncursesw/curses.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "Object.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "RichString.h"
#include "Vector.h"
#include "XUtils.h"

typedef enum BacktraceScreenDisplayOptions_ {
   NO_OPTION = 0,
#if defined(HAVE_DEMANGLING)
   DEMANGLE_NAME_FUNCTION = 1 << 0,
#endif
} BacktraceScreenDisplayOptions;

#if defined(HAVE_DEMANGLING)
static BacktraceScreenDisplayOptions displayOptions = DEMANGLE_NAME_FUNCTION;
#else
static BacktraceScreenDisplayOptions displayOptions = NO_OPTION;
#endif

static const char* const BacktraceScreenFunctions[] = {
   "Refresh ",
#if defined(HAVE_DEMANGLING)
   "Show demangled function name",
#endif
   "Done   ",
   NULL
};

static const char* const BacktraceScreenKeys[] = {
   "F1",
#if defined(HAVE_DEMANGLING)
   "F2",
#endif
   "Esc"
};

static const int BacktraceScreenEvents[] = {
   KEY_F(1),
#if defined(HAVE_DEMANGLING)
   KEY_F(2),
#endif
   27,
};

static void Frame_display(const Object* super, RichString* out) {
   const BacktraceFrame* const frame = (const BacktraceFrame* const)super;

   char bufferNumberOfFrame[16] = {'\0'};
   int len = snprintf(bufferNumberOfFrame, sizeof(bufferNumberOfFrame), "#%-3d ", frame->index);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_GREEN], bufferNumberOfFrame, len);

   char bufferAddress[32] = {'\0'};
   len = snprintf(bufferAddress, sizeof(bufferAddress), "0x%016zx ", frame->address);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_BLUE], bufferAddress, len);

#if defined(HAVE_DEMANGLING)
   if ((displayOptions& DEMANGLE_NAME_FUNCTION) == DEMANGLE_NAME_FUNCTION &&
         frame->demangleFunctionName) {
      RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->demangleFunctionName);
   } else {
      RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->functionName);
   }
#else
   RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->functionName);
#endif

   char bufferFrameOffset[16] = {'\0'};
   len = snprintf(bufferFrameOffset, sizeof(bufferFrameOffset), "+%zu ", frame->offset);
   RichString_appendAscii(out, CRT_colors[DYNAMIC_YELLOW], bufferFrameOffset);

   if (frame->isSignalFrame) {
      RichString_appendAscii(out, CRT_colors[DYNAMIC_RED], " signal frame");
   }

   if (frame->objectPath) {
      RichString_appendAscii(out, CRT_colors[DYNAMIC_CYAN], frame->objectPath);
   }
}

static int Frame_compare(const void* object1, const void* object2) {
   const BacktraceFrame* frame1 = (const BacktraceFrame*)object1;
   const BacktraceFrame* frame2 = (const BacktraceFrame*)object2;
   return String_eq(frame1->functionName, frame2->functionName);
}

static void Frame_delete(Object* object) {
   BacktraceFrame* this = (BacktraceFrame*)object;
   if (this->functionName) {
      free(this->functionName);
   }

#if defined(HAVE_DEMANGLING)
   if (this->demangleFunctionName) {
      free(this->demangleFunctionName);
   }
#endif

   if (this->objectPath) {
      free(this->objectPath);
   }

   free(this);
}

static void BacktracePanel_setError(BacktracePanel* this, char* error) {
   assert(error != NULL);
   assert(this->error == false);

   Panel* super = &this->super;
   Panel_prune(super);

   Vector* lines = this->super.items;
   Vector_delete(lines);
   this->super.items = Vector_new(Class(ListItem), true, DEFAULT_SIZE);
   Panel_set(super, 0, (Object*)ListItem_new(error, 0));
}

static void BacktracePanel_populateFrames(BacktracePanel* this) {
   if (this->error) {
      return;
   }

   char* error = NULL;
   Vector* lines = this->super.items;
   Platform_getBacktrace(lines, this->process, &error);
   if (error) {
      BacktracePanel_setError(this, error);
      free(error);
   }

   this->super.needsRedraw = true;
}

BacktracePanel* BacktracePanel_new(const Process* process) {
   BacktracePanel* this = AllocThis(BacktracePanel);
   this->process = process;
   this->error = false;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(BacktraceFrame), true, FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents));

   char* header = NULL;
   xAsprintf(& header, "Backtrace of '%s' (%d)", process->procComm, Process_getPid(process));
   Panel_setHeader(super, header);
   free(header);

   BacktracePanel_populateFrames(this);
   return this;
}

BacktraceFrame* BacktraceFrame_new(void) {
   BacktraceFrame* this = AllocThis(BacktraceFrame);
   this->index = 0;;
   this->address = 0;
   this->offset = 0;
   this->functionName = NULL;
   this->demangleFunctionName = NULL;
   this->objectPath = NULL;
   this->isSignalFrame = false;
   return this;
}

static HandlerResult BacktracePanel_eventHandler(Panel* super, int ch) {
   BacktracePanel* this = (BacktracePanel*)super;

   HandlerResult result = IGNORED;
   switch (ch) {
   case KEY_F(1):
      Panel_prune(super);
      BacktracePanel_populateFrames(this);
      break;

#if defined(HAVE_DEMANGLING)
   case KEY_F(2):
      if ((displayOptions& DEMANGLE_NAME_FUNCTION) == DEMANGLE_NAME_FUNCTION) {
         displayOptions &= ~DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), "Show mangled function name");
      } else {
         displayOptions |= DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), "Show demangled function name");
      }
      this->super.needsRedraw = true;
      break;
#endif
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
   .compare = Frame_compare,
   .delete = Frame_delete,
   .display = Frame_display,
};
#endif

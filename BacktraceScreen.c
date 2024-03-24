/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "BacktraceScreen.h"

#include "config.h" // IWYU pragma: keep

#ifdef BACKTRACE_ENABLED
#include "Platform.h"

#include <sys/wait.h>

#include "CRT.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "RichString.h"
#include "XUtils.h"

static const char* const BacktraceScreenFunctions[] = {"Done   ", NULL};

static const char* const BacktraceScreenKeys[] = {"Esc"};

static const int BacktraceScreenEvents[] = {27};

static void Frame_display(const Object* super, RichString* out) {
   const Frame* const frame = (const Frame*)super;

   char bufferNumberOfFrame[16] = {'\0'};
   int len = snprintf(bufferNumberOfFrame, sizeof(bufferNumberOfFrame), "#%-3d ", frame->index);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_GREEN], bufferNumberOfFrame, len);

   char bufferAddress[32] = {'\0'};
   len = snprintf(bufferAddress, sizeof(bufferAddress), "0x%016zx ", frame->address);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_BLUE], bufferAddress, len);

   RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->functionName);
   if (frame->isSignalFrame) {
      RichString_appendAscii(out, CRT_colors[DYNAMIC_RED], " signal frame");
   }

   char bufferFrameOffset[16] = {'\0'};
   len = snprintf(bufferFrameOffset, sizeof(bufferFrameOffset), "+%zu", frame->offset);
   RichString_appendAscii(out, CRT_colors[DYNAMIC_YELLOW], bufferFrameOffset);
}

BacktracePanel* BacktracePanel_new(const Process* process) {
   BacktracePanel* this = CallocThis(BacktracePanel);
   this->process = process;

   Panel* super = (Panel*) this;

   Vector* frames = Vector_new(Class(Frame), false, DEFAULT_SIZE);
   char* error = NULL;
   Plateform_getBacktraces(frames, process, &error);

   if (error) {
      Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents));
      Panel_add(super, (Object*)ListItem_new(error, 0));
      free(error);
   } else {
      Panel_init(super, 1, 1, 1, 1, Class(Frame), true, FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents));
      for (int i = 0; i < Vector_size(frames); i++) {
         Frame* frame = (Frame*) Vector_get(frames, i);
         Panel_add(super, (Object*) frame);
      }
   }

   Vector_delete(frames);

   char* header = NULL;
   xAsprintf(&header, "Backtrace of '%s' (%d)", process->procComm, Process_getPid(process));
   Panel_setHeader(super, header);
   free(header);
   return this;
}

Frame* Frame_new(void) {
   Frame* this = CallocThis(Frame);
   return this;
}

static int Frame_compare(const void* object1, const void* object2) {
   const Frame* frame1 = (const Frame*)object1;
   const Frame* frame2 = (const Frame*)object2;
   return String_eq(frame1->functionName, frame2->functionName);
}

static void Frame_delete(Object* object) {
   Frame* this = (Frame*)object;
   if (this->functionName) {
      free(this->functionName);
   }

   free(this);
}

void BacktracePanel_delete(Object* object) {
   Panel_delete(object);
}

const PanelClass BacktracePanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = BacktracePanel_delete,
   },
};

const ObjectClass Frame_class = {
   .extends = Class(Object),
   .compare = Frame_compare,
   .delete = Frame_delete,
   .display = Frame_display,
};
#endif

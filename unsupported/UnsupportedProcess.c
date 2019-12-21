/*
htop - UnsupportedProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "UnsupportedProcess.h"
#include <stdlib.h>


Process* UnsupportedProcess_new(Settings* settings) {
   Process* this = xCalloc(1, sizeof(Process));
   Object_setClass(this, Class(Process));
   Process_init(this, settings);
   return this;
}

void UnsupportedProcess_delete(Object* cast) {
   Process* this = (Process*) cast;
   Object_setClass(this, Class(Process));
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}

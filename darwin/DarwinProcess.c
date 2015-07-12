/*
htop - DarwinProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "DarwinProcess.h"
#include <stdlib.h>

/*{
#include "Settings.h"

#define Process_delete UnsupportedProcess_delete

}*/

Process* DarwinProcess_new(Settings* settings) {
   Process* this = calloc(sizeof(Process), 1);
   Object_setClass(this, Class(Process));
   Process_init(this, settings);
   return this;
}

void DarwinProcess_delete(Object* cast) {
   Process* this = (Process*) cast;
   Object_setClass(this, Class(Process));
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}


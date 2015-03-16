/*
htop - UnsupportedProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "UnsupportedProcess.h"
#include <stdlib.h>

/*{

typedef struct UnsupportedProcess_ {
   Process super;
   // add platform-specific fields here
} UnsupportedProcess;

#define Process_delete UnsupportedProcess_delete

}*/

UnsupportedProcess* UnsupportedProcess_new(Settings* settings) {
   UnsupportedProcess* this = calloc(sizeof(UnsupportedProcess), 1);
   Object_setClass(this, Class(Process));
   Process_init(&this->super, settings);
   return this;
}

void UnsupportedProcess_delete(Object* cast) {
   UnsupportedProcess* this = (UnsupportedProcess*) cast;
   Object_setClass(this, Class(Process));
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}


#ifndef HEADER_CommandScreen
#define HEADER_CommandScreen

#include "InfoScreen.h"

typedef struct CommandScreen_ {
   InfoScreen super;
} CommandScreen;

extern InfoScreenClass CommandScreen_class;

CommandScreen* CommandScreen_new(Process* process);

void CommandScreen_delete(Object* this);

#endif

#ifndef HEADER_CommandScreen
#define HEADER_CommandScreen

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct CommandScreen_ {
   InfoScreen super;
} CommandScreen;

extern const InfoScreenClass CommandScreen_class;

CommandScreen* CommandScreen_new(Process* process);

void CommandScreen_delete(Object* this);

#endif

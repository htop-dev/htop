#ifndef HEADER_EnvScreen
#define HEADER_EnvScreen

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct EnvScreen_ {
   InfoScreen super;
} EnvScreen;

extern const InfoScreenClass EnvScreen_class;

EnvScreen* EnvScreen_new(Process* process);

void EnvScreen_delete(Object* this);

#endif

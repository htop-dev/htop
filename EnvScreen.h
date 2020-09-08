#ifndef HEADER_EnvScreen
#define HEADER_EnvScreen

#include "InfoScreen.h"

typedef struct EnvScreen_ {
   InfoScreen super;
} EnvScreen;

extern InfoScreenClass EnvScreen_class;

EnvScreen* EnvScreen_new(Process* process);

void EnvScreen_delete(Object* this);

void EnvScreen_draw(InfoScreen* this);

void EnvScreen_scan(InfoScreen* this);

#endif

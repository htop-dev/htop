#ifndef HEADER_KeyBinds
#define HEADER_KeyBinds
#include "Settings.h"

int KeyBinds_obtainBind(const Settings *settings, int key);

// to my knowledge the 600 range is empty
typedef enum KeyBinds_ {
   BIND_LEFT=600,
   BIND_DOWN,
   BIND_UP,
   BIND_RIGHT
} KeyBinds;

#endif

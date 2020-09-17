#include "KeyBinds.h"
#include "Settings.h"
#include "Panel.h"

int KeyBinds_obtainBind(const Settings *settings, int key) {
   if (settings->vimMode) {
      switch(key) {
         case 'h':
            return BIND_LEFT;
         case 'j':
            return BIND_DOWN;
         case 'k':
            return BIND_UP;
         case 'l':
            return BIND_RIGHT;
      }
   }
   switch(key) {
      case KEY_LEFT:
      case KEY_CTRL('B'):
          return BIND_LEFT;
      #ifdef KEY_C_DOWN
      case KEY_C_DOWN:
      #endif
      case KEY_DOWN:
      case KEY_CTRL('N'):
          return BIND_DOWN;
      #ifdef KEY_C_UP
      case KEY_C_UP:
      #endif
      case KEY_UP:
      case KEY_CTRL('P'):
          return BIND_UP;
      case KEY_RIGHT:
      case KEY_CTRL('F'):
      case 9:
          return BIND_RIGHT;
   }
   return key;
}

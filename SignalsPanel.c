/*
htop - SignalsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "SignalsPanel.h"
#include "Platform.h"

#include "ListItem.h"
#include "RichString.h"

#include <stdlib.h>
#include <assert.h>

#include <ctype.h>

/*{

typedef struct SignalItem_ {
   const char* name;
   int number;
} SignalItem;

}*/

Panel* SignalsPanel_new() {
   Panel* this = Panel_new(1, 1, 1, 1, true, Class(ListItem), FunctionBar_newEnterEsc("Send   ", "Cancel "));
   for(unsigned int i = 0; i < Platform_numberOfSignals; i++)
      Panel_set(this, i, (Object*) ListItem_new(Platform_signals[i].name, Platform_signals[i].number));
   Panel_setHeader(this, "Send signal:");
   Panel_setSelected(this, DEFAULT_SIGNAL);
   return this;
}

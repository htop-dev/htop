
#include "SignalsPanel.h"
#include "Panel.h"
#include "SignalItem.h"
#include "RichString.h"

#include "debug.h"
#include <assert.h>

#include <ctype.h>

/*{

typedef struct SignalsPanel_ {
   Panel super;

   int state;
   Signal** signals;
} SignalsPanel;

}*/

static HandlerResult SignalsPanel_eventHandler(Panel* super, int ch) {
   SignalsPanel* this = (SignalsPanel*) super;

   int size = Panel_getSize(super);
   
   if (ch <= 255 && isdigit(ch)) {
      int signal = ch-48 + this->state;
      for (int i = 0; i < size; i++)
         if (((Signal*) Panel_get(super, i))->number == signal) {
            Panel_setSelected(super, i);
            break;
         }
      this->state = signal * 10;
      if (this->state > 100)
         this->state = 0;
      return HANDLED;
   } else {
      this->state = 0;
   }
   if (ch == 13) {
      return BREAK_LOOP;
   }
   return IGNORED;
}

static void SignalsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   SignalsPanel* this = (SignalsPanel*) object;
   Panel_done(super);
   free(this->signals);
   free(this);
}

SignalsPanel* SignalsPanel_new(int x, int y, int w, int h) {
   SignalsPanel* this = (SignalsPanel*) malloc(sizeof(SignalsPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, x, y, w, h, SIGNAL_CLASS, true);
   ((Object*)this)->delete = SignalsPanel_delete;

   this->signals = Signal_getSignalTable();
   super->eventHandler = SignalsPanel_eventHandler;
   int sigCount = Signal_getSignalCount();
   for(int i = 0; i < sigCount; i++)
      Panel_set(super, i, (Object*) this->signals[i]);
   SignalsPanel_reset(this);
   return this;
}

void SignalsPanel_reset(SignalsPanel* this) {
   Panel* super = (Panel*) this;

   Panel_setHeader(super, "Send signal:");
   Panel_setSelected(super, 16); // 16th item is SIGTERM
   this->state = 0;
}

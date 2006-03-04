
#include "SignalsListBox.h"
#include "ListBox.h"
#include "SignalItem.h"
#include "RichString.h"

#include "debug.h"
#include <assert.h>

#include <ctype.h>

/*{

typedef struct SignalsListBox_ {
   ListBox super;

   int state;
   Signal** signals;
} SignalsListBox;

}*/

SignalsListBox* SignalsListBox_new(int x, int y, int w, int h) {
   SignalsListBox* this = (SignalsListBox*) malloc(sizeof(SignalsListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, x, y, w, h, SIGNAL_CLASS, true);
   ((Object*)this)->delete = SignalsListBox_delete;

   this->signals = Signal_getSignalTable();
   super->eventHandler = SignalsListBox_EventHandler;
   int sigCount = Signal_getSignalCount();
   for(int i = 0; i < sigCount; i++)
      ListBox_set(super, i, (Object*) this->signals[i]);
   SignalsListBox_reset(this);
   return this;
}

void SignalsListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   SignalsListBox* this = (SignalsListBox*) object;
   ListBox_done(super);
   free(this->signals);
   free(this);
}

void SignalsListBox_reset(SignalsListBox* this) {
   ListBox* super = (ListBox*) this;

   ListBox_setHeader(super, "Send signal:");
   ListBox_setSelected(super, 16); // 16th item is SIGTERM
   this->state = 0;
}

HandlerResult SignalsListBox_EventHandler(ListBox* super, int ch) {
   SignalsListBox* this = (SignalsListBox*) super;

   int size = ListBox_getSize(super);
   
   if (ch <= 255 && isdigit(ch)) {
      int signal = ch-48 + this->state;
      for (int i = 0; i < size; i++)
         if (((Signal*) ListBox_get(super, i))->number == signal) {
            ListBox_setSelected(super, i);
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

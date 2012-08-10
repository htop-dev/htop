/*
htop - Panel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"

#include "CRT.h"
#include "RichString.h"
#include "ListItem.h"
#include "String.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <curses.h>

//#link curses

/*{
#include "Object.h"
#include "Vector.h"

typedef struct Panel_ Panel;

typedef enum HandlerResult_ {
   HANDLED,
   IGNORED,
   BREAK_LOOP
} HandlerResult;

#define EVENT_SETSELECTED -1

typedef HandlerResult(*Panel_EventHandler)(Panel*, int);

struct Panel_ {
   Object super;
   int x, y, w, h;
   WINDOW* window;
   Vector* items;
   int selected;
   int scrollV, scrollH;
   int scrollHAmount;
   int oldSelected;
   bool needsRedraw;
   RichString header;
   Panel_EventHandler eventHandler;
   char* eventHandlerBuffer;
};

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef DEBUG
char* PANEL_CLASS = "Panel";
#else
#define PANEL_CLASS NULL
#endif

#define KEY_CTRLN      0016            /* control-n key */
#define KEY_CTRLP      0020            /* control-p key */
#define KEY_CTRLF      0006            /* control-f key */
#define KEY_CTRLB      0002            /* control-b key */

Panel* Panel_new(int x, int y, int w, int h, char* type, bool owner, Object_Compare compare) {
   Panel* this;
   this = malloc(sizeof(Panel));
   Panel_init(this, x, y, w, h, type, owner);
   this->items->compare = compare;
   return this;
}

void Panel_delete(Object* cast) {
   Panel* this = (Panel*)cast;
   Panel_done(this);
   free(this);
}

void Panel_init(Panel* this, int x, int y, int w, int h, char* type, bool owner) {
   Object* super = (Object*) this;
   Object_setClass(this, PANEL_CLASS);
   super->delete = Panel_delete;
   this->x = x;
   this->y = y;
   this->w = w;
   this->h = h;
   this->eventHandler = NULL;
   this->eventHandlerBuffer = NULL;
   this->items = Vector_new(type, owner, DEFAULT_SIZE, ListItem_compare);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
   RichString_beginAllocated(this->header);
   if (String_eq(CRT_termType, "linux"))
      this->scrollHAmount = 20;
   else
      this->scrollHAmount = 5;
}

void Panel_done(Panel* this) {
   assert (this != NULL);
   free(this->eventHandlerBuffer);
   Vector_delete(this->items);
   RichString_end(this->header);
}

RichString* Panel_getHeader(Panel* this) {
   assert (this != NULL);

   this->needsRedraw = true;
   return &(this->header);
}

inline void Panel_setHeader(Panel* this, const char* header) {
   RichString_write(&(this->header), CRT_colors[PANEL_HEADER_FOCUS], header);
   this->needsRedraw = true;
}

void Panel_setEventHandler(Panel* this, Panel_EventHandler eh) {
   this->eventHandler = eh;
}

void Panel_move(Panel* this, int x, int y) {
   assert (this != NULL);

   this->x = x;
   this->y = y;
   this->needsRedraw = true;
}

void Panel_resize(Panel* this, int w, int h) {
   assert (this != NULL);

   if (RichString_sizeVal(this->header) > 0)
      h--;
   this->w = w;
   this->h = h;
   this->needsRedraw = true;
}

void Panel_prune(Panel* this) {
   assert (this != NULL);

   Vector_prune(this->items);
   this->scrollV = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
}

void Panel_add(Panel* this, Object* o) {
   assert (this != NULL);

   Vector_add(this->items, o);
   this->needsRedraw = true;
}

void Panel_insert(Panel* this, int i, Object* o) {
   assert (this != NULL);

   Vector_insert(this->items, i, o);
   this->needsRedraw = true;
}

void Panel_set(Panel* this, int i, Object* o) {
   assert (this != NULL);

   Vector_set(this->items, i, o);
}

Object* Panel_get(Panel* this, int i) {
   assert (this != NULL);

   return Vector_get(this->items, i);
}

Object* Panel_remove(Panel* this, int i) {
   assert (this != NULL);

   this->needsRedraw = true;
   Object* removed = Vector_remove(this->items, i);
   if (this->selected > 0 && this->selected >= Vector_size(this->items))
      this->selected--;
   return removed;
}

Object* Panel_getSelected(Panel* this) {
   assert (this != NULL);
   if (Vector_size(this->items) > 0)
      return Vector_get(this->items, this->selected);
   else
      return NULL;
}

void Panel_moveSelectedUp(Panel* this) {
   assert (this != NULL);

   Vector_moveUp(this->items, this->selected);
   if (this->selected > 0)
      this->selected--;
}

void Panel_moveSelectedDown(Panel* this) {
   assert (this != NULL);

   Vector_moveDown(this->items, this->selected);
   if (this->selected + 1 < Vector_size(this->items))
      this->selected++;
}

int Panel_getSelectedIndex(Panel* this) {
   assert (this != NULL);

   return this->selected;
}

int Panel_size(Panel* this) {
   assert (this != NULL);

   return Vector_size(this->items);
}

void Panel_setSelected(Panel* this, int selected) {
   assert (this != NULL);

   selected = MAX(0, MIN(Vector_size(this->items) - 1, selected));
   this->selected = selected;
   if (this->eventHandler) {
      this->eventHandler(this, EVENT_SETSELECTED);
   }
}

void Panel_draw(Panel* this, bool focus) {
   assert (this != NULL);

   int itemCount = Vector_size(this->items);
   int scrollH = this->scrollH;
   int y = this->y; int x = this->x;
   int first = this->scrollV;
   if (itemCount > this->h && first > itemCount - this->h) {
      first = itemCount - this->h;
      this->scrollV = first;
   }
   int last = MIN(itemCount, first + MIN(itemCount, this->h));
   if (this->selected < first) {
      first = this->selected;
      this->scrollV = first;
      this->needsRedraw = true;
   }
   if (this->selected >= last) {
      last = MIN(itemCount, this->selected + 1);
      first = MAX(0, last - this->h);
      this->scrollV = first;
      this->needsRedraw = true;
   }
   assert(first >= 0);
   assert(last <= itemCount);

   int headerLen = RichString_sizeVal(this->header);
   if (headerLen > 0) {
      int attr = focus
               ? CRT_colors[PANEL_HEADER_FOCUS]
               : CRT_colors[PANEL_HEADER_UNFOCUS];
      attrset(attr);
      mvhline(y, x, ' ', this->w);
      if (scrollH < headerLen) {
         RichString_printoffnVal(this->header, y, x, scrollH,
            MIN(headerLen - scrollH, this->w));
      }
      attrset(CRT_colors[RESET_COLOR]);
      y++;
   }
   
   int highlight = focus
                 ? CRT_colors[PANEL_HIGHLIGHT_FOCUS]
                 : CRT_colors[PANEL_HIGHLIGHT_UNFOCUS];

   if (this->needsRedraw) {

      for(int i = first, j = 0; j < this->h && i < last; i++, j++) {
         Object* itemObj = Vector_get(this->items, i);
         RichString_begin(item);
         itemObj->display(itemObj, &item);
         int itemLen = RichString_sizeVal(item);
         int amt = MIN(itemLen - scrollH, this->w);
         if (i == this->selected) {
            attrset(highlight);
            RichString_setAttr(&item, highlight);
            mvhline(y + j, x+0, ' ', this->w);
            if (amt > 0)
               RichString_printoffnVal(item, y+j, x+0, scrollH, amt);
            attrset(CRT_colors[RESET_COLOR]);
         } else {
            mvhline(y+j, x+0, ' ', this->w);
            if (amt > 0)
               RichString_printoffnVal(item, y+j, x+0, scrollH, amt);
         }
         RichString_end(item);
      }
      for (int i = y + (last - first); i < y + this->h; i++)
         mvhline(i, x+0, ' ', this->w);
      this->needsRedraw = false;

   } else {
      Object* oldObj = Vector_get(this->items, this->oldSelected);
      RichString_begin(old);
      oldObj->display(oldObj, &old);
      int oldLen = RichString_sizeVal(old);
      Object* newObj = Vector_get(this->items, this->selected);
      RichString_begin(new);
      newObj->display(newObj, &new);
      int newLen = RichString_sizeVal(new);
      mvhline(y+ this->oldSelected - this->scrollV, x+0, ' ', this->w);
      if (scrollH < oldLen)
         RichString_printoffnVal(old, y+this->oldSelected - this->scrollV, x,
            this->scrollH, MIN(oldLen - scrollH, this->w));
      attrset(highlight);
      mvhline(y+this->selected - this->scrollV, x+0, ' ', this->w);
      RichString_setAttr(&new, highlight);
      if (scrollH < newLen)
         RichString_printoffnVal(new, y+this->selected - this->scrollV, x,
            this->scrollH, MIN(newLen - scrollH, this->w));
      attrset(CRT_colors[RESET_COLOR]);
      RichString_end(new);
      RichString_end(old);
   }
   this->oldSelected = this->selected;
   move(0, 0);
}

bool Panel_onKey(Panel* this, int key) {
   assert (this != NULL);
   switch (key) {
   case KEY_DOWN:
   case KEY_CTRLN:
      if (this->selected + 1 < Vector_size(this->items))
         this->selected++;
      return true;
   case KEY_UP:
   case KEY_CTRLP:
      if (this->selected > 0)
         this->selected--;
      return true;
   #ifdef KEY_C_DOWN
   case KEY_C_DOWN:
      if (this->selected + 1 < Vector_size(this->items)) {
         this->selected++;
         if (this->scrollV < Vector_size(this->items) - this->h) {
            this->scrollV++;
            this->needsRedraw = true;
         }
      }
      return true;
   #endif
   #ifdef KEY_C_UP
   case KEY_C_UP:
      if (this->selected > 0) {
         this->selected--;
         if (this->scrollV > 0) {
            this->scrollV--;
            this->needsRedraw = true;
         }
      }
      return true;
   #endif
   case KEY_LEFT:
   case KEY_CTRLB:
      if (this->scrollH > 0) {
         this->scrollH -= 5;
         this->needsRedraw = true;
      }
      return true;
   case KEY_RIGHT:
   case KEY_CTRLF:
      this->scrollH += 5;
      this->needsRedraw = true;
      return true;
   case KEY_PPAGE:
      this->selected -= (this->h - 1);
      this->scrollV -= (this->h - 1);
      if (this->selected < 0)
         this->selected = 0;
      if (this->scrollV < 0)
         this->scrollV = 0;
      this->needsRedraw = true;
      return true;
   case KEY_NPAGE:
      this->selected += (this->h - 1);
      int size = Vector_size(this->items);
      if (this->selected >= size)
         this->selected = size - 1;
      this->scrollV += (this->h - 1);
      if (this->scrollV >= MAX(0, size - this->h))
         this->scrollV = MAX(0, size - this->h - 1);
      this->needsRedraw = true;
      return true;
   case KEY_HOME:
      this->selected = 0;
      return true;
   case KEY_END:
      this->selected = Vector_size(this->items) - 1;
      return true;
   }
   return false;
}


HandlerResult Panel_selectByTyping(Panel* this, int ch) {
   int size = Panel_size(this);
   if (!this->eventHandlerBuffer)
      this->eventHandlerBuffer = calloc(100, 1);

   if (isalnum(ch)) {
      int len = strlen(this->eventHandlerBuffer);
      if (len < 99) {
         this->eventHandlerBuffer[len] = ch;
         this->eventHandlerBuffer[len+1] = '\0';
      }
      for (int try = 0; try < 2; try++) {
         len = strlen(this->eventHandlerBuffer);
         for (int i = 0; i < size; i++) {
            char* cur = ((ListItem*) Panel_get(this, i))->value;
            while (*cur == ' ') cur++;
            if (strncasecmp(cur, this->eventHandlerBuffer, len) == 0) {
               Panel_setSelected(this, i);
               return HANDLED;
            }
         }
         this->eventHandlerBuffer[0] = ch;
         this->eventHandlerBuffer[1] = '\0';
      }
      return HANDLED;
   } else if (ch != ERR) {
      this->eventHandlerBuffer[0] = '\0';
   }
   if (ch == 13) {
      return BREAK_LOOP;
   }
   return IGNORED;
}

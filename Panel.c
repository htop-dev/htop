/*
htop - Panel.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"
#include "Panel.h"
#include "Vector.h"
#include "CRT.h"
#include "RichString.h"
#include "ListItem.h"

#include <math.h>
#include <stdbool.h>

#include "debug.h"
#include <assert.h>

#include <curses.h>
//#link curses

/*{

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
   this->items = Vector_new(type, owner, DEFAULT_SIZE, ListItem_compare);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
   this->header.len = 0;
   if (String_eq(CRT_termType, "linux"))
      this->scrollHAmount = 40;
   else
      this->scrollHAmount = 5;
}

void Panel_done(Panel* this) {
   assert (this != NULL);
   Vector_delete(this->items);
}

inline void Panel_setRichHeader(Panel* this, RichString header) {
   assert (this != NULL);

   this->header = header;
   this->needsRedraw = true;
}

inline void Panel_setHeader(Panel* this, char* header) {
   Panel_setRichHeader(this, RichString_quickString(CRT_colors[PANEL_HEADER_FOCUS], header));
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

   if (this->header.len > 0)
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

   return Vector_get(this->items, this->selected);
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

int Panel_getSize(Panel* this) {
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

   int first, last;
   int itemCount = Vector_size(this->items);
   int scrollH = this->scrollH;
   int y = this->y; int x = this->x;
   first = this->scrollV;

   if (this->h > itemCount) {
      last = this->scrollV + itemCount;
      move(y + last, x + 0);
   } else {
      last = MIN(itemCount, this->scrollV + this->h);
   }
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

   if (this->header.len > 0) {
      int attr = focus
               ? CRT_colors[PANEL_HEADER_FOCUS]
               : CRT_colors[PANEL_HEADER_UNFOCUS];
      attrset(attr);
      mvhline(y, x, ' ', this->w);
      if (scrollH < this->header.len) {
         RichString_printoffnVal(this->header, y, x, scrollH,
            MIN(this->header.len - scrollH, this->w));
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
         RichString itemRef;
         RichString_initVal(itemRef);
         itemObj->display(itemObj, &itemRef);
         int amt = MIN(itemRef.len - scrollH, this->w);
         if (i == this->selected) {
            attrset(highlight);
            RichString_setAttr(&itemRef, highlight);
            mvhline(y + j, x+0, ' ', this->w);
            if (amt > 0)
               RichString_printoffnVal(itemRef, y+j, x+0, scrollH, amt);
            attrset(CRT_colors[RESET_COLOR]);
         } else {
            mvhline(y+j, x+0, ' ', this->w);
            if (amt > 0)
               RichString_printoffnVal(itemRef, y+j, x+0, scrollH, amt);
         }
      }
      for (int i = y + (last - first); i < y + this->h; i++)
         mvhline(i, x+0, ' ', this->w);
      this->needsRedraw = false;

   } else {
      Object* oldObj = Vector_get(this->items, this->oldSelected);
      RichString oldRef;
      RichString_initVal(oldRef);
      oldObj->display(oldObj, &oldRef);
      Object* newObj = Vector_get(this->items, this->selected);
      RichString newRef;
      RichString_initVal(newRef);
      newObj->display(newObj, &newRef);
      mvhline(y+ this->oldSelected - this->scrollV, x+0, ' ', this->w);
      if (scrollH < oldRef.len)
         RichString_printoffnVal(oldRef, y+this->oldSelected - this->scrollV, x,
            this->scrollH, MIN(oldRef.len - scrollH, this->w));
      attrset(highlight);
      mvhline(y+this->selected - this->scrollV, x+0, ' ', this->w);
      RichString_setAttr(&newRef, highlight);
      if (scrollH < newRef.len)
         RichString_printoffnVal(newRef, y+this->selected - this->scrollV, x,
            this->scrollH, MIN(newRef.len - scrollH, this->w));
      attrset(CRT_colors[RESET_COLOR]);
   }
   this->oldSelected = this->selected;
   move(0, 0);
}

void Panel_onKey(Panel* this, int key) {
   assert (this != NULL);
   switch (key) {
   case KEY_DOWN:
      if (this->selected + 1 < Vector_size(this->items))
         this->selected++;
      break;
   case KEY_UP:
      if (this->selected > 0)
         this->selected--;
      break;
   case KEY_LEFT:
      if (this->scrollH > 0) {
         this->scrollH -= this->scrollHAmount;
         this->needsRedraw = true;
      }
      break;
   case KEY_RIGHT:
      this->scrollH += this->scrollHAmount;
      this->needsRedraw = true;
      break;
   case KEY_PPAGE:
      this->selected -= this->h;
      if (this->selected < 0)
         this->selected = 0;
      break;
   case KEY_NPAGE:
      this->selected += this->h;
      int size = Vector_size(this->items);
      if (this->selected >= size)
         this->selected = size - 1;
      break;
   case KEY_HOME:
      this->selected = 0;
      break;
   case KEY_END:
      this->selected = Vector_size(this->items) - 1;
      break;
   }
}

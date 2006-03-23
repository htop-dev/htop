/*
htop - ListBox.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"
#include "ListBox.h"
#include "TypedVector.h"
#include "CRT.h"
#include "RichString.h"

#include <math.h>
#include <stdbool.h>

#include "debug.h"
#include <assert.h>

#include <curses.h>
//#link curses

/*{

typedef struct ListBox_ ListBox;

typedef enum HandlerResult_ {
   HANDLED,
   IGNORED,
   BREAK_LOOP
} HandlerResult;

typedef HandlerResult(*ListBox_EventHandler)(ListBox*, int);

struct ListBox_ {
   Object super;
   int x, y, w, h;
   WINDOW* window;
   TypedVector* items;
   int selected;
   int scrollV, scrollH;
   int oldSelected;
   bool needsRedraw;
   RichString header;
   ListBox_EventHandler eventHandler;
};

extern char* LISTBOX_CLASS;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

/* private property */
char* LISTBOX_CLASS = "ListBox";

ListBox* ListBox_new(int x, int y, int w, int h, char* type, bool owner) {
   ListBox* this;
   this = malloc(sizeof(ListBox));
   ListBox_init(this, x, y, w, h, type, owner);
   return this;
}

void ListBox_delete(Object* cast) {
   ListBox* this = (ListBox*)cast;
   ListBox_done(this);
   free(this);
}

void ListBox_init(ListBox* this, int x, int y, int w, int h, char* type, bool owner) {
   Object* super = (Object*) this;
   super->class = LISTBOX_CLASS;
   super->delete = ListBox_delete;
   this->x = x;
   this->y = y;
   this->w = w;
   this->h = h;
   this->eventHandler = NULL;
   this->items = TypedVector_new(type, owner, DEFAULT_SIZE);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
   this->header.len = 0;
}

void ListBox_done(ListBox* this) {
   assert (this != NULL);
   RichString_delete(this->header);
   TypedVector_delete(this->items);
}

inline void ListBox_setRichHeader(ListBox* this, RichString header) {
   assert (this != NULL);

   if (this->header.len > 0) {
      RichString_delete(this->header);
   }
   this->header = header;
   this->needsRedraw = true;
}

inline void ListBox_setHeader(ListBox* this, char* header) {
   ListBox_setRichHeader(this, RichString_quickString(CRT_colors[PANEL_HEADER_FOCUS], header));
}

void ListBox_setEventHandler(ListBox* this, ListBox_EventHandler eh) {
   this->eventHandler = eh;
}

void ListBox_move(ListBox* this, int x, int y) {
   assert (this != NULL);

   this->x = x;
   this->y = y;
   this->needsRedraw = true;
}

void ListBox_resize(ListBox* this, int w, int h) {
   assert (this != NULL);

   if (this->header.len > 0)
      h--;
   this->w = w;
   this->h = h;
   this->needsRedraw = true;
}

void ListBox_prune(ListBox* this) {
   assert (this != NULL);

   TypedVector_prune(this->items);
   this->scrollV = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
}

void ListBox_add(ListBox* this, Object* o) {
   assert (this != NULL);

   TypedVector_add(this->items, o);
   this->needsRedraw = true;
}

void ListBox_insert(ListBox* this, int i, Object* o) {
   assert (this != NULL);

   TypedVector_insert(this->items, i, o);
   this->needsRedraw = true;
}

void ListBox_set(ListBox* this, int i, Object* o) {
   assert (this != NULL);

   TypedVector_set(this->items, i, o);
}

Object* ListBox_get(ListBox* this, int i) {
   assert (this != NULL);

   return TypedVector_get(this->items, i);
}

Object* ListBox_remove(ListBox* this, int i) {
   assert (this != NULL);

   this->needsRedraw = true;
   Object* removed = TypedVector_remove(this->items, i);
   if (this->selected > 0 && this->selected >= TypedVector_size(this->items))
      this->selected--;
   return removed;
}

Object* ListBox_getSelected(ListBox* this) {
   assert (this != NULL);

   return TypedVector_get(this->items, this->selected);
}

void ListBox_moveSelectedUp(ListBox* this) {
   assert (this != NULL);

   TypedVector_moveUp(this->items, this->selected);
   if (this->selected > 0)
      this->selected--;
}

void ListBox_moveSelectedDown(ListBox* this) {
   assert (this != NULL);

   TypedVector_moveDown(this->items, this->selected);
   if (this->selected + 1 < TypedVector_size(this->items))
      this->selected++;
}

int ListBox_getSelectedIndex(ListBox* this) {
   assert (this != NULL);

   return this->selected;
}

int ListBox_getSize(ListBox* this) {
   assert (this != NULL);

   return TypedVector_size(this->items);
}

void ListBox_setSelected(ListBox* this, int selected) {
   assert (this != NULL);

   selected = MAX(0, MIN(TypedVector_size(this->items) - 1, selected));
   this->selected = selected;
}

void ListBox_draw(ListBox* this, bool focus) {
   assert (this != NULL);

   int first, last;
   int itemCount = TypedVector_size(this->items);
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
         mvaddchnstr(y, x, this->header.chstr + scrollH,
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
         Object* itemObj = TypedVector_get(this->items, i);
         RichString itemRef = RichString_new();
         itemObj->display(itemObj, &itemRef);
         int amt = MIN(itemRef.len - scrollH, this->w);
         if (i == this->selected) {
            attrset(highlight);
            RichString_setAttr(&itemRef, highlight);
            mvhline(y + j, x+0, ' ', this->w);
            if (amt > 0)
               mvaddchnstr(y+j, x+0, itemRef.chstr + scrollH, amt);
            attrset(CRT_colors[RESET_COLOR]);
         } else {
            mvhline(y+j, x+0, ' ', this->w);
            if (amt > 0)
               mvaddchnstr(y+j, x+0, itemRef.chstr + scrollH, amt);
         }
      }
      for (int i = y + (last - first); i < y + this->h; i++)
         mvhline(i, x+0, ' ', this->w);
      this->needsRedraw = false;

   } else {
      Object* oldObj = TypedVector_get(this->items, this->oldSelected);
      RichString oldRef = RichString_new();
      oldObj->display(oldObj, &oldRef);
      Object* newObj = TypedVector_get(this->items, this->selected);
      RichString newRef = RichString_new();
      newObj->display(newObj, &newRef);
      mvhline(y+ this->oldSelected - this->scrollV, x+0, ' ', this->w);
      if (scrollH < oldRef.len)
         mvaddchnstr(y+ this->oldSelected - this->scrollV, x+0, oldRef.chstr + this->scrollH, MIN(oldRef.len - scrollH, this->w));
      attrset(highlight);
      mvhline(y+this->selected - this->scrollV, x+0, ' ', this->w);
      RichString_setAttr(&newRef, highlight);
      if (scrollH < newRef.len)
         mvaddchnstr(y+this->selected - this->scrollV, x+0, newRef.chstr + this->scrollH, MIN(newRef.len - scrollH, this->w));
      attrset(CRT_colors[RESET_COLOR]);
   }
   this->oldSelected = this->selected;
   move(0, 0);
}

void ListBox_onKey(ListBox* this, int key) {
   assert (this != NULL);
   switch (key) {
   case KEY_DOWN:
      if (this->selected + 1 < TypedVector_size(this->items))
         this->selected++;
      break;
   case KEY_UP:
      if (this->selected > 0)
         this->selected--;
      break;
   case KEY_LEFT:
      if (this->scrollH > 0) {
         this->scrollH -= 5;
         this->needsRedraw = true;
      }
      break;
   case KEY_RIGHT:
      this->scrollH += 5;
      this->needsRedraw = true;
      break;
   case KEY_PPAGE:
      this->selected -= this->h;
      if (this->selected < 0)
         this->selected = 0;
      break;
   case KEY_NPAGE:
      this->selected += this->h;
      int size = TypedVector_size(this->items);
      if (this->selected >= size)
         this->selected = size - 1;
      break;
   case KEY_HOME:
      this->selected = 0;
      break;
   case KEY_END:
      this->selected = TypedVector_size(this->items) - 1;
      break;
   }
}

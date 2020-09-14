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
#include "StringUtils.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

PanelClass Panel_class = {
   .super = {
      .extends = Class(Object),
      .delete = Panel_delete
   },
   .eventHandler = Panel_selectByTyping,
};

Panel* Panel_new(int x, int y, int w, int h, bool owner, ObjectClass* type, FunctionBar* fuBar) {
   Panel* this;
   this = xMalloc(sizeof(Panel));
   Object_setClass(this, Class(Panel));
   Panel_init(this, x, y, w, h, type, owner, fuBar);
   return this;
}

void Panel_delete(Object* cast) {
   Panel* this = (Panel*)cast;
   Panel_done(this);
   free(this);
}

void Panel_init(Panel* this, int x, int y, int w, int h, ObjectClass* type, bool owner, FunctionBar* fuBar) {
   this->x = x;
   this->y = y;
   this->w = w;
   this->h = h;
   this->eventHandlerState = NULL;
   this->items = Vector_new(type, owner, DEFAULT_SIZE);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
   RichString_beginAllocated(this->header);
   this->defaultBar = fuBar;
   this->currentBar = fuBar;
   this->selectionColor = CRT_colors[PANEL_SELECTION_FOCUS];
}

void Panel_done(Panel* this) {
   assert (this != NULL);
   free(this->eventHandlerState);
   Vector_delete(this->items);
   FunctionBar_delete(this->defaultBar);
   RichString_end(this->header);
}

void Panel_setSelectionColor(Panel* this, int color) {
   this->selectionColor = color;
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

   int size = Vector_size(this->items);
   if (selected >= size) {
      selected = size - 1;
   }
   if (selected < 0)
      selected = 0;
   this->selected = selected;
   if (Panel_eventHandlerFn(this)) {
      Panel_eventHandler(this, EVENT_SET_SELECTED);
   }
}

void Panel_splice(Panel *this, Vector* from) {
   assert (this != NULL);
   assert (from != NULL);

   Vector_splice(this->items, from);
   this->needsRedraw = true;
}

void Panel_draw(Panel* this, bool focus) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   int scrollH = this->scrollH;
   int y = this->y;
   int x = this->x;
   int h = this->h;

   int headerLen = RichString_sizeVal(this->header);
   if (headerLen > 0) {
      int attr = focus
               ? CRT_colors[PANEL_HEADER_FOCUS]
               : CRT_colors[PANEL_HEADER_UNFOCUS];
      attrset(attr);
      mvhline(y, x, ' ', this->w);
      if (scrollH < headerLen) {
         RichString_printoffnVal(this->header, y, x, scrollH,
            MINIMUM(headerLen - scrollH, this->w));
      }
      attrset(CRT_colors[RESET_COLOR]);
      y++;
   }

   // ensure scroll area is on screen
   if (this->scrollV < 0) {
      this->scrollV = 0;
      this->needsRedraw = true;
   } else if (this->scrollV >= size) {
      this->scrollV = MAXIMUM(size - 1, 0);
      this->needsRedraw = true;
   }
   // ensure selection is on screen
   if (this->selected < this->scrollV) {
      this->scrollV = this->selected;
      this->needsRedraw = true;
   } else if (this->selected >= this->scrollV + h) {
      this->scrollV = this->selected - h + 1;
      this->needsRedraw = true;
   }

   int first = this->scrollV;
   int upTo = MINIMUM(first + h, size);

   int selectionColor = focus
                 ? this->selectionColor
                 : CRT_colors[PANEL_SELECTION_UNFOCUS];

   if (this->needsRedraw) {
      int line = 0;
      for(int i = first; line < h && i < upTo; i++) {
         Object* itemObj = Vector_get(this->items, i);
         assert(itemObj); if(!itemObj) continue;
         RichString_begin(item);
         Object_display(itemObj, &item);
         int itemLen = RichString_sizeVal(item);
         int amt = MINIMUM(itemLen - scrollH, this->w);
         bool selected = (i == this->selected);
         if (selected) {
            attrset(selectionColor);
            RichString_setAttr(&item, selectionColor);
            this->selectedLen = itemLen;
         }
         mvhline(y + line, x, ' ', this->w);
         if (amt > 0)
            RichString_printoffnVal(item, y + line, x, scrollH, amt);
         if (selected)
            attrset(CRT_colors[RESET_COLOR]);
         RichString_end(item);
         line++;
      }
      while (line < h) {
         mvhline(y + line, x, ' ', this->w);
         line++;
      }
      this->needsRedraw = false;

   } else {
      Object* oldObj = Vector_get(this->items, this->oldSelected);
      assert(oldObj);
      RichString_begin(old);
      Object_display(oldObj, &old);
      int oldLen = RichString_sizeVal(old);
      Object* newObj = Vector_get(this->items, this->selected);
      RichString_begin(new);
      Object_display(newObj, &new);
      int newLen = RichString_sizeVal(new);
      this->selectedLen = newLen;
      mvhline(y+ this->oldSelected - first, x+0, ' ', this->w);
      if (scrollH < oldLen)
         RichString_printoffnVal(old, y+this->oldSelected - first, x,
            scrollH, MINIMUM(oldLen - scrollH, this->w));
      attrset(selectionColor);
      mvhline(y+this->selected - first, x+0, ' ', this->w);
      RichString_setAttr(&new, selectionColor);
      if (scrollH < newLen)
         RichString_printoffnVal(new, y+this->selected - first, x,
            scrollH, MINIMUM(newLen - scrollH, this->w));
      attrset(CRT_colors[RESET_COLOR]);
      RichString_end(new);
      RichString_end(old);
   }
   this->oldSelected = this->selected;
   move(0, 0);
}

bool Panel_onKey(Panel* this, int key) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   switch (key) {
   case KEY_DOWN:
   case KEY_CTRL('N'):
      this->selected++;
      break;
   case KEY_UP:
   case KEY_CTRL('P'):
      this->selected--;
      break;
   #ifdef KEY_C_DOWN
   case KEY_C_DOWN:
      this->selected++;
      break;
   #endif
   #ifdef KEY_C_UP
   case KEY_C_UP:
      this->selected--;
      break;
   #endif
   case KEY_LEFT:
   case KEY_CTRL('B'):
      if (this->scrollH > 0) {
         this->scrollH -= MAXIMUM(CRT_scrollHAmount, 0);
         this->needsRedraw = true;
      }
      break;
   case KEY_RIGHT:
   case KEY_CTRL('F'):
      this->scrollH += CRT_scrollHAmount;
      this->needsRedraw = true;
      break;
   case KEY_PPAGE:
      this->selected -= (this->h - 1);
      this->scrollV = MAXIMUM(0, this->scrollV - this->h + 1);
      this->needsRedraw = true;
      break;
   case KEY_NPAGE:
      this->selected += (this->h - 1);
      this->scrollV = MAXIMUM(0, MINIMUM(Vector_size(this->items) - this->h,
                                 this->scrollV + this->h - 1));
      this->needsRedraw = true;
      break;
   case KEY_WHEELUP:
      this->selected -= CRT_scrollWheelVAmount;
      this->scrollV -= CRT_scrollWheelVAmount;
      this->needsRedraw = true;
      break;
   case KEY_WHEELDOWN:
   {
      this->selected += CRT_scrollWheelVAmount;
      this->scrollV += CRT_scrollWheelVAmount;
      if (this->scrollV > Vector_size(this->items) - this->h) {
         this->scrollV = Vector_size(this->items) - this->h;
      }
      this->needsRedraw = true;
      break;
   }
   case KEY_HOME:
      this->selected = 0;
      break;
   case KEY_END:
      this->selected = size - 1;
      break;
   case KEY_CTRL('A'):
   case '^':
      this->scrollH = 0;
      this->needsRedraw = true;
      break;
   case KEY_CTRL('E'):
   case '$':
      this->scrollH = MAXIMUM(this->selectedLen - this->w, 0);
      this->needsRedraw = true;
      break;
   default:
      return false;
   }

   // ensure selection within bounds
   if (this->selected < 0 || size == 0) {
      this->selected = 0;
      this->needsRedraw = true;
   } else if (this->selected >= size) {
      this->selected = size - 1;
      this->needsRedraw = true;
   }
   return true;
}


HandlerResult Panel_selectByTyping(Panel* this, int ch) {
   int size = Panel_size(this);
   if (!this->eventHandlerState)
      this->eventHandlerState = xCalloc(100, sizeof(char));
   char* buffer = this->eventHandlerState;

   if (ch > 0 && ch < 255 && isalnum(ch)) {
      int len = strlen(buffer);
      if (len < 99) {
         buffer[len] = ch;
         buffer[len+1] = '\0';
      }
      for (int try = 0; try < 2; try++) {
         len = strlen(buffer);
         for (int i = 0; i < size; i++) {
            char* cur = ((ListItem*) Panel_get(this, i))->value;
            while (*cur == ' ') cur++;
            if (strncasecmp(cur, buffer, len) == 0) {
               Panel_setSelected(this, i);
               return HANDLED;
            }
         }
         // if current word did not match,
         // retry considering the character the start of a new word.
         buffer[0] = ch;
         buffer[1] = '\0';
      }
      return HANDLED;
   } else if (ch != ERR) {
      buffer[0] = '\0';
   }
   if (ch == 13) {
      return BREAK_LOOP;
   }
   return IGNORED;
}

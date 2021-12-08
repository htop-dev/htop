/*
htop - Panel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "CRT.h"
#include "ListItem.h"
#include "Macros.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"


const PanelClass Panel_class = {
   .super = {
      .extends = Class(Object),
      .delete = Panel_delete
   },
   .eventHandler = Panel_selectByTyping,
};

Panel* Panel_new(int x, int y, int w, int h, const ObjectClass* type, bool owner, FunctionBar* fuBar) {
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

void Panel_init(Panel* this, int x, int y, int w, int h, const ObjectClass* type, bool owner, FunctionBar* fuBar) {
   this->x = x;
   this->y = y;
   this->w = w;
   this->h = h;
   this->cursorX = 0;
   this->cursorY = 0;
   this->eventHandlerState = NULL;
   this->items = Vector_new(type, owner, DEFAULT_SIZE);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->selectedLen = 0;
   this->needsRedraw = true;
   this->cursorOn = false;
   this->wasFocus = false;
   RichString_beginAllocated(this->header);
   this->defaultBar = fuBar;
   this->currentBar = fuBar;
   this->selectionColorId = PANEL_SELECTION_FOCUS;
}

void Panel_done(Panel* this) {
   assert (this != NULL);
   free(this->eventHandlerState);
   Vector_delete(this->items);
   FunctionBar_delete(this->defaultBar);
   RichString_delete(&this->header);
}

void Panel_setCursorToSelection(Panel* this) {
   this->cursorY = this->y + this->selected - this->scrollV + 1;
   this->cursorX = this->x + this->selectedLen - this->scrollH;
}

void Panel_setSelectionColor(Panel* this, ColorElements colorId) {
   this->selectionColorId = colorId;
}

inline void Panel_setHeader(Panel* this, const char* header) {
   RichString_writeWide(&(this->header), CRT_colors[PANEL_HEADER_FOCUS], header);
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
   if (this->selected > 0 && this->selected >= Vector_size(this->items)) {
      this->selected--;
   }

   return removed;
}

Object* Panel_getSelected(Panel* this) {
   assert (this != NULL);
   if (Vector_size(this->items) > 0) {
      return Vector_get(this->items, this->selected);
   } else {
      return NULL;
   }
}

void Panel_moveSelectedUp(Panel* this) {
   assert (this != NULL);

   Vector_moveUp(this->items, this->selected);
   if (this->selected > 0) {
      this->selected--;
   }
}

void Panel_moveSelectedDown(Panel* this) {
   assert (this != NULL);

   Vector_moveDown(this->items, this->selected);
   if (this->selected + 1 < Vector_size(this->items)) {
      this->selected++;
   }
}

int Panel_getSelectedIndex(const Panel* this) {
   assert (this != NULL);

   return this->selected;
}

int Panel_size(const Panel* this) {
   assert (this != NULL);

   return Vector_size(this->items);
}

void Panel_setSelected(Panel* this, int selected) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   if (selected >= size) {
      selected = size - 1;
   }
   if (selected < 0) {
      selected = 0;
   }
   this->selected = selected;
   if (Panel_eventHandlerFn(this)) {
      Panel_eventHandler(this, EVENT_SET_SELECTED);
   }
}

void Panel_splice(Panel* this, Vector* from) {
   assert (this != NULL);
   assert (from != NULL);

   Vector_splice(this->items, from);
   this->needsRedraw = true;
}

void Panel_draw(Panel* this, bool force_redraw, bool focus, bool highlightSelected, bool hideFunctionBar) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   int scrollH = this->scrollH;
   int y = this->y;
   int x = this->x;
   int h = this->h;

   if (hideFunctionBar)
      h++;

   const int header_attr = focus
                         ? CRT_colors[PANEL_HEADER_FOCUS]
                         : CRT_colors[PANEL_HEADER_UNFOCUS];
   if (force_redraw) {
      if (Panel_printHeaderFn(this))
         Panel_printHeader(this);
      else
         RichString_setAttr(&this->header, header_attr);
   }
   int headerLen = RichString_sizeVal(this->header);
   if (headerLen > 0) {
      attrset(header_attr);
      mvhline(y, x, ' ', this->w);
      if (scrollH < headerLen) {
         RichString_printoffnVal(this->header, y, x, scrollH,
            MINIMUM(headerLen - scrollH, this->w));
      }
      attrset(CRT_colors[RESET_COLOR]);
      y++;
      h--;
   }

   // ensure scroll area is on screen
   if (this->scrollV < 0) {
      this->scrollV = 0;
      this->needsRedraw = true;
   } else if (this->scrollV > size - h) {
      this->scrollV = MAXIMUM(size - h, 0);
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
                      ? CRT_colors[this->selectionColorId]
                      : CRT_colors[PANEL_SELECTION_UNFOCUS];

   if (this->needsRedraw || force_redraw) {
      int line = 0;
      for (int i = first; line < h && i < upTo; i++) {
         const Object* itemObj = Vector_get(this->items, i);
         RichString_begin(item);
         Object_display(itemObj, &item);
         int itemLen = RichString_sizeVal(item);
         int amt = MINIMUM(itemLen - scrollH, this->w);
         if (highlightSelected && i == this->selected) {
            item.highlightAttr = selectionColor;
         }
         if (item.highlightAttr) {
            attrset(item.highlightAttr);
            RichString_setAttr(&item, item.highlightAttr);
            this->selectedLen = itemLen;
         }
         mvhline(y + line, x, ' ', this->w);
         if (amt > 0)
            RichString_printoffnVal(item, y + line, x, scrollH, amt);
         if (item.highlightAttr)
            attrset(CRT_colors[RESET_COLOR]);
         RichString_delete(&item);
         line++;
      }
      while (line < h) {
         mvhline(y + line, x, ' ', this->w);
         line++;
      }

   } else {
      const Object* oldObj = Vector_get(this->items, this->oldSelected);
      RichString_begin(old);
      Object_display(oldObj, &old);
      int oldLen = RichString_sizeVal(old);
      const Object* newObj = Vector_get(this->items, this->selected);
      RichString_begin(new);
      Object_display(newObj, &new);
      int newLen = RichString_sizeVal(new);
      this->selectedLen = newLen;
      mvhline(y + this->oldSelected - first, x + 0, ' ', this->w);
      if (scrollH < oldLen)
         RichString_printoffnVal(old, y + this->oldSelected - first, x,
            scrollH, MINIMUM(oldLen - scrollH, this->w));
      attrset(selectionColor);
      mvhline(y + this->selected - first, x + 0, ' ', this->w);
      RichString_setAttr(&new, selectionColor);
      if (scrollH < newLen)
         RichString_printoffnVal(new, y + this->selected - first, x,
            scrollH, MINIMUM(newLen - scrollH, this->w));
      attrset(CRT_colors[RESET_COLOR]);
      RichString_delete(&new);
      RichString_delete(&old);
   }

   if (focus && (this->needsRedraw || force_redraw || !this->wasFocus)) {
      if (Panel_drawFunctionBarFn(this))
         Panel_drawFunctionBar(this, hideFunctionBar);
      else if (!hideFunctionBar)
         FunctionBar_draw(this->currentBar);
   }

   this->oldSelected = this->selected;
   this->wasFocus = focus;
   this->needsRedraw = false;
}

static int Panel_headerHeight(const Panel* this) {
   return RichString_sizeVal(this->header) > 0 ? 1 : 0;
}

bool Panel_onKey(Panel* this, int key) {
   assert (this != NULL);

   const int size = Vector_size(this->items);

   #define PANEL_SCROLL(amount)                                                                                     \
   do {                                                                                                             \
      this->selected += (amount);                                                                                   \
      this->scrollV = CLAMP(this->scrollV + (amount), 0, MAXIMUM(0, (size - this->h - Panel_headerHeight(this))));  \
      this->needsRedraw = true;                                                                                     \
   } while (0)

   switch (key) {
   case KEY_DOWN:
   case KEY_CTRL('N'):
   #ifdef KEY_C_DOWN
   case KEY_C_DOWN:
   #endif
      this->selected++;
      break;

   case KEY_UP:
   case KEY_CTRL('P'):
   #ifdef KEY_C_UP
   case KEY_C_UP:
   #endif
      this->selected--;
      break;

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
      PANEL_SCROLL(-(this->h - Panel_headerHeight(this)));
      break;

   case KEY_NPAGE:
      PANEL_SCROLL(+(this->h - Panel_headerHeight(this)));
      break;

   case KEY_WHEELUP:
      PANEL_SCROLL(-CRT_scrollWheelVAmount);
      break;

   case KEY_WHEELDOWN:
      PANEL_SCROLL(+CRT_scrollWheelVAmount);
      break;

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

   #undef PANEL_SCROLL

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

   if (0 < ch && ch < 255 && isgraph((unsigned char)ch)) {
      int len = strlen(buffer);
      if (!len) {
         if ('/' == ch) {
            ch = '\001';
         } else if ('q' == ch) {
            return BREAK_LOOP;
         }
      } else if (1 == len && '\001' == buffer[0]) {
         len--;
      }

      if (len < 99) {
         buffer[len] = (char) ch;
         buffer[len + 1] = '\0';
      }

      for (int try = 0; try < 2; try++) {
         len = strlen(buffer);
         for (int i = 0; i < size; i++) {
            const char* cur = ((ListItem*) Panel_get(this, i))->value;
            while (*cur == ' ') cur++;
            if (strncasecmp(cur, buffer, len) == 0) {
               Panel_setSelected(this, i);
               return HANDLED;
            }
         }

         // if current word did not match,
         // retry considering the character the start of a new word.
         buffer[0] = (char) ch;
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

int Panel_getCh(Panel* this) {
   if (this->cursorOn) {
      move(this->cursorY, this->cursorX);
      curs_set(1);
   } else {
      curs_set(0);
   }
#ifdef HAVE_SET_ESCDELAY
   set_escdelay(25);
#endif
   return getch();
}

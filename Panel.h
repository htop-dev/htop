#ifndef HEADER_Panel
#define HEADER_Panel
/*
htop - Panel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <assert.h>
#include <stdbool.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Object.h"
#include "RichString.h"
#include "Vector.h"


struct Panel_;
typedef struct Panel_ Panel;

typedef enum HandlerResult_ {
   HANDLED     = 0x01,
   IGNORED     = 0x02,
   BREAK_LOOP  = 0x04,
   REFRESH     = 0x08,
   REDRAW      = 0x10,
   RESCAN      = 0x20,
   RESIZE      = 0x40,
   SYNTH_KEY   = 0x80,
} HandlerResult;

#define EVENT_SET_SELECTED (-1)

#define EVENT_HEADER_CLICK(x_) (-10000 + (x_))
#define EVENT_IS_HEADER_CLICK(ev_) ((ev_) >= -10000 && (ev_) <= -9000)
#define EVENT_HEADER_CLICK_GET_X(ev_) ((ev_) + 10000)

#define EVENT_SCREEN_TAB_CLICK(x_) (-20000 + (x_))
#define EVENT_IS_SCREEN_TAB_CLICK(ev_) ((ev_) >= -20000 && (ev_) < -10000)
#define EVENT_SCREEN_TAB_GET_X(ev_) ((ev_) + 20000)

typedef HandlerResult (*Panel_EventHandler)(Panel*, int);
typedef void (*Panel_DrawFunctionBar)(Panel*, bool);
typedef void (*Panel_PrintHeader)(Panel*);

typedef struct PanelClass_ {
   const ObjectClass super;
   const Panel_EventHandler eventHandler;
   const Panel_DrawFunctionBar drawFunctionBar;
   const Panel_PrintHeader printHeader;
} PanelClass;

#define As_Panel(this_)                        ((const PanelClass*)((this_)->super.klass))
#define Panel_eventHandlerFn(this_)            As_Panel(this_)->eventHandler
#define Panel_eventHandler(this_, ev_)         (assert(As_Panel(this_)->eventHandler), As_Panel(this_)->eventHandler((Panel*)(this_), ev_))
#define Panel_drawFunctionBarFn(this_)         As_Panel(this_)->drawFunctionBar
#define Panel_drawFunctionBar(this_, hideFB_)  (assert(As_Panel(this_)->drawFunctionBar), As_Panel(this_)->drawFunctionBar((Panel*)(this_), hideFB_))
#define Panel_printHeaderFn(this_)             As_Panel(this_)->printHeader
#define Panel_printHeader(this_)               (assert(As_Panel(this_)->printHeader), As_Panel(this_)->printHeader((Panel*)(this_)))

struct Panel_ {
   Object super;
   int x, y, w, h;
   int cursorX, cursorY;
   Vector* items;
   int selected;
   int oldSelected;
   int selectedLen;
   void* eventHandlerState;
   int scrollV;
   int scrollH;
   bool needsRedraw;
   bool cursorOn;
   bool wasFocus;
   FunctionBar* currentBar;
   FunctionBar* defaultBar;
   RichString header;
   ColorElements selectionColorId;
};

#define Panel_setDefaultBar(this_) do { (this_)->currentBar = (this_)->defaultBar; } while (0)

#define KEY_CTRL(l) ((l)-'A'+1)

extern const PanelClass Panel_class;

Panel* Panel_new(int x, int y, int w, int h, const ObjectClass* type, bool owner, FunctionBar* fuBar);

void Panel_delete(Object* cast);

void Panel_init(Panel* this, int x, int y, int w, int h, const ObjectClass* type, bool owner, FunctionBar* fuBar);

void Panel_done(Panel* this);

void Panel_setCursorToSelection(Panel* this);

void Panel_setSelectionColor(Panel* this, ColorElements colorId);

void Panel_setHeader(Panel* this, const char* header);

void Panel_move(Panel* this, int x, int y);

void Panel_resize(Panel* this, int w, int h);

void Panel_prune(Panel* this);

void Panel_add(Panel* this, Object* o);

void Panel_insert(Panel* this, int i, Object* o);

void Panel_set(Panel* this, int i, Object* o);

Object* Panel_get(Panel* this, int i);

Object* Panel_remove(Panel* this, int i);

Object* Panel_getSelected(Panel* this);

void Panel_moveSelectedUp(Panel* this);

void Panel_moveSelectedDown(Panel* this);

int Panel_getSelectedIndex(const Panel* this);

int Panel_size(const Panel* this);

void Panel_setSelected(Panel* this, int selected);

void Panel_draw(Panel* this, bool force_redraw, bool focus, bool highlightSelected, bool hideFunctionBar);

void Panel_splice(Panel* this, Vector* from);

bool Panel_onKey(Panel* this, int key);

HandlerResult Panel_selectByTyping(Panel* this, int ch);

int Panel_getCh(Panel* this);

#endif

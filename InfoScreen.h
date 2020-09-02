#ifndef HEADER_InfoScreen
#define HEADER_InfoScreen

#include "Process.h"
#include "Panel.h"
#include "FunctionBar.h"
#include "IncSet.h"

typedef struct InfoScreen_ InfoScreen;

typedef void(*InfoScreen_Scan)(InfoScreen*);
typedef void(*InfoScreen_Draw)(InfoScreen*);
typedef void(*InfoScreen_OnErr)(InfoScreen*);
typedef bool(*InfoScreen_OnKey)(InfoScreen*, int);

typedef struct InfoScreenClass_ {
   ObjectClass super;
   const InfoScreen_Scan scan;
   const InfoScreen_Draw draw;
   const InfoScreen_OnErr onErr;
   const InfoScreen_OnKey onKey;
} InfoScreenClass;

#define As_InfoScreen(this_)          ((InfoScreenClass*)(((InfoScreen*)(this_))->super.klass))
#define InfoScreen_scan(this_)        As_InfoScreen(this_)->scan((InfoScreen*)(this_))
#define InfoScreen_draw(this_)        As_InfoScreen(this_)->draw((InfoScreen*)(this_))
#define InfoScreen_onErr(this_)       As_InfoScreen(this_)->onErr((InfoScreen*)(this_))
#define InfoScreen_onKey(this_, ch_)  As_InfoScreen(this_)->onKey((InfoScreen*)(this_), ch_)

struct InfoScreen_ {
   Object super;
   Process* process;
   Panel* display;
   FunctionBar* bar;
   IncSet* inc;
   Vector* lines;
};

InfoScreen* InfoScreen_init(InfoScreen* this, Process* process, FunctionBar* bar, int height, const char* panelHeader);

InfoScreen* InfoScreen_done(InfoScreen* this);

void InfoScreen_drawTitled(InfoScreen* this, const char* fmt, ...);

void InfoScreen_addLine(InfoScreen* this, const char* line);

void InfoScreen_appendLine(InfoScreen* this, const char* line);

void InfoScreen_run(InfoScreen* this);

#endif

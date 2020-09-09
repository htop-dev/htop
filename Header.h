#ifndef HEADER_Header
#define HEADER_Header
/*
htop - Header.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"
#include "Settings.h"
#include "Vector.h"

typedef struct Header_ {
   Vector** columns;
   Settings* settings;
   struct ProcessList_* pl;
   int nrColumns;
   int pad;
   int height;
} Header;

#define Header_forEachColumn(this_, i_) for (int (i_)=0; (i_) < (this_)->nrColumns; ++(i_))

Header* Header_new(struct ProcessList_* pl, Settings* settings, int nrColumns);

void Header_delete(Header* this);

void Header_populateFromSettings(Header* this);

void Header_writeBackToSettings(const Header* this);

MeterModeId Header_addMeterByName(Header* this, char* name, int column);

void Header_setMode(Header* this, int i, MeterModeId mode, int column);

Meter* Header_addMeterByClass(Header* this, MeterClass* type, int param, int column);

int Header_size(Header* this, int column);

char* Header_readMeterName(Header* this, int i, int column);

MeterModeId Header_readMeterMode(Header* this, int i, int column);

void Header_reinit(Header* this);

void Header_draw(const Header* this);

int Header_calculateHeight(Header* this);

#endif

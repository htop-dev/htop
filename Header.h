#ifndef HEADER_Header
#define HEADER_Header
/*
htop - Header.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"
#include "ProcessList.h"
#include "Settings.h"
#include "Vector.h"

typedef struct Header_ {
   Vector** columns;
   Settings* settings;
   ProcessList* pl;
   int nrColumns;
   int pad;
   int height;
} Header;

#define Header_forEachColumn(this_, i_) for (int (i_)=0; (i_) < (this_)->nrColumns; ++(i_))

Header* Header_new(ProcessList* pl, Settings* settings, int nrColumns);

void Header_delete(Header* this);

void Header_populateFromSettings(Header* this);

void Header_writeBackToSettings(const Header* this);

MeterModeId Header_addMeterByName(Header* this, const char* name, int column);

void Header_setMode(Header* this, int i, MeterModeId mode, int column);

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, unsigned int param, int column);

int Header_size(const Header* this, int column);

MeterModeId Header_readMeterMode(const Header* this, int i, int column);

void Header_reinit(Header* this);

void Header_draw(const Header* this);

void Header_updateData(Header* this);

int Header_calculateHeight(Header* this);

#endif

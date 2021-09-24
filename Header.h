#ifndef HEADER_Header
#define HEADER_Header
/*
htop - Header.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "HeaderLayout.h"
#include "Meter.h"
#include "ProcessList.h"
#include "Settings.h"
#include "Vector.h"


typedef struct Header_ {
   Vector** columns;
   Settings* settings;
   ProcessList* pl;
   HeaderLayout headerLayout;
   int pad;
   int height;
} Header;

#define Header_forEachColumn(this_, i_) for (size_t (i_)=0, H_fEC_numColumns_ = HeaderLayout_getColumns((this_)->headerLayout); (i_) < H_fEC_numColumns_; ++(i_))

Header* Header_new(ProcessList* pl, Settings* settings, HeaderLayout hLayout);

void Header_delete(Header* this);

void Header_setLayout(Header* this, HeaderLayout hLayout);

void Header_populateFromSettings(Header* this);

void Header_writeBackToSettings(const Header* this);

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, unsigned int param, unsigned int column);

void Header_reinit(Header* this);

void Header_draw(const Header* this);

void Header_updateData(Header* this);

int Header_calculateHeight(Header* this);

#endif

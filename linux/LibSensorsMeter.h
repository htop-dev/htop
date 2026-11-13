#ifndef HEADER_LibSensorsMeter
#define HEADER_LibSensorsMeter
/*
htop - linux/LibSensorsMeter.h
(C) 2026 Massimo Mazzariol
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Hashtable.h"
#include "Meter.h"
#include "RichString.h"


Hashtable* LibSensorsMeter_new(void);

void LibSensorsMeter_done(Hashtable* table);

void LibSensorsMeter_init(Meter* meter);

void LibSensorsMeter_updateValues(Meter* meter);

bool LibSensorsMeter_consumeSamplingRequest(void);

void LibSensorsMeter_display(const Meter* meter, RichString* out);

#endif

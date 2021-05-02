#ifndef HEADER_LibSensorsTempMeter
#define HEADER_LibSensorsTempMeter
/*
htop - LibSensorsTempMeter.h
(C) 2021 htop dev team
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

#ifdef HAVE_SENSORS_SENSORS_H
extern const MeterClass LibSensorsTempMeter_class;
#endif

#endif /* HEADER_LibSensorsTempMeter */

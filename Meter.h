#ifndef HEADER_Meter
#define HEADER_Meter
/*
htop - Meter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#include "ListItem.h"
#include "Machine.h"
#include "Macros.h"
#include "MeterMode.h"
#include "Object.h"


#define METER_TXTBUFFER_LEN 256
#define MAX_METER_GRAPHDATA_VALUES 32768

#define METER_BUFFER_CHECK(buffer, size, written)          \
   do {                                                    \
      if ((written) < 0 || (size_t)(written) >= (size)) {  \
         return;                                           \
      }                                                    \
      (buffer) += (written);                               \
      (size) -= (size_t)(written);                         \
   } while (0)

#define METER_BUFFER_APPEND_CHR(buffer, size, c)           \
   do {                                                    \
      if ((size) < 2) {                                    \
         return;                                           \
      }                                                    \
      *(buffer)++ = c;                                     \
      *(buffer) = '\0';                                    \
      (size)--;                                            \
      if ((size) == 0) {                                   \
         return;                                           \
      }                                                    \
   } while (0)


struct Meter_;
typedef struct Meter_ Meter;

typedef ATTR_NONNULL void (*Meter_Init)(Meter*);
typedef ATTR_NONNULL void (*Meter_Done)(Meter*);
typedef ATTR_NONNULL void (*Meter_UpdateMode)(Meter*, MeterModeId);
typedef ATTR_NONNULL void (*Meter_UpdateValues)(Meter*);
typedef ATTR_NONNULL void (*Meter_Draw)(Meter*, int, int, int);
typedef ATTR_NONNULL const char* (*Meter_GetCaption)(const Meter*);
typedef ATTR_NONNULL ATTR_ACCESS3_W(2, 3) void (*Meter_GetUiName)(const Meter*, char*, size_t);

typedef struct MeterClass_ {
   const ObjectClass super;
   const Meter_Init init;
   const Meter_Done done;
   const Meter_UpdateMode updateMode;
   const Meter_UpdateValues updateValues;
   const Meter_Draw draw;
   const Meter_GetCaption getCaption;
   const Meter_GetUiName getUiName;
   const MeterModeId defaultMode;
   const uint32_t supportedModes;          /* bitset of supported modes, 1<<mode_id */
   const double total;
   const int* const attributes;
   const char* const name;                 /* internal name of the meter, must not contain any space */
   const char* const uiName;               /* display name in header setup menu */
   const char* const caption;              /* prefix in the actual header */
   const char* const description;          /* optional meter description in header setup menu */
   const uint8_t maxItems;
   const bool isMultiColumn;               /* whether the meter draws multiple sub-columns (defaults to false) */

   /* Specifies how the meter is rendered in bar or graph mode:
      true: a percent bar or graph with 'total' representing 100% or maximum.
      false: the meter has no definite maximum; 'total' repesents initial
      maximum value while actual maximum is updated automatically. */
   const bool isPercentChart;
} MeterClass;

#define As_Meter(this_)                ((const MeterClass*)((this_)->super.klass))
#define Meter_initFn(this_)            As_Meter(this_)->init
#define Meter_init(this_)              As_Meter(this_)->init((Meter*)(this_))
#define Meter_done(this_)              As_Meter(this_)->done((Meter*)(this_))
#define Meter_updateModeFn(this_)      As_Meter(this_)->updateMode
#define Meter_updateMode(this_, m_)    As_Meter(this_)->updateMode((Meter*)(this_), m_)
#define Meter_drawFn(this_)            As_Meter(this_)->draw
#define Meter_doneFn(this_)            As_Meter(this_)->done
#define Meter_updateValues(this_)      As_Meter(this_)->updateValues((Meter*)(this_))
#define Meter_getUiNameFn(this_)       As_Meter(this_)->getUiName
#define Meter_getUiName(this_,n_,l_)   As_Meter(this_)->getUiName((const Meter*)(this_),n_,l_)
#define Meter_getCaptionFn(this_)      As_Meter(this_)->getCaption
#define Meter_getCaption(this_)        (Meter_getCaptionFn(this_) ? As_Meter(this_)->getCaption((const Meter*)(this_)) : (this_)->caption)
#define Meter_supportedModes(this_)    As_Meter(this_)->supportedModes
#define Meter_attributes(this_)        As_Meter(this_)->attributes
#define Meter_name(this_)              As_Meter(this_)->name
#define Meter_uiName(this_)            As_Meter(this_)->uiName
#define Meter_maxItems(this_)          As_Meter(this_)->maxItems
#define Meter_isMultiColumn(this_)     As_Meter(this_)->isMultiColumn
#define Meter_isPercentChart(this_)    As_Meter(this_)->isPercentChart

typedef struct GraphData_ {
   struct timeval time;
   size_t nValues;
   void* buffer;
} GraphData;

struct Meter_ {
   Object super;
   Meter_Draw draw;
   const Machine* host;

   char* caption;
   MeterModeId mode;
   unsigned int param;
   GraphData drawData;
   int h;
   int columnWidthCount;      /**< only used internally by the Header */
   uint8_t curItems;
   const int* curAttributes;
   char txtBuffer[METER_TXTBUFFER_LEN];
   double* values;
   double total;
   void* meterData;
};

typedef enum {
   RATESTATUS_DATA,
   RATESTATUS_INIT,
   RATESTATUS_NODATA,
   RATESTATUS_STALE
} MeterRateStatus;

extern const MeterClass Meter_class;

Meter* Meter_new(const Machine* host, unsigned int param, const MeterClass* type);

/* Converts 'value' in kibibytes into a human readable string.
   Example output strings: "0K", "1023K", "98.7M" and "1.23G" */
int Meter_humanUnit(char* buffer, double value, size_t size);

void Meter_delete(Object* cast);

void Meter_setCaption(Meter* this, const char* caption);

void Meter_setMode(Meter* this, MeterModeId modeIndex);

MeterModeId Meter_nextSupportedMode(const Meter* this);

ListItem* Meter_toListItem(const Meter* this, bool moving);

extern const MeterClass BlankMeter_class;

#endif

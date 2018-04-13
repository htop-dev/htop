#ifndef HEADER_Meter
#define HEADER_Meter
/*
htop - Meter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

#include "ListItem.h"
#include "Object.h"
#include "ProcessList.h"

#define GRAPH_NUM_RECORDS 256

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

typedef void(*Meter_Init)(Meter*);
typedef void(*Meter_Done)(Meter*);
typedef void(*Meter_UpdateMode)(Meter*, int);
typedef void(*Meter_UpdateValues)(Meter*, char*, size_t);
typedef void(*Meter_Draw)(Meter*, int, int, int);

typedef struct MeterClass_ {
   const ObjectClass super;
   const Meter_Init init;
   const Meter_Done done;
   const Meter_UpdateMode updateMode;
   const Meter_Draw draw;
   const Meter_UpdateValues updateValues;
   const int defaultMode;
   // For "total" variable, sign matters.
   // >0: Full/maximum value is stable (at least for a short duration). Will
   //     draw as percent graph. e.g. CPU & swap.
   // <0: No stable maximum. Will draw with dynamic scale. e.g. loadavg.
   // (total == 0) will bring weird behavior for now. Avoid.
   const double total;
   const int* const attributes;
   const char* const name;                 /* internal name of the meter, must not contain any space */
   const char* const uiName;               /* display name in header setup menu */
   const char* const caption;              /* prefix in the actual header */
   const char* const description;          /* optional meter description in header setup menu */
   const uint8_t maxItems;
} MeterClass;

#define As_Meter(this_)                ((const MeterClass*)((this_)->super.klass))
#define Meter_initFn(this_)            As_Meter(this_)->init
#define Meter_init(this_)              As_Meter(this_)->init((Meter*)(this_))
#define Meter_done(this_)              As_Meter(this_)->done((Meter*)(this_))
#define Meter_updateModeFn(this_)      As_Meter(this_)->updateMode
#define Meter_updateMode(this_, m_)    As_Meter(this_)->updateMode((Meter*)(this_), m_)
#define Meter_drawFn(this_)            As_Meter(this_)->draw
#define Meter_doneFn(this_)            As_Meter(this_)->done
#define Meter_updateValues(this_, buf_, sz_) \
                                       As_Meter(this_)->updateValues((Meter*)(this_), buf_, sz_)
#define Meter_defaultMode(this_)       As_Meter(this_)->defaultMode
#define Meter_getMaxItems(this_)       As_Meter(this_)->maxItems
#define Meter_attributes(this_)        As_Meter(this_)->attributes
#define Meter_name(this_)              As_Meter(this_)->name
#define Meter_uiName(this_)            As_Meter(this_)->uiName

typedef struct GraphData_ {
   struct timeval time;
   double* values;
   double* stack1;
   double* stack2;
   int* colors;
   unsigned int colorRowSize;
   int drawOffset;
} GraphData;

struct Meter_ {
   Object super;
   Meter_Draw draw;

   char* caption;
   int mode;
   int param;
   GraphData* drawData;
   int h;
   int columnWidthCount;      /*<< only used internally by the Header */
   const ProcessList* pl;
   uint8_t curItems;
   const int* curAttributes;
   double* values;
   double total;
   void* meterData;
};

typedef struct MeterMode_ {
   Meter_Draw draw;
   const char* uiName;
   int h;
} MeterMode;

typedef enum {
   CUSTOM_METERMODE = 0,
   BAR_METERMODE,
   TEXT_METERMODE,
   GRAPH_METERMODE,
   LED_METERMODE,
   LAST_METERMODE
} MeterModeId;

#define IS_POWER_OF_2(x) ((x) > 0 && !((x) & ((x) - 1)))

#ifndef __has_builtin
# define __has_builtin(x) 0
#endif
#if (__has_builtin(__builtin_clz) || \
    ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)))
# define HAS_BUILTIN_CLZ 1
# define HAS_ILOG2 1
/*
 * ilog2(x): base-2 logarithm of an unsigned integer x, rounded down, but
 *           ilog2(0U) yields undefined behavior.
 * (You may use ilog2(x | 1) to define the (x == 0) behavior.)
 */
# define ilog2(x) ((sizeof(x) <= sizeof(unsigned int)) ? \
     (CHAR_BIT*sizeof(unsigned int)-1-__builtin_clz(x)) : \
     (sizeof(x) <= sizeof(unsigned long)) ? \
     (CHAR_BIT*sizeof(unsigned long)-1-__builtin_clzl(x)) : \
     (CHAR_BIT*sizeof(unsigned long long)-1-__builtin_clzll(x)))
#endif // __has_builtin(__builtin_clz) || GNU C 3.4 or later

extern const MeterClass Meter_class;

Meter* Meter_new(const ProcessList* pl, int param, const MeterClass* type);

int Meter_humanUnit(char* buffer, unsigned long int value, size_t size);

void Meter_delete(Object* cast);

void Meter_setCaption(Meter* this, const char* caption);

void Meter_setMode(Meter* this, int modeIndex);

ListItem* Meter_toListItem(const Meter* this, bool moving);

extern const MeterMode* const Meter_modes[];

extern const MeterClass BlankMeter_class;

#endif

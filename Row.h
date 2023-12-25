#ifndef HEADER_Row
#define HEADER_Row
/*
htop - Row.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "Object.h"
#include "RichString.h"
#include "RowField.h"


extern uint8_t Row_fieldWidths[LAST_RESERVED_FIELD];
#define ROW_MIN_PID_DIGITS 5
#define ROW_MAX_PID_DIGITS 19
#define ROW_MIN_UID_DIGITS 5
#define ROW_MAX_UID_DIGITS 20
extern int Row_pidDigits;
extern int Row_uidDigits;

struct Machine_;     // IWYU pragma: keep
struct Settings_;    // IWYU pragma: keep
struct Table_;       // IWYU pragma: keep

/* Class representing entities (such as processes) that can be
 * represented in a tabular form in the lower half of the htop
 * display. */

typedef struct Row_ {
   /* Super object for emulated OOP */
   Object super;

   /* Pointer to quasi-global data */
   const struct Machine_* host;

   int id;
   int group;
   int parent;

   /* Has no known parent */
   bool isRoot;

   /* Whether the row was tagged by the user */
   bool tag;

   /* Whether to display this row */
   bool show;

   /* Whether this row was shown last cycle */
   bool wasShown;

   /* Whether to show children of this row in tree-mode */
   bool showChildren;

   /* Whether the row was updated during the last scan */
   bool updated;

   /*
    * Internal state for tree-mode.
    */
   int32_t indent;
   unsigned int tree_depth;

   /*
    * Internal time counts for showing new and exited processes.
    */
   uint64_t seenStampMs;
   uint64_t tombStampMs;
} Row;

typedef Row* (*Row_New)(const struct Machine_*);
typedef void (*Row_WriteField)(const Row*, RichString*, RowField);
typedef bool (*Row_IsHighlighted)(const Row*);
typedef bool (*Row_IsVisible)(const Row*, const struct Table_*);
typedef bool (*Row_MatchesFilter)(const Row*, const struct Table_*);
typedef const char* (*Row_SortKeyString)(Row*);
typedef int (*Row_CompareByParent)(const Row*, const Row*);

int Row_compare(const void* v1, const void* v2);

typedef struct RowClass_ {
   const ObjectClass super;
   const Row_IsHighlighted isHighlighted;
   const Row_IsVisible isVisible;
   const Row_WriteField writeField;
   const Row_MatchesFilter matchesFilter;
   const Row_SortKeyString sortKeyString;
   const Row_CompareByParent compareByParent;
} RowClass;

#define As_Row(this_)  ((const RowClass*)((this_)->super.klass))

#define Row_isHighlighted(r_)  (As_Row(r_)->isHighlighted ? (As_Row(r_)->isHighlighted(r_)) : false)
#define Row_isVisible(r_, t_)  (As_Row(r_)->isVisible ? (As_Row(r_)->isVisible(r_, t_)) : true)
#define Row_matchesFilter(r_, t_)  (As_Row(r_)->matchesFilter ? (As_Row(r_)->matchesFilter(r_, t_)) : false)
#define Row_sortKeyString(r_)  (As_Row(r_)->sortKeyString ? (As_Row(r_)->sortKeyString(r_)) : "")
#define Row_compareByParent(r1_, r2_)  (As_Row(r1_)->compareByParent ? (As_Row(r1_)->compareByParent(r1_, r2_)) : Row_compareByParent_Base(r1_, r2_))

#define ONE_K 1024UL
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)
#define ONE_T (1ULL * ONE_G * ONE_K)
#define ONE_P (1ULL * ONE_T * ONE_K)

#define ONE_DECIMAL_K 1000UL
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)
#define ONE_DECIMAL_T (1ULL * ONE_DECIMAL_G * ONE_DECIMAL_K)
#define ONE_DECIMAL_P (1ULL * ONE_DECIMAL_T * ONE_DECIMAL_K)

extern const RowClass Row_class;

void Row_init(Row* this, const struct Machine_* host);

void Row_done(Row* this);

void Row_display(const Object* cast, RichString* out);

void Row_toggleTag(Row* this);

void Row_resetFieldWidths(void);

void Row_updateFieldWidth(RowField key, size_t width);

void Row_printLeftAlignedField(RichString* str, int attr, const char* content, unsigned int width);

const char* RowField_alignedTitle(const struct Settings_* settings, RowField field);

RowField RowField_keyAt(const struct Settings_* settings, int at);

/* Sets the size of the PID column based on the passed PID */
void Row_setPidColumnWidth(pid_t maxPid);

/* Sets the size of the UID column based on the passed UID */
void Row_setUidColumnWidth(uid_t maxUid);

/* Takes number in kibibytes (base 1024). Prints 6 columns. */
void Row_printKBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number in bytes (base 1024). Prints 6 columns. */
void Row_printBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number as count (base 1000). Prints 12 columns. */
void Row_printCount(RichString* str, unsigned long long number, bool coloring);

/* Takes time in hundredths of a seconds. Prints 9 columns. */
void Row_printTime(RichString* str, unsigned long long totalHundredths, bool coloring);

/* Takes rate in bare unit (base 1024) per second. Prints 12 columns. */
void Row_printRate(RichString* str, double rate, bool coloring);

int Row_printPercentage(float val, char* buffer, size_t n, uint8_t width, int* attr);

void Row_display(const Object* cast, RichString* out);

static inline int Row_idEqualCompare(const void* v1, const void* v2) {
   const int p1 = ((const Row*)v1)->id;
   const int p2 = ((const Row*)v2)->id;
   return p1 != p2; /* return zero when equal */
}

/* Routines used primarily with the tree view */
static inline int Row_getGroupOrParent(const Row* this) {
   return this->group == this->id ? this->parent : this->group;
}

static inline bool Row_isChildOf(const Row* this, int id) {
   return id == Row_getGroupOrParent(this);
}

int Row_compareByParent_Base(const void* v1, const void* v2);

#endif

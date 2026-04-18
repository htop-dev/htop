#ifndef HEADER_History
#define HEADER_History
/*
htop - History.h
(C) 2004-2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "LineEditor.h"


#define HISTORY_MAX_ENTRIES 512

typedef struct History_ {
   char** entries;    /* array of history strings, oldest first */
   size_t count;      /* current number of entries */
   size_t capacity;   /* allocated capacity */
   size_t position;      /* current browse position: count = "at new input" */
   char saved[LINEEDITOR_MAX + 1]; /* saved current input while browsing */
   char* filename;    /* path to history file (may be NULL = no read / write) */
} History;

/* Create a new History, loading from the given file (may be NULL = init new history) */
History* History_new(const char* filename);

/* Free all resources */
void History_delete(History* this);

/* Add an entry to the history (deduplicates identical entries).
   Resets the browse position to "at new input". */
void History_add(History* this, const char* entry);

/* Save history to file (noop if filename is NULL) */
void History_save(const History* this);

/* Navigate history: back=true goes to older entries, back=false goes to newer.
   Saves/restores the current editor content as needed.
   Returns the string to put in the editor, or NULL if already at the limit. */
const char* History_navigate(History* this, LineEditor* editor, bool back);

/* Reset browse position to "at new input" (discards saved input state) */
void History_resetPosition(History* this);

#endif

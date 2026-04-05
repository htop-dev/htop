/*
htop - History.c
(C) 2004-2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "History.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Macros.h"
#include "XUtils.h"


static void History_load(History* this) {
   if (!this->filename)
      return;
   FILE* fp = fopen(this->filename, "r");
   if (!fp)
      return;

   char line[LINEEDITOR_MAX + 2];
   while (fgets(line, sizeof(line), fp)) {
      size_t len = strlen(line);
      /* strip trailing newline */
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
         line[--len] = '\0';
      if (len == 0)
         continue;

      History_add(this, line);
   }
   fclose(fp);
}

History* History_new(const char* filename) {
   History* this = xCalloc(1, sizeof(History));
   this->capacity = 64;
   this->entries = xCalloc(this->capacity, sizeof(char*));
   this->count = 0;
   this->position = 0;
   this->saved[0] = '\0';
   this->filename = filename ? xStrdup(filename) : NULL;

   if (this->filename)
      History_load(this);

   this->position = this->count;

   return this;
}

void History_delete(History* this) {
   for (size_t i = 0; i < this->count; i++)
      free(this->entries[i]);
   free(this->entries);
   free(this->filename);
   free(this);
}

void History_save(const History* this) {
   if (!this->filename)
      return;
   /* Settings_write writes things via a temp file & rename, we do it less robust but faster here: */
   int fd = open(this->filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
   if (fd == -1)
      return;
   FILE* fp = fdopen(fd, "w");
   if (!fp) {
      close(fd); // fd not consumed on failure, so close it
      return;
   }
   size_t start = (this->count > HISTORY_MAX_ENTRIES) ? this->count - HISTORY_MAX_ENTRIES : 0;
   for (size_t i = start; i < this->count; i++)
      fprintf(fp, "%s\n", this->entries[i]);
   fclose(fp);
}

void History_add(History* this, const char* entry) {
   if (!entry || entry[0] == '\0')
      return;

   /* Deduplicate: remove previous identical entry if present */
   for (size_t i = 0; i < this->count; i++) {
      if (String_eq(this->entries[i], entry)) {
         free(this->entries[i]);
         memmove(this->entries + i, this->entries + i + 1, (this->count - i - 1) * sizeof(char*));
         this->count--;
         break;
      }
   }

   /* Grow array if needed */
   if (this->count >= this->capacity) {
      if (this->capacity < HISTORY_MAX_ENTRIES) {
         this->capacity = MINIMUM(this->capacity * 2, (size_t)HISTORY_MAX_ENTRIES);
         this->entries = xReallocArray(this->entries, this->capacity, sizeof(char*));
      } else {
         /* Drop oldest entry */
         free(this->entries[0]);
         memmove(this->entries, this->entries + 1, (this->count - 1) * sizeof(char*));
         this->count--;
      }
   }

   this->entries[this->count++] = xStrdup(entry);

   /* Reset position to "at new input" */
   this->position = this->count;
   this->saved[0] = '\0';
}

const char* History_navigate(History* this, LineEditor* editor, bool back) {
   if (this->count == 0)
      return NULL;

   if (back) {
      /* Going back (up arrow) */
      if (this->position == this->count) {
         /* Save current editor content before entering history */
         strncpy(this->saved, LineEditor_getText(editor), LINEEDITOR_MAX);
         this->saved[LINEEDITOR_MAX] = '\0';
      }
      if (this->position > 0) {
         this->position--;
         return this->entries[this->position];
      }
      return NULL; /* Already at oldest entry */
   } else {
      /* Going forward (down arrow) */
      if (this->position >= this->count)
         return NULL; /* Already at newest */
      this->position++;
      if (this->position == this->count) {
         /* Restore saved input */
         return this->saved;
      }
      return this->entries[this->position];
   }
}

void History_resetPosition(History* this) {
   this->position = this->count;
   this->saved[0] = '\0';
}

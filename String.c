/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define _GNU_SOURCE
#include "String.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"

/*{
#define String_startsWith(s, match) (strstr((s), (match)) == (s))
}*/

inline void String_delete(char* s) {
   free(s);
}

inline char* String_copy(char* orig) {
   return strdup(orig);
}

char* String_cat(char* s1, char* s2) {
   int l1 = strlen(s1);
   int l2 = strlen(s2);
   char* out = malloc(l1 + l2 + 1);
   strncpy(out, s1, l1);
   strncpy(out+l1, s2, l2+1);
   return out;
}

char* String_trim(char* in) {
   while (in[0] == ' ' || in[0] == '\t' || in[0] == '\n') {
      in++;
   }
   int len = strlen(in);
   while (len > 0 && (in[len-1] == ' ' || in[len-1] == '\t' || in[len-1] == '\n')) {
      len--;
   }
   char* out = malloc(len+1);
   strncpy(out, in, len);
   out[len] = '\0';
   return out;
}

char* String_copyUpTo(char* orig, char upTo) {
   int len;
   
   int origLen = strlen(orig);
   char* at = strchr(orig, upTo);
   if (at != NULL)
      len = at - orig;
   else
      len = origLen;
   char* copy = (char*) malloc(len+1);
   strncpy(copy, orig, len);
   copy[len] = '\0';
   return copy;
}

char* String_sub(char* orig, int from, int to) {
   char* copy;
   int len;
   
   len = strlen(orig);
   if (to > len)
      to = len;
   if (from > len)
      to = len;
   len = to-from+1;
   copy = (char*) malloc(len+1);
   strncpy(copy, orig+from, len);
   copy[len] = '\0';
   return copy;
}

void String_println(char* s) {
   printf("%s\n", s);
}

void String_print(char* s) {
   printf("%s", s);
}

void String_printInt(int i) {
   printf("%i", i);
}

void String_printPointer(void* p) {
   printf("%p", p);
}

inline int String_eq(const char* s1, const char* s2) {
   if (s1 == NULL || s2 == NULL) {
      if (s1 == NULL && s2 == NULL)
         return 1;
      else
         return 0;
   }
   return (strcmp(s1, s2) == 0);
}

char** String_split(char* s, char sep) {
   const int rate = 10;
   char** out = (char**) malloc(sizeof(char*) * rate);
   int ctr = 0;
   int blocks = rate;
   char* where;
   while ((where = strchr(s, sep)) != NULL) {
      int size = where - s;
      char* token = (char*) malloc(size + 1);
      strncpy(token, s, size);
      token[size] = '\0';
      out[ctr] = token;
      ctr++;
      if (ctr == blocks) {
         blocks += rate;
         out = (char**) realloc(out, sizeof(char*) * blocks);
      }
      s += size + 1;
   }
   if (s[0] != '\0') {
      int size = strlen(s);
      char* token = (char*) malloc(size + 1);
      strncpy(token, s, size + 1);
      out[ctr] = token;
      ctr++;
   }
   out = realloc(out, sizeof(char*) * (ctr + 1));
   out[ctr] = NULL;
   return out;
}

void String_freeArray(char** s) {
   for (int i = 0; s[i] != NULL; i++) {
      free(s[i]);
   }
   free(s);
}

int String_startsWith_i(char* s, char* match) {
   return (strncasecmp(s, match, strlen(match)) == 0);
}

int String_contains_i(char* s, char* match) {
   int lens = strlen(s);
   int lenmatch = strlen(match);
   for (int i = 0; i < lens-lenmatch; i++) {
      if (strncasecmp(s, match, strlen(match)) == 0)
         return 1;
      s++;
   }
   return 0;
}

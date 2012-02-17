/*
htop - String.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "String.h"

#include "config.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

/*{
#define String_startsWith(s, match) (strstr((s), (match)) == (s))
}*/

char* String_cat(const char* s1, const char* s2) {
   int l1 = strlen(s1);
   int l2 = strlen(s2);
   char* out = malloc(l1 + l2 + 1);
   strncpy(out, s1, l1);
   strncpy(out+l1, s2, l2+1);
   return out;
}

char* String_trim(const char* in) {
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

inline int String_eq(const char* s1, const char* s2) {
   if (s1 == NULL || s2 == NULL) {
      if (s1 == NULL && s2 == NULL)
         return 1;
      else
         return 0;
   }
   return (strcmp(s1, s2) == 0);
}

char** String_split(const char* s, char sep, int* n) {
   *n = 0;
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
   *n = ctr;
   return out;
}

void String_freeArray(char** s) {
   for (int i = 0; s[i] != NULL; i++) {
      free(s[i]);
   }
   free(s);
}

int String_contains_i(const char* s, const char* match) {
   int lens = strlen(s);
   int lenmatch = strlen(match);
   for (int i = 0; i < lens-lenmatch; i++) {
      if (strncasecmp(s, match, strlen(match)) == 0)
         return 1;
      s++;
   }
   return 0;
}

char* String_getToken(const char* line, const unsigned short int numMatch) {
   const unsigned short int len = strlen(line);
   char inWord = 0;
   unsigned short int count = 0;
   char match[50];

   unsigned short int foundCount = 0;

   for (unsigned short int i = 0; i < len; i++) {
      char lastState = inWord;
      inWord = line[i] == ' ' ? 0:1;

      if (lastState == 0 && inWord == 1)
         count++;
    
      if(inWord == 1){
         if (count == numMatch && line[i] != ' ' && line[i] != '\0' && line[i] != '\n' && line[i] != (char)EOF) {
            match[foundCount] = line[i];
            foundCount++;
         }
      }
   }

   match[foundCount] = '\0';
   return((char*)strdup(match));
}

#ifndef HEADER_Hashtable
#define HEADER_Hashtable
/*
htop - Hashtable.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>


typedef unsigned int hkey_t;

typedef void(*Hashtable_PairFunction)(hkey_t key, void* value, void* userdata);

typedef struct HashtableItem_ {
   hkey_t key;
   unsigned int probe;
   void* value;
} HashtableItem;

typedef struct Hashtable_ {
   unsigned int size;
   HashtableItem* buckets;
   unsigned int items;
   bool owner;
} Hashtable;

#ifndef NDEBUG

unsigned int Hashtable_count(const Hashtable* this);

#endif /* NDEBUG */

Hashtable* Hashtable_new(unsigned int size, bool owner);

void Hashtable_delete(Hashtable* this);

void Hashtable_clear(Hashtable* this);

void Hashtable_setSize(Hashtable* this, unsigned int size);

void Hashtable_put(Hashtable* this, hkey_t key, void* value);

void* Hashtable_remove(Hashtable* this, hkey_t key);

void* Hashtable_get(Hashtable* this, hkey_t key);

void Hashtable_foreach(Hashtable* this, Hashtable_PairFunction f, void* userData);

#endif

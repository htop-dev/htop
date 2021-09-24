#ifndef HEADER_Hashtable
#define HEADER_Hashtable
/*
htop - Hashtable.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>


typedef unsigned int ht_key_t;

typedef void(*Hashtable_PairFunction)(ht_key_t key, void* value, void* userdata);

typedef struct Hashtable_ Hashtable;

#ifndef NDEBUG

size_t Hashtable_count(const Hashtable* this);

#endif /* NDEBUG */

Hashtable* Hashtable_new(size_t size, bool owner);

void Hashtable_delete(Hashtable* this);

void Hashtable_clear(Hashtable* this);

void Hashtable_setSize(Hashtable* this, size_t size);

void Hashtable_put(Hashtable* this, ht_key_t key, void* value);

void* Hashtable_remove(Hashtable* this, ht_key_t key);

void* Hashtable_get(Hashtable* this, ht_key_t key);

void Hashtable_foreach(Hashtable* this, Hashtable_PairFunction f, void* userData);

#endif

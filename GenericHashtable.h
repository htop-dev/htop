#ifndef HEADER_GenericHashtable
#define HEADER_GenericHashtable

#include <stdbool.h>
#include <stddef.h>


typedef void* ght_data_t;
//typedef const void* ght_const_key_t;

typedef size_t(*GenericHashtable_HashFunction)(ght_data_t value);
typedef int(*GenericHashtable_CompareFunction)(ght_data_t a, ght_data_t b);
typedef void(*GenericHashtable_DestroyFunction)(ght_data_t value);
//typedef void(*GenericHashtable_PairFunction)(ght_data_t value, void* userdata);

typedef struct GenericHashtable_ GenericHashtable;

GenericHashtable* GenericHashtable_new(
      size_t size,
      bool owner,
      GenericHashtable_HashFunction hash,
      GenericHashtable_CompareFunction compare,
      GenericHashtable_DestroyFunction destroy);

void GenericHashtable_delete(GenericHashtable* this);

void GenericHashtable_clear(GenericHashtable* this);

void GenericHashtable_setSize(GenericHashtable* this, size_t size);

void GenericHashtable_put(GenericHashtable* this, ght_data_t value);

//void* GenericHashtable_remove(GenericHashtable* this, ght_data_t value);

ght_data_t GenericHashtable_get(GenericHashtable* this, ght_data_t value);

//void GenericHashtable_foreach(GenericHashtable* this, GenericHashtable_PairFunction f, void* userData);

#endif /* HEADER_GenericHashtable */

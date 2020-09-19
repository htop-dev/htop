#ifndef HEADER_XAlloc
#define HEADER_XAlloc

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Macros.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>

void fail(void) ATTR_NORETURN;

void* xMalloc(size_t size);

void* xCalloc(size_t nmemb, size_t size);

void* xRealloc(void* ptr, size_t size);

#endif

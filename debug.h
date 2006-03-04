
#ifdef DEBUG

#include "DebugMemory.h"

#define calloc(a, b) DebugMemory_calloc(a, b, __FILE__, __LINE__);
#define malloc(x) DebugMemory_malloc(x, __FILE__, __LINE__);
#define realloc(x,s) DebugMemory_realloc(x, s, __FILE__, __LINE__);
#define strdup(x) DebugMemory_strdup(x, __FILE__, __LINE__);
#define free(x) DebugMemory_free(x, __FILE__, __LINE__);

#define debug_done() DebugMemory_report();

#endif

#ifndef DEBUG

#define NDEBUG

#define debug_done() sleep(0)

#endif

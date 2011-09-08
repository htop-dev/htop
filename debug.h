
#if defined(DEBUG)

   /* Full debug */
   #include "DebugMemory.h"
   #define calloc(a, b) DebugMemory_calloc(a, b, __FILE__, __LINE__)
   #define malloc(x) DebugMemory_malloc(x, __FILE__, __LINE__, #x)
   #define realloc(x,s) DebugMemory_realloc(x, s, __FILE__, __LINE__, #x)
   #define strdup(x) DebugMemory_strdup(x, __FILE__, __LINE__)
   #define free(x) DebugMemory_free(x, __FILE__, __LINE__)
   #define debug_done() DebugMemory_report(); _nc_free_and_exit()

#elif defined(DEBUGLITE)

   /* Assertions and core only */
   #ifdef NDEBUG
   #undef NDEBUG
   #endif
   #define debug_done() 

#else

   /* No debugging */
   #define NDEBUG
   #define debug_done() 

#endif


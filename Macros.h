#ifndef HEADER_Macros
#define HEADER_Macros

#ifndef MINIMUM
#define MINIMUM(a, b)		((a) < (b) ? (a) : (b))
#endif

#ifndef MAXIMUM
#define MAXIMUM(a, b)		((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)	(assert(low < high), ((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x)          (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef  __GNUC__  // defined by GCC and Clang

#define ATTR_FORMAT(type, index, check) __attribute__((format (type, index, check)))
#define ATTR_NONNULL                    __attribute__((nonnull))
#define ATTR_NORETURN                   __attribute__((noreturn))
#define ATTR_UNUSED                     __attribute__((unused))

#else /* __GNUC__ */

#define ATTR_FORMAT(type, index, check)
#define ATTR_NONNULL
#define ATTR_NORETURN
#define ATTR_UNUSED

#endif /* __GNUC__ */

// ignore casts discarding const specifier, e.g.
//     const char []     ->  char * / void *
//     const char *[2]'  ->  char *const *
#ifdef __clang__
#define IGNORE_WCASTQUAL_BEGIN  _Pragma("clang diagnostic push") \
                                _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#define IGNORE_WCASTQUAL_END    _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define IGNORE_WCASTQUAL_BEGIN  _Pragma("GCC diagnostic push") \
                                _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define IGNORE_WCASTQUAL_END    _Pragma("GCC diagnostic pop")
#else
#define IGNORE_WCASTQUAL_BEGIN
#define IGNORE_WCASTQUAL_END
#endif

#endif

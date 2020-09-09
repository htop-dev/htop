#ifndef HEADER_Macros
#define HEADER_Macros

#ifndef MINIMUM
#define MINIMUM(a, b)		((a) < (b) ? (a) : (b))
#endif

#ifndef MAXIMUM
#define MAXIMUM(a, b)		((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)	(((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

#endif

#ifndef HEADER_Debug
#define HEADER_Debug
/*
htop - Debug.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <assert.h> // IWYU pragma: keep


/*
 * static_assert() hack for pre-C11
 * TODO: drop after moving to -std=c11 or newer
 */

/* C11 guarantees _Static_assert is a keyword */
#if (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 201112L
# if !defined(_Static_assert)
#  define _Static_assert(expr, msg)                                    \
   extern int (*__Static_assert_function (void))                       \
      [!!sizeof (struct { int __error_if_negative: (expr) ? 2 : -1; })]
# endif
#endif

/* C23 guarantees static_assert is a keyword or a macro */
#if (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 202311L
# if !defined(static_assert)
#  define static_assert(expr, msg) _Static_assert(expr, msg)
# endif
#endif

#endif /* HEADER_Debug */

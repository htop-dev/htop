#ifndef HEADER_ProvideDemangle
#define HEADER_ProvideDemangle
/*
htop - generic/ProvideDemangle.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

// IWYU pragma: no_include "config.h"

// IWYU pragma: begin_exports

#if !defined(HAVE_DECL_BASENAME)
// Suppress libiberty's own declaration of basename().
// It's a pity that we need this workaround as libiberty developers
// refuse fix their headers and export an unwanted interface to us.
// htop doesn't use basename() API. (The POSIX version is flawed by
// design; libiberty's ships with GNU version of basename() that's
// incompatible with POSIX.)
// <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122729>
#define HAVE_DECL_BASENAME 1
#endif

#if defined(HAVE_DEMANGLE_H)
#include <demangle.h>
#elif defined(HAVE_LIBIBERTY_DEMANGLE_H)
#include <libiberty/demangle.h>
#endif

// IWYU pragma: end_exports

#endif /* HEADER_ProvideDemangle */

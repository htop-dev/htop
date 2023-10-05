#ifndef HEADER_ProvideTerm
#define HEADER_ProvideTerm
/*
htop - ProvideTerm.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

// IWYU pragma: begin_exports

#if defined(HAVE_NCURSESW_TERM_H)
#include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_TERM_H)
#include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
#include <term.h>
#endif

// IWYU pragma: end_exports

#endif // HEADER_ProvideTerm

#ifndef HEADER_ProvideCurses
#define HEADER_ProvideCurses
/*
htop - RichString.h
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


#include "config.h"

// IWYU pragma: begin_exports

#if defined(HAVE_NCURSESW_CURSES_H)
#include <ncursesw/curses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#include <ncurses/curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_CURSES_H)
#include <curses.h>
#endif

#ifdef HAVE_LIBNCURSESW
#include <wchar.h>
#include <wctype.h>
#endif

// IWYU pragma: end_exports

#endif // HEADER_ProvideCurses

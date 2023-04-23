#ifndef HEADER_ScreenWarning
#define HEADER_ScreenWarning
/*
htop - ScreenWarning.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"


ATTR_FORMAT(printf, 1, 2)
void ScreenWarning_add(const char* fmt, ...);

void ScreenWarning_display(void);

#endif /* HEADER_ScreenWarning */

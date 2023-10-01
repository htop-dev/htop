/*
htop - signals/SignalList.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "signals/SignalList.h"

#include "Macros.h"

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel", .number = 0 },
/*
 * The Performance Co-Pilot build should not pick up signals of the underlying
 * platform.
 */
#ifndef HTOP_PCP
#include "signals/SignalList.in.sorted"
#endif
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

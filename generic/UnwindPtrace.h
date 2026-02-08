#ifndef HEADER_UnwindPtrace
#define HEADER_UnwindPtrace
/*
htop - generic/UnwindPtrace.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/types.h>

#include "Vector.h"


#ifdef HAVE_LIBUNWIND_PTRACE
void UnwindPtrace_makeBacktrace(Vector* frames, pid_t pid, char** error);
#endif

#endif

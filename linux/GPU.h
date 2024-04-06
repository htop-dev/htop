#ifndef HEADER_GPU
#define HEADER_GPU
/*
htop - GPU.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Compat.h"
#include "linux/LinuxProcess.h"
#include "linux/LinuxProcessTable.h"


void GPU_readProcessData(LinuxProcessTable* lpt, LinuxProcess* lp, openat_arg_t procFd);

#endif /* HEADER_GPU */

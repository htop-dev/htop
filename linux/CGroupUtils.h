#ifndef HEADER_CGroupUtils
#define HEADER_CGroupUtils
/*
htop - CGroupUtils.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>


bool CGroup_filterName(const char *cgroup, char* buf, size_t bufsize);

#endif /* HEADER_CGroupUtils */

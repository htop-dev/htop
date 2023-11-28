#ifndef HEADER_CGroupUtils
#define HEADER_CGroupUtils
/*
htop - CGroupUtils.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


char* CGroup_filterName(const char* cgroup);
char* CGroup_filterContainer(const char* cgroup);

#endif /* HEADER_CGroupUtils */

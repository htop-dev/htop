/*
htop - generic/hostname.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h"  // IWYU pragma: keep

#include "generic/hostname.h"

#include <unistd.h>


void Generic_hostname(char* buffer, size_t size) {
   gethostname(buffer, size - 1);
   buffer[size - 1] = '\0';
}

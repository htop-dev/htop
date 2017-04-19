/*
htop - dragonflybsd/DragonFlyBSDCRT.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "CRT.h"
#include <stdio.h>
#include <stdlib.h>

void CRT_handleSIGSEGV(int sgn) {
   (void) sgn;
   CRT_done();
   fprintf(stderr, "\n\nhtop " VERSION " aborting.\n");
   fprintf(stderr, "\nPlease contact your DragonFlyBSD package maintainer!\n\n");
   abort();
}


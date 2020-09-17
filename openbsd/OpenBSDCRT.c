/*
htop - OpenBSDCRT.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "OpenBSDCRT.h"
#include "CRT.h"
#include <stdio.h>
#include <stdlib.h>

void CRT_handleSIGSEGV(int sgn) {
   (void) sgn;
   CRT_done();
   fprintf(stderr, "\n\nhtop " VERSION " aborting.\n");
   fprintf(stderr, "\nUnfortunately, you seem to be using an unsupported platform!");
   fprintf(stderr, "\nPlease contact your platform package maintainer!\n\n");
   abort();
}

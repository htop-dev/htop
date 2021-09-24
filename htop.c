/*
htop - htop.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CommandLine.h"


int main(int argc, char** argv) {
   return CommandLine_run(PACKAGE, argc, argv);
}

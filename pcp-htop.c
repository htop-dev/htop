/*
htop - pcp-htop.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <pcp/pmapi.h>

#include "CommandLine.h"
#include "Platform.h"


const char* program = "pcp-htop";

int main(int argc, char** argv) {
   pmSetProgname(program);

#ifdef PCP_DERIVED_OPTION_NOVALUE
   /*
    * set OPTION_NOVALUE to map undefined metric operands to novalue()
    */
    (void)pmSetDerivedControl(PCP_DERIVED_OPTION_NOVALUE, 1);
#endif

   /* extract environment variables */
   opts.flags |= PM_OPTFLAG_ENV_ONLY;
   (void)pmGetOptions(argc, argv, &opts);

   return CommandLine_run(argc, argv);
}

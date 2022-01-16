#ifndef HEADER_FreeBSDProcessField
#define HEADER_FreeBSDProcessField
/*
htop - freebsd/ProcessField.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


#define PLATFORM_PROCESS_FIELDS  \
   JID = 100,                    \
   JAIL = 101,                   \
   EMULATION = 102,              \
                                 \
   DUMMY_BUMP_FIELD = CWD,       \
   // End of list


#endif /* HEADER_FreeBSDProcessField */

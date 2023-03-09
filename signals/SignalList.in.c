/*
htop - signals/SignalList.in.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <signal.h>

#define HTOP_SIGENTRY(x) htop_sigstart x htop_sigmid #x htop_sigend


HTOP_SIGENTRY(SIGABRT)

HTOP_SIGENTRY(SIGALRM)

HTOP_SIGENTRY(SIGBUS)

/* On Solaris. */
#ifdef SIGCANCEL
HTOP_SIGENTRY(SIGCANCEL)
#endif

#ifdef SIGCHLD
HTOP_SIGENTRY(SIGCHLD)
#endif

/* A synonym for SIGCHLD on Linux/MIPS, original name on SysV, omitting
   if SIGCHLD is also defined, since confusion is unlikely. */
#ifndef SIGCHLD
HTOP_SIGENTRY(SIGCLD)
#endif

HTOP_SIGENTRY(SIGCONT)

/* On BSDs and Alpha, Sparc and MIPS Linuxes. */
#ifdef SIGEMT
HTOP_SIGENTRY(SIGEMT)
#endif

HTOP_SIGENTRY(SIGFPE)

/* On Solaris. */
#ifdef SIGFREEZE
HTOP_SIGENTRY(SIGFREEZE)
#endif

HTOP_SIGENTRY(SIGHUP)

HTOP_SIGENTRY(SIGILL)

/* On Solaris. Also a synonym for SIGPWR on Linux/Alpha. */
#ifdef SIGINFO
HTOP_SIGENTRY(SIGINFO)
#endif

HTOP_SIGENTRY(SIGINT)

HTOP_SIGENTRY(SIGIO)

/* A synonym for SIGABRT on Linux and Solaris, found on OpenBSD and NetBSD,
 * not on FreeBSD.
 */
#ifdef SIGIOT
HTOP_SIGENTRY(SIGIOT)
#endif

/* On Solaris. */
#ifdef SIGJVM1
HTOP_SIGENTRY(SIGJVM1)
#endif

/* On Solaris. */
#ifdef SIGJVM2
HTOP_SIGENTRY(SIGJVM2)
#endif

HTOP_SIGENTRY(SIGKILL)

/* On FreeBSD. */
#ifdef SIGLIBRT
HTOP_SIGENTRY(SIGLIBRT)
#endif

/* On Solaris and Linux/SPARC. */
#ifdef SIGLOST
HTOP_SIGENTRY(SIGLOST)
#endif

/* On Solaris. */
#ifdef SIGLWP
HTOP_SIGENTRY(SIGLWP)
#endif

HTOP_SIGENTRY(SIGPIPE)

/* A synonym for SIGIO on Linux and Solaris. */
#ifdef SIGPOLL
HTOP_SIGENTRY(SIGPOLL)
#endif

HTOP_SIGENTRY(SIGPROF)

/* Linux, NetBSD and SysV. */
#ifdef SIGPWR
HTOP_SIGENTRY(SIGPWR)
#endif

HTOP_SIGENTRY(SIGQUIT)

HTOP_SIGENTRY(SIGSEGV)

/* Not on BSDs. */
#ifdef SIGSTKFLT
HTOP_SIGENTRY(SIGSTKFLT)
#endif

HTOP_SIGENTRY(SIGSTOP)

HTOP_SIGENTRY(SIGSYS)

HTOP_SIGENTRY(SIGTERM)

/* On Solaris. */
#ifdef SIGTHAW
HTOP_SIGENTRY(SIGTHAW)
#endif

/* On FreeBSD and OpenBSD. */
#ifdef SIGTHR
HTOP_SIGENTRY(SIGTHR)
#endif

HTOP_SIGENTRY(SIGTRAP)

HTOP_SIGENTRY(SIGTSTP)

HTOP_SIGENTRY(SIGTTIN)

HTOP_SIGENTRY(SIGTTOU)

/* A synonym for SIGSYS on Linux, deprecated.
#ifdef SIGUNUSED
HTOP_SIGENTRY(SIGUNUSED)
#endif
*/

HTOP_SIGENTRY(SIGURG)

HTOP_SIGENTRY(SIGUSR1)

HTOP_SIGENTRY(SIGUSR2)

HTOP_SIGENTRY(SIGVTALRM)

HTOP_SIGENTRY(SIGXCPU)

HTOP_SIGENTRY(SIGXFSZ)

/* On Solaris. */
#ifdef SIGXRES
HTOP_SIGENTRY(SIGXRES)
#endif

/* On Solaris. */
#ifdef SIGWAITING
HTOP_SIGENTRY(SIGWAITING)
#endif

HTOP_SIGENTRY(SIGWINCH)

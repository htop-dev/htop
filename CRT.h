#ifndef HEADER_CRT
#define HEADER_CRT
/*
htop - CRT.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"

#include <stdbool.h>

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef enum ColorSchemes_ {
   COLORSCHEME_DEFAULT = 0,
   COLORSCHEME_MONOCHROME,
   COLORSCHEME_BLACKONWHITE,
   COLORSCHEME_LIGHTTERMINAL,
   COLORSCHEME_MIDNIGHT,
   COLORSCHEME_BLACKNIGHT,
   COLORSCHEME_BROKENGRAY,
   LAST_COLORSCHEME,
} ColorSchemes;

typedef enum ColorElements_ {
   RESET_COLOR,
   DEFAULT_COLOR,
   FUNCTION_BAR,
   FUNCTION_KEY,
   FAILED_SEARCH,
   PAUSED,
   PANEL_HEADER_FOCUS,
   PANEL_HEADER_UNFOCUS,
   PANEL_SELECTION_FOCUS,
   PANEL_SELECTION_FOLLOW,
   PANEL_SELECTION_UNFOCUS,
   LARGE_NUMBER,
   METER_TEXT,
   METER_VALUE,
   METER_VALUE_NOTICE,
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
   LED_COLOR,
   UPTIME,
   BATTERY,
   TASKS_RUNNING,
#ifdef HAVE_LIBSENSORS
   TEMPERATURE_COOL,
   TEMPERATURE_MEDIUM,
   TEMPERATURE_HOT,
#endif
   SWAP,
   PROCESS,
   PROCESS_SHADOW,
   PROCESS_TAG,
   PROCESS_MEGABYTES,
   PROCESS_GIGABYTES,
   PROCESS_TREE,
   PROCESS_R_STATE,
   PROCESS_D_STATE,
   PROCESS_BASENAME,
   PROCESS_HIGH_PRIORITY,
   PROCESS_LOW_PRIORITY,
   PROCESS_THREAD,
   PROCESS_THREAD_BASENAME,
   BAR_BORDER,
   BAR_SHADOW,
   GRAPH_1,
   GRAPH_2,
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_BUFFERS_TEXT,
   MEMORY_CACHE,
   LOAD,
   LOAD_AVERAGE_FIFTEEN,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_ONE,
   CHECK_BOX,
   CHECK_MARK,
   CHECK_TEXT,
   CLOCK,
   DATE,
   DATETIME,
   HELP_BOLD,
   HOSTNAME,
   CPU_NICE,
   CPU_NICE_TEXT,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_IOWAIT,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   PRESSURE_STALL_TEN,
   PRESSURE_STALL_SIXTY,
   PRESSURE_STALL_THREEHUNDRED,
   ZFS_MFU,
   ZFS_MRU,
   ZFS_ANON,
   ZFS_HEADER,
   ZFS_OTHER,
   ZFS_COMPRESSED,
   ZFS_RATIO,
   LAST_COLORELEMENT
} ColorElements;

void CRT_fatalError(const char* note) ATTR_NORETURN;

void CRT_handleSIGSEGV(int signal) ATTR_NORETURN;

#define KEY_WHEELUP   KEY_F(20)
#define KEY_WHEELDOWN KEY_F(21)
#define KEY_RECLICK   KEY_F(22)
#define KEY_ALT(x)    (KEY_F(64 - 26) + ((x) - 'A'))


#ifdef HAVE_LIBNCURSESW

extern bool CRT_utf8;

#endif

extern const char *const *CRT_treeStr;

extern int CRT_delay;

extern const int* CRT_colors;

extern int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT];

extern int CRT_cursorX;

extern int CRT_scrollHAmount;

extern int CRT_scrollWheelVAmount;

extern const char* CRT_termType;

extern int CRT_colorScheme;

#ifdef HAVE_SETUID_ENABLED

void CRT_dropPrivileges(void);

void CRT_restorePrivileges(void);

#else /* HAVE_SETUID_ENABLED */

/* Turn setuid operations into NOPs */
static inline void CRT_dropPrivileges(void) { }
static inline void CRT_restorePrivileges(void) { }

#endif /* HAVE_SETUID_ENABLED */

void CRT_init(int delay, int colorScheme, bool allowUnicode);

void CRT_done(void);

int CRT_readKey(void);

void CRT_disableDelay(void);

void CRT_enableDelay(void);

void CRT_setColors(int colorScheme);

const char* CRT_degreeSign(void);

#endif

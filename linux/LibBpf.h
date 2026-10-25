#ifndef HEADER_LibBpf
#define HEADER_LibBpf
/*
htop - linux/LibBpf.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Shared libbpf runtime for the eBPF based monitors. libbpf is loaded via dlopen, so htop keeps working when
the library is missing at runtime. Every monitor embeds its own eBPF
object and loads it through this module, so a monitor whose programs
the kernel refuses does not take the others down with it.
*/

#include "config.h" // IWYU pragma: keep

#ifndef HAVE_EBPF
#error Compiling this file requires HAVE_EBPF
#endif

#include <stdbool.h>
#include <stddef.h>

#include "Macros.h"


/* Upper bound on the number of eBPF programs attached per object (one per SEC section) */
#define LIBBPF_MAX_PROGRAMS 24

typedef struct LibBpfObject_ {
   /* Short name of the monitor, used to prefix diagnostics (e.g. "netmon") */
   const char* name;

   /* struct bpf_object*, NULL while not loaded */
   void* obj;

   /* struct bpf_link* of every attached program; bpf_object__close()
    * alone does not destroy them */
   void* links[LIBBPF_MAX_PROGRAMS];
   size_t nLinks;
} LibBpfObject;

/* Maps the section of a program whose attach target does not exist on this
 * kernel to a note on when the target was introduced; NULL if unexpected */
typedef const char* (*LibBpf_ExpectedMissingFn)(const char* section);

/* Diagnostics on stderr, prefixed with "htop-<name>: "; compiled out in release (NDEBUG) builds */
void LibBpf_debug(const char* name, const char* fmt, ...) ATTR_FORMAT(printf, 2, 3);

/* Opens and loads the embedded eBPF ELF object [start, end) and attaches
 * every program in it. Programs whose attach target is missing are skipped.
 * Returns true if at least one program got attached. */
bool LibBpf_openObject(LibBpfObject* this, const unsigned char* start, const unsigned char* end, LibBpf_ExpectedMissingFn expectedMissing);

/* Detaches all programs and closes the object; safe to call repeatedly */
void LibBpf_closeObject(LibBpfObject* this);

/* File descriptor of the named map of a loaded object, or -1 */
int LibBpf_mapFd(const LibBpfObject* this, const char* mapName);

int LibBpf_mapLookupElem(int fd, const void* key, void* value);
int LibBpf_mapDeleteElem(int fd, const void* key);
int LibBpf_mapGetNextKey(int fd, const void* key, void* nextKey);

/* Unloads libbpf; every object must have been closed before */
void LibBpf_done(void);

#endif /* HEADER_LibBpf */

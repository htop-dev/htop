/*
htop - linux/LibBpf.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LibBpf.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "XUtils.h"


/* libbpf function pointers, resolved via dlopen */
typedef void* (*BpfObjectOpenMemFn)(const void*, size_t, const void*);
typedef int (*BpfObjectLoadFn)(void*);
typedef void* (*BpfObjectNextProgramFn)(const void*, void*);
typedef void* (*BpfProgramAttachFn)(void*);
typedef const char* (*BpfProgramSectionFn)(const void*);
typedef int (*BpfLinkDestroyFn)(void*);
typedef void* (*BpfObjectFindMapByNameFn)(const void*, const char*);
typedef int (*BpfMapFdFn)(const void*);
typedef int (*BpfMapLookupElemFn)(int, const void*, void*);
typedef int (*BpfMapDeleteElemFn)(int, const void*);
typedef int (*BpfMapGetNextKeyFn)(int, const void*, void*);
typedef void (*BpfObjectCloseFn)(void*);
typedef long (*BpfGetErrorFn)(const void*);

static BpfObjectOpenMemFn sym_bpf_object__open_mem;
static BpfObjectLoadFn sym_bpf_object__load;
static BpfObjectNextProgramFn sym_bpf_object__next_program;
static BpfProgramAttachFn sym_bpf_program__attach;
static BpfProgramSectionFn sym_bpf_program__section_name;
static BpfLinkDestroyFn sym_bpf_link__destroy;
static BpfObjectFindMapByNameFn sym_bpf_object__find_map_by_name;
static BpfMapFdFn sym_bpf_map__fd;
static BpfMapLookupElemFn sym_bpf_map_lookup_elem;
static BpfMapDeleteElemFn sym_bpf_map_delete_elem;
static BpfMapGetNextKeyFn sym_bpf_map_get_next_key;
static BpfObjectCloseFn sym_bpf_object__close;
static BpfGetErrorFn sym_libbpf_get_error;

static void* dlopenHandle = NULL;
static const char* dlopenName = NULL;

/* libbpf_print_fn_t: int (*)(int level, const char* format, va_list args) */
typedef int (*LibbpfPrintFn)(int, const char*, va_list);
typedef LibbpfPrintFn (*LibbpfSetPrintFn)(LibbpfPrintFn);

static int LibBpf_quietLibbpfPrint(int level, const char* format, va_list args) {
   (void) level;
   (void) format;
   (void) args;
   return 0;
}

/* Keep libbpf's own messages out of stderr; our own diagnostics
 * (when compiled in) tell the same story more concisely. */
static LibbpfSetPrintFn sym_libbpf_set_print;

/* In release (NDEBUG) builds diagnostic output is compiled out entirely */
void LibBpf_debug(const char* name, const char* fmt, ...) {
#ifdef NDEBUG
   (void) name;
   (void) fmt;
#else
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, "htop-%s: ", name);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
#endif
}

/* dlopen "libbpf.so.1"/"libbpf.so.0", fall back to the unversioned name */
static bool LibBpf_resolveLibbpf(const char* name) {
   if (dlopenHandle)
      return true;

   static const char* const libbpf_candidates[] = {
      "libbpf.so.1",
      "libbpf.so.0",
      "libbpf.so",
      NULL,
   };
   const char* lastError = NULL;
   for (size_t i = 0; libbpf_candidates[i]; i++) {
      dlopenHandle = dlopen(libbpf_candidates[i], RTLD_LAZY);
      if (dlopenHandle) {
         dlopenName = libbpf_candidates[i];
         break;
      }
      lastError = dlerror();
   }
   if (!dlopenHandle) {
      char* tried = String_join(", ", libbpf_candidates);
      LibBpf_debug(name, "eBPF monitoring needs the libbpf runtime library, none of %s could be loaded%s%s\n",
                   tried, lastError ? ": " : "", lastError ? lastError : "");
      free(tried);
      return false;
   }

   #define resolve(symbolname) do {                                        \
      dlerror();                                                           \
      *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);    \
      if (!sym_##symbolname) {                                             \
         const char* e = dlerror();                                        \
         LibBpf_debug(name, "%s is missing symbol %s%s%s\n",               \
                      dlopenName, #symbolname, e ? ": " : "", e ? e : ""); \
         dlclose(dlopenHandle);                                            \
         dlopenHandle = NULL;                                              \
         return false;                                                     \
      }                                                                    \
   } while (0)

   resolve(bpf_object__open_mem);
   resolve(bpf_object__load);
   resolve(bpf_object__next_program);
   resolve(bpf_program__attach);
   resolve(bpf_program__section_name);
   resolve(bpf_link__destroy);
   resolve(bpf_object__find_map_by_name);
   resolve(bpf_map__fd);
   resolve(bpf_map_lookup_elem);
   resolve(bpf_map_delete_elem);
   resolve(bpf_map_get_next_key);
   resolve(bpf_object__close);
   resolve(libbpf_get_error);

   #undef resolve

   dlerror();
   *((void **)(&sym_libbpf_set_print)) = dlsym(dlopenHandle, "libbpf_set_print");
   if (sym_libbpf_set_print)
      sym_libbpf_set_print(LibBpf_quietLibbpfPrint);

   return true;
}

void LibBpf_closeObject(LibBpfObject* this) {
   for (size_t i = 0; i < this->nLinks; i++) {
      if (this->links[i]) {
         sym_bpf_link__destroy(this->links[i]);
         this->links[i] = NULL;
      }
   }
   this->nLinks = 0;
   if (this->obj) {
      sym_bpf_object__close(this->obj);
      this->obj = NULL;
   }
}

/* One line summarizing why the kernel refused to load our eBPF programs.
 * Numbers are the stable Linux capability ids for CAP_SYS_ADMIN(21) and
 * CAP_BPF(39). */
#ifndef NDEBUG
static void LibBpf_reportLoadFailure(const char* name) {
   unsigned long long capEff = 0;
   FILE* fp = fopen("/proc/self/status", "re");
   if (fp) {
      char* line;
      bool found = false;
      while (!found && (line = String_readLine(fp)) != NULL) {
         if (String_startsWith(line, "CapEff:")) {
            capEff = strtoull(line + strlen("CapEff:"), NULL, 16);
            found = true;
         }
         free(line);
      }
      fclose(fp);
   }

   char lockdown[64] = "unreadable";
   fp = fopen("/sys/kernel/security/lockdown", "r");
   if (fp) {
      char* line = String_readLine(fp);
      fclose(fp);
      if (line) {
         String_safeStrncpy(lockdown, line, sizeof(lockdown));
         free(line);
      }
   }

   char paranoid[32] = "unreadable";
   fp = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
   if (fp) {
      char* line = String_readLine(fp);
      fclose(fp);
      if (line) {
         String_safeStrncpy(paranoid, line, sizeof(paranoid));
         free(line);
      }
   }

   LibBpf_debug(name, "CapEff=0x%llx (need CAP_BPF or CAP_SYS_ADMIN), lockdown=%s, perf_event_paranoid=%s\n",
                capEff, lockdown, paranoid);
}
#endif

bool LibBpf_openObject(LibBpfObject* this, const unsigned char* start, const unsigned char* end, LibBpf_ExpectedMissingFn expectedMissing) {
   if (!LibBpf_resolveLibbpf(this->name))
      return false;

   size_t objSize = (size_t) (end - start);
   if (!objSize) {
      LibBpf_debug(this->name, "embedded eBPF object is empty\n");
      return false;
   }

   void* obj = sym_bpf_object__open_mem(start, objSize, NULL);
   if (sym_libbpf_get_error(obj) != 0) {
      int e = errno;
      LibBpf_debug(this->name, "bpf_object__open_mem failed%s%s\n", e ? ": " : "", e ? strerror(e) : "");
      return false;
   }
   this->obj = obj;

   if (sym_bpf_object__load(obj) != 0) {
      int e = errno;
      LibBpf_debug(this->name, "bpf_object__load failed (kernel refused the eBPF programs or maps%s%s)\n",
                   e ? ": " : "", e ? strerror(e) : "");
#ifndef NDEBUG
      if (e == EPERM)
         LibBpf_reportLoadFailure(this->name);
#endif
      goto fail;
   }

/* Attach every program parsed from SEC().
 * A probe whose target symbol does not exist on this kernel (e.g.
 * ping_v6_sendmsg() without CONFIG_IPV6) is skipped rather than fatal;
 * the probes that did attach still account the traffic they see. */
   this->nLinks = 0;
#if !defined(NDEBUG)
   struct LibBpfAttachResult {
      const char* section;
      bool attached;
      int error;
   } results[LIBBPF_MAX_PROGRAMS];
   size_t nResults = 0;
   bool anyNotAllowed = false;
#endif
   void* prog = NULL;
   while ((prog = sym_bpf_object__next_program(obj, prog)) != NULL) {
      void* link = sym_bpf_program__attach(prog);
      /* libbpf 0.x returns ERR_PTR for failures, 1.x returns NULL. */
      long linkErr = sym_libbpf_get_error(link);
#if !defined(NDEBUG)
      int error = linkErr ? (int) -linkErr : 0;
      if (nResults < LIBBPF_MAX_PROGRAMS) {
         results[nResults].section = sym_bpf_program__section_name(prog);
         results[nResults].attached = !linkErr;
         results[nResults].error = error;
         nResults++;
      }
#endif
      if (linkErr) {
#if !defined(NDEBUG)
         if (error == EPERM)
            anyNotAllowed = true;
#endif
      } else if (this->nLinks < LIBBPF_MAX_PROGRAMS) {
         this->links[this->nLinks++] = link;
      } else {
         sym_bpf_link__destroy(link);
      }
   }
   if (!this->nLinks)
      goto fail;
#if !defined(NDEBUG)
   LibBpf_debug(this->name, "eBPF probe attach summary (load succeeded):\n");
   for (size_t i = 0; i < nResults; i++) {
      const char* section = results[i].section ? results[i].section : "<unknown>";
      if (results[i].attached) {
         LibBpf_debug(this->name, "  %-26s attached\n", section);
      } else if (results[i].error == ENOENT) {
         const char* note = expectedMissing ? expectedMissing(section) : NULL;
         LibBpf_debug(this->name, "  %-26s skipped: expected missing (%s)\n", section, note ? note : "symbol not found");
      } else {
         LibBpf_debug(this->name, "  %-26s skipped: %s\n", section, results[i].error ? strerror(results[i].error) : "attach failed");
      }
   }
   if (anyNotAllowed) {
      char paranoid[32] = "unreadable";
      FILE* fp = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
      if (fp) {
         char* line = String_readLine(fp);
         fclose(fp);
         if (line) {
            String_safeStrncpy(paranoid, line, sizeof(paranoid));
            free(line);
         }
      }
      LibBpf_debug(this->name, "kprobe attach needs CAP_SYS_ADMIN or perf_event_paranoid<=2 (current: %s; a \"Lockdown:\" line in dmesg means kernel lockdown is the cause)\n", paranoid);
   }
#else
   (void) expectedMissing;
#endif

   LibBpf_debug(this->name, "eBPF monitoring active via %s\n", dlopenName);
   return true;

fail:
   LibBpf_closeObject(this);
   return false;
}

int LibBpf_mapFd(const LibBpfObject* this, const char* mapName) {
   void* map = sym_bpf_object__find_map_by_name(this->obj, mapName);
   if (!map) {
      LibBpf_debug(this->name, "map %s not found in eBPF object\n", mapName);
      return -1;
   }

   int fd = sym_bpf_map__fd(map);
   if (fd < 0)
      LibBpf_debug(this->name, "cannot get file descriptor for map %s\n", mapName);
   return fd;
}

int LibBpf_mapLookupElem(int fd, const void* key, void* value) {
   return sym_bpf_map_lookup_elem(fd, key, value);
}

int LibBpf_mapDeleteElem(int fd, const void* key) {
   return sym_bpf_map_delete_elem(fd, key);
}

int LibBpf_mapGetNextKey(int fd, const void* key, void* nextKey) {
   return sym_bpf_map_get_next_key(fd, key, nextKey);
}

void LibBpf_done(void) {
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
}

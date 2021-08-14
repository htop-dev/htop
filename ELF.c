#include "ELF.h"

#ifdef HAVE_LIBELF

#include <errno.h>
#include <gelf.h>

#include "CRT.h"
#include "GenericHashtable.h"


static GenericHashtable* resultCache = NULL;

struct CEntry {
   dev_t dev;
   ino_t ino;
   off_t size;
   time_t mtime;
   elf_state_t data;
   char* runpath;
};

static size_t CEntry_Hash(ght_data_t data) {
   const struct CEntry* ce = (struct CEntry*) data;
   return ce->ino;
}

static int CEntry_Compare(ght_data_t a, ght_data_t b) {
   const struct CEntry* ceA = (const struct CEntry*) a;
   const struct CEntry* ceB = (const struct CEntry*) b;
   int r;

   r = SPACESHIP_NUMBER(ceA->dev, ceB->dev);
   if (r)
      return r;

   r = SPACESHIP_NUMBER(ceA->ino, ceB->ino);
   if (r)
      return r;

   return 0;
}

static void CEntry_Destroy(ght_data_t data) {
   struct CEntry* ce = (struct CEntry*) data;
   free(ce->runpath);
   free(ce);
}

void ELF_init() {
   if (!resultCache)
      resultCache = GenericHashtable_new(100, true, CEntry_Hash, CEntry_Compare, CEntry_Destroy);
}

void ELF_cleanup() {
   if (resultCache) {
      GenericHashtable_delete(resultCache);
      resultCache = NULL;
   }
}

static bool isStackProtectorFuncName(const char* func) {
   static const char* const protectorNames[] = {
      "__intel_security_cookie",
      "__stack_chk_fail",
   };

   if (func[0] != '_' || func[1] != '_')
      return false;

   for (size_t i = 0; i < ARRAYSIZE(protectorNames); i++)
      if (String_eq(func, protectorNames[i]))
         return true;

   return false;
}

static bool binarySearch(const char* const* data, size_t dataSize, const char* name) {
   size_t min = 0;
   size_t max = dataSize;

   while (min < max) {
      size_t pos = (max + min) / 2;

      int r = strcmp(data[pos], name);
      if (r == 0)
         return true;

      if (r > 0)
         max = pos;
      else
         min = pos + 1;
   }

   return false;
}

static bool isFortifyFuncName(const char* func) {
   static const char* const fortifyNames[] = {
      "__asprintf_chk",
      "__confstr_chk",
      "__dprintf_chk",
      "__explicit_bzero_chk",
      "__fdelt_chk",
      "__fgets_chk",
      "__fgets_unlocked_chk",
      "__fgetws_chk",
      "__fgetws_unlocked_chk",
      "__fprintf_chk",
      "__fread_chk",
      "__fread_unlocked_chk",
      "__fwprintf_chk",
      "__getcwd_chk",
      "__getdomainname_chk",
      "__getgroups_chk",
      "__gethostname_chk",
      "__getlogin_r_chk",
      "__gets_chk",
      "__getwd_chk",
      "__longjmp_chk",
      "__mbsnrtowcs_chk",
      "__mbsrtowcs_chk",
      "__mbstowcs_chk",
      "__memcpy_chk",
      "__memmove_chk",
      "__mempcpy_chk",
      "__memset_chk",
      "__obstack_printf_chk",
      "__obstack_vprintf_chk",
      "__poll_chk",
      "__ppoll_chk",
      "__pread64_chk",
      "__pread_chk",
      "__printf_chk",
      "__ptsname_r_chk",
      "__read_chk",
      "__readlinkat_chk",
      "__readlink_chk",
      "__realpath_chk",
      "__recv_chk",
      "__recvfrom_chk",
      "__snprintf_chk",
      "__sprintf_chk",
      "__stpcpy_chk",
      "__stpncpy_chk",
      "__strcat_chk",
      "__strcpy_chk",
      "__strncat_chk",
      "__strncpy_chk",
      "__swprintf_chk",
      "__syslog_chk",
      "__ttyname_r_chk",
      "__vasprintf_chk",
      "__vdprintf_chk",
      "__vfprintf_chk",
      "__vfwprintf_chk",
      "__vprintf_chk",
      "__vsnprintf_chk",
      "__vsprintf_chk",
      "__vswprintf_chk",
      "__vsyslog_chk",
      "__vwprintf_chk",
      "__wcpcpy_chk",
      "__wcpncpy_chk",
      "__wcrtomb_chk",
      "__wcscat_chk",
      "__wcscpy_chk",
      "__wcsncat_chk",
      "__wcsncpy_chk",
      "__wcsnrtombs_chk",
      "__wcsrtombs_chk",
      "__wcstombs_chk",
      "__wctomb_chk",
      "__wmemcpy_chk",
      "__wmemmove_chk",
      "__wmempcpy_chk",
      "__wmemset_chk",
      "__wprintf_chk",
   };

   if (func[0] != '_' || func[1] != '_')
      return false;

   return binarySearch(fortifyNames, ARRAYSIZE(fortifyNames), func);
}

static bool isFortifyableFuncName(const char* func) {
   static const char* const fortifyableNames[] = {
      "asprintf",
      "confstr",
      "dprintf",
      "explicit_bzero",
      "fdelt",
      "fgets",
      "fgets_unlocked",
      "fgetws",
      "fgetws_unlocked",
      "fprintf",
      "fread",
      "fread_unlocked",
      "fwprintf",
      "getcwd",
      "getdomainname",
      "getgroups",
      "gethostname",
      "getlogin_r",
      "gets",
      "getwd",
      "longjmp",
      "mbsnrtowcs",
      "mbsrtowcs",
      "mbstowcs",
      "memcpy",
      "memmove",
      "mempcpy",
      "memset",
      "obstack_printf",
      "obstack_vprintf",
      "poll",
      "ppoll",
      "read64",
      "pread",
      "printf",
      "ptsname_r",
      "read",
      "readlinkat",
      "readlink",
      "realpath",
      "recv",
      "recvfrom",
      "snprintf",
      "sprintf",
      "stpcpy",
      "stpncpy",
      "strcat",
      "strcpy",
      "strncat",
      "strncpy",
      "swprintf",
      "syslog",
      "ttyname_r",
      "vasprintf",
      "vdprintf",
      "vfprintf",
      "vfwprintf",
      "vprintf",
      "vsnprintf",
      "vsprintf",
      "vswprintf",
      "vsyslog",
      "vwprintf",
      "wcpcpy",
      "wcpncpy",
      "wcrtomb",
      "wcscat",
      "wcscpy",
      "wcsncat",
      "wcsncpy",
      "wcsnrtombs",
      "wcsrtombs",
      "wcstombs",
      "wctomb",
      "wmemcpy",
      "wmemmove",
      "wmempcpy",
      "wmemset",
      "wprintf",
   };

   return binarySearch(fortifyableNames, ARRAYSIZE(fortifyableNames), func);
}

static elf_state_t scanBinary(int fd, char** runpath) {
   Elf* e = NULL;
   int rc = -1;
   elf_state_t result = 0;

   if (elf_version(EV_CURRENT) == EV_NONE) {
      fprintf(stderr, "ELF library initialization failed: %s\n", elf_errmsg(-1));
      goto failure;
   }

   e = elf_begin(fd, ELF_C_READ, NULL);
   if (!e) {
      fprintf(stderr, "elf_begin() failed: %s\n", elf_errmsg(-1));
      goto failure;
   }

   if (elf_kind(e) != ELF_K_ELF) {
      fprintf(stderr, "not an ELF object\n");
      rc = -2;
      goto failure;
   }

   int class = gelf_getclass(e);
   if (class == ELFCLASSNONE) {
      fprintf(stderr, "getclass() failed: %s\n", elf_errmsg(-1));
      goto failure;
   }
   if (class == ELFCLASS32) {
      result |= ELF_32_BIT;
   } else if (class == ELFCLASS64) {
      result |= ELF_64_BIT;
   }

   size_t n;
   if (elf_getphdrnum(e, &n) != 0) {
      fprintf(stderr, "elf_getphdrnum() failed: %s\n", elf_errmsg(-1));
      goto failure;
   }

   for (size_t i = 0; i < n; i++) {
      GElf_Phdr phdr;
      if (gelf_getphdr(e, i, &phdr) != &phdr) {
         fprintf(stderr, "gelf_getphdr() failed: %s\n", elf_errmsg(-1));
         goto failure;
      }

      if (phdr.p_type == PT_GNU_RELRO && phdr.p_flags == PF_R)
         result |= ELF_HARDEN_RELRO;

      if (phdr.p_type == PT_GNU_STACK && !(phdr.p_flags & PF_X))
         result |= ELF_HARDEN_NX;
   }

   size_t shstrndx;
   if (elf_getshdrstrndx(e, &shstrndx) != 0) {
      fprintf(stderr, "elf_getshdrstrndx() failed: %s\n", elf_errmsg(-1));
      goto failure;
   }

   Elf_Scn* scn = NULL;
   while ((scn = elf_nextscn(e, scn)) != NULL) {
      GElf_Shdr shdr;
      if (gelf_getshdr(scn, &shdr) != & shdr) {
         fprintf(stderr, "getshdr() failed: %s\n", elf_errmsg(-1));
         goto failure;
      }

      if (shdr.sh_type == SHT_DYNSYM) {
         Elf_Data* data = NULL;
         while ((data = elf_getdata(scn, data)) != NULL) {
            for (size_t i = 0; i < (shdr.sh_size / shdr.sh_entsize); i++) {
               GElf_Sym sym;
               if (gelf_getsym(data, i, &sym) != &sym) {
                  fprintf(stderr, "gelf_getsym() failed: %s\n", elf_errmsg(-1));
                  goto failure;
               }

               const char* funcName = elf_strptr(e, shdr.sh_link, sym.st_name);

               if ((sym.st_info & 0xf) == STT_FUNC &&
                  (((unsigned char)sym.st_info) >> 4) == STB_GLOBAL &&
                  ((sym.st_other) & 0x03) == STV_DEFAULT) {
                  if (!(result & ELF_HARDEN_STACKPROTECTOR) && isStackProtectorFuncName(funcName))
                     result |= ELF_HARDEN_STACKPROTECTOR;
                  else if (!(result & ELF_HARDEN_FORTIFY) && isFortifyFuncName(funcName))
                     result |= ELF_HARDEN_FORTIFY;
                  else if (!(result & ELF_HARDEN_FORTIFIABLE) && isFortifyableFuncName(funcName))
                     result |= ELF_HARDEN_FORTIFIABLE;
               }

               if ((result & ELF_HARDEN_STACKPROTECTOR) && (result & ELF_HARDEN_FORTIFY) && (result & ELF_HARDEN_FORTIFIABLE))
                  goto stop;
            }
         }
         stop: ;
      }

      if (shdr.sh_type == SHT_DYNAMIC) {
         Elf_Data* data = NULL;
         while ((data = elf_getdata(scn, data)) != NULL) {
            for (size_t i = 0; i < (shdr.sh_size / shdr.sh_entsize); i++) {
               GElf_Dyn dyn;
               if (gelf_getdyn(data, i, &dyn) != &dyn) {
                  fprintf(stderr, "gelf_getdyn() failed: %s\n", elf_errmsg(-1));
                  goto failure;
               }

               if (dyn.d_tag == DT_FLAGS && dyn.d_un.d_val == DF_BIND_NOW)
                  result |= ELF_HARDEN_BINDNOW;

               if (dyn.d_tag == DT_FLAGS_1 && (dyn.d_un.d_val & DF_1_PIE))
                  result |= ELF_HARDEN_PIE;

               if (dyn.d_tag == DT_RPATH)
                  result |= ELF_HARDEN_RPATH;

               if (dyn.d_tag == DT_RUNPATH) {
                  result |= ELF_HARDEN_RUNPATH;
                  if (runpath)
                     free_and_xStrdup(runpath, elf_strptr(e, shdr.sh_link, dyn.d_un.d_ptr));
               }
            }
         }
      }
   }

   elf_end(e);
   return result;

failure:
   if (e)
      elf_end(e);
   return (rc == -2) ? ELF_FLAG_NO_ELF : ELF_FLAG_ELF_ERROR;
}

void ELF_writeHardeningField(RichString* str, elf_state_t es) {
   if (es & ELF_FLAG_ELF_ERROR) {
      RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "(elf error)");
      RichString_appendChr(str, 0, ' ', 10);
      return;
   }

   if (es & ELF_FLAG_IO_ERROR) {
      RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "(io error)");
      RichString_appendChr(str, 0, ' ', 11);
      return;
   }

   if (es & ELF_FLAG_NO_ACCESS) {
      RichString_appendAscii(str, CRT_colors[PROCESS_SHADOW], "(no access)");
      RichString_appendChr(str, 0, ' ', 10);
      return;
   }

   if (es & ELF_FLAG_NO_ELF) {
      RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "(no elf)");
      RichString_appendChr(str, 0, ' ', 13);
      return;
   }

   if (!(es & ELF_FLAG_SCANNED)) {
      RichString_appendAscii(str, CRT_colors[PROCESS], "(not scanned)");
      RichString_appendChr(str, 0, ' ', 8);
      return;
   }

   if ((es & ELF_HARDEN_RELRO) &&
       (es & ELF_HARDEN_BINDNOW) &&
       (es & ELF_HARDEN_STACKPROTECTOR) &&
       (es & ELF_HARDEN_NX) &&
       (es & ELF_HARDEN_PIE) &&
       !(es & ELF_HARDEN_RPATH) &&
       !(es & ELF_HARDEN_RUNPATH) &&
       (es & ELF_HARDEN_FORTIFY)) {
      RichString_appendAscii(str, CRT_colors[PROCESS_LOW_PRIORITY], "(ok)");
      RichString_appendChr(str, 0, ' ', 17);
      return;
   }

   int written = 0;

   if (!(es & ELF_HARDEN_RELRO)) {
      written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "RELRO ");
   } else if (!(es & ELF_HARDEN_BINDNOW)) {
      written += RichString_appendAscii(str, CRT_colors[MEMORY_CACHE], "BNOW ");
   }

   if (!(es & ELF_HARDEN_STACKPROTECTOR))
      written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "SP ");

   if (!(es & ELF_HARDEN_NX))
      written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "NX ");

   if (!(es & ELF_HARDEN_PIE))
      written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "PIE ");

   if (es & ELF_HARDEN_RPATH)
      written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "RPATH ");

   if (es & ELF_HARDEN_RUNPATH)
      written += RichString_appendAscii(str, CRT_colors[MEMORY_CACHE], "RUNPATH ");

   if (!(es & ELF_HARDEN_FORTIFY)) {
      if (es & ELF_HARDEN_FORTIFIABLE) {
         written += RichString_appendAscii(str, CRT_colors[PROCESS_HIGH_PRIORITY], "FF ");
      } else {
         written += RichString_appendAscii(str, CRT_colors[MEMORY_CACHE], "FF? ");
      }
   }

   if (written < 21)
      RichString_appendChr(str, 0, ' ', 21 - written);
}

void ELF_readData(Process* proc, openat_arg_t procFd) {
   int r;

   int fd = Compat_openat(procFd, "exe", O_RDONLY);
   if (fd < 0) {
      proc->elfState = (errno == EACCES || errno == ENOENT) ? ELF_FLAG_NO_ACCESS : ELF_FLAG_IO_ERROR;
      free(proc->elfRunpath);
      proc->elfRunpath = NULL;
      return;
   }

   struct stat statbuf;
   r = fstat(fd, &statbuf);
   if (r < 0) {
      proc->elfState = ELF_FLAG_IO_ERROR;
      free(proc->elfRunpath);
      proc->elfRunpath = NULL;
      close(fd);
      return;
   }

   bool newEntry = false;
   struct CEntry* centry = GenericHashtable_get(resultCache, &((struct CEntry) { .dev = statbuf.st_dev, .ino = statbuf.st_ino, }));
   if (centry) {
      if (statbuf.st_size == centry->size &&
          statbuf.st_mtime == centry->mtime) {
         proc->elfState = centry->data;
         if (centry->runpath) {
            free_and_xStrdup(&proc->elfRunpath, centry->runpath);
         } else {
            free(proc->elfRunpath);
            proc->elfRunpath = NULL;
         }
         close(fd);
         return;
      }
   } else {
      centry = xCalloc(1, sizeof(struct CEntry));
      newEntry = true;
   }

   elf_state_t data = scanBinary(fd, &centry->runpath) | ELF_FLAG_SCANNED;

   close(fd);

   centry->ino = statbuf.st_ino;
   centry->dev = statbuf.st_dev;
   centry->size = statbuf.st_size;
   centry->mtime = statbuf.st_mtime;
   centry->data = data;

   if (newEntry)
      GenericHashtable_put(resultCache, centry);

   proc->elfState = data;
   if (centry->runpath) {
      free_and_xStrdup(&proc->elfRunpath, centry->runpath);
   } else {
      free(proc->elfRunpath);
      proc->elfRunpath = NULL;
   }
}

#else /* HAVE_LIBELF */

static int make_iso_compilers_happy ATTR_UNUSED;

#endif /* HAVE_LIBELF */

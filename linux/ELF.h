#ifndef HEADER_ELF
#define HEADER_ELF

#include "config.h" // IWYU pragma: keep

#include "Compat.h"
#include "LinuxProcess.h"


#define ELF_HARDEN_BINDNOW         (1u <<  0)
#define ELF_HARDEN_FORTIFIABLE     (1u <<  1)
#define ELF_HARDEN_FORTIFY         (1u <<  2)
#define ELF_HARDEN_NX              (1u <<  3)
#define ELF_HARDEN_PIE             (1u <<  4)
#define ELF_HARDEN_RELRO           (1u <<  5)
#define ELF_HARDEN_RPATH           (1u <<  6)
#define ELF_HARDEN_RUNPATH         (1u <<  7)
#define ELF_HARDEN_STACKPROTECTOR  (1u <<  8)
#define ELF_32_BIT                 (1u <<  9)
#define ELF_64_BIT                 (1u << 10)
#define ELF_FLAG_SCANNED           (1u << 11)
#define ELF_FLAG_NO_ACCESS         (1u << 12)
#define ELF_FLAG_NO_ELF            (1u << 13)
#define ELF_FLAG_ELF_ERROR         (1u << 14)
#define ELF_FLAG_IO_ERROR          (1u << 15)


void ELF_init(void);
void ELF_cleanup(void);

void ELF_readData(LinuxProcess* lp, openat_arg_t procFd);

void ELF_writeHardeningField(RichString* str, elf_state_t es);

#endif /* HEADER_ELF */

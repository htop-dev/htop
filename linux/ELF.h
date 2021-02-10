#ifndef HEADER_ELF
#define HEADER_ELF

#include "config.h" // IWYU pragma: keep

#include "Compat.h"
#include "LinuxProcess.h"


#define ELF_HARDEN_BINDNOW         (1U <<  0)
#define ELF_HARDEN_FORTIFIABLE     (1U <<  1)
#define ELF_HARDEN_FORTIFY         (1U <<  2)
#define ELF_HARDEN_NX              (1U <<  3)
#define ELF_HARDEN_PIE             (1U <<  4)
#define ELF_HARDEN_RELRO           (1U <<  5)
#define ELF_HARDEN_RPATH           (1U <<  6)
#define ELF_HARDEN_RUNPATH         (1U <<  7)
#define ELF_HARDEN_STACKPROTECTOR  (1U <<  8)
#define ELF_32_BIT                 (1U <<  9)
#define ELF_64_BIT                 (1U << 10)
#define ELF_FLAG_SCANNED           (1U << 11)
#define ELF_FLAG_NO_ACCESS         (1U << 12)
#define ELF_FLAG_NO_ELF            (1U << 13)
#define ELF_FLAG_ELF_ERROR         (1U << 14)
#define ELF_FLAG_IO_ERROR          (1U << 15)


void ELF_init(void);
void ELF_cleanup(void);

void ELF_readData(LinuxProcess* lp, openat_arg_t procFd);

void ELF_writeHardeningField(RichString* str, elf_state_t es);

#endif /* HEADER_ELF */

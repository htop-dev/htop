/*
 * Copyright © 2010-2011 Université Bordeaux 1
 * Copyright © 2010 Cisco Systems, Inc.  All rights reserved.
 *
 * See COPYING in top-level directory.
 */

/* Internals for x86's cpuid.  */

#ifndef HWLOC_PRIVATE_CPUID_H
#define HWLOC_PRIVATE_CPUID_H

#ifdef HWLOC_X86_32_ARCH
static __hwloc_inline int hwloc_have_cpuid(void)
{
  int ret;
  unsigned tmp, tmp2;
  asm(
      "mov $0,%0\n\t"   /* Not supported a priori */

      "pushfl   \n\t"   /* Save flags */

      "pushfl   \n\t"                                           \
      "pop %1   \n\t"   /* Get flags */                         \

#define TRY_TOGGLE                                              \
      "xor $0x00200000,%1\n\t"        /* Try to toggle ID */    \
      "mov %1,%2\n\t"   /* Save expected value */               \
      "push %1  \n\t"                                           \
      "popfl    \n\t"   /* Try to toggle */                     \
      "pushfl   \n\t"                                           \
      "pop %1   \n\t"                                           \
      "cmp %1,%2\n\t"   /* Compare with expected value */       \
      "jnz Lhwloc1\n\t"   /* Unexpected, failure */               \

      TRY_TOGGLE        /* Try to set/clear */
      TRY_TOGGLE        /* Try to clear/set */

      "mov $1,%0\n\t"   /* Passed the test! */

      "Lhwloc1: \n\t"
      "popfl    \n\t"   /* Restore flags */

      : "=r" (ret), "=&r" (tmp), "=&r" (tmp2));
  return ret;
}
#endif /* HWLOC_X86_32_ARCH */
#ifdef HWLOC_X86_64_ARCH
static __hwloc_inline int hwloc_have_cpuid(void) { return 1; }
#endif /* HWLOC_X86_64_ARCH */

static __hwloc_inline void hwloc_cpuid(unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
  asm(
#ifdef HWLOC_X86_32_ARCH 
  "push %%ebx\n\t"
#endif
  "cpuid\n\t"
#ifdef HWLOC_X86_32_ARCH 
  "mov %%ebx,%1\n\t"
  "pop %%ebx\n\t"
#endif
  : "+a" (*eax),
#ifdef HWLOC_X86_32_ARCH 
    "=r" (*ebx),
#else
    "=b" (*ebx),
#endif
    "+c" (*ecx), "=d" (*edx));
}

#endif /* HWLOC_PRIVATE_CPUID_H */

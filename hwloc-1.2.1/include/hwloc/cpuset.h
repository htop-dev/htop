/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009-2010 Université Bordeaux 1
 * Copyright © 2009-2010 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief The old deprecated Cpuset API.
 * This interface should not be used anymore, it will be dropped in a later release.
 *
 * hwloc/bitmap.h should be used instead. Most hwloc_cpuset_foo functions are
 * replaced with hwloc_bitmap_foo. The only exceptions are:
 * - hwloc_cpuset_from_string -> hwloc_bitmap_sscanf
 * - hwloc_cpuset_cpu -> hwloc_bitmap_only
 * - hwloc_cpuset_all_but_cpu -> hwloc_bitmap_allbut
 */

#ifndef HWLOC_CPUSET_H
#define HWLOC_CPUSET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hwloc/bitmap.h"

static __hwloc_inline hwloc_bitmap_t __hwloc_attribute_deprecated hwloc_cpuset_alloc(void) { return hwloc_bitmap_alloc(); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_free(hwloc_bitmap_t bitmap) { hwloc_bitmap_free(bitmap); }
static __hwloc_inline hwloc_bitmap_t __hwloc_attribute_deprecated hwloc_cpuset_dup(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_dup(bitmap); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_copy(hwloc_bitmap_t dst, hwloc_const_bitmap_t src) { hwloc_bitmap_copy(dst, src); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_snprintf(char * __hwloc_restrict buf, size_t buflen, hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_snprintf(buf, buflen, bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_asprintf(char ** strp, hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_asprintf(strp, bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_from_string(hwloc_bitmap_t bitmap, const char * __hwloc_restrict string) { return hwloc_bitmap_sscanf(bitmap, string); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_zero(hwloc_bitmap_t bitmap) { hwloc_bitmap_zero(bitmap); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_fill(hwloc_bitmap_t bitmap) { hwloc_bitmap_fill(bitmap); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_from_ulong(hwloc_bitmap_t bitmap, unsigned long mask) { hwloc_bitmap_from_ulong(bitmap, mask); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_from_ith_ulong(hwloc_bitmap_t bitmap, unsigned i, unsigned long mask) { hwloc_bitmap_from_ith_ulong(bitmap, i, mask); }
static __hwloc_inline unsigned __hwloc_attribute_deprecated long hwloc_cpuset_to_ulong(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_to_ulong(bitmap); }
static __hwloc_inline unsigned __hwloc_attribute_deprecated long hwloc_cpuset_to_ith_ulong(hwloc_const_bitmap_t bitmap, unsigned i) { return hwloc_bitmap_to_ith_ulong(bitmap, i); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_cpu(hwloc_bitmap_t bitmap, unsigned index_) { hwloc_bitmap_only(bitmap, index_); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_all_but_cpu(hwloc_bitmap_t bitmap, unsigned index_) { hwloc_bitmap_allbut(bitmap, index_); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_set(hwloc_bitmap_t bitmap, unsigned index_) { hwloc_bitmap_set(bitmap, index_); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_set_range(hwloc_bitmap_t bitmap, unsigned begin, unsigned end) { hwloc_bitmap_set_range(bitmap, begin, end); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_set_ith_ulong(hwloc_bitmap_t bitmap, unsigned i, unsigned long mask) { hwloc_bitmap_set_ith_ulong(bitmap, i, mask); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_clr(hwloc_bitmap_t bitmap, unsigned index_) { hwloc_bitmap_clr(bitmap, index_); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_clr_range(hwloc_bitmap_t bitmap, unsigned begin, unsigned end) { hwloc_bitmap_clr_range(bitmap, begin, end); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_isset(hwloc_const_bitmap_t bitmap, unsigned index_) { return hwloc_bitmap_isset(bitmap, index_); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_iszero(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_iszero(bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_isfull(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_isfull(bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_isequal(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { return hwloc_bitmap_isequal(bitmap1, bitmap2); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_intersects(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { return hwloc_bitmap_intersects(bitmap1, bitmap2); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_isincluded(hwloc_const_bitmap_t sub_bitmap, hwloc_const_bitmap_t super_bitmap) { return hwloc_bitmap_isincluded(sub_bitmap, super_bitmap); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_or(hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { hwloc_bitmap_or(res, bitmap1, bitmap2); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_and(hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { hwloc_bitmap_and(res, bitmap1, bitmap2); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_andnot(hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { hwloc_bitmap_andnot(res, bitmap1, bitmap2); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_xor(hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { hwloc_bitmap_xor(res, bitmap1, bitmap2); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_not(hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap) { hwloc_bitmap_not(res, bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_first(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_first(bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_last(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_last(bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_next(hwloc_const_bitmap_t bitmap, unsigned prev) { return hwloc_bitmap_next(bitmap, prev); }
static __hwloc_inline void __hwloc_attribute_deprecated hwloc_cpuset_singlify(hwloc_bitmap_t bitmap) { hwloc_bitmap_singlify(bitmap); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_compare_first(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { return hwloc_bitmap_compare_first(bitmap1, bitmap2); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_compare(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) { return hwloc_bitmap_compare(bitmap1, bitmap2); }
static __hwloc_inline int __hwloc_attribute_deprecated hwloc_cpuset_weight(hwloc_const_bitmap_t bitmap) { return hwloc_bitmap_weight(bitmap); }

#define hwloc_cpuset_foreach_begin hwloc_bitmap_foreach_begin
#define hwloc_cpuset_foreach_end hwloc_bitmap_foreach_end

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HWLOC_CPUSET_H */

/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief The bitmap API, for use in hwloc itself.
 */

#ifndef HWLOC_BITMAP_H
#define HWLOC_BITMAP_H

#include <hwloc/autogen/config.h>
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup hwlocality_bitmap The bitmap API
 *
 * The ::hwloc_bitmap_t type represents a set of objects, typically OS
 * processors -- which may actually be hardware threads (represented
 * by ::hwloc_cpuset_t, which is a typedef for ::hwloc_bitmap_t) -- or
 * memory nodes (represented by ::hwloc_nodeset_t, which is also a
 * typedef for ::hwloc_bitmap_t).  
 *
 * <em>Both CPU and node sets are always indexed by OS physical number.</em>
 *
 * \note CPU sets and nodesets are described in \ref hwlocality_sets.
 *
 * A bitmap may be of infinite size.
 * @{
 */


/** \brief
 * Set of bits represented as an opaque pointer to an internal bitmap.
 */
typedef struct hwloc_bitmap_s * hwloc_bitmap_t;
/** \brief a non-modifiable ::hwloc_bitmap_t */
typedef const struct hwloc_bitmap_s * hwloc_const_bitmap_t;


/*
 * Bitmap allocation, freeing and copying.
 */

/** \brief Allocate a new empty bitmap.
 *
 * \returns A valid bitmap or \c NULL.
 *
 * The bitmap should be freed by a corresponding call to
 * hwloc_bitmap_free().
 */
HWLOC_DECLSPEC hwloc_bitmap_t hwloc_bitmap_alloc(void) __hwloc_attribute_malloc;

/** \brief Allocate a new full bitmap. */
HWLOC_DECLSPEC hwloc_bitmap_t hwloc_bitmap_alloc_full(void) __hwloc_attribute_malloc;

/** \brief Free bitmap \p bitmap.
 *
 * If \p bitmap is \c NULL, no operation is performed.
 */
HWLOC_DECLSPEC void hwloc_bitmap_free(hwloc_bitmap_t bitmap);

/** \brief Duplicate bitmap \p bitmap by allocating a new bitmap and copying \p bitmap contents.
 *
 * If \p bitmap is \c NULL, \c NULL is returned.
 */
HWLOC_DECLSPEC hwloc_bitmap_t hwloc_bitmap_dup(hwloc_const_bitmap_t bitmap) __hwloc_attribute_malloc;

/** \brief Copy the contents of bitmap \p src into the already allocated bitmap \p dst */
HWLOC_DECLSPEC void hwloc_bitmap_copy(hwloc_bitmap_t dst, hwloc_const_bitmap_t src);


/*
 * Bitmap/String Conversion
 */

/** \brief Stringify a bitmap.
 *
 * Up to \p buflen characters may be written in buffer \p buf.
 *
 * If \p buflen is 0, \p buf may safely be \c NULL.
 *
 * \return the number of character that were actually written if not truncating,
 * or that would have been written (not including the ending \\0).
 */
HWLOC_DECLSPEC int hwloc_bitmap_snprintf(char * __hwloc_restrict buf, size_t buflen, hwloc_const_bitmap_t bitmap);

/** \brief Stringify a bitmap into a newly allocated string.
 */
HWLOC_DECLSPEC int hwloc_bitmap_asprintf(char ** strp, hwloc_const_bitmap_t bitmap);

/** \brief Parse a bitmap string and stores it in bitmap \p bitmap.
 */
HWLOC_DECLSPEC int hwloc_bitmap_sscanf(hwloc_bitmap_t bitmap, const char * __hwloc_restrict string);

/** \brief Stringify a bitmap in the list format.
 *
 * Lists are comma-separated indexes or ranges.
 * Ranges are dash separated indexes.
 * The last range may not have a ending indexes if the bitmap is infinite.
 *
 * Up to \p buflen characters may be written in buffer \p buf.
 *
 * If \p buflen is 0, \p buf may safely be \c NULL.
 *
 * \return the number of character that were actually written if not truncating,
 * or that would have been written (not including the ending \\0).
 */
HWLOC_DECLSPEC int hwloc_bitmap_list_snprintf(char * __hwloc_restrict buf, size_t buflen, hwloc_const_bitmap_t bitmap);

/** \brief Stringify a bitmap into a newly allocated list string.
 */
HWLOC_DECLSPEC int hwloc_bitmap_list_asprintf(char ** strp, hwloc_const_bitmap_t bitmap);

/** \brief Parse a list string and stores it in bitmap \p bitmap.
 */
HWLOC_DECLSPEC int hwloc_bitmap_list_sscanf(hwloc_bitmap_t bitmap, const char * __hwloc_restrict string);

/** \brief Stringify a bitmap in the taskset-specific format.
 *
 * The taskset command manipulates bitmap strings that contain a single
 * (possible very long) hexadecimal number starting with 0x.
 *
 * Up to \p buflen characters may be written in buffer \p buf.
 *
 * If \p buflen is 0, \p buf may safely be \c NULL.
 *
 * \return the number of character that were actually written if not truncating,
 * or that would have been written (not including the ending \\0).
 */
HWLOC_DECLSPEC int hwloc_bitmap_taskset_snprintf(char * __hwloc_restrict buf, size_t buflen, hwloc_const_bitmap_t bitmap);

/** \brief Stringify a bitmap into a newly allocated taskset-specific string.
 */
HWLOC_DECLSPEC int hwloc_bitmap_taskset_asprintf(char ** strp, hwloc_const_bitmap_t bitmap);

/** \brief Parse a taskset-specific bitmap string and stores it in bitmap \p bitmap.
 */
HWLOC_DECLSPEC int hwloc_bitmap_taskset_sscanf(hwloc_bitmap_t bitmap, const char * __hwloc_restrict string);


/*
 * Building bitmaps.
 */

/** \brief Empty the bitmap \p bitmap */
HWLOC_DECLSPEC void hwloc_bitmap_zero(hwloc_bitmap_t bitmap);

/** \brief Fill bitmap \p bitmap with all possible indexes (even if those objects don't exist or are otherwise unavailable) */
HWLOC_DECLSPEC void hwloc_bitmap_fill(hwloc_bitmap_t bitmap);

/** \brief Empty the bitmap \p bitmap and add bit \p id */
HWLOC_DECLSPEC void hwloc_bitmap_only(hwloc_bitmap_t bitmap, unsigned id);

/** \brief Fill the bitmap \p and clear the index \p id */
HWLOC_DECLSPEC void hwloc_bitmap_allbut(hwloc_bitmap_t bitmap, unsigned id);

/** \brief Setup bitmap \p bitmap from unsigned long \p mask */
HWLOC_DECLSPEC void hwloc_bitmap_from_ulong(hwloc_bitmap_t bitmap, unsigned long mask);

/** \brief Setup bitmap \p bitmap from unsigned long \p mask used as \p i -th subset */
HWLOC_DECLSPEC void hwloc_bitmap_from_ith_ulong(hwloc_bitmap_t bitmap, unsigned i, unsigned long mask);


/*
 * Modifying bitmaps.
 */

/** \brief Add index \p id in bitmap \p bitmap */
HWLOC_DECLSPEC void hwloc_bitmap_set(hwloc_bitmap_t bitmap, unsigned id);

/** \brief Add indexes from \p begin to \p end in bitmap \p bitmap.
 *
 * If \p end is \c -1, the range is infinite.
 */
HWLOC_DECLSPEC void hwloc_bitmap_set_range(hwloc_bitmap_t bitmap, unsigned begin, int end);

/** \brief Replace \p i -th subset of bitmap \p bitmap with unsigned long \p mask */
HWLOC_DECLSPEC void hwloc_bitmap_set_ith_ulong(hwloc_bitmap_t bitmap, unsigned i, unsigned long mask);

/** \brief Remove index \p id from bitmap \p bitmap */
HWLOC_DECLSPEC void hwloc_bitmap_clr(hwloc_bitmap_t bitmap, unsigned id);

/** \brief Remove indexes from \p begin to \p end in bitmap \p bitmap.
 *
 * If \p end is \c -1, the range is infinite.
 */
HWLOC_DECLSPEC void hwloc_bitmap_clr_range(hwloc_bitmap_t bitmap, unsigned begin, int end);

/** \brief Keep a single index among those set in bitmap \p bitmap
 *
 * May be useful before binding so that the process does not
 * have a chance of migrating between multiple logical CPUs
 * in the original mask.
 */
HWLOC_DECLSPEC void hwloc_bitmap_singlify(hwloc_bitmap_t bitmap);


/*
 * Consulting bitmaps.
 */

/** \brief Convert the beginning part of bitmap \p bitmap into unsigned long \p mask */
HWLOC_DECLSPEC unsigned long hwloc_bitmap_to_ulong(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Convert the \p i -th subset of bitmap \p bitmap into unsigned long mask */
HWLOC_DECLSPEC unsigned long hwloc_bitmap_to_ith_ulong(hwloc_const_bitmap_t bitmap, unsigned i) __hwloc_attribute_pure;

/** \brief Test whether index \p id is part of bitmap \p bitmap */
HWLOC_DECLSPEC int hwloc_bitmap_isset(hwloc_const_bitmap_t bitmap, unsigned id) __hwloc_attribute_pure;

/** \brief Test whether bitmap \p bitmap is empty */
HWLOC_DECLSPEC int hwloc_bitmap_iszero(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Test whether bitmap \p bitmap is completely full */
HWLOC_DECLSPEC int hwloc_bitmap_isfull(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Compute the first index (least significant bit) in bitmap \p bitmap
 *
 * \return -1 if no index is set.
 */
HWLOC_DECLSPEC int hwloc_bitmap_first(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Compute the next index in bitmap \p bitmap which is after index \p prev
 *
 * If \p prev is -1, the first index is returned.
 *
 * \return -1 if no index with higher index is bitmap.
 */
HWLOC_DECLSPEC int hwloc_bitmap_next(hwloc_const_bitmap_t bitmap, int prev) __hwloc_attribute_pure;

/** \brief Compute the last index (most significant bit) in bitmap \p bitmap
 *
 * \return -1 if no index is bitmap, or if the index bitmap is infinite.
 */
HWLOC_DECLSPEC int hwloc_bitmap_last(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Compute the "weight" of bitmap \p bitmap (i.e., number of
 * indexes that are in the bitmap).
 *
 * \return the number of indexes that are in the bitmap.
 */
HWLOC_DECLSPEC int hwloc_bitmap_weight(hwloc_const_bitmap_t bitmap) __hwloc_attribute_pure;

/** \brief Loop macro iterating on bitmap \p bitmap
 * \hideinitializer
 *
 * \p index is the loop variable; it should be an unsigned int.  The
 * first iteration will set \p index to the lowest index in the bitmap.
 * Successive iterations will iterate through, in order, all remaining
 * indexes that in the bitmap.  To be specific: each iteration will return a
 * value for \p index such that hwloc_bitmap_isset(bitmap, index) is true.
 *
 * The assert prevents the loop from being infinite if the bitmap is infinite.
 */
#define hwloc_bitmap_foreach_begin(id, bitmap) \
do { \
        assert(hwloc_bitmap_weight(bitmap) != -1); \
        for (id = hwloc_bitmap_first(bitmap); \
             (unsigned) id != (unsigned) -1; \
             id = hwloc_bitmap_next(bitmap, id)) { \
/** \brief End of loop. Needs a terminating ';'.
 * \hideinitializer
 *
 * \sa hwloc_bitmap_foreach_begin */
#define hwloc_bitmap_foreach_end() \
        } \
} while (0)


/*
 * Combining bitmaps.
 */

/** \brief Or bitmaps \p bitmap1 and \p bitmap2 and store the result in bitmap \p res */
HWLOC_DECLSPEC void hwloc_bitmap_or (hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2);

/** \brief And bitmaps \p bitmap1 and \p bitmap2 and store the result in bitmap \p res */
HWLOC_DECLSPEC void hwloc_bitmap_and (hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2);

/** \brief And bitmap \p bitmap1 and the negation of \p bitmap2 and store the result in bitmap \p res */
HWLOC_DECLSPEC void hwloc_bitmap_andnot (hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2);

/** \brief Xor bitmaps \p bitmap1 and \p bitmap2 and store the result in bitmap \p res */
HWLOC_DECLSPEC void hwloc_bitmap_xor (hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2);

/** \brief Negate bitmap \p bitmap and store the result in bitmap \p res */
HWLOC_DECLSPEC void hwloc_bitmap_not (hwloc_bitmap_t res, hwloc_const_bitmap_t bitmap);


/*
 * Comparing bitmaps.
 */

/** \brief Test whether bitmaps \p bitmap1 and \p bitmap2 intersects */
HWLOC_DECLSPEC int hwloc_bitmap_intersects (hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) __hwloc_attribute_pure;

/** \brief Test whether bitmap \p sub_bitmap is part of bitmap \p super_bitmap */
HWLOC_DECLSPEC int hwloc_bitmap_isincluded (hwloc_const_bitmap_t sub_bitmap, hwloc_const_bitmap_t super_bitmap) __hwloc_attribute_pure;

/** \brief Test whether bitmap \p bitmap1 is equal to bitmap \p bitmap2 */
HWLOC_DECLSPEC int hwloc_bitmap_isequal (hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) __hwloc_attribute_pure;

/** \brief Compare bitmaps \p bitmap1 and \p bitmap2 using their lowest index.
 *
 * Smaller least significant bit is smaller.
 * The empty bitmap is considered higher than anything.
 */
HWLOC_DECLSPEC int hwloc_bitmap_compare_first(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) __hwloc_attribute_pure;

/** \brief Compare bitmaps \p bitmap1 and \p bitmap2 using their highest index.
 *
 * Higher most significant bit is higher.
 * The empty bitmap is considered lower than anything.
 */
HWLOC_DECLSPEC int hwloc_bitmap_compare(hwloc_const_bitmap_t bitmap1, hwloc_const_bitmap_t bitmap2) __hwloc_attribute_pure;

/** @} */


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* HWLOC_BITMAP_H */

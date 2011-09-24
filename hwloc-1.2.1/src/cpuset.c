/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/* cpuset.h converts from the old cpuset API to the new bitmap API, we don't want it here */
#ifndef HWLOC_CPUSET_H
/* make sure cpuset.h will not be automatically included here */
#define HWLOC_CPUSET_H 1
#else
#error Do not include cpuset.h in cpuset.c
#endif

#include <private/autogen/config.h>
#include <private/misc.h>
#include <private/private.h>
#include <hwloc/bitmap.h>

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

/* TODO
 * - have a way to change the initial allocation size
 * - preallocate inside the bitmap structure (so that the whole structure is a cacheline for instance)
 *   and allocate a dedicated array only later when reallocating larger
 */

/* magic number */
#define HWLOC_BITMAP_MAGIC 0x20091007

/* actual opaque type internals */
struct hwloc_bitmap_s {
  unsigned ulongs_count; /* how many ulong bitmasks are valid, >= 1 */
  unsigned ulongs_allocated; /* how many ulong bitmasks are allocated, >= ulongs_count */
  unsigned long *ulongs;
  int infinite; /* set to 1 if all bits beyond ulongs are set */
#ifdef HWLOC_DEBUG
  int magic;
#endif
};

/* overzealous check in debug-mode, not as powerful as valgrind but still useful */
#ifdef HWLOC_DEBUG
#define HWLOC__BITMAP_CHECK(set) do {				\
  assert((set)->magic == HWLOC_BITMAP_MAGIC);			\
  assert((set)->ulongs_count >= 1);				\
  assert((set)->ulongs_allocated >= (set)->ulongs_count);	\
} while (0)
#else
#define HWLOC__BITMAP_CHECK(set)
#endif

/* extract a subset from a set using an index or a cpu */
#define HWLOC_SUBBITMAP_INDEX(cpu)		((cpu)/(HWLOC_BITS_PER_LONG))
#define HWLOC_SUBBITMAP_CPU_ULBIT(cpu)		((cpu)%(HWLOC_BITS_PER_LONG))
/* Read from a bitmap ulong without knowing whether x is valid.
 * Writers should make sure that x is valid and modify set->ulongs[x] directly.
 */
#define HWLOC_SUBBITMAP_READULONG(set,x)	((x) < (set)->ulongs_count ? (set)->ulongs[x] : (set)->infinite ? HWLOC_SUBBITMAP_FULL : HWLOC_SUBBITMAP_ZERO)

/* predefined subset values */
#define HWLOC_SUBBITMAP_ZERO			0UL
#define HWLOC_SUBBITMAP_FULL			(~0UL)
#define HWLOC_SUBBITMAP_ULBIT(bit)		(1UL<<(bit))
#define HWLOC_SUBBITMAP_CPU(cpu)		HWLOC_SUBBITMAP_ULBIT(HWLOC_SUBBITMAP_CPU_ULBIT(cpu))
#define HWLOC_SUBBITMAP_ULBIT_TO(bit)		(HWLOC_SUBBITMAP_FULL>>(HWLOC_BITS_PER_LONG-1-(bit)))
#define HWLOC_SUBBITMAP_ULBIT_FROM(bit)		(HWLOC_SUBBITMAP_FULL<<(bit))
#define HWLOC_SUBBITMAP_ULBIT_FROMTO(begin,end)	(HWLOC_SUBBITMAP_ULBIT_TO(end) & HWLOC_SUBBITMAP_ULBIT_FROM(begin))

struct hwloc_bitmap_s * hwloc_bitmap_alloc(void)
{
  struct hwloc_bitmap_s * set;

  set = malloc(sizeof(struct hwloc_bitmap_s));
  if (!set)
    return NULL;

  set->ulongs_count = 1;
  set->ulongs_allocated = 64/sizeof(unsigned long);
  set->ulongs = malloc(64);
  if (!set->ulongs) {
    free(set);
    return NULL;
  }

  set->ulongs[0] = HWLOC_SUBBITMAP_ZERO;
  set->infinite = 0;
#ifdef HWLOC_DEBUG
  set->magic = HWLOC_BITMAP_MAGIC;
#endif
  return set;
}

struct hwloc_bitmap_s * hwloc_bitmap_alloc_full(void)
{
  struct hwloc_bitmap_s * set = hwloc_bitmap_alloc();
  if (set) {
    set->infinite = 1;
    set->ulongs[0] = HWLOC_SUBBITMAP_FULL;
  }
  return set;
}

void hwloc_bitmap_free(struct hwloc_bitmap_s * set)
{
  if (!set)
    return;

  HWLOC__BITMAP_CHECK(set);
#ifdef HWLOC_DEBUG
  set->magic = 0;
#endif

  free(set->ulongs);
  free(set);
}

/* enlarge until it contains at least needed_count ulongs.
 */
static void
hwloc_bitmap_enlarge_by_ulongs(struct hwloc_bitmap_s * set, unsigned needed_count)
{
  unsigned tmp = 1 << hwloc_flsl((unsigned long) needed_count - 1);
  if (tmp > set->ulongs_allocated) {
    set->ulongs = realloc(set->ulongs, tmp * sizeof(unsigned long));
    assert(set->ulongs);
    set->ulongs_allocated = tmp;
  }
}

/* enlarge until it contains at least needed_count ulongs,
 * and update new ulongs according to the infinite field.
 */
static void
hwloc_bitmap_realloc_by_ulongs(struct hwloc_bitmap_s * set, unsigned needed_count)
{
  unsigned i;

  HWLOC__BITMAP_CHECK(set);

  if (needed_count <= set->ulongs_count)
    return;

  /* realloc larger if needed */
  hwloc_bitmap_enlarge_by_ulongs(set, needed_count);

  /* fill the newly allocated subset depending on the infinite flag */
  for(i=set->ulongs_count; i<needed_count; i++)
    set->ulongs[i] = set->infinite ? HWLOC_SUBBITMAP_FULL : HWLOC_SUBBITMAP_ZERO;
  set->ulongs_count = needed_count;
}

/* realloc until it contains at least cpu+1 bits */
#define hwloc_bitmap_realloc_by_cpu_index(set, cpu) hwloc_bitmap_realloc_by_ulongs(set, ((cpu)/HWLOC_BITS_PER_LONG)+1)

/* reset a bitmap to exactely the needed size.
 * the caller must reinitialize all ulongs and the infinite flag later.
 */
static void
hwloc_bitmap_reset_by_ulongs(struct hwloc_bitmap_s * set, unsigned needed_count)
{
  hwloc_bitmap_enlarge_by_ulongs(set, needed_count);
  set->ulongs_count = needed_count;
}

/* reset until it contains exactly cpu+1 bits (roundup to a ulong).
 * the caller must reinitialize all ulongs and the infinite flag later.
 */
#define hwloc_bitmap_reset_by_cpu_index(set, cpu) hwloc_bitmap_reset_by_ulongs(set, ((cpu)/HWLOC_BITS_PER_LONG)+1)

struct hwloc_bitmap_s * hwloc_bitmap_dup(const struct hwloc_bitmap_s * old)
{
  struct hwloc_bitmap_s * new;

  if (!old)
    return NULL;

  HWLOC__BITMAP_CHECK(old);

  new = malloc(sizeof(struct hwloc_bitmap_s));
  if (!new)
    return NULL;

  new->ulongs = malloc(old->ulongs_allocated * sizeof(unsigned long));
  if (!new->ulongs) {
    free(new);
    return NULL;
  }
  new->ulongs_allocated = old->ulongs_allocated;
  new->ulongs_count = old->ulongs_count;
  memcpy(new->ulongs, old->ulongs, new->ulongs_count * sizeof(unsigned long));
  new->infinite = old->infinite;
#ifdef HWLOC_DEBUG
  new->magic = HWLOC_BITMAP_MAGIC;
#endif
  return new;
}

void hwloc_bitmap_copy(struct hwloc_bitmap_s * dst, const struct hwloc_bitmap_s * src)
{
  HWLOC__BITMAP_CHECK(dst);
  HWLOC__BITMAP_CHECK(src);

  hwloc_bitmap_reset_by_ulongs(dst, src->ulongs_count);

  memcpy(dst->ulongs, src->ulongs, src->ulongs_count * sizeof(unsigned long));
  dst->infinite = src->infinite;
}

/* Strings always use 32bit groups */
#define HWLOC_PRIxSUBBITMAP		"%08lx"
#define HWLOC_BITMAP_SUBSTRING_SIZE	32
#define HWLOC_BITMAP_SUBSTRING_LENGTH	(HWLOC_BITMAP_SUBSTRING_SIZE/4)
#define HWLOC_BITMAP_STRING_PER_LONG	(HWLOC_BITS_PER_LONG/HWLOC_BITMAP_SUBSTRING_SIZE)

int hwloc_bitmap_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  ssize_t size = buflen;
  char *tmp = buf;
  int res, ret = 0;
  int needcomma = 0;
  int i;
  unsigned long accum = 0;
  int accumed = 0;
#if HWLOC_BITS_PER_LONG == HWLOC_BITMAP_SUBSTRING_SIZE
  const unsigned long accum_mask = ~0UL;
#else /* HWLOC_BITS_PER_LONG != HWLOC_BITMAP_SUBSTRING_SIZE */
  const unsigned long accum_mask = ((1UL << HWLOC_BITMAP_SUBSTRING_SIZE) - 1) << (HWLOC_BITS_PER_LONG - HWLOC_BITMAP_SUBSTRING_SIZE);
#endif /* HWLOC_BITS_PER_LONG != HWLOC_BITMAP_SUBSTRING_SIZE */

  HWLOC__BITMAP_CHECK(set);

  /* mark the end in case we do nothing later */
  if (buflen > 0)
    tmp[0] = '\0';

  if (set->infinite) {
    res = hwloc_snprintf(tmp, size, "0xf...f");
    needcomma = 1;
    if (res < 0)
      return -1;
    ret += res;
    if (res >= size)
      res = size>0 ? size - 1 : 0;
    tmp += res;
    size -= res;
    /* optimize a common case: full bitmap should appear as 0xf...f instead of 0xf...f,0xffffffff */
    if (set->ulongs_count == 1 && set->ulongs[0] == HWLOC_SUBBITMAP_FULL)
      return ret;
  }

  i=set->ulongs_count-1;
  while (i>=0 || accumed) {
    /* Refill accumulator */
    if (!accumed) {
      accum = set->ulongs[i--];
      accumed = HWLOC_BITS_PER_LONG;
    }

    if (accum & accum_mask) {
      /* print the whole subset if not empty */
        res = hwloc_snprintf(tmp, size, needcomma ? ",0x" HWLOC_PRIxSUBBITMAP : "0x" HWLOC_PRIxSUBBITMAP,
		     (accum & accum_mask) >> (HWLOC_BITS_PER_LONG - HWLOC_BITMAP_SUBSTRING_SIZE));
      needcomma = 1;
    } else if (i == -1 && accumed == HWLOC_BITMAP_SUBSTRING_SIZE) {
      /* print a single 0 to mark the last subset */
      res = hwloc_snprintf(tmp, size, needcomma ? ",0x0" : "0x0");
    } else if (needcomma) {
      res = hwloc_snprintf(tmp, size, ",");
    } else {
      res = 0;
    }
    if (res < 0)
      return -1;
    ret += res;

#if HWLOC_BITS_PER_LONG == HWLOC_BITMAP_SUBSTRING_SIZE
    accum = 0;
    accumed = 0;
#else
    accum <<= HWLOC_BITMAP_SUBSTRING_SIZE;
    accumed -= HWLOC_BITMAP_SUBSTRING_SIZE;
#endif

    if (res >= size)
      res = size>0 ? size - 1 : 0;

    tmp += res;
    size -= res;
  }

  return ret;
}

int hwloc_bitmap_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  int len;
  char *buf;

  HWLOC__BITMAP_CHECK(set);

  len = hwloc_bitmap_snprintf(NULL, 0, set);
  buf = malloc(len+1);
  *strp = buf;
  return hwloc_bitmap_snprintf(buf, len+1, set);
}

int hwloc_bitmap_sscanf(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string)
{
  const char * current = string;
  unsigned long accum = 0;
  int count=0;
  int infinite = 0;

  /* count how many substrings there are */
  count++;
  while ((current = strchr(current+1, ',')) != NULL)
    count++;

  current = string;
  if (!strncmp("0xf...f", current, 7)) {
    current += 7;
    if (*current != ',') {
      /* special case for infinite/full cpuset */
      hwloc_bitmap_fill(set);
      return 0;
    }
    current++;
    infinite = 1;
    count--;
  }

  hwloc_bitmap_reset_by_ulongs(set, (count + HWLOC_BITMAP_STRING_PER_LONG - 1) / HWLOC_BITMAP_STRING_PER_LONG);
  set->infinite = 0;

  while (*current != '\0') {
    unsigned long val;
    char *next;
    val = strtoul(current, &next, 16);

    assert(count > 0);
    count--;

    accum |= (val << ((count * HWLOC_BITMAP_SUBSTRING_SIZE) % HWLOC_BITS_PER_LONG));
    if (!(count % HWLOC_BITMAP_STRING_PER_LONG)) {
      set->ulongs[count / HWLOC_BITMAP_STRING_PER_LONG] = accum;
      accum = 0;
    }

    if (*next != ',') {
      if (*next || count > 0)
	goto failed;
      else
	break;
    }
    current = (const char*) next+1;
  }

  set->infinite = infinite; /* set at the end, to avoid spurious realloc with filled new ulongs */

  return 0;

 failed:
  /* failure to parse */
  hwloc_bitmap_zero(set);
  return -1;
}

int hwloc_bitmap_list_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  int prev = -1;
  hwloc_bitmap_t reverse;
  ssize_t size = buflen;
  char *tmp = buf;
  int res, ret = 0;
  int needcomma = 0;

  HWLOC__BITMAP_CHECK(set);

  reverse = hwloc_bitmap_alloc(); /* FIXME: add hwloc_bitmap_alloc_size() + hwloc_bitmap_init_allocated() to avoid malloc? */
  hwloc_bitmap_not(reverse, set);

  /* mark the end in case we do nothing later */
  if (buflen > 0)
    tmp[0] = '\0';

  while (1) {
    int begin, end;

    begin = hwloc_bitmap_next(set, prev);
    if (begin == -1)
      break;
    end = hwloc_bitmap_next(reverse, begin);

    if (end == begin+1) {
      res = hwloc_snprintf(tmp, size, needcomma ? ",%d" : "%d", begin);
    } else if (end == -1) {
      res = hwloc_snprintf(tmp, size, needcomma ? ",%d-" : "%d-", begin);
    } else {
      res = hwloc_snprintf(tmp, size, needcomma ? ",%d-%d" : "%d-%d", begin, end-1);
    }
    if (res < 0) {
      hwloc_bitmap_free(reverse);
      return -1;
    }
    ret += res;

    if (res >= size)
      res = size>0 ? size - 1 : 0;

    tmp += res;
    size -= res;
    needcomma = 1;

    if (end == -1)
      break;
    else
      prev = end - 1;
  }

  hwloc_bitmap_free(reverse);

  return ret;
}

int hwloc_bitmap_list_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  int len;
  char *buf;

  HWLOC__BITMAP_CHECK(set);

  len = hwloc_bitmap_list_snprintf(NULL, 0, set);
  buf = malloc(len+1);
  *strp = buf;
  return hwloc_bitmap_list_snprintf(buf, len+1, set);
}

int hwloc_bitmap_list_sscanf(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string)
{
  const char * current = string;
  char *next;
  long begin = -1, val;

  hwloc_bitmap_zero(set);

  while (*current != '\0') {

    /* ignore empty ranges */
    while (*current == ',')
      current++;

    val = strtoul(current, &next, 0);
    /* make sure we got at least one digit */
    if (next == current)
      goto failed;

    if (begin != -1) {
      /* finishing a range */
      hwloc_bitmap_set_range(set, begin, val);
      begin = -1;

    } else if (*next == '-') {
      /* starting a new range */
      if (*(next+1) == '\0') {
	/* infinite range */
	hwloc_bitmap_set_range(set, val, -1);
        break;
      } else {
	/* normal range */
	begin = val;
      }

    } else if (*next == ',' || *next == '\0') {
      /* single digit */
      hwloc_bitmap_set(set, val);
    }

    if (*next == '\0')
      break;
    current = next+1;
  }

  return 0;

 failed:
  /* failure to parse */
  hwloc_bitmap_zero(set);
  return -1;
}

int hwloc_bitmap_taskset_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  ssize_t size = buflen;
  char *tmp = buf;
  int res, ret = 0;
  int started = 0;
  int i;

  HWLOC__BITMAP_CHECK(set);

  /* mark the end in case we do nothing later */
  if (buflen > 0)
    tmp[0] = '\0';

  if (set->infinite) {
    res = hwloc_snprintf(tmp, size, "0xf...f");
    started = 1;
    if (res < 0)
      return -1;
    ret += res;
    if (res >= size)
      res = size>0 ? size - 1 : 0;
    tmp += res;
    size -= res;
    /* optimize a common case: full bitmap should appear as 0xf...f instead of 0xf...fffffffff */
    if (set->ulongs_count == 1 && set->ulongs[0] == HWLOC_SUBBITMAP_FULL)
      return ret;
  }

  i=set->ulongs_count-1;
  while (i>=0) {
    unsigned long val = set->ulongs[i--];
    if (started) {
      /* print the whole subset */
#if HWLOC_BITS_PER_LONG == 64
      res = hwloc_snprintf(tmp, size, "%016lx", val);
#else
      res = hwloc_snprintf(tmp, size, "%08lx", val);
#endif
    } else if (val || i == -1) {
      res = hwloc_snprintf(tmp, size, "0x%lx", val);
      started = 1;
    } else {
      res = 0;
    }
    if (res < 0)
      return -1;
    ret += res;
    if (res >= size)
      res = size>0 ? size - 1 : 0;
    tmp += res;
    size -= res;
  }

  return ret;
}

int hwloc_bitmap_taskset_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set)
{
  int len;
  char *buf;

  HWLOC__BITMAP_CHECK(set);

  len = hwloc_bitmap_taskset_snprintf(NULL, 0, set);
  buf = malloc(len+1);
  *strp = buf;
  return hwloc_bitmap_taskset_snprintf(buf, len+1, set);
}

int hwloc_bitmap_taskset_sscanf(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string)
{
  const char * current = string;
  int chars;
  int count;
  int infinite = 0;

  current = string;
  if (!strncmp("0xf...f", current, 7)) {
    /* infinite bitmap */
    infinite = 1;
    current += 7;
    if (*current == '\0') {
      /* special case for infinite/full bitmap */
      hwloc_bitmap_fill(set);
      return 0;
    }
  } else {
    /* finite bitmap */
    if (!strncmp("0x", current, 2))
      current += 2;
    if (*current == '\0') {
      /* special case for empty bitmap */
      hwloc_bitmap_zero(set);
      return 0;
    }
  }
  /* we know there are other characters now */

  chars = strlen(current);
  count = (chars * 4 + HWLOC_BITS_PER_LONG - 1) / HWLOC_BITS_PER_LONG;

  hwloc_bitmap_reset_by_ulongs(set, count);
  set->infinite = 0;

  while (*current != '\0') {
    int tmpchars;
    char ustr[17];
    unsigned long val;
    char *next;

    tmpchars = chars % (HWLOC_BITS_PER_LONG/4);
    if (!tmpchars)
      tmpchars = (HWLOC_BITS_PER_LONG/4);

    memcpy(ustr, current, tmpchars);
    ustr[tmpchars] = '\0';
    val = strtoul(ustr, &next, 16);
    if (*next != '\0')
      goto failed;

    set->ulongs[count-1] = val;

    current += tmpchars;
    chars -= tmpchars;
    count--;
  }

  set->infinite = infinite; /* set at the end, to avoid spurious realloc with filled new ulongs */

  return 0;

 failed:
  /* failure to parse */
  hwloc_bitmap_zero(set);
  return -1;
}

static void hwloc_bitmap__zero(struct hwloc_bitmap_s *set)
{
	unsigned i;
	for(i=0; i<set->ulongs_count; i++)
		set->ulongs[i] = HWLOC_SUBBITMAP_ZERO;
	set->infinite = 0;
}

void hwloc_bitmap_zero(struct hwloc_bitmap_s * set)
{
	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_ulongs(set, 1);
	hwloc_bitmap__zero(set);
}

static void hwloc_bitmap__fill(struct hwloc_bitmap_s * set)
{
	unsigned i;
	for(i=0; i<set->ulongs_count; i++)
		set->ulongs[i] = HWLOC_SUBBITMAP_FULL;
	set->infinite = 1;
}

void hwloc_bitmap_fill(struct hwloc_bitmap_s * set)
{
	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_ulongs(set, 1);
	hwloc_bitmap__fill(set);
}

void hwloc_bitmap_from_ulong(struct hwloc_bitmap_s *set, unsigned long mask)
{
	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_ulongs(set, 1);
	set->ulongs[0] = mask; /* there's always at least one ulong allocated */
	set->infinite = 0;
}

void hwloc_bitmap_from_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask)
{
	unsigned j;

	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_ulongs(set, i+1);
	set->ulongs[i] = mask;
	for(j=0; j<i; j++)
		set->ulongs[j] = HWLOC_SUBBITMAP_ZERO;
	set->infinite = 0;
}

unsigned long hwloc_bitmap_to_ulong(const struct hwloc_bitmap_s *set)
{
	HWLOC__BITMAP_CHECK(set);

	return set->ulongs[0]; /* there's always at least one ulong allocated */
}

unsigned long hwloc_bitmap_to_ith_ulong(const struct hwloc_bitmap_s *set, unsigned i)
{
	HWLOC__BITMAP_CHECK(set);

	return HWLOC_SUBBITMAP_READULONG(set, i);
}

void hwloc_bitmap_only(struct hwloc_bitmap_s * set, unsigned cpu)
{
	unsigned index_ = HWLOC_SUBBITMAP_INDEX(cpu);

	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_cpu_index(set, cpu);
	hwloc_bitmap__zero(set);
	set->ulongs[index_] |= HWLOC_SUBBITMAP_CPU(cpu);
}

void hwloc_bitmap_allbut(struct hwloc_bitmap_s * set, unsigned cpu)
{
	unsigned index_ = HWLOC_SUBBITMAP_INDEX(cpu);

	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_reset_by_cpu_index(set, cpu);
	hwloc_bitmap__fill(set);
	set->ulongs[index_] &= ~HWLOC_SUBBITMAP_CPU(cpu);
}

void hwloc_bitmap_set(struct hwloc_bitmap_s * set, unsigned cpu)
{
	unsigned index_ = HWLOC_SUBBITMAP_INDEX(cpu);

	HWLOC__BITMAP_CHECK(set);

	/* nothing to do if setting inside the infinite part of the bitmap */
	if (set->infinite && cpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
		return;

	hwloc_bitmap_realloc_by_cpu_index(set, cpu);
	set->ulongs[index_] |= HWLOC_SUBBITMAP_CPU(cpu);
}

void hwloc_bitmap_set_range(struct hwloc_bitmap_s * set, unsigned begincpu, int _endcpu)
{
	unsigned i;
	unsigned beginset,endset;
	unsigned endcpu = (unsigned) _endcpu;

	HWLOC__BITMAP_CHECK(set);

	if (_endcpu == -1) {
		set->infinite = 1;
		/* keep endcpu == -1 since this unsigned is actually larger than anything else */
	}

	if (set->infinite) {
		/* truncate the range according to the infinite part of the bitmap */
		if (endcpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
			endcpu = set->ulongs_count * HWLOC_BITS_PER_LONG - 1;
		if (begincpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
			return;
	}
	if (endcpu < begincpu)
		return;
	hwloc_bitmap_realloc_by_cpu_index(set, endcpu);

	beginset = HWLOC_SUBBITMAP_INDEX(begincpu);
	endset = HWLOC_SUBBITMAP_INDEX(endcpu);
	for(i=beginset+1; i<endset; i++)
		set->ulongs[i] = HWLOC_SUBBITMAP_FULL;
	if (beginset == endset) {
		set->ulongs[beginset] |= HWLOC_SUBBITMAP_ULBIT_FROMTO(HWLOC_SUBBITMAP_CPU_ULBIT(begincpu), HWLOC_SUBBITMAP_CPU_ULBIT(endcpu));
	} else {
		set->ulongs[beginset] |= HWLOC_SUBBITMAP_ULBIT_FROM(HWLOC_SUBBITMAP_CPU_ULBIT(begincpu));
		set->ulongs[endset] |= HWLOC_SUBBITMAP_ULBIT_TO(HWLOC_SUBBITMAP_CPU_ULBIT(endcpu));
	}
}

void hwloc_bitmap_set_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask)
{
	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_realloc_by_ulongs(set, i+1);
	set->ulongs[i] = mask;
}

void hwloc_bitmap_clr(struct hwloc_bitmap_s * set, unsigned cpu)
{
	unsigned index_ = HWLOC_SUBBITMAP_INDEX(cpu);

	HWLOC__BITMAP_CHECK(set);

	/* nothing to do if clearing inside the infinitely-unset part of the bitmap */
	if (!set->infinite && cpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
		return;

	hwloc_bitmap_realloc_by_cpu_index(set, cpu);
	set->ulongs[index_] &= ~HWLOC_SUBBITMAP_CPU(cpu);
}

void hwloc_bitmap_clr_range(struct hwloc_bitmap_s * set, unsigned begincpu, int _endcpu)
{
	unsigned i;
	unsigned beginset,endset;
	unsigned endcpu = (unsigned) _endcpu;

	HWLOC__BITMAP_CHECK(set);

	if (_endcpu == -1) {
		set->infinite = 0;
		/* keep endcpu == -1 since this unsigned is actually larger than anything else */
	}

	if (!set->infinite) {
		/* truncate the range according to the infinitely-unset part of the bitmap */
		if (endcpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
			endcpu = set->ulongs_count * HWLOC_BITS_PER_LONG - 1;
		if (begincpu >= set->ulongs_count * HWLOC_BITS_PER_LONG)
			return;
	}
	if (endcpu < begincpu)
		return;
	hwloc_bitmap_realloc_by_cpu_index(set, endcpu);

	beginset = HWLOC_SUBBITMAP_INDEX(begincpu);
	endset = HWLOC_SUBBITMAP_INDEX(endcpu);
	for(i=beginset+1; i<endset; i++)
		set->ulongs[i] = HWLOC_SUBBITMAP_ZERO;
	if (beginset == endset) {
		set->ulongs[beginset] &= ~HWLOC_SUBBITMAP_ULBIT_FROMTO(HWLOC_SUBBITMAP_CPU_ULBIT(begincpu), HWLOC_SUBBITMAP_CPU_ULBIT(endcpu));
	} else {
		set->ulongs[beginset] &= ~HWLOC_SUBBITMAP_ULBIT_FROM(HWLOC_SUBBITMAP_CPU_ULBIT(begincpu));
		set->ulongs[endset] &= ~HWLOC_SUBBITMAP_ULBIT_TO(HWLOC_SUBBITMAP_CPU_ULBIT(endcpu));
	}
}

int hwloc_bitmap_isset(const struct hwloc_bitmap_s * set, unsigned cpu)
{
	unsigned index_ = HWLOC_SUBBITMAP_INDEX(cpu);

	HWLOC__BITMAP_CHECK(set);

	return (HWLOC_SUBBITMAP_READULONG(set, index_) & HWLOC_SUBBITMAP_CPU(cpu)) != 0;
}

int hwloc_bitmap_iszero(const struct hwloc_bitmap_s *set)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set);

	if (set->infinite)
		return 0;
	for(i=0; i<set->ulongs_count; i++)
		if (set->ulongs[i] != HWLOC_SUBBITMAP_ZERO)
			return 0;
	return 1;
}

int hwloc_bitmap_isfull(const struct hwloc_bitmap_s *set)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set);

	if (!set->infinite)
		return 0;
	for(i=0; i<set->ulongs_count; i++)
		if (set->ulongs[i] != HWLOC_SUBBITMAP_FULL)
			return 0;
	return 1;
}

int hwloc_bitmap_isequal (const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	for(i=0; i<set1->ulongs_count || i<set2->ulongs_count; i++)
		if (HWLOC_SUBBITMAP_READULONG(set1, i) != HWLOC_SUBBITMAP_READULONG(set2, i))
			return 0;

	if (set1->infinite != set2->infinite)
		return 0;

	return 1;
}

int hwloc_bitmap_intersects (const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	for(i=0; i<set1->ulongs_count || i<set2->ulongs_count; i++)
		if ((HWLOC_SUBBITMAP_READULONG(set1, i) & HWLOC_SUBBITMAP_READULONG(set2, i)) != HWLOC_SUBBITMAP_ZERO)
			return 1;

	if (set1->infinite && set2->infinite)
		return 0;

	return 0;
}

int hwloc_bitmap_isincluded (const struct hwloc_bitmap_s *sub_set, const struct hwloc_bitmap_s *super_set)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(sub_set);
	HWLOC__BITMAP_CHECK(super_set);

	for(i=0; i<sub_set->ulongs_count; i++)
		if (HWLOC_SUBBITMAP_READULONG(super_set, i) != (HWLOC_SUBBITMAP_READULONG(super_set, i) | HWLOC_SUBBITMAP_READULONG(sub_set, i)))
			return 0;

	if (sub_set->infinite && !super_set->infinite)
		return 0;

	return 1;
}

void hwloc_bitmap_or (struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	const struct hwloc_bitmap_s *largest = set1->ulongs_count > set2->ulongs_count ? set1 : set2;
	unsigned i;

	HWLOC__BITMAP_CHECK(res);
	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	hwloc_bitmap_realloc_by_ulongs(res, largest->ulongs_count); /* cannot reset since the output may also be an input */

	for(i=0; i<res->ulongs_count; i++)
		res->ulongs[i] = HWLOC_SUBBITMAP_READULONG(set1, i) | HWLOC_SUBBITMAP_READULONG(set2, i);

	res->infinite = set1->infinite || set2->infinite;
}

void hwloc_bitmap_and (struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	const struct hwloc_bitmap_s *largest = set1->ulongs_count > set2->ulongs_count ? set1 : set2;
	unsigned i;

	HWLOC__BITMAP_CHECK(res);
	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	hwloc_bitmap_realloc_by_ulongs(res, largest->ulongs_count); /* cannot reset since the output may also be an input */

	for(i=0; i<res->ulongs_count; i++)
		res->ulongs[i] = HWLOC_SUBBITMAP_READULONG(set1, i) & HWLOC_SUBBITMAP_READULONG(set2, i);

	res->infinite = set1->infinite && set2->infinite;
}

void hwloc_bitmap_andnot (struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	const struct hwloc_bitmap_s *largest = set1->ulongs_count > set2->ulongs_count ? set1 : set2;
	unsigned i;

	HWLOC__BITMAP_CHECK(res);
	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	hwloc_bitmap_realloc_by_ulongs(res, largest->ulongs_count); /* cannot reset since the output may also be an input */

	for(i=0; i<res->ulongs_count; i++)
		res->ulongs[i] = HWLOC_SUBBITMAP_READULONG(set1, i) & ~HWLOC_SUBBITMAP_READULONG(set2, i);

	res->infinite = set1->infinite && !set2->infinite;
}

void hwloc_bitmap_xor (struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2)
{
	const struct hwloc_bitmap_s *largest = set1->ulongs_count > set2->ulongs_count ? set1 : set2;
	unsigned i;

	HWLOC__BITMAP_CHECK(res);
	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	hwloc_bitmap_realloc_by_ulongs(res, largest->ulongs_count); /* cannot reset since the output may also be an input */

	for(i=0; i<res->ulongs_count; i++)
		res->ulongs[i] = HWLOC_SUBBITMAP_READULONG(set1, i) ^ HWLOC_SUBBITMAP_READULONG(set2, i);

	res->infinite = (!set1->infinite) != (!set2->infinite);
}

void hwloc_bitmap_not (struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(res);
	HWLOC__BITMAP_CHECK(set);

	hwloc_bitmap_realloc_by_ulongs(res, set->ulongs_count); /* cannot reset since the output may also be an input */

	for(i=0; i<res->ulongs_count; i++)
		res->ulongs[i] = ~HWLOC_SUBBITMAP_READULONG(set, i);

	res->infinite = !set->infinite;
}

int hwloc_bitmap_first(const struct hwloc_bitmap_s * set)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set);

	for(i=0; i<set->ulongs_count; i++) {
		/* subsets are unsigned longs, use ffsl */
		unsigned long w = set->ulongs[i];
		if (w)
			return hwloc_ffsl(w) - 1 + HWLOC_BITS_PER_LONG*i;
	}

	if (set->infinite)
		return set->ulongs_count * HWLOC_BITS_PER_LONG;

	return -1;
}

int hwloc_bitmap_last(const struct hwloc_bitmap_s * set)
{
	int i;

	HWLOC__BITMAP_CHECK(set);

	if (set->infinite)
		return -1;

	for(i=set->ulongs_count-1; i>=0; i--) {
		/* subsets are unsigned longs, use flsl */
		unsigned long w = set->ulongs[i];
		if (w)
			return hwloc_flsl(w) - 1 + HWLOC_BITS_PER_LONG*i;
	}

	return -1;
}

int hwloc_bitmap_next(const struct hwloc_bitmap_s * set, int prev_cpu)
{
	unsigned i = HWLOC_SUBBITMAP_INDEX(prev_cpu + 1);

	HWLOC__BITMAP_CHECK(set);

	if (i >= set->ulongs_count) {
		if (set->infinite)
			return prev_cpu + 1;
		else
			return -1;
	}

	for(; i<set->ulongs_count; i++) {
		/* subsets are unsigned longs, use ffsl */
		unsigned long w = set->ulongs[i];

		/* if the prev cpu is in the same word as the possible next one,
		   we need to mask out previous cpus */
		if (prev_cpu >= 0 && HWLOC_SUBBITMAP_INDEX((unsigned) prev_cpu) == i)
			w &= ~HWLOC_SUBBITMAP_ULBIT_TO(HWLOC_SUBBITMAP_CPU_ULBIT(prev_cpu));

		if (w)
			return hwloc_ffsl(w) - 1 + HWLOC_BITS_PER_LONG*i;
	}

	if (set->infinite)
		return set->ulongs_count * HWLOC_BITS_PER_LONG;

	return -1;
}

void hwloc_bitmap_singlify(struct hwloc_bitmap_s * set)
{
	unsigned i;
	int found = 0;

	HWLOC__BITMAP_CHECK(set);

	for(i=0; i<set->ulongs_count; i++) {
		if (found) {
			set->ulongs[i] = HWLOC_SUBBITMAP_ZERO;
			continue;
		} else {
			/* subsets are unsigned longs, use ffsl */
			unsigned long w = set->ulongs[i];
			if (w) {
				int _ffs = hwloc_ffsl(w);
				set->ulongs[i] = HWLOC_SUBBITMAP_CPU(_ffs-1);
				found = 1;
			}
		}
	}

	if (set->infinite) {
		if (found) {
			set->infinite = 0;
		} else {
			/* set the first non allocated bit */
			unsigned first = set->ulongs_count * HWLOC_BITS_PER_LONG;
			set->infinite = 0; /* do not let realloc fill the newly allocated sets */
			hwloc_bitmap_set(set, first);
		}
	}
}

int hwloc_bitmap_compare_first(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2)
{
	unsigned i;

	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	for(i=0; i<set1->ulongs_count || i<set2->ulongs_count; i++) {
		unsigned long w1 = HWLOC_SUBBITMAP_READULONG(set1, i);
		unsigned long w2 = HWLOC_SUBBITMAP_READULONG(set2, i);
		if (w1 || w2) {
			int _ffs1 = hwloc_ffsl(w1);
			int _ffs2 = hwloc_ffsl(w2);
			/* if both have a bit set, compare for real */
			if (_ffs1 && _ffs2)
				return _ffs1-_ffs2;
			/* one is empty, and it is considered higher, so reverse-compare them */
			return _ffs2-_ffs1;
		}
	}
	if ((!set1->infinite) != (!set2->infinite))
		return !!set1->infinite - !!set2->infinite;
	return 0;
}

int hwloc_bitmap_compare(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2)
{
	const struct hwloc_bitmap_s *largest = set1->ulongs_count > set2->ulongs_count ? set1 : set2;
	int i;

	HWLOC__BITMAP_CHECK(set1);
	HWLOC__BITMAP_CHECK(set2);

	if ((!set1->infinite) != (!set2->infinite))
		return !!set1->infinite - !!set2->infinite;

	for(i=largest->ulongs_count-1; i>=0; i--) {
		unsigned long val1 = HWLOC_SUBBITMAP_READULONG(set1, (unsigned) i);
		unsigned long val2 = HWLOC_SUBBITMAP_READULONG(set2, (unsigned) i);
		if (val1 == val2)
			continue;
		return val1 < val2 ? -1 : 1;
	}

	return 0;
}

int hwloc_bitmap_weight(const struct hwloc_bitmap_s * set)
{
	int weight = 0;
	unsigned i;

	HWLOC__BITMAP_CHECK(set);

	if (set->infinite)
		return -1;

	for(i=0; i<set->ulongs_count; i++)
		weight += hwloc_weight_long(set->ulongs[i]);
	return weight;
}


/********************************************************************
 * everything below should be dropped when hwloc/cpuset.h is dropped
 */

/* for HWLOC_DECLSPEC */
#include <hwloc/autogen/config.h>

/* forward declarations (public headers do not export this API anymore) */
HWLOC_DECLSPEC struct hwloc_bitmap_s * hwloc_cpuset_alloc(void);
HWLOC_DECLSPEC void hwloc_cpuset_free(struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC struct hwloc_bitmap_s * hwloc_cpuset_dup(const struct hwloc_bitmap_s * old);
HWLOC_DECLSPEC void hwloc_cpuset_copy(struct hwloc_bitmap_s * dst, const struct hwloc_bitmap_s * src);
HWLOC_DECLSPEC int hwloc_cpuset_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set);
HWLOC_DECLSPEC int hwloc_cpuset_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set);
HWLOC_DECLSPEC int hwloc_cpuset_from_string(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string);
HWLOC_DECLSPEC int hwloc_cpuset_taskset_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set);
HWLOC_DECLSPEC int hwloc_cpuset_taskset_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set);
HWLOC_DECLSPEC int hwloc_cpuset_taskset_sscanf(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string);
HWLOC_DECLSPEC void hwloc_cpuset_zero(struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC void hwloc_cpuset_fill(struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC void hwloc_cpuset_from_ulong(struct hwloc_bitmap_s *set, unsigned long mask);
HWLOC_DECLSPEC void hwloc_cpuset_from_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask);
HWLOC_DECLSPEC unsigned long hwloc_cpuset_to_ulong(const struct hwloc_bitmap_s *set);
HWLOC_DECLSPEC unsigned long hwloc_cpuset_to_ith_ulong(const struct hwloc_bitmap_s *set, unsigned i);
HWLOC_DECLSPEC void hwloc_cpuset_cpu(struct hwloc_bitmap_s * set, unsigned cpu);
HWLOC_DECLSPEC void hwloc_cpuset_all_but_cpu(struct hwloc_bitmap_s * set, unsigned cpu);
HWLOC_DECLSPEC void hwloc_cpuset_set(struct hwloc_bitmap_s * set, unsigned cpu);
HWLOC_DECLSPEC void hwloc_cpuset_set_range(struct hwloc_bitmap_s * set, unsigned begincpu, unsigned endcpu);
HWLOC_DECLSPEC void hwloc_cpuset_set_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask);
HWLOC_DECLSPEC void hwloc_cpuset_clr(struct hwloc_bitmap_s * set, unsigned cpu);
HWLOC_DECLSPEC void hwloc_cpuset_clr_range(struct hwloc_bitmap_s * set, unsigned begincpu, unsigned endcpu);
HWLOC_DECLSPEC int hwloc_cpuset_isset(const struct hwloc_bitmap_s * set, unsigned cpu);
HWLOC_DECLSPEC int hwloc_cpuset_iszero(const struct hwloc_bitmap_s *set);
HWLOC_DECLSPEC int hwloc_cpuset_isfull(const struct hwloc_bitmap_s *set);
HWLOC_DECLSPEC int hwloc_cpuset_isequal(const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC int hwloc_cpuset_intersects(const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC int hwloc_cpuset_isincluded(const struct hwloc_bitmap_s *sub_set, const struct hwloc_bitmap_s *super_set);
HWLOC_DECLSPEC void hwloc_cpuset_or(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC void hwloc_cpuset_and(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC void hwloc_cpuset_andnot(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC void hwloc_cpuset_xor(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2);
HWLOC_DECLSPEC void hwloc_cpuset_not(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set);
HWLOC_DECLSPEC int hwloc_cpuset_first(const struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC int hwloc_cpuset_last(const struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC int hwloc_cpuset_next(const struct hwloc_bitmap_s * set, unsigned prev_cpu);
HWLOC_DECLSPEC void hwloc_cpuset_singlify(struct hwloc_bitmap_s * set);
HWLOC_DECLSPEC int hwloc_cpuset_compare_first(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2);
HWLOC_DECLSPEC int hwloc_cpuset_compare(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2);
HWLOC_DECLSPEC int hwloc_cpuset_weight(const struct hwloc_bitmap_s * set);

/* actual symbols converting from cpuset ABI into bitmap ABI */
struct hwloc_bitmap_s * hwloc_cpuset_alloc(void) { return hwloc_bitmap_alloc(); }
void hwloc_cpuset_free(struct hwloc_bitmap_s * set) { hwloc_bitmap_free(set); }
struct hwloc_bitmap_s * hwloc_cpuset_dup(const struct hwloc_bitmap_s * old) { return hwloc_bitmap_dup(old); }
void hwloc_cpuset_copy(struct hwloc_bitmap_s * dst, const struct hwloc_bitmap_s * src) { hwloc_bitmap_copy(dst, src); }
int hwloc_cpuset_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set) { return hwloc_bitmap_snprintf(buf, buflen, set); }
int hwloc_cpuset_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set) { return hwloc_bitmap_asprintf(strp, set); }
int hwloc_cpuset_from_string(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string) { return hwloc_bitmap_sscanf(set, string); }
int hwloc_cpuset_taskset_snprintf(char * __hwloc_restrict buf, size_t buflen, const struct hwloc_bitmap_s * __hwloc_restrict set) { return hwloc_bitmap_taskset_snprintf(buf, buflen, set); }
int hwloc_cpuset_taskset_asprintf(char ** strp, const struct hwloc_bitmap_s * __hwloc_restrict set) { return hwloc_bitmap_taskset_asprintf(strp, set); }
int hwloc_cpuset_taskset_sscanf(struct hwloc_bitmap_s *set, const char * __hwloc_restrict string) { return hwloc_bitmap_taskset_sscanf(set, string); }
void hwloc_cpuset_zero(struct hwloc_bitmap_s * set) { hwloc_bitmap_zero(set); }
void hwloc_cpuset_fill(struct hwloc_bitmap_s * set) { hwloc_bitmap_fill(set); }
void hwloc_cpuset_from_ulong(struct hwloc_bitmap_s *set, unsigned long mask) { hwloc_bitmap_from_ulong(set, mask); }
void hwloc_cpuset_from_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask) { hwloc_bitmap_from_ith_ulong(set, i, mask); }
unsigned long hwloc_cpuset_to_ulong(const struct hwloc_bitmap_s *set) { return hwloc_bitmap_to_ulong(set); }
unsigned long hwloc_cpuset_to_ith_ulong(const struct hwloc_bitmap_s *set, unsigned i) { return hwloc_bitmap_to_ith_ulong(set, i); }
void hwloc_cpuset_cpu(struct hwloc_bitmap_s * set, unsigned cpu) { hwloc_bitmap_only(set, cpu); }
void hwloc_cpuset_all_but_cpu(struct hwloc_bitmap_s * set, unsigned cpu) { hwloc_bitmap_allbut(set, cpu); }
void hwloc_cpuset_set(struct hwloc_bitmap_s * set, unsigned cpu) { hwloc_bitmap_set(set, cpu); }
void hwloc_cpuset_set_range(struct hwloc_bitmap_s * set, unsigned begincpu, unsigned endcpu) { hwloc_bitmap_set_range(set, begincpu, endcpu); }
void hwloc_cpuset_set_ith_ulong(struct hwloc_bitmap_s *set, unsigned i, unsigned long mask) { hwloc_bitmap_set_ith_ulong(set, i, mask); }
void hwloc_cpuset_clr(struct hwloc_bitmap_s * set, unsigned cpu) { hwloc_bitmap_clr(set, cpu); }
void hwloc_cpuset_clr_range(struct hwloc_bitmap_s * set, unsigned begincpu, unsigned endcpu) { hwloc_bitmap_clr_range(set, begincpu, endcpu); }
int hwloc_cpuset_isset(const struct hwloc_bitmap_s * set, unsigned cpu) { return hwloc_bitmap_isset(set, cpu); }
int hwloc_cpuset_iszero(const struct hwloc_bitmap_s *set) { return hwloc_bitmap_iszero(set); }
int hwloc_cpuset_isfull(const struct hwloc_bitmap_s *set) { return hwloc_bitmap_isfull(set); }
int hwloc_cpuset_isequal(const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { return hwloc_bitmap_isequal(set1, set2); }
int hwloc_cpuset_intersects(const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { return hwloc_bitmap_intersects(set1, set2); }
int hwloc_cpuset_isincluded(const struct hwloc_bitmap_s *sub_set, const struct hwloc_bitmap_s *super_set) { return hwloc_bitmap_isincluded(sub_set, super_set); }
void hwloc_cpuset_or(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { hwloc_bitmap_or(res, set1, set2); }
void hwloc_cpuset_and(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { hwloc_bitmap_and(res, set1, set2); }
void hwloc_cpuset_andnot(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { hwloc_bitmap_andnot(res, set1, set2); }
void hwloc_cpuset_xor(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set1, const struct hwloc_bitmap_s *set2) { hwloc_bitmap_xor(res, set1, set2); }
void hwloc_cpuset_not(struct hwloc_bitmap_s *res, const struct hwloc_bitmap_s *set) { hwloc_bitmap_not(res, set); }
int hwloc_cpuset_first(const struct hwloc_bitmap_s * set) { return hwloc_bitmap_first(set); }
int hwloc_cpuset_last(const struct hwloc_bitmap_s * set) { return hwloc_bitmap_last(set); }
int hwloc_cpuset_next(const struct hwloc_bitmap_s * set, unsigned prev_cpu) { return hwloc_bitmap_next(set, prev_cpu); }
void hwloc_cpuset_singlify(struct hwloc_bitmap_s * set) { hwloc_bitmap_singlify(set); }
int hwloc_cpuset_compare_first(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2) { return hwloc_bitmap_compare_first(set1, set2); }
int hwloc_cpuset_compare(const struct hwloc_bitmap_s * set1, const struct hwloc_bitmap_s * set2) { return hwloc_bitmap_compare(set1, set2); }
int hwloc_cpuset_weight(const struct hwloc_bitmap_s * set) { return hwloc_bitmap_weight(set); }

/*
 * end of everything to be dropped when hwloc/cpuset.h is dropped
 *****************************************************************/

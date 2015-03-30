/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util.c
 * \brief Common functions for strings, IO, network, data structures,
 * process control.
 **/

/* This is required on rh7 to make strptime not complain.
 */
#define _GNU_SOURCE

#include "orconfig.h"
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#define UTIL_PRIVATE
#include "util.h"
#include "torlog.h"
#include "crypto.h"
#include "torint.h"
#include "container.h"
#include "address.h"
#include "sandbox.h"
#include "backtrace.h"
#include "util_process.h"

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <process.h>
#include <tchar.h>
#include <winbase.h>
#else
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#endif

/* math.h needs this on Linux */
#ifndef _USE_ISOC99_
#define _USE_ISOC99_ 1
#endif
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_MALLOC_MALLOC_H
#include <malloc/malloc.h>
#endif
#ifdef HAVE_MALLOC_H
#if !defined(OPENBSD) && !defined(__FreeBSD__)
/* OpenBSD has a malloc.h, but for our purposes, it only exists in order to
 * scold us for being so stupid as to autodetect its presence.  To be fair,
 * they've done this since 1996, when autoconf was only 5 years old. */
#include <malloc.h>
#endif
#endif
#ifdef HAVE_MALLOC_NP_H
#include <malloc_np.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/* =====
 * Assertion helper.
 * ===== */
/** Helper for tor_assert: report the assertion failure. */
void
tor_assertion_failed_(const char *fname, unsigned int line,
                      const char *func, const char *expr)
{
  char buf[256];
  log_err(LD_BUG, "%s:%u: %s: Assertion %s failed; aborting.",
          fname, line, func, expr);
  tor_snprintf(buf, sizeof(buf),
               "Assertion %s failed in %s at %s:%u",
               expr, func, fname, line);
  log_backtrace(LOG_ERR, LD_BUG, buf);
}

/* =====
 * Memory management
 * ===== */
#ifdef USE_DMALLOC
 #undef strndup
 #include <dmalloc.h>
 /* Macro to pass the extra dmalloc args to another function. */
 #define DMALLOC_FN_ARGS , file, line

 #if defined(HAVE_DMALLOC_STRDUP)
 /* the dmalloc_strdup should be fine as defined */
 #elif defined(HAVE_DMALLOC_STRNDUP)
 #define dmalloc_strdup(file, line, string, xalloc_b) \
         dmalloc_strndup(file, line, (string), -1, xalloc_b)
 #else
 #error "No dmalloc_strdup or equivalent"
 #endif

#else /* not using dmalloc */

 #define DMALLOC_FN_ARGS
#endif

/** Allocate a chunk of <b>size</b> bytes of memory, and return a pointer to
 * result.  On error, log and terminate the process.  (Same as malloc(size),
 * but never returns NULL.)
 *
 * <b>file</b> and <b>line</b> are used if dmalloc is enabled, and
 * ignored otherwise.
 */
void *
tor_malloc_(size_t size DMALLOC_PARAMS)
{
  void *result;

  tor_assert(size < SIZE_T_CEILING);

#ifndef MALLOC_ZERO_WORKS
  /* Some libc mallocs don't work when size==0. Override them. */
  if (size==0) {
    size=1;
  }
#endif

#ifdef USE_DMALLOC
  result = dmalloc_malloc(file, line, size, DMALLOC_FUNC_MALLOC, 0, 0);
#else
  result = malloc(size);
#endif

  if (PREDICT_UNLIKELY(result == NULL)) {
    log_err(LD_MM,"Out of memory on malloc(). Dying.");
    /* If these functions die within a worker process, they won't call
     * spawn_exit, but that's ok, since the parent will run out of memory soon
     * anyway. */
    exit(1);
  }
  return result;
}

/** Allocate a chunk of <b>size</b> bytes of memory, fill the memory with
 * zero bytes, and return a pointer to the result.  Log and terminate
 * the process on error.  (Same as calloc(size,1), but never returns NULL.)
 */
void *
tor_malloc_zero_(size_t size DMALLOC_PARAMS)
{
  /* You may ask yourself, "wouldn't it be smart to use calloc instead of
   * malloc+memset?  Perhaps libc's calloc knows some nifty optimization trick
   * we don't!"  Indeed it does, but its optimizations are only a big win when
   * we're allocating something very big (it knows if it just got the memory
   * from the OS in a pre-zeroed state).  We don't want to use tor_malloc_zero
   * for big stuff, so we don't bother with calloc. */
  void *result = tor_malloc_(size DMALLOC_FN_ARGS);
  memset(result, 0, size);
  return result;
}

/** Allocate a chunk of <b>nmemb</b>*<b>size</b> bytes of memory, fill
 * the memory with zero bytes, and return a pointer to the result.
 * Log and terminate the process on error.  (Same as
 * calloc(<b>nmemb</b>,<b>size</b>), but never returns NULL.)
 *
 * XXXX This implementation probably asserts in cases where it could
 * work, because it only tries dividing SIZE_MAX by size (according to
 * the calloc(3) man page, the size of an element of the nmemb-element
 * array to be allocated), not by nmemb (which could in theory be
 * smaller than size).  Don't do that then.
 */
void *
tor_calloc_(size_t nmemb, size_t size DMALLOC_PARAMS)
{
  /* You may ask yourself, "wouldn't it be smart to use calloc instead of
   * malloc+memset?  Perhaps libc's calloc knows some nifty optimization trick
   * we don't!"  Indeed it does, but its optimizations are only a big win when
   * we're allocating something very big (it knows if it just got the memory
   * from the OS in a pre-zeroed state).  We don't want to use tor_malloc_zero
   * for big stuff, so we don't bother with calloc. */
  void *result;
  size_t max_nmemb = (size == 0) ? SIZE_MAX : SIZE_MAX/size;

  tor_assert(nmemb < max_nmemb);

  result = tor_malloc_zero_((nmemb * size) DMALLOC_FN_ARGS);
  return result;
}

/** Change the size of the memory block pointed to by <b>ptr</b> to <b>size</b>
 * bytes long; return the new memory block.  On error, log and
 * terminate. (Like realloc(ptr,size), but never returns NULL.)
 */
void *
tor_realloc_(void *ptr, size_t size DMALLOC_PARAMS)
{
  void *result;

  tor_assert(size < SIZE_T_CEILING);

#ifdef USE_DMALLOC
  result = dmalloc_realloc(file, line, ptr, size, DMALLOC_FUNC_REALLOC, 0);
#else
  result = realloc(ptr, size);
#endif

  if (PREDICT_UNLIKELY(result == NULL)) {
    log_err(LD_MM,"Out of memory on realloc(). Dying.");
    exit(1);
  }
  return result;
}

/** Return a newly allocated copy of the NUL-terminated string s. On
 * error, log and terminate.  (Like strdup(s), but never returns
 * NULL.)
 */
char *
tor_strdup_(const char *s DMALLOC_PARAMS)
{
  char *dup;
  tor_assert(s);

#ifdef USE_DMALLOC
  dup = dmalloc_strdup(file, line, s, 0);
#else
  dup = strdup(s);
#endif
  if (PREDICT_UNLIKELY(dup == NULL)) {
    log_err(LD_MM,"Out of memory on strdup(). Dying.");
    exit(1);
  }
  return dup;
}

/** Allocate and return a new string containing the first <b>n</b>
 * characters of <b>s</b>.  If <b>s</b> is longer than <b>n</b>
 * characters, only the first <b>n</b> are copied.  The result is
 * always NUL-terminated.  (Like strndup(s,n), but never returns
 * NULL.)
 */
char *
tor_strndup_(const char *s, size_t n DMALLOC_PARAMS)
{
  char *dup;
  tor_assert(s);
  tor_assert(n < SIZE_T_CEILING);
  dup = tor_malloc_((n+1) DMALLOC_FN_ARGS);
  /* Performance note: Ordinarily we prefer strlcpy to strncpy.  But
   * this function gets called a whole lot, and platform strncpy is
   * much faster than strlcpy when strlen(s) is much longer than n.
   */
  strncpy(dup, s, n);
  dup[n]='\0';
  return dup;
}

/** Allocate a chunk of <b>len</b> bytes, with the same contents as the
 * <b>len</b> bytes starting at <b>mem</b>. */
void *
tor_memdup_(const void *mem, size_t len DMALLOC_PARAMS)
{
  char *dup;
  tor_assert(len < SIZE_T_CEILING);
  tor_assert(mem);
  dup = tor_malloc_(len DMALLOC_FN_ARGS);
  memcpy(dup, mem, len);
  return dup;
}

/** As tor_memdup(), but add an extra 0 byte at the end of the resulting
 * memory. */
void *
tor_memdup_nulterm_(const void *mem, size_t len DMALLOC_PARAMS)
{
  char *dup;
  tor_assert(len < SIZE_T_CEILING+1);
  tor_assert(mem);
  dup = tor_malloc_(len+1 DMALLOC_FN_ARGS);
  memcpy(dup, mem, len);
  dup[len] = '\0';
  return dup;
}

/** Helper for places that need to take a function pointer to the right
 * spelling of "free()". */
void
tor_free_(void *mem)
{
  tor_free(mem);
}

/** Call the platform malloc info function, and dump the results to the log at
 * level <b>severity</b>.  If no such function exists, do nothing. */
void
tor_log_mallinfo(int severity)
{
#ifdef HAVE_MALLINFO
  struct mallinfo mi;
  memset(&mi, 0, sizeof(mi));
  mi = mallinfo();
  tor_log(severity, LD_MM,
      "mallinfo() said: arena=%d, ordblks=%d, smblks=%d, hblks=%d, "
      "hblkhd=%d, usmblks=%d, fsmblks=%d, uordblks=%d, fordblks=%d, "
      "keepcost=%d",
      mi.arena, mi.ordblks, mi.smblks, mi.hblks,
      mi.hblkhd, mi.usmblks, mi.fsmblks, mi.uordblks, mi.fordblks,
      mi.keepcost);
#else
  (void)severity;
#endif
#ifdef USE_DMALLOC
  dmalloc_log_changed(0, /* Since the program started. */
                      1, /* Log info about non-freed pointers. */
                      0, /* Do not log info about freed pointers. */
                      0  /* Do not log individual pointers. */
                      );
#endif
}

/* =====
 * Math
 * ===== */

/**
 * Returns the natural logarithm of d base e.  We defined this wrapper here so
 * to avoid conflicts with old versions of tor_log(), which were named log().
 */
double
tor_mathlog(double d)
{
  return log(d);
}

/** Return the long integer closest to <b>d</b>. We define this wrapper
 * here so that not all users of math.h need to use the right incantations
 * to get the c99 functions. */
long
tor_lround(double d)
{
#if defined(HAVE_LROUND)
  return lround(d);
#elif defined(HAVE_RINT)
  return (long)rint(d);
#else
  return (long)(d > 0 ? d + 0.5 : ceil(d - 0.5));
#endif
}

/** Return the 64-bit integer closest to d.  We define this wrapper here so
 * that not all users of math.h need to use the right incantations to get the
 * c99 functions. */
int64_t
tor_llround(double d)
{
#if defined(HAVE_LLROUND)
  return (int64_t)llround(d);
#elif defined(HAVE_RINT)
  return (int64_t)rint(d);
#else
  return (int64_t)(d > 0 ? d + 0.5 : ceil(d - 0.5));
#endif
}

/** Returns floor(log2(u64)).  If u64 is 0, (incorrectly) returns 0. */
int
tor_log2(uint64_t u64)
{
  int r = 0;
  if (u64 >= (U64_LITERAL(1)<<32)) {
    u64 >>= 32;
    r = 32;
  }
  if (u64 >= (U64_LITERAL(1)<<16)) {
    u64 >>= 16;
    r += 16;
  }
  if (u64 >= (U64_LITERAL(1)<<8)) {
    u64 >>= 8;
    r += 8;
  }
  if (u64 >= (U64_LITERAL(1)<<4)) {
    u64 >>= 4;
    r += 4;
  }
  if (u64 >= (U64_LITERAL(1)<<2)) {
    u64 >>= 2;
    r += 2;
  }
  if (u64 >= (U64_LITERAL(1)<<1)) {
    u64 >>= 1;
    r += 1;
  }
  return r;
}

/** Return the power of 2 in range [1,UINT64_MAX] closest to <b>u64</b>.  If
 * there are two powers of 2 equally close, round down. */
uint64_t
round_to_power_of_2(uint64_t u64)
{
  int lg2;
  uint64_t low;
  uint64_t high;
  if (u64 == 0)
    return 1;

  lg2 = tor_log2(u64);
  low = U64_LITERAL(1) << lg2;

  if (lg2 == 63)
    return low;

  high = U64_LITERAL(1) << (lg2+1);
  if (high - u64 < u64 - low)
    return high;
  else
    return low;
}

/** Return the lowest x such that x is at least <b>number</b>, and x modulo
 * <b>divisor</b> == 0. */
unsigned
round_to_next_multiple_of(unsigned number, unsigned divisor)
{
  number += divisor - 1;
  number -= number % divisor;
  return number;
}

/** Return the lowest x such that x is at least <b>number</b>, and x modulo
 * <b>divisor</b> == 0. */
uint32_t
round_uint32_to_next_multiple_of(uint32_t number, uint32_t divisor)
{
  number += divisor - 1;
  number -= number % divisor;
  return number;
}

/** Return the lowest x such that x is at least <b>number</b>, and x modulo
 * <b>divisor</b> == 0. */
uint64_t
round_uint64_to_next_multiple_of(uint64_t number, uint64_t divisor)
{
  number += divisor - 1;
  number -= number % divisor;
  return number;
}

/** Return the number of bits set in <b>v</b>. */
int
n_bits_set_u8(uint8_t v)
{
  static const int nybble_table[] = {
    0, /* 0000 */
    1, /* 0001 */
    1, /* 0010 */
    2, /* 0011 */
    1, /* 0100 */
    2, /* 0101 */
    2, /* 0110 */
    3, /* 0111 */
    1, /* 1000 */
    2, /* 1001 */
    2, /* 1010 */
    3, /* 1011 */
    2, /* 1100 */
    3, /* 1101 */
    3, /* 1110 */
    4, /* 1111 */
  };

  return nybble_table[v & 15] + nybble_table[v>>4];
}

/* =====
 * String manipulation
 * ===== */

/** Remove from the string <b>s</b> every character which appears in
 * <b>strip</b>. */
void
tor_strstrip(char *s, const char *strip)
{
  char *read = s;
  while (*read) {
    if (strchr(strip, *read)) {
      ++read;
    } else {
      *s++ = *read++;
    }
  }
  *s = '\0';
}

/** Return a pointer to a NUL-terminated hexadecimal string encoding
 * the first <b>fromlen</b> bytes of <b>from</b>. (fromlen must be \<= 32.) The
 * result does not need to be deallocated, but repeated calls to
 * hex_str will trash old results.
 */
const char *
hex_str(const char *from, size_t fromlen)
{
  static char buf[65];
  if (fromlen>(sizeof(buf)-1)/2)
    fromlen = (sizeof(buf)-1)/2;
  base16_encode(buf,sizeof(buf),from,fromlen);
  return buf;
}

/** Convert all alphabetic characters in the nul-terminated string <b>s</b> to
 * lowercase. */
void
tor_strlower(char *s)
{
  while (*s) {
    *s = TOR_TOLOWER(*s);
    ++s;
  }
}

/** Convert all alphabetic characters in the nul-terminated string <b>s</b> to
 * lowercase. */
void
tor_strupper(char *s)
{
  while (*s) {
    *s = TOR_TOUPPER(*s);
    ++s;
  }
}

/** Return 1 if every character in <b>s</b> is printable, else return 0.
 */
int
tor_strisprint(const char *s)
{
  while (*s) {
    if (!TOR_ISPRINT(*s))
      return 0;
    s++;
  }
  return 1;
}

/** Return 1 if no character in <b>s</b> is uppercase, else return 0.
 */
int
tor_strisnonupper(const char *s)
{
  while (*s) {
    if (TOR_ISUPPER(*s))
      return 0;
    s++;
  }
  return 1;
}

/** As strcmp, except that either string may be NULL.  The NULL string is
 * considered to be before any non-NULL string. */
int
strcmp_opt(const char *s1, const char *s2)
{
  if (!s1) {
    if (!s2)
      return 0;
    else
      return -1;
  } else if (!s2) {
    return 1;
  } else {
    return strcmp(s1, s2);
  }
}

/** Compares the first strlen(s2) characters of s1 with s2.  Returns as for
 * strcmp.
 */
int
strcmpstart(const char *s1, const char *s2)
{
  size_t n = strlen(s2);
  return strncmp(s1, s2, n);
}

/** Compare the s1_len-byte string <b>s1</b> with <b>s2</b>,
 * without depending on a terminating nul in s1.  Sorting order is first by
 * length, then lexically; return values are as for strcmp.
 */
int
strcmp_len(const char *s1, const char *s2, size_t s1_len)
{
  size_t s2_len = strlen(s2);
  if (s1_len < s2_len)
    return -1;
  if (s1_len > s2_len)
    return 1;
  return fast_memcmp(s1, s2, s2_len);
}

/** Compares the first strlen(s2) characters of s1 with s2.  Returns as for
 * strcasecmp.
 */
int
strcasecmpstart(const char *s1, const char *s2)
{
  size_t n = strlen(s2);
  return strncasecmp(s1, s2, n);
}

/** Compares the last strlen(s2) characters of s1 with s2.  Returns as for
 * strcmp.
 */
int
strcmpend(const char *s1, const char *s2)
{
  size_t n1 = strlen(s1), n2 = strlen(s2);
  if (n2>n1)
    return strcmp(s1,s2);
  else
    return strncmp(s1+(n1-n2), s2, n2);
}

/** Compares the last strlen(s2) characters of s1 with s2.  Returns as for
 * strcasecmp.
 */
int
strcasecmpend(const char *s1, const char *s2)
{
  size_t n1 = strlen(s1), n2 = strlen(s2);
  if (n2>n1) /* then they can't be the same; figure out which is bigger */
    return strcasecmp(s1,s2);
  else
    return strncasecmp(s1+(n1-n2), s2, n2);
}

/** Compare the value of the string <b>prefix</b> with the start of the
 * <b>memlen</b>-byte memory chunk at <b>mem</b>.  Return as for strcmp.
 *
 * [As fast_memcmp(mem, prefix, strlen(prefix)) but returns -1 if memlen is
 * less than strlen(prefix).]
 */
int
fast_memcmpstart(const void *mem, size_t memlen,
                const char *prefix)
{
  size_t plen = strlen(prefix);
  if (memlen < plen)
    return -1;
  return fast_memcmp(mem, prefix, plen);
}

/** Given a nul-terminated string s, set every character before the nul
 * to zero. */
void
tor_strclear(char *s)
{
  while (*s) {
    *s++ = '\0';
  }
}

/** Return a pointer to the first char of s that is not whitespace and
 * not a comment, or to the terminating NUL if no such character exists.
 */
const char *
eat_whitespace(const char *s)
{
  tor_assert(s);

  while (1) {
    switch (*s) {
    case '\0':
    default:
      return s;
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      ++s;
      break;
    case '#':
      ++s;
      while (*s && *s != '\n')
        ++s;
    }
  }
}

/** Return a pointer to the first char of s that is not whitespace and
 * not a comment, or to the terminating NUL if no such character exists.
 */
const char *
eat_whitespace_eos(const char *s, const char *eos)
{
  tor_assert(s);
  tor_assert(eos && s <= eos);

  while (s < eos) {
    switch (*s) {
    case '\0':
    default:
      return s;
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      ++s;
      break;
    case '#':
      ++s;
      while (s < eos && *s && *s != '\n')
        ++s;
    }
  }
  return s;
}

/** Return a pointer to the first char of s that is not a space or a tab
 * or a \\r, or to the terminating NUL if no such character exists. */
const char *
eat_whitespace_no_nl(const char *s)
{
  while (*s == ' ' || *s == '\t' || *s == '\r')
    ++s;
  return s;
}

/** As eat_whitespace_no_nl, but stop at <b>eos</b> whether we have
 * found a non-whitespace character or not. */
const char *
eat_whitespace_eos_no_nl(const char *s, const char *eos)
{
  while (s < eos && (*s == ' ' || *s == '\t' || *s == '\r'))
    ++s;
  return s;
}

/** Return a pointer to the first char of s that is whitespace or <b>#</b>,
 * or to the terminating NUL if no such character exists.
 */
const char *
find_whitespace(const char *s)
{
  /* tor_assert(s); */
  while (1) {
    switch (*s)
    {
    case '\0':
    case '#':
    case ' ':
    case '\r':
    case '\n':
    case '\t':
      return s;
    default:
      ++s;
    }
  }
}

/** As find_whitespace, but stop at <b>eos</b> whether we have found a
 * whitespace or not. */
const char *
find_whitespace_eos(const char *s, const char *eos)
{
  /* tor_assert(s); */
  while (s < eos) {
    switch (*s)
    {
    case '\0':
    case '#':
    case ' ':
    case '\r':
    case '\n':
    case '\t':
      return s;
    default:
      ++s;
    }
  }
  return s;
}

/** Return the first occurrence of <b>needle</b> in <b>haystack</b> that
 * occurs at the start of a line (that is, at the beginning of <b>haystack</b>
 * or immediately after a newline).  Return NULL if no such string is found.
 */
const char *
find_str_at_start_of_line(const char *haystack, const char *needle)
{
  size_t needle_len = strlen(needle);

  do {
    if (!strncmp(haystack, needle, needle_len))
      return haystack;

    haystack = strchr(haystack, '\n');
    if (!haystack)
      return NULL;
    else
      ++haystack;
  } while (*haystack);

  return NULL;
}

/** Returns true if <b>string</b> could be a C identifier.
    A C identifier must begin with a letter or an underscore and the
    rest of its characters can be letters, numbers or underscores. No
    length limit is imposed. */
int
string_is_C_identifier(const char *string)
{
  size_t iter;
  size_t length = strlen(string);
  if (!length)
    return 0;

  for (iter = 0; iter < length ; iter++) {
    if (iter == 0) {
      if (!(TOR_ISALPHA(string[iter]) ||
            string[iter] == '_'))
        return 0;
    } else {
      if (!(TOR_ISALPHA(string[iter]) ||
            TOR_ISDIGIT(string[iter]) ||
            string[iter] == '_'))
        return 0;
    }
  }

  return 1;
}

/** Return true iff the 'len' bytes at 'mem' are all zero. */
int
tor_mem_is_zero(const char *mem, size_t len)
{
  static const char ZERO[] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  };
  while (len >= sizeof(ZERO)) {
    /* It's safe to use fast_memcmp here, since the very worst thing an
     * attacker could learn is how many initial bytes of a secret were zero */
    if (fast_memcmp(mem, ZERO, sizeof(ZERO)))
      return 0;
    len -= sizeof(ZERO);
    mem += sizeof(ZERO);
  }
  /* Deal with leftover bytes. */
  if (len)
    return fast_memeq(mem, ZERO, len);

  return 1;
}

/** Return true iff the DIGEST_LEN bytes in digest are all zero. */
int
tor_digest_is_zero(const char *digest)
{
  static const uint8_t ZERO_DIGEST[] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
  };
  return tor_memeq(digest, ZERO_DIGEST, DIGEST_LEN);
}

/** Return true if <b>string</b> is a valid 'key=[value]' string.
 *  "value" is optional, to indicate the empty string. Log at logging
 *  <b>severity</b> if something ugly happens. */
int
string_is_key_value(int severity, const char *string)
{
  /* position of equal sign in string */
  const char *equal_sign_pos = NULL;

  tor_assert(string);

  if (strlen(string) < 2) { /* "x=" is shortest args string */
    tor_log(severity, LD_GENERAL, "'%s' is too short to be a k=v value.",
            escaped(string));
    return 0;
  }

  equal_sign_pos = strchr(string, '=');
  if (!equal_sign_pos) {
    tor_log(severity, LD_GENERAL, "'%s' is not a k=v value.", escaped(string));
    return 0;
  }

  /* validate that the '=' is not in the beginning of the string. */
  if (equal_sign_pos == string) {
    tor_log(severity, LD_GENERAL, "'%s' is not a valid k=v value.",
            escaped(string));
    return 0;
  }

  return 1;
}

/** Return true iff the DIGEST256_LEN bytes in digest are all zero. */
int
tor_digest256_is_zero(const char *digest)
{
  return tor_mem_is_zero(digest, DIGEST256_LEN);
}

/* Helper: common code to check whether the result of a strtol or strtoul or
 * strtoll is correct. */
#define CHECK_STRTOX_RESULT()                           \
  /* Did an overflow occur? */                          \
  if (errno == ERANGE)                                  \
    goto err;                                           \
  /* Was at least one character converted? */           \
  if (endptr == s)                                      \
    goto err;                                           \
  /* Were there unexpected unconverted characters? */   \
  if (!next && *endptr)                                 \
    goto err;                                           \
  /* Is r within limits? */                             \
  if (r < min || r > max)                               \
    goto err;                                           \
  if (ok) *ok = 1;                                      \
  if (next) *next = endptr;                             \
  return r;                                             \
 err:                                                   \
  if (ok) *ok = 0;                                      \
  if (next) *next = endptr;                             \
  return 0

/** Extract a long from the start of <b>s</b>, in the given numeric
 * <b>base</b>.  If <b>base</b> is 0, <b>s</b> is parsed as a decimal,
 * octal, or hex number in the syntax of a C integer literal.  If
 * there is unconverted data and <b>next</b> is provided, set
 * *<b>next</b> to the first unconverted character.  An error has
 * occurred if no characters are converted; or if there are
 * unconverted characters and <b>next</b> is NULL; or if the parsed
 * value is not between <b>min</b> and <b>max</b>.  When no error
 * occurs, return the parsed value and set *<b>ok</b> (if provided) to
 * 1.  When an error occurs, return 0 and set *<b>ok</b> (if provided)
 * to 0.
 */
long
tor_parse_long(const char *s, int base, long min, long max,
               int *ok, char **next)
{
  char *endptr;
  long r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return 0;
  }

  errno = 0;
  r = strtol(s, &endptr, base);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long(), but return an unsigned long. */
unsigned long
tor_parse_ulong(const char *s, int base, unsigned long min,
                unsigned long max, int *ok, char **next)
{
  char *endptr;
  unsigned long r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return 0;
  }

  errno = 0;
  r = strtoul(s, &endptr, base);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long(), but return a double. */
double
tor_parse_double(const char *s, double min, double max, int *ok, char **next)
{
  char *endptr;
  double r;

  errno = 0;
  r = strtod(s, &endptr);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long, but return a uint64_t.  Only base 10 is guaranteed to
 * work for now. */
uint64_t
tor_parse_uint64(const char *s, int base, uint64_t min,
                 uint64_t max, int *ok, char **next)
{
  char *endptr;
  uint64_t r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return 0;
  }

  errno = 0;
#ifdef HAVE_STRTOULL
  r = (uint64_t)strtoull(s, &endptr, base);
#elif defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER < 1300
  tor_assert(base <= 10);
  r = (uint64_t)_atoi64(s);
  endptr = (char*)s;
  while (TOR_ISSPACE(*endptr)) endptr++;
  while (TOR_ISDIGIT(*endptr)) endptr++;
#else
  r = (uint64_t)_strtoui64(s, &endptr, base);
#endif
#elif SIZEOF_LONG == 8
  r = (uint64_t)strtoul(s, &endptr, base);
#else
#error "I don't know how to parse 64-bit numbers."
#endif

  CHECK_STRTOX_RESULT();
}

/** Encode the <b>srclen</b> bytes at <b>src</b> in a NUL-terminated,
 * uppercase hexadecimal string; store it in the <b>destlen</b>-byte buffer
 * <b>dest</b>.
 */
void
base16_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *end;
  char *cp;

  tor_assert(destlen >= srclen*2+1);
  tor_assert(destlen < SIZE_T_CEILING);

  cp = dest;
  end = src+srclen;
  while (src<end) {
    *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) >> 4 ];
    *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) & 0xf ];
    ++src;
  }
  *cp = '\0';
}

/** Helper: given a hex digit, return its value, or -1 if it isn't hex. */
static INLINE int
hex_decode_digit_(char c)
{
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'A': case 'a': return 10;
    case 'B': case 'b': return 11;
    case 'C': case 'c': return 12;
    case 'D': case 'd': return 13;
    case 'E': case 'e': return 14;
    case 'F': case 'f': return 15;
    default:
      return -1;
  }
}

/** Helper: given a hex digit, return its value, or -1 if it isn't hex. */
int
hex_decode_digit(char c)
{
  return hex_decode_digit_(c);
}

/** Given a hexadecimal string of <b>srclen</b> bytes in <b>src</b>, decode it
 * and store the result in the <b>destlen</b>-byte buffer at <b>dest</b>.
 * Return 0 on success, -1 on failure. */
int
base16_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *end;

  int v1,v2;
  if ((srclen % 2) != 0)
    return -1;
  if (destlen < srclen/2 || destlen > SIZE_T_CEILING)
    return -1;

  memset(dest, 0, destlen);

  end = src+srclen;
  while (src<end) {
    v1 = hex_decode_digit_(*src);
    v2 = hex_decode_digit_(*(src+1));
    if (v1<0||v2<0)
      return -1;
    *(uint8_t*)dest = (v1<<4)|v2;
    ++dest;
    src+=2;
  }
  return 0;
}

/** Allocate and return a new string representing the contents of <b>s</b>,
 * surrounded by quotes and using standard C escapes.
 *
 * Generally, we use this for logging values that come in over the network to
 * keep them from tricking users, and for sending certain values to the
 * controller.
 *
 * We trust values from the resolver, OS, configuration file, and command line
 * to not be maliciously ill-formed.  We validate incoming routerdescs and
 * SOCKS requests and addresses from BEGIN cells as they're parsed;
 * afterwards, we trust them as non-malicious.
 */
char *
esc_for_log(const char *s)
{
  const char *cp;
  char *result, *outp;
  size_t len = 3;
  if (!s) {
    return tor_strdup("(null)");
  }

  for (cp = s; *cp; ++cp) {
    switch (*cp) {
      case '\\':
      case '\"':
      case '\'':
      case '\r':
      case '\n':
      case '\t':
        len += 2;
        break;
      default:
        if (TOR_ISPRINT(*cp) && ((uint8_t)*cp)<127)
          ++len;
        else
          len += 4;
        break;
    }
  }

  result = outp = tor_malloc(len);
  *outp++ = '\"';
  for (cp = s; *cp; ++cp) {
    switch (*cp) {
      case '\\':
      case '\"':
      case '\'':
        *outp++ = '\\';
        *outp++ = *cp;
        break;
      case '\n':
        *outp++ = '\\';
        *outp++ = 'n';
        break;
      case '\t':
        *outp++ = '\\';
        *outp++ = 't';
        break;
      case '\r':
        *outp++ = '\\';
        *outp++ = 'r';
        break;
      default:
        if (TOR_ISPRINT(*cp) && ((uint8_t)*cp)<127) {
          *outp++ = *cp;
        } else {
          tor_snprintf(outp, 5, "\\%03o", (int)(uint8_t) *cp);
          outp += 4;
        }
        break;
    }
  }

  *outp++ = '\"';
  *outp++ = 0;

  return result;
}

/** Allocate and return a new string representing the contents of <b>s</b>,
 * surrounded by quotes and using standard C escapes.
 *
 * THIS FUNCTION IS NOT REENTRANT.  Don't call it from outside the main
 * thread.  Also, each call invalidates the last-returned value, so don't
 * try log_warn(LD_GENERAL, "%s %s", escaped(a), escaped(b));
 */
const char *
escaped(const char *s)
{
  static char *escaped_val_ = NULL;
  tor_free(escaped_val_);

  if (s)
    escaped_val_ = esc_for_log(s);
  else
    escaped_val_ = NULL;

  return escaped_val_;
}

/** Return a newly allocated string equal to <b>string</b>, except that every
 * character in <b>chars_to_escape</b> is preceded by a backslash. */
char *
tor_escape_str_for_pt_args(const char *string, const char *chars_to_escape)
{
  char *new_string = NULL;
  char *new_cp = NULL;
  size_t length, new_length;

  tor_assert(string);

  length = strlen(string);

  if (!length) /* If we were given the empty string, return the same. */
    return tor_strdup("");
  /* (new_length > SIZE_MAX) => ((length * 2) + 1 > SIZE_MAX) =>
     (length*2 > SIZE_MAX - 1) => (length > (SIZE_MAX - 1)/2) */
  if (length > (SIZE_MAX - 1)/2) /* check for overflow */
    return NULL;

  /* this should be enough even if all characters must be escaped */
  new_length = (length * 2) + 1;

  new_string = new_cp = tor_malloc(new_length);

  while (*string) {
    if (strchr(chars_to_escape, *string))
      *new_cp++ = '\\';

    *new_cp++ = *string++;
  }

  *new_cp = '\0'; /* NUL-terminate the new string */

  return new_string;
}

/* =====
 * Time
 * ===== */

/** Return the number of microseconds elapsed between *start and *end.
 */
long
tv_udiff(const struct timeval *start, const struct timeval *end)
{
  long udiff;
  long secdiff = end->tv_sec - start->tv_sec;

  if (labs(secdiff+1) > LONG_MAX/1000000) {
    log_warn(LD_GENERAL, "comparing times on microsecond detail too far "
             "apart: %ld seconds", secdiff);
    return LONG_MAX;
  }

  udiff = secdiff*1000000L + (end->tv_usec - start->tv_usec);
  return udiff;
}

/** Return the number of milliseconds elapsed between *start and *end.
 */
long
tv_mdiff(const struct timeval *start, const struct timeval *end)
{
  long mdiff;
  long secdiff = end->tv_sec - start->tv_sec;

  if (labs(secdiff+1) > LONG_MAX/1000) {
    log_warn(LD_GENERAL, "comparing times on millisecond detail too far "
             "apart: %ld seconds", secdiff);
    return LONG_MAX;
  }

  /* Subtract and round */
  mdiff = secdiff*1000L +
      ((long)end->tv_usec - (long)start->tv_usec + 500L) / 1000L;
  return mdiff;
}

/**
 * Converts timeval to milliseconds.
 */
int64_t
tv_to_msec(const struct timeval *tv)
{
  int64_t conv = ((int64_t)tv->tv_sec)*1000L;
  /* Round ghetto-style */
  conv += ((int64_t)tv->tv_usec+500)/1000L;
  return conv;
}

/** Yield true iff <b>y</b> is a leap-year. */
#define IS_LEAPYEAR(y) (!(y % 4) && ((y % 100) || !(y % 400)))
/** Helper: Return the number of leap-days between Jan 1, y1 and Jan 1, y2. */
static int
n_leapdays(int y1, int y2)
{
  --y1;
  --y2;
  return (y2/4 - y1/4) - (y2/100 - y1/100) + (y2/400 - y1/400);
}
/** Number of days per month in non-leap year; used by tor_timegm. */
static const int days_per_month[] =
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/** Compute a time_t given a struct tm.  The result is given in UTC, and
 * does not account for leap seconds.  Return 0 on success, -1 on failure.
 */
int
tor_timegm(const struct tm *tm, time_t *time_out)
{
  /* This is a pretty ironclad timegm implementation, snarfed from Python2.2.
   * It's way more brute-force than fiddling with tzset().
   */
  time_t year, days, hours, minutes, seconds;
  int i;
  year = tm->tm_year + 1900;
  if (year < 1970 || tm->tm_mon < 0 || tm->tm_mon > 11 ||
      tm->tm_year >= INT32_MAX-1900) {
    log_warn(LD_BUG, "Out-of-range argument to tor_timegm");
    return -1;
  }
  days = 365 * (year-1970) + n_leapdays(1970,(int)year);
  for (i = 0; i < tm->tm_mon; ++i)
    days += days_per_month[i];
  if (tm->tm_mon > 1 && IS_LEAPYEAR(year))
    ++days;
  days += tm->tm_mday - 1;
  hours = days*24 + tm->tm_hour;

  minutes = hours*60 + tm->tm_min;
  seconds = minutes*60 + tm->tm_sec;
  *time_out = seconds;
  return 0;
}

/* strftime is locale-specific, so we need to replace those parts */

/** A c-locale array of 3-letter names of weekdays, starting with Sun. */
static const char *WEEKDAY_NAMES[] =
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
/** A c-locale array of 3-letter names of months, starting with Jan. */
static const char *MONTH_NAMES[] =
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/** Set <b>buf</b> to the RFC1123 encoding of the UTC value of <b>t</b>.
 * The buffer must be at least RFC1123_TIME_LEN+1 bytes long.
 *
 * (RFC1123 format is "Fri, 29 Sep 2006 15:54:20 GMT". Note the "GMT"
 * rather than "UTC".)
 */
void
format_rfc1123_time(char *buf, time_t t)
{
  struct tm tm;

  tor_gmtime_r(&t, &tm);

  strftime(buf, RFC1123_TIME_LEN+1, "___, %d ___ %Y %H:%M:%S GMT", &tm);
  tor_assert(tm.tm_wday >= 0);
  tor_assert(tm.tm_wday <= 6);
  memcpy(buf, WEEKDAY_NAMES[tm.tm_wday], 3);
  tor_assert(tm.tm_mon >= 0);
  tor_assert(tm.tm_mon <= 11);
  memcpy(buf+8, MONTH_NAMES[tm.tm_mon], 3);
}

/** Parse the (a subset of) the RFC1123 encoding of some time (in UTC) from
 * <b>buf</b>, and store the result in *<b>t</b>.
 *
 * Note that we only accept the subset generated by format_rfc1123_time above,
 * not the full range of formats suggested by RFC 1123.
 *
 * Return 0 on success, -1 on failure.
*/
int
parse_rfc1123_time(const char *buf, time_t *t)
{
  struct tm tm;
  char month[4];
  char weekday[4];
  int i, m;
  unsigned tm_mday, tm_year, tm_hour, tm_min, tm_sec;

  if (strlen(buf) != RFC1123_TIME_LEN)
    return -1;
  memset(&tm, 0, sizeof(tm));
  if (tor_sscanf(buf, "%3s, %2u %3s %u %2u:%2u:%2u GMT", weekday,
             &tm_mday, month, &tm_year, &tm_hour,
             &tm_min, &tm_sec) < 7) {
    char *esc = esc_for_log(buf);
    log_warn(LD_GENERAL, "Got invalid RFC1123 time %s", esc);
    tor_free(esc);
    return -1;
  }
  if (tm_mday < 1 || tm_mday > 31 || tm_hour > 23 || tm_min > 59 ||
      tm_sec > 60 || tm_year >= INT32_MAX || tm_year < 1970) {
    char *esc = esc_for_log(buf);
    log_warn(LD_GENERAL, "Got invalid RFC1123 time %s", esc);
    tor_free(esc);
    return -1;
  }
  tm.tm_mday = (int)tm_mday;
  tm.tm_year = (int)tm_year;
  tm.tm_hour = (int)tm_hour;
  tm.tm_min = (int)tm_min;
  tm.tm_sec = (int)tm_sec;

  m = -1;
  for (i = 0; i < 12; ++i) {
    if (!strcmp(month, MONTH_NAMES[i])) {
      m = i;
      break;
    }
  }
  if (m<0) {
    char *esc = esc_for_log(buf);
    log_warn(LD_GENERAL, "Got invalid RFC1123 time %s: No such month", esc);
    tor_free(esc);
    return -1;
  }
  tm.tm_mon = m;

  if (tm.tm_year < 1970) {
    char *esc = esc_for_log(buf);
    log_warn(LD_GENERAL,
             "Got invalid RFC1123 time %s. (Before 1970)", esc);
    tor_free(esc);
    return -1;
  }
  tm.tm_year -= 1900;

  return tor_timegm(&tm, t);
}

/** Set <b>buf</b> to the ISO8601 encoding of the local value of <b>t</b>.
 * The buffer must be at least ISO_TIME_LEN+1 bytes long.
 *
 * (ISO8601 format is 2006-10-29 10:57:20)
 */
void
format_local_iso_time(char *buf, time_t t)
{
  struct tm tm;
  strftime(buf, ISO_TIME_LEN+1, "%Y-%m-%d %H:%M:%S", tor_localtime_r(&t, &tm));
}

/** Set <b>buf</b> to the ISO8601 encoding of the GMT value of <b>t</b>.
 * The buffer must be at least ISO_TIME_LEN+1 bytes long.
 */
void
format_iso_time(char *buf, time_t t)
{
  struct tm tm;
  strftime(buf, ISO_TIME_LEN+1, "%Y-%m-%d %H:%M:%S", tor_gmtime_r(&t, &tm));
}

/** As format_iso_time, but use the yyyy-mm-ddThh:mm:ss format to avoid
 * embedding an internal space. */
void
format_iso_time_nospace(char *buf, time_t t)
{
  format_iso_time(buf, t);
  buf[10] = 'T';
}

/** As format_iso_time_nospace, but include microseconds in decimal
 * fixed-point format.  Requires that buf be at least ISO_TIME_USEC_LEN+1
 * bytes long. */
void
format_iso_time_nospace_usec(char *buf, const struct timeval *tv)
{
  tor_assert(tv);
  format_iso_time_nospace(buf, (time_t)tv->tv_sec);
  tor_snprintf(buf+ISO_TIME_LEN, 8, ".%06d", (int)tv->tv_usec);
}

/** Given an ISO-formatted UTC time value (after the epoch) in <b>cp</b>,
 * parse it and store its value in *<b>t</b>.  Return 0 on success, -1 on
 * failure.  Ignore extraneous stuff in <b>cp</b> separated by whitespace from
 * the end of the time string. */
int
parse_iso_time(const char *cp, time_t *t)
{
  struct tm st_tm;
  unsigned int year=0, month=0, day=0, hour=0, minute=0, second=0;
  if (tor_sscanf(cp, "%u-%2u-%2u %2u:%2u:%2u", &year, &month,
                &day, &hour, &minute, &second) < 6) {
    char *esc = esc_for_log(cp);
    log_warn(LD_GENERAL, "ISO time %s was unparseable", esc);
    tor_free(esc);
    return -1;
  }
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
          hour > 23 || minute > 59 || second > 60 || year >= INT32_MAX) {
    char *esc = esc_for_log(cp);
    log_warn(LD_GENERAL, "ISO time %s was nonsensical", esc);
    tor_free(esc);
    return -1;
  }
  st_tm.tm_year = (int)year-1900;
  st_tm.tm_mon = month-1;
  st_tm.tm_mday = day;
  st_tm.tm_hour = hour;
  st_tm.tm_min = minute;
  st_tm.tm_sec = second;

  if (st_tm.tm_year < 70) {
    char *esc = esc_for_log(cp);
    log_warn(LD_GENERAL, "Got invalid ISO time %s. (Before 1970)", esc);
    tor_free(esc);
    return -1;
  }
  return tor_timegm(&st_tm, t);
}

/** Given a <b>date</b> in one of the three formats allowed by HTTP (ugh),
 * parse it into <b>tm</b>.  Return 0 on success, negative on failure. */
int
parse_http_time(const char *date, struct tm *tm)
{
  const char *cp;
  char month[4];
  char wkday[4];
  int i;
  unsigned tm_mday, tm_year, tm_hour, tm_min, tm_sec;

  tor_assert(tm);
  memset(tm, 0, sizeof(*tm));

  /* First, try RFC1123 or RFC850 format: skip the weekday.  */
  if ((cp = strchr(date, ','))) {
    ++cp;
    if (*cp != ' ')
      return -1;
    ++cp;
    if (tor_sscanf(cp, "%2u %3s %4u %2u:%2u:%2u GMT",
               &tm_mday, month, &tm_year,
               &tm_hour, &tm_min, &tm_sec) == 6) {
      /* rfc1123-date */
      tm_year -= 1900;
    } else if (tor_sscanf(cp, "%2u-%3s-%2u %2u:%2u:%2u GMT",
                      &tm_mday, month, &tm_year,
                      &tm_hour, &tm_min, &tm_sec) == 6) {
      /* rfc850-date */
    } else {
      return -1;
    }
  } else {
    /* No comma; possibly asctime() format. */
    if (tor_sscanf(date, "%3s %3s %2u %2u:%2u:%2u %4u",
               wkday, month, &tm_mday,
               &tm_hour, &tm_min, &tm_sec, &tm_year) == 7) {
      tm_year -= 1900;
    } else {
      return -1;
    }
  }
  tm->tm_mday = (int)tm_mday;
  tm->tm_year = (int)tm_year;
  tm->tm_hour = (int)tm_hour;
  tm->tm_min = (int)tm_min;
  tm->tm_sec = (int)tm_sec;

  month[3] = '\0';
  /* Okay, now decode the month. */
  /* set tm->tm_mon to dummy value so the check below fails. */
  tm->tm_mon = -1;
  for (i = 0; i < 12; ++i) {
    if (!strcasecmp(MONTH_NAMES[i], month)) {
      tm->tm_mon = i;
    }
  }

  if (tm->tm_year < 0 ||
      tm->tm_mon < 0  || tm->tm_mon > 11 ||
      tm->tm_mday < 1 || tm->tm_mday > 31 ||
      tm->tm_hour < 0 || tm->tm_hour > 23 ||
      tm->tm_min < 0  || tm->tm_min > 59 ||
      tm->tm_sec < 0  || tm->tm_sec > 60)
    return -1; /* Out of range, or bad month. */

  return 0;
}

/** Given an <b>interval</b> in seconds, try to write it to the
 * <b>out_len</b>-byte buffer in <b>out</b> in a human-readable form.
 * Return 0 on success, -1 on failure.
 */
int
format_time_interval(char *out, size_t out_len, long interval)
{
  /* We only report seconds if there's no hours. */
  long sec = 0, min = 0, hour = 0, day = 0;
  if (interval < 0)
    interval = -interval;

  if (interval >= 86400) {
    day = interval / 86400;
    interval %= 86400;
  }
  if (interval >= 3600) {
    hour = interval / 3600;
    interval %= 3600;
  }
  if (interval >= 60) {
    min = interval / 60;
    interval %= 60;
  }
  sec = interval;

  if (day) {
    return tor_snprintf(out, out_len, "%ld days, %ld hours, %ld minutes",
                        day, hour, min);
  } else if (hour) {
    return tor_snprintf(out, out_len, "%ld hours, %ld minutes", hour, min);
  } else if (min) {
    return tor_snprintf(out, out_len, "%ld minutes, %ld seconds", min, sec);
  } else {
    return tor_snprintf(out, out_len, "%ld seconds", sec);
  }
}

/* =====
 * Cached time
 * ===== */

#ifndef TIME_IS_FAST
/** Cached estimate of the current time.  Updated around once per second;
 * may be a few seconds off if we are really busy.  This is a hack to avoid
 * calling time(NULL) (which not everybody has optimized) on critical paths.
 */
static time_t cached_approx_time = 0;

/** Return a cached estimate of the current time from when
 * update_approx_time() was last called.  This is a hack to avoid calling
 * time(NULL) on critical paths: please do not even think of calling it
 * anywhere else. */
time_t
approx_time(void)
{
  return cached_approx_time;
}

/** Update the cached estimate of the current time.  This function SHOULD be
 * called once per second, and MUST be called before the first call to
 * get_approx_time. */
void
update_approx_time(time_t now)
{
  cached_approx_time = now;
}
#endif

/* =====
 * Rate limiting
 * ===== */

/** If the rate-limiter <b>lim</b> is ready at <b>now</b>, return the number
 * of calls to rate_limit_is_ready (including this one!) since the last time
 * rate_limit_is_ready returned nonzero.  Otherwise return 0. */
static int
rate_limit_is_ready(ratelim_t *lim, time_t now)
{
  if (lim->rate + lim->last_allowed <= now) {
    int res = lim->n_calls_since_last_time + 1;
    lim->last_allowed = now;
    lim->n_calls_since_last_time = 0;
    return res;
  } else {
    ++lim->n_calls_since_last_time;
    return 0;
  }
}

/** If the rate-limiter <b>lim</b> is ready at <b>now</b>, return a newly
 * allocated string indicating how many messages were suppressed, suitable to
 * append to a log message.  Otherwise return NULL. */
char *
rate_limit_log(ratelim_t *lim, time_t now)
{
  int n;
  if ((n = rate_limit_is_ready(lim, now))) {
    if (n == 1) {
      return tor_strdup("");
    } else {
      char *cp=NULL;
      tor_asprintf(&cp,
                   " [%d similar message(s) suppressed in last %d seconds]",
                   n-1, lim->rate);
      return cp;
    }
  } else {
    return NULL;
  }
}

/* =====
 * File helpers
 * ===== */

/** Write <b>count</b> bytes from <b>buf</b> to <b>fd</b>.  <b>isSocket</b>
 * must be 1 if fd was returned by socket() or accept(), and 0 if fd
 * was returned by open().  Return the number of bytes written, or -1
 * on error.  Only use if fd is a blocking fd.  */
ssize_t
write_all(tor_socket_t fd, const char *buf, size_t count, int isSocket)
{
  size_t written = 0;
  ssize_t result;
  tor_assert(count < SSIZE_T_MAX);

  while (written != count) {
    if (isSocket)
      result = tor_socket_send(fd, buf+written, count-written, 0);
    else
      result = write((int)fd, buf+written, count-written);
    if (result<0)
      return -1;
    written += result;
  }
  return (ssize_t)count;
}

/** Read from <b>fd</b> to <b>buf</b>, until we get <b>count</b> bytes
 * or reach the end of the file. <b>isSocket</b> must be 1 if fd
 * was returned by socket() or accept(), and 0 if fd was returned by
 * open().  Return the number of bytes read, or -1 on error. Only use
 * if fd is a blocking fd. */
ssize_t
read_all(tor_socket_t fd, char *buf, size_t count, int isSocket)
{
  size_t numread = 0;
  ssize_t result;

  if (count > SIZE_T_CEILING || count > SSIZE_T_MAX)
    return -1;

  while (numread != count) {
    if (isSocket)
      result = tor_socket_recv(fd, buf+numread, count-numread, 0);
    else
      result = read((int)fd, buf+numread, count-numread);
    if (result<0)
      return -1;
    else if (result == 0)
      break;
    numread += result;
  }
  return (ssize_t)numread;
}

/*
 *    Filesystem operations.
 */

/** Clean up <b>name</b> so that we can use it in a call to "stat".  On Unix,
 * we do nothing.  On Windows, we remove a trailing slash, unless the path is
 * the root of a disk. */
static void
clean_name_for_stat(char *name)
{
#ifdef _WIN32
  size_t len = strlen(name);
  if (!len)
    return;
  if (name[len-1]=='\\' || name[len-1]=='/') {
    if (len == 1 || (len==3 && name[1]==':'))
      return;
    name[len-1]='\0';
  }
#else
  (void)name;
#endif
}

/** Return FN_ERROR if filename can't be read, FN_NOENT if it doesn't
 * exist, FN_FILE if it is a regular file, or FN_DIR if it's a
 * directory.  On FN_ERROR, sets errno. */
file_status_t
file_status(const char *fname)
{
  struct stat st;
  char *f;
  int r;
  f = tor_strdup(fname);
  clean_name_for_stat(f);
  log_debug(LD_FS, "stat()ing %s", f);
  r = stat(sandbox_intern_string(f), &st);
  tor_free(f);
  if (r) {
    if (errno == ENOENT) {
      return FN_NOENT;
    }
    return FN_ERROR;
  }
  if (st.st_mode & S_IFDIR)
    return FN_DIR;
  else if (st.st_mode & S_IFREG)
    return FN_FILE;
#ifndef _WIN32
  else if (st.st_mode & S_IFIFO)
    return FN_FILE;
#endif
  else
    return FN_ERROR;
}

/** Check whether <b>dirname</b> exists and is private.  If yes return 0.  If
 * it does not exist, and <b>check</b>&CPD_CREATE is set, try to create it
 * and return 0 on success. If it does not exist, and
 * <b>check</b>&CPD_CHECK, and we think we can create it, return 0.  Else
 * return -1.  If CPD_GROUP_OK is set, then it's okay if the directory
 * is group-readable, but in all cases we create the directory mode 0700.
 * If CPD_CHECK_MODE_ONLY is set, then we don't alter the directory permissions
 * if they are too permissive: we just return -1.
 * When effective_user is not NULL, check permissions against the given user
 * and its primary group.
 */
int
check_private_dir(const char *dirname, cpd_check_t check,
                  const char *effective_user)
{
  int r;
  struct stat st;
  char *f;
#ifndef _WIN32
  int mask;
  const struct passwd *pw = NULL;
  uid_t running_uid;
  gid_t running_gid;
#else
  (void)effective_user;
#endif

  tor_assert(dirname);
  f = tor_strdup(dirname);
  clean_name_for_stat(f);
  log_debug(LD_FS, "stat()ing %s", f);
  r = stat(sandbox_intern_string(f), &st);
  tor_free(f);
  if (r) {
    if (errno != ENOENT) {
      log_warn(LD_FS, "Directory %s cannot be read: %s", dirname,
               strerror(errno));
      return -1;
    }
    if (check & CPD_CREATE) {
      log_info(LD_GENERAL, "Creating directory %s", dirname);
#if defined (_WIN32) && !defined (WINCE)
      r = mkdir(dirname);
#else
      r = mkdir(dirname, 0700);
#endif
      if (r) {
        log_warn(LD_FS, "Error creating directory %s: %s", dirname,
            strerror(errno));
        return -1;
      }
    } else if (!(check & CPD_CHECK)) {
      log_warn(LD_FS, "Directory %s does not exist.", dirname);
      return -1;
    }
    /* XXXX In the case where check==CPD_CHECK, we should look at the
     * parent directory a little harder. */
    return 0;
  }
  if (!(st.st_mode & S_IFDIR)) {
    log_warn(LD_FS, "%s is not a directory", dirname);
    return -1;
  }
#ifndef _WIN32
  if (effective_user) {
    /* Look up the user and group information.
     * If we have a problem, bail out. */
    pw = tor_getpwnam(effective_user);
    if (pw == NULL) {
      log_warn(LD_CONFIG, "Error setting configured user: %s not found",
               effective_user);
      return -1;
    }
    running_uid = pw->pw_uid;
    running_gid = pw->pw_gid;
  } else {
    running_uid = getuid();
    running_gid = getgid();
  }

  if (st.st_uid != running_uid) {
    const struct passwd *pw = NULL;
    char *process_ownername = NULL;

    pw = tor_getpwuid(running_uid);
    process_ownername = pw ? tor_strdup(pw->pw_name) : tor_strdup("<unknown>");

    pw = tor_getpwuid(st.st_uid);

    log_warn(LD_FS, "%s is not owned by this user (%s, %d) but by "
        "%s (%d). Perhaps you are running Tor as the wrong user?",
                         dirname, process_ownername, (int)running_uid,
                         pw ? pw->pw_name : "<unknown>", (int)st.st_uid);

    tor_free(process_ownername);
    return -1;
  }
  if ((check & CPD_GROUP_OK) && st.st_gid != running_gid) {
    struct group *gr;
    char *process_groupname = NULL;
    gr = getgrgid(running_gid);
    process_groupname = gr ? tor_strdup(gr->gr_name) : tor_strdup("<unknown>");
    gr = getgrgid(st.st_gid);

    log_warn(LD_FS, "%s is not owned by this group (%s, %d) but by group "
             "%s (%d).  Are you running Tor as the wrong user?",
             dirname, process_groupname, (int)running_gid,
             gr ?  gr->gr_name : "<unknown>", (int)st.st_gid);

    tor_free(process_groupname);
    return -1;
  }
  if (check & CPD_GROUP_OK) {
    mask = 0027;
  } else {
    mask = 0077;
  }
  if (st.st_mode & mask) {
    unsigned new_mode;
    if (check & CPD_CHECK_MODE_ONLY) {
      log_warn(LD_FS, "Permissions on directory %s are too permissive.",
               dirname);
      return -1;
    }
    log_warn(LD_FS, "Fixing permissions on directory %s", dirname);
    new_mode = st.st_mode;
    new_mode |= 0700; /* Owner should have rwx */
    new_mode &= ~mask; /* Clear the other bits that we didn't want set...*/
    if (chmod(dirname, new_mode)) {
      log_warn(LD_FS, "Could not chmod directory %s: %s", dirname,
          strerror(errno));
      return -1;
    } else {
      return 0;
    }
  }
#endif
  return 0;
}

/** Create a file named <b>fname</b> with the contents <b>str</b>.  Overwrite
 * the previous <b>fname</b> if possible.  Return 0 on success, -1 on failure.
 *
 * This function replaces the old file atomically, if possible.  This
 * function, and all other functions in util.c that create files, create them
 * with mode 0600.
 */
int
write_str_to_file(const char *fname, const char *str, int bin)
{
#ifdef _WIN32
  if (!bin && strchr(str, '\r')) {
    log_warn(LD_BUG,
             "We're writing a text string that already contains a CR to %s",
             escaped(fname));
  }
#endif
  return write_bytes_to_file(fname, str, strlen(str), bin);
}

/** Represents a file that we're writing to, with support for atomic commit:
 * we can write into a temporary file, and either remove the file on
 * failure, or replace the original file on success. */
struct open_file_t {
  char *tempname; /**< Name of the temporary file. */
  char *filename; /**< Name of the original file. */
  unsigned rename_on_close:1; /**< Are we using the temporary file or not? */
  unsigned binary:1; /**< Did we open in binary mode? */
  int fd; /**< fd for the open file. */
  FILE *stdio_file; /**< stdio wrapper for <b>fd</b>. */
};

/** Try to start writing to the file in <b>fname</b>, passing the flags
 * <b>open_flags</b> to the open() syscall, creating the file (if needed) with
 * access value <b>mode</b>.  If the O_APPEND flag is set, we append to the
 * original file.  Otherwise, we open a new temporary file in the same
 * directory, and either replace the original or remove the temporary file
 * when we're done.
 *
 * Return the fd for the newly opened file, and store working data in
 * *<b>data_out</b>.  The caller should not close the fd manually:
 * instead, call finish_writing_to_file() or abort_writing_to_file().
 * Returns -1 on failure.
 *
 * NOTE: When not appending, the flags O_CREAT and O_TRUNC are treated
 * as true and the flag O_EXCL is treated as false.
 *
 * NOTE: Ordinarily, O_APPEND means "seek to the end of the file before each
 * write()".  We don't do that.
 */
int
start_writing_to_file(const char *fname, int open_flags, int mode,
                      open_file_t **data_out)
{
  open_file_t *new_file = tor_malloc_zero(sizeof(open_file_t));
  const char *open_name;
  int append = 0;

  tor_assert(fname);
  tor_assert(data_out);
#if (O_BINARY != 0 && O_TEXT != 0)
  tor_assert((open_flags & (O_BINARY|O_TEXT)) != 0);
#endif
  new_file->fd = -1;
  new_file->filename = tor_strdup(fname);
  if (open_flags & O_APPEND) {
    open_name = fname;
    new_file->rename_on_close = 0;
    append = 1;
    open_flags &= ~O_APPEND;
  } else {
    tor_asprintf(&new_file->tempname, "%s.tmp", fname);
    open_name = new_file->tempname;
    /* We always replace an existing temporary file if there is one. */
    open_flags |= O_CREAT|O_TRUNC;
    open_flags &= ~O_EXCL;
    new_file->rename_on_close = 1;
  }
#if O_BINARY != 0
  if (open_flags & O_BINARY)
    new_file->binary = 1;
#endif

  new_file->fd = tor_open_cloexec(open_name, open_flags, mode);
  if (new_file->fd < 0) {
    log_warn(LD_FS, "Couldn't open \"%s\" (%s) for writing: %s",
        open_name, fname, strerror(errno));
    goto err;
  }
  if (append) {
    if (tor_fd_seekend(new_file->fd) < 0) {
      log_warn(LD_FS, "Couldn't seek to end of file \"%s\": %s", open_name,
               strerror(errno));
      goto err;
    }
  }

  *data_out = new_file;

  return new_file->fd;

 err:
  if (new_file->fd >= 0)
    close(new_file->fd);
  *data_out = NULL;
  tor_free(new_file->filename);
  tor_free(new_file->tempname);
  tor_free(new_file);
  return -1;
}

/** Given <b>file_data</b> from start_writing_to_file(), return a stdio FILE*
 * that can be used to write to the same file.  The caller should not mix
 * stdio calls with non-stdio calls. */
FILE *
fdopen_file(open_file_t *file_data)
{
  tor_assert(file_data);
  if (file_data->stdio_file)
    return file_data->stdio_file;
  tor_assert(file_data->fd >= 0);
  if (!(file_data->stdio_file = fdopen(file_data->fd,
                                       file_data->binary?"ab":"a"))) {
    log_warn(LD_FS, "Couldn't fdopen \"%s\" [%d]: %s", file_data->filename,
             file_data->fd, strerror(errno));
  }
  return file_data->stdio_file;
}

/** Combines start_writing_to_file with fdopen_file(): arguments are as
 * for start_writing_to_file, but  */
FILE *
start_writing_to_stdio_file(const char *fname, int open_flags, int mode,
                            open_file_t **data_out)
{
  FILE *res;
  if (start_writing_to_file(fname, open_flags, mode, data_out)<0)
    return NULL;
  if (!(res = fdopen_file(*data_out))) {
    abort_writing_to_file(*data_out);
    *data_out = NULL;
  }
  return res;
}

/** Helper function: close and free the underlying file and memory in
 * <b>file_data</b>.  If we were writing into a temporary file, then delete
 * that file (if abort_write is true) or replaces the target file with
 * the temporary file (if abort_write is false). */
static int
finish_writing_to_file_impl(open_file_t *file_data, int abort_write)
{
  int r = 0;

  tor_assert(file_data && file_data->filename);
  if (file_data->stdio_file) {
    if (fclose(file_data->stdio_file)) {
      log_warn(LD_FS, "Error closing \"%s\": %s", file_data->filename,
               strerror(errno));
      abort_write = r = -1;
    }
  } else if (file_data->fd >= 0 && close(file_data->fd) < 0) {
    log_warn(LD_FS, "Error flushing \"%s\": %s", file_data->filename,
             strerror(errno));
    abort_write = r = -1;
  }

  if (file_data->rename_on_close) {
    tor_assert(file_data->tempname && file_data->filename);
    if (abort_write) {
      int res = unlink(file_data->tempname);
      if (res != 0) {
        /* We couldn't unlink and we'll leave a mess behind */
        log_warn(LD_FS, "Failed to unlink %s: %s",
                 file_data->tempname, strerror(errno));
        r = -1;
      }
    } else {
      tor_assert(strcmp(file_data->filename, file_data->tempname));
      if (replace_file(file_data->tempname, file_data->filename)) {
        log_warn(LD_FS, "Error replacing \"%s\": %s", file_data->filename,
                 strerror(errno));
        r = -1;
      }
    }
  }

  tor_free(file_data->filename);
  tor_free(file_data->tempname);
  tor_free(file_data);

  return r;
}

/** Finish writing to <b>file_data</b>: close the file handle, free memory as
 * needed, and if using a temporary file, replace the original file with
 * the temporary file. */
int
finish_writing_to_file(open_file_t *file_data)
{
  return finish_writing_to_file_impl(file_data, 0);
}

/** Finish writing to <b>file_data</b>: close the file handle, free memory as
 * needed, and if using a temporary file, delete it. */
int
abort_writing_to_file(open_file_t *file_data)
{
  return finish_writing_to_file_impl(file_data, 1);
}

/** Helper: given a set of flags as passed to open(2), open the file
 * <b>fname</b> and write all the sized_chunk_t structs in <b>chunks</b> to
 * the file.  Do so as atomically as possible e.g. by opening temp files and
 * renaming. */
static int
write_chunks_to_file_impl(const char *fname, const smartlist_t *chunks,
                          int open_flags)
{
  open_file_t *file = NULL;
  int fd;
  ssize_t result;
  fd = start_writing_to_file(fname, open_flags, 0600, &file);
  if (fd<0)
    return -1;
  SMARTLIST_FOREACH(chunks, sized_chunk_t *, chunk,
  {
    result = write_all(fd, chunk->bytes, chunk->len, 0);
    if (result < 0) {
      log_warn(LD_FS, "Error writing to \"%s\": %s", fname,
          strerror(errno));
      goto err;
    }
    tor_assert((size_t)result == chunk->len);
  });

  return finish_writing_to_file(file);
 err:
  abort_writing_to_file(file);
  return -1;
}

/** Given a smartlist of sized_chunk_t, write them to a file
 * <b>fname</b>, overwriting or creating the file as necessary.
 * If <b>no_tempfile</b> is 0 then the file will be written
 * atomically. */
int
write_chunks_to_file(const char *fname, const smartlist_t *chunks, int bin,
                     int no_tempfile)
{
  int flags = OPEN_FLAGS_REPLACE|(bin?O_BINARY:O_TEXT);

  if (no_tempfile) {
    /* O_APPEND stops write_chunks_to_file from using tempfiles */
    flags |= O_APPEND;
  }
  return write_chunks_to_file_impl(fname, chunks, flags);
}

/** Write <b>len</b> bytes, starting at <b>str</b>, to <b>fname</b>
    using the open() flags passed in <b>flags</b>. */
static int
write_bytes_to_file_impl(const char *fname, const char *str, size_t len,
                         int flags)
{
  int r;
  sized_chunk_t c = { str, len };
  smartlist_t *chunks = smartlist_new();
  smartlist_add(chunks, &c);
  r = write_chunks_to_file_impl(fname, chunks, flags);
  smartlist_free(chunks);
  return r;
}

/** As write_str_to_file, but does not assume a NUL-terminated
 * string. Instead, we write <b>len</b> bytes, starting at <b>str</b>. */
MOCK_IMPL(int,
write_bytes_to_file,(const char *fname, const char *str, size_t len,
                     int bin))
{
  return write_bytes_to_file_impl(fname, str, len,
                                  OPEN_FLAGS_REPLACE|(bin?O_BINARY:O_TEXT));
}

/** As write_bytes_to_file, but if the file already exists, append the bytes
 * to the end of the file instead of overwriting it. */
int
append_bytes_to_file(const char *fname, const char *str, size_t len,
                     int bin)
{
  return write_bytes_to_file_impl(fname, str, len,
                                  OPEN_FLAGS_APPEND|(bin?O_BINARY:O_TEXT));
}

/** Like write_str_to_file(), but also return -1 if there was a file
    already residing in <b>fname</b>. */
int
write_bytes_to_new_file(const char *fname, const char *str, size_t len,
                        int bin)
{
  return write_bytes_to_file_impl(fname, str, len,
                                  OPEN_FLAGS_DONT_REPLACE|
                                  (bin?O_BINARY:O_TEXT));
}

/**
 * Read the contents of the open file <b>fd</b> presuming it is a FIFO
 * (or similar) file descriptor for which the size of the file isn't
 * known ahead of time. Return NULL on failure, and a NUL-terminated
 * string on success.  On success, set <b>sz_out</b> to the number of
 * bytes read.
 */
char *
read_file_to_str_until_eof(int fd, size_t max_bytes_to_read, size_t *sz_out)
{
  ssize_t r;
  size_t pos = 0;
  char *string = NULL;
  size_t string_max = 0;

  if (max_bytes_to_read+1 >= SIZE_T_CEILING)
    return NULL;

  do {
    /* XXXX This "add 1K" approach is a little goofy; if we care about
     * performance here, we should be doubling.  But in practice we shouldn't
     * be using this function on big files anyway. */
    string_max = pos + 1024;
    if (string_max > max_bytes_to_read)
      string_max = max_bytes_to_read + 1;
    string = tor_realloc(string, string_max);
    r = read(fd, string + pos, string_max - pos - 1);
    if (r < 0) {
      tor_free(string);
      return NULL;
    }

    pos += r;
  } while (r > 0 && pos < max_bytes_to_read);

  *sz_out = pos;
  string[pos] = '\0';
  return string;
}

/** Read the contents of <b>filename</b> into a newly allocated
 * string; return the string on success or NULL on failure.
 *
 * If <b>stat_out</b> is provided, store the result of stat()ing the
 * file into <b>stat_out</b>.
 *
 * If <b>flags</b> &amp; RFTS_BIN, open the file in binary mode.
 * If <b>flags</b> &amp; RFTS_IGNORE_MISSING, don't warn if the file
 * doesn't exist.
 */
/*
 * This function <em>may</em> return an erroneous result if the file
 * is modified while it is running, but must not crash or overflow.
 * Right now, the error case occurs when the file length grows between
 * the call to stat and the call to read_all: the resulting string will
 * be truncated.
 */
char *
read_file_to_str(const char *filename, int flags, struct stat *stat_out)
{
  int fd; /* router file */
  struct stat statbuf;
  char *string;
  ssize_t r;
  int bin = flags & RFTS_BIN;

  tor_assert(filename);

  fd = tor_open_cloexec(filename,O_RDONLY|(bin?O_BINARY:O_TEXT),0);
  if (fd<0) {
    int severity = LOG_WARN;
    int save_errno = errno;
    if (errno == ENOENT && (flags & RFTS_IGNORE_MISSING))
      severity = LOG_INFO;
    log_fn(severity, LD_FS,"Could not open \"%s\": %s",filename,
           strerror(errno));
    errno = save_errno;
    return NULL;
  }

  if (fstat(fd, &statbuf)<0) {
    int save_errno = errno;
    close(fd);
    log_warn(LD_FS,"Could not fstat \"%s\".",filename);
    errno = save_errno;
    return NULL;
  }

#ifndef _WIN32
/** When we detect that we're reading from a FIFO, don't read more than
 * this many bytes.  It's insane overkill for most uses. */
#define FIFO_READ_MAX (1024*1024)
  if (S_ISFIFO(statbuf.st_mode)) {
    size_t sz = 0;
    string = read_file_to_str_until_eof(fd, FIFO_READ_MAX, &sz);
    if (string && stat_out) {
      statbuf.st_size = sz;
      memcpy(stat_out, &statbuf, sizeof(struct stat));
    }
    close(fd);
    return string;
  }
#endif

  if ((uint64_t)(statbuf.st_size)+1 >= SIZE_T_CEILING) {
    close(fd);
    return NULL;
  }

  string = tor_malloc((size_t)(statbuf.st_size+1));

  r = read_all(fd,string,(size_t)statbuf.st_size,0);
  if (r<0) {
    int save_errno = errno;
    log_warn(LD_FS,"Error reading from file \"%s\": %s", filename,
             strerror(errno));
    tor_free(string);
    close(fd);
    errno = save_errno;
    return NULL;
  }
  string[r] = '\0'; /* NUL-terminate the result. */

#if defined(_WIN32) || defined(__CYGWIN__)
  if (!bin && strchr(string, '\r')) {
    log_debug(LD_FS, "We didn't convert CRLF to LF as well as we hoped "
              "when reading %s. Coping.",
              filename);
    tor_strstrip(string, "\r");
    r = strlen(string);
  }
  if (!bin) {
    statbuf.st_size = (size_t) r;
  } else
#endif
    if (r != statbuf.st_size) {
      /* Unless we're using text mode on win32, we'd better have an exact
       * match for size. */
      int save_errno = errno;
      log_warn(LD_FS,"Could read only %d of %ld bytes of file \"%s\".",
               (int)r, (long)statbuf.st_size,filename);
      tor_free(string);
      close(fd);
      errno = save_errno;
      return NULL;
    }
  close(fd);
  if (stat_out) {
    memcpy(stat_out, &statbuf, sizeof(struct stat));
  }

  return string;
}

#define TOR_ISODIGIT(c) ('0' <= (c) && (c) <= '7')

/** Given a c-style double-quoted escaped string in <b>s</b>, extract and
 * decode its contents into a newly allocated string.  On success, assign this
 * string to *<b>result</b>, assign its length to <b>size_out</b> (if
 * provided), and return a pointer to the position in <b>s</b> immediately
 * after the string.  On failure, return NULL.
 */
static const char *
unescape_string(const char *s, char **result, size_t *size_out)
{
  const char *cp;
  char *out;
  if (s[0] != '\"')
    return NULL;
  cp = s+1;
  while (1) {
    switch (*cp) {
      case '\0':
      case '\n':
        return NULL;
      case '\"':
        goto end_of_loop;
      case '\\':
        if (cp[1] == 'x' || cp[1] == 'X') {
          if (!(TOR_ISXDIGIT(cp[2]) && TOR_ISXDIGIT(cp[3])))
            return NULL;
          cp += 4;
        } else if (TOR_ISODIGIT(cp[1])) {
          cp += 2;
          if (TOR_ISODIGIT(*cp)) ++cp;
          if (TOR_ISODIGIT(*cp)) ++cp;
        } else if (cp[1] == 'n' || cp[1] == 'r' || cp[1] == 't' || cp[1] == '"'
                   || cp[1] == '\\' || cp[1] == '\'') {
          cp += 2;
        } else {
          return NULL;
        }
        break;
      default:
        ++cp;
        break;
    }
  }
 end_of_loop:
  out = *result = tor_malloc(cp-s + 1);
  cp = s+1;
  while (1) {
    switch (*cp)
      {
      case '\"':
        *out = '\0';
        if (size_out) *size_out = out - *result;
        return cp+1;
      case '\0':
        tor_fragile_assert();
        tor_free(*result);
        return NULL;
      case '\\':
        switch (cp[1])
          {
          case 'n': *out++ = '\n'; cp += 2; break;
          case 'r': *out++ = '\r'; cp += 2; break;
          case 't': *out++ = '\t'; cp += 2; break;
          case 'x': case 'X':
            {
              int x1, x2;

              x1 = hex_decode_digit(cp[2]);
              x2 = hex_decode_digit(cp[3]);
              if (x1 == -1 || x2 == -1) {
                  tor_free(*result);
                  return NULL;
              }

              *out++ = ((x1<<4) + x2);
              cp += 4;
            }
            break;
          case '0': case '1': case '2': case '3': case '4': case '5':
          case '6': case '7':
            {
              int n = cp[1]-'0';
              cp += 2;
              if (TOR_ISODIGIT(*cp)) { n = n*8 + *cp-'0'; cp++; }
              if (TOR_ISODIGIT(*cp)) { n = n*8 + *cp-'0'; cp++; }
              if (n > 255) { tor_free(*result); return NULL; }
              *out++ = (char)n;
            }
            break;
          case '\'':
          case '\"':
          case '\\':
          case '\?':
            *out++ = cp[1];
            cp += 2;
            break;
          default:
            tor_free(*result); return NULL;
          }
        break;
      default:
        *out++ = *cp++;
      }
  }
}

/** Given a string containing part of a configuration file or similar format,
 * advance past comments and whitespace and try to parse a single line.  If we
 * parse a line successfully, set *<b>key_out</b> to a new string holding the
 * key portion and *<b>value_out</b> to a new string holding the value portion
 * of the line, and return a pointer to the start of the next line.  If we run
 * out of data, return a pointer to the end of the string.  If we encounter an
 * error, return NULL and set *<b>err_out</b> (if provided) to an error
 * message.
 */
const char *
parse_config_line_from_str_verbose(const char *line, char **key_out,
                                   char **value_out,
                                   const char **err_out)
{
  /* I believe the file format here is supposed to be:
     FILE = (EMPTYLINE | LINE)* (EMPTYLASTLINE | LASTLINE)?

     EMPTYLASTLINE = SPACE* | COMMENT
     EMPTYLINE = EMPTYLASTLINE NL
     SPACE = ' ' | '\r' | '\t'
     COMMENT = '#' NOT-NL*
     NOT-NL = Any character except '\n'
     NL = '\n'

     LASTLINE = SPACE* KEY SPACE* VALUES
     LINE = LASTLINE NL
     KEY = KEYCHAR+
     KEYCHAR = Any character except ' ', '\r', '\n', '\t', '#', "\"

     VALUES = QUOTEDVALUE | NORMALVALUE
     QUOTEDVALUE = QUOTE QVCHAR* QUOTE EOLSPACE?
     QUOTE = '"'
     QVCHAR = KEYCHAR | ESC ('n' | 't' | 'r' | '"' | ESC |'\'' | OCTAL | HEX)
     ESC = "\\"
     OCTAL = ODIGIT (ODIGIT ODIGIT?)?
     HEX = ('x' | 'X') HEXDIGIT HEXDIGIT
     ODIGIT = '0' .. '7'
     HEXDIGIT = '0'..'9' | 'a' .. 'f' | 'A' .. 'F'
     EOLSPACE = SPACE* COMMENT?

     NORMALVALUE = (VALCHAR | ESC ESC_IGNORE | CONTINUATION)* EOLSPACE?
     VALCHAR = Any character except ESC, '#', and '\n'
     ESC_IGNORE = Any character except '#' or '\n'
     CONTINUATION = ESC NL ( COMMENT NL )*
   */

  const char *key, *val, *cp;
  int continuation = 0;

  tor_assert(key_out);
  tor_assert(value_out);

  *key_out = *value_out = NULL;
  key = val = NULL;
  /* Skip until the first keyword. */
  while (1) {
    while (TOR_ISSPACE(*line))
      ++line;
    if (*line == '#') {
      while (*line && *line != '\n')
        ++line;
    } else {
      break;
    }
  }

  if (!*line) { /* End of string? */
    *key_out = *value_out = NULL;
    return line;
  }

  /* Skip until the next space or \ followed by newline. */
  key = line;
  while (*line && !TOR_ISSPACE(*line) && *line != '#' &&
         ! (line[0] == '\\' && line[1] == '\n'))
    ++line;
  *key_out = tor_strndup(key, line-key);

  /* Skip until the value. */
  while (*line == ' ' || *line == '\t')
    ++line;

  val = line;

  /* Find the end of the line. */
  if (*line == '\"') { // XXX No continuation handling is done here
    if (!(line = unescape_string(line, value_out, NULL))) {
      if (err_out)
        *err_out = "Invalid escape sequence in quoted string";
      return NULL;
    }
    while (*line == ' ' || *line == '\t')
      ++line;
    if (*line && *line != '#' && *line != '\n') {
      if (err_out)
        *err_out = "Excess data after quoted string";
      return NULL;
    }
  } else {
    /* Look for the end of the line. */
    while (*line && *line != '\n' && (*line != '#' || continuation)) {
      if (*line == '\\' && line[1] == '\n') {
        continuation = 1;
        line += 2;
      } else if (*line == '#') {
        do {
          ++line;
        } while (*line && *line != '\n');
        if (*line == '\n')
          ++line;
      } else {
        ++line;
      }
    }

    if (*line == '\n') {
      cp = line++;
    } else {
      cp = line;
    }
    /* Now back cp up to be the last nonspace character */
    while (cp>val && TOR_ISSPACE(*(cp-1)))
      --cp;

    tor_assert(cp >= val);

    /* Now copy out and decode the value. */
    *value_out = tor_strndup(val, cp-val);
    if (continuation) {
      char *v_out, *v_in;
      v_out = v_in = *value_out;
      while (*v_in) {
        if (*v_in == '#') {
          do {
            ++v_in;
          } while (*v_in && *v_in != '\n');
          if (*v_in == '\n')
            ++v_in;
        } else if (v_in[0] == '\\' && v_in[1] == '\n') {
          v_in += 2;
        } else {
          *v_out++ = *v_in++;
        }
      }
      *v_out = '\0';
    }
  }

  if (*line == '#') {
    do {
      ++line;
    } while (*line && *line != '\n');
  }
  while (TOR_ISSPACE(*line)) ++line;

  return line;
}

/** Expand any homedir prefix on <b>filename</b>; return a newly allocated
 * string. */
char *
expand_filename(const char *filename)
{
  tor_assert(filename);
#ifdef _WIN32
  return tor_strdup(filename);
#else
  if (*filename == '~') {
    char *home, *result=NULL;
    const char *rest;

    if (filename[1] == '/' || filename[1] == '\0') {
      home = getenv("HOME");
      if (!home) {
        log_warn(LD_CONFIG, "Couldn't find $HOME environment variable while "
                 "expanding \"%s\"; defaulting to \"\".", filename);
        home = tor_strdup("");
      } else {
        home = tor_strdup(home);
      }
      rest = strlen(filename)>=2?(filename+2):"";
    } else {
#ifdef HAVE_PWD_H
      char *username, *slash;
      slash = strchr(filename, '/');
      if (slash)
        username = tor_strndup(filename+1,slash-filename-1);
      else
        username = tor_strdup(filename+1);
      if (!(home = get_user_homedir(username))) {
        log_warn(LD_CONFIG,"Couldn't get homedir for \"%s\"",username);
        tor_free(username);
        return NULL;
      }
      tor_free(username);
      rest = slash ? (slash+1) : "";
#else
      log_warn(LD_CONFIG, "Couldn't expend homedir on system without pwd.h");
      return tor_strdup(filename);
#endif
    }
    tor_assert(home);
    /* Remove trailing slash. */
    if (strlen(home)>1 && !strcmpend(home,PATH_SEPARATOR)) {
      home[strlen(home)-1] = '\0';
    }
    tor_asprintf(&result,"%s"PATH_SEPARATOR"%s",home,rest);
    tor_free(home);
    return result;
  } else {
    return tor_strdup(filename);
  }
#endif
}

#define MAX_SCANF_WIDTH 9999

/** Helper: given an ASCII-encoded decimal digit, return its numeric value.
 * NOTE: requires that its input be in-bounds. */
static int
digit_to_num(char d)
{
  int num = ((int)d) - (int)'0';
  tor_assert(num <= 9 && num >= 0);
  return num;
}

/** Helper: Read an unsigned int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return 0.  On failure, return -1. */
static int
scan_unsigned(const char **bufp, unsigned long *out, int width, int base)
{
  unsigned long result = 0;
  int scanned_so_far = 0;
  const int hex = base==16;
  tor_assert(base == 10 || base == 16);
  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=MAX_SCANF_WIDTH;

  while (**bufp && (hex?TOR_ISXDIGIT(**bufp):TOR_ISDIGIT(**bufp))
         && scanned_so_far < width) {
    int digit = hex?hex_decode_digit(*(*bufp)++):digit_to_num(*(*bufp)++);
    unsigned long new_result = result * base + digit;
    if (new_result < result)
      return -1; /* over/underflow. */
    result = new_result;
    ++scanned_so_far;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return -1;

  *out = result;
  return 0;
}

/** Helper: Read an signed int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return 0.  On failure, return -1. */
static int
scan_signed(const char **bufp, long *out, int width)
{
  int neg = 0;
  unsigned long result = 0;

  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
    --width;
  }

  if (scan_unsigned(bufp, &result, width, 10) < 0)
    return -1;

  if (neg) {
    if (result > ((unsigned long)LONG_MAX) + 1)
      return -1; /* Underflow */
    *out = -(long)result;
  } else {
    if (result > LONG_MAX)
      return -1; /* Overflow */
    *out = (long)result;
  }

  return 0;
}

/** Helper: Read a decimal-formatted double from *<b>bufp</b> of up to
 * <b>width</b> characters.  (Handle arbitrary width if <b>width</b> is less
 * than 0.)  On success, store the result in <b>out</b>, advance bufp to the
 * next character, and return 0.  On failure, return -1. */
static int
scan_double(const char **bufp, double *out, int width)
{
  int neg = 0;
  double result = 0;
  int scanned_so_far = 0;

  if (!bufp || !*bufp || !out)
    return -1;
  if (width<0)
    width=MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
  }

  while (**bufp && TOR_ISDIGIT(**bufp) && scanned_so_far < width) {
    const int digit = digit_to_num(*(*bufp)++);
    result = result * 10 + digit;
    ++scanned_so_far;
  }
  if (**bufp == '.') {
    double fracval = 0, denominator = 1;
    ++*bufp;
    ++scanned_so_far;
    while (**bufp && TOR_ISDIGIT(**bufp) && scanned_so_far < width) {
      const int digit = digit_to_num(*(*bufp)++);
      fracval = fracval * 10 + digit;
      denominator *= 10;
      ++scanned_so_far;
    }
    result += fracval / denominator;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return -1;

  *out = neg ? -result : result;
  return 0;
}

/** Helper: copy up to <b>width</b> non-space characters from <b>bufp</b> to
 * <b>out</b>.  Make sure <b>out</b> is nul-terminated. Advance <b>bufp</b>
 * to the next non-space character or the EOS. */
static int
scan_string(const char **bufp, char *out, int width)
{
  int scanned_so_far = 0;
  if (!bufp || !out || width < 0)
    return -1;
  while (**bufp && ! TOR_ISSPACE(**bufp) && scanned_so_far < width) {
    *out++ = *(*bufp)++;
    ++scanned_so_far;
  }
  *out = '\0';
  return 0;
}

/** Locale-independent, minimal, no-surprises scanf variant, accepting only a
 * restricted pattern format.  For more info on what it supports, see
 * tor_sscanf() documentation.  */
int
tor_vsscanf(const char *buf, const char *pattern, va_list ap)
{
  int n_matched = 0;

  while (*pattern) {
    if (*pattern != '%') {
      if (*buf == *pattern) {
        ++buf;
        ++pattern;
        continue;
      } else {
        return n_matched;
      }
    } else {
      int width = -1;
      int longmod = 0;
      ++pattern;
      if (TOR_ISDIGIT(*pattern)) {
        width = digit_to_num(*pattern++);
        while (TOR_ISDIGIT(*pattern)) {
          width *= 10;
          width += digit_to_num(*pattern++);
          if (width > MAX_SCANF_WIDTH)
            return -1;
        }
        if (!width) /* No zero-width things. */
          return -1;
      }
      if (*pattern == 'l') {
        longmod = 1;
        ++pattern;
      }
      if (*pattern == 'u' || *pattern == 'x') {
        unsigned long u;
        const int base = (*pattern == 'u') ? 10 : 16;
        if (!*buf)
          return n_matched;
        if (scan_unsigned(&buf, &u, width, base)<0)
          return n_matched;
        if (longmod) {
          unsigned long *out = va_arg(ap, unsigned long *);
          *out = u;
        } else {
          unsigned *out = va_arg(ap, unsigned *);
          if (u > UINT_MAX)
            return n_matched;
          *out = (unsigned) u;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'f') {
        double *d = va_arg(ap, double *);
        if (!longmod)
          return -1; /* float not supported */
        if (!*buf)
          return n_matched;
        if (scan_double(&buf, d, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'd') {
        long lng=0;
        if (scan_signed(&buf, &lng, width)<0)
          return n_matched;
        if (longmod) {
          long *out = va_arg(ap, long *);
          *out = lng;
        } else {
          int *out = va_arg(ap, int *);
          if (lng < INT_MIN || lng > INT_MAX)
            return n_matched;
          *out = (int)lng;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 's') {
        char *s = va_arg(ap, char *);
        if (longmod)
          return -1;
        if (width < 0)
          return -1;
        if (scan_string(&buf, s, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'c') {
        char *ch = va_arg(ap, char *);
        if (longmod)
          return -1;
        if (width != -1)
          return -1;
        if (!*buf)
          return n_matched;
        *ch = *buf++;
        ++pattern;
        ++n_matched;
      } else if (*pattern == '%') {
        if (*buf != '%')
          return n_matched;
        if (longmod)
          return -1;
        ++buf;
        ++pattern;
      } else {
        return -1; /* Unrecognized pattern component. */
      }
    }
  }

  return n_matched;
}

/** Minimal sscanf replacement: parse <b>buf</b> according to <b>pattern</b>
 * and store the results in the corresponding argument fields.  Differs from
 * sscanf in that:
 * <ul><li>It only handles %u, %lu, %x, %lx, %[NUM]s, %d, %ld, %lf, and %c.
 *     <li>It only handles decimal inputs for %lf. (12.3, not 1.23e1)
 *     <li>It does not handle arbitrarily long widths.
 *     <li>Numbers do not consume any space characters.
 *     <li>It is locale-independent.
 *     <li>%u and %x do not consume any space.
 *     <li>It returns -1 on malformed patterns.</ul>
 *
 * (As with other locale-independent functions, we need this to parse data that
 * is in ASCII without worrying that the C library's locale-handling will make
 * miscellaneous characters look like numbers, spaces, and so on.)
 */
int
tor_sscanf(const char *buf, const char *pattern, ...)
{
  int r;
  va_list ap;
  va_start(ap, pattern);
  r = tor_vsscanf(buf, pattern, ap);
  va_end(ap);
  return r;
}

/** Append the string produced by tor_asprintf(<b>pattern</b>, <b>...</b>)
 * to <b>sl</b>. */
void
smartlist_add_asprintf(struct smartlist_t *sl, const char *pattern, ...)
{
  va_list ap;
  va_start(ap, pattern);
  smartlist_add_vasprintf(sl, pattern, ap);
  va_end(ap);
}

/** va_list-based backend of smartlist_add_asprintf. */
void
smartlist_add_vasprintf(struct smartlist_t *sl, const char *pattern,
                        va_list args)
{
  char *str = NULL;

  tor_vasprintf(&str, pattern, args);
  tor_assert(str != NULL);

  smartlist_add(sl, str);
}

/** Return a new list containing the filenames in the directory <b>dirname</b>.
 * Return NULL on error or if <b>dirname</b> is not a directory.
 */
smartlist_t *
tor_listdir(const char *dirname)
{
  smartlist_t *result;
#ifdef _WIN32
  char *pattern=NULL;
  TCHAR tpattern[MAX_PATH] = {0};
  char name[MAX_PATH*2+1] = {0};
  HANDLE handle;
  WIN32_FIND_DATA findData;
  tor_asprintf(&pattern, "%s\\*", dirname);
#ifdef UNICODE
  mbstowcs(tpattern,pattern,MAX_PATH);
#else
  strlcpy(tpattern, pattern, MAX_PATH);
#endif
  if (INVALID_HANDLE_VALUE == (handle = FindFirstFile(tpattern, &findData))) {
    tor_free(pattern);
    return NULL;
  }
  result = smartlist_new();
  while (1) {
#ifdef UNICODE
    wcstombs(name,findData.cFileName,MAX_PATH);
    name[sizeof(name)-1] = '\0';
#else
    strlcpy(name,findData.cFileName,sizeof(name));
#endif
    if (strcmp(name, ".") &&
        strcmp(name, "..")) {
      smartlist_add(result, tor_strdup(name));
    }
    if (!FindNextFile(handle, &findData)) {
      DWORD err;
      if ((err = GetLastError()) != ERROR_NO_MORE_FILES) {
        char *errstr = format_win32_error(err);
        log_warn(LD_FS, "Error reading directory '%s': %s", dirname, errstr);
        tor_free(errstr);
      }
      break;
    }
  }
  FindClose(handle);
  tor_free(pattern);
#else
  const char *prot_dname = sandbox_intern_string(dirname);
  DIR *d;
  struct dirent *de;
  if (!(d = opendir(prot_dname)))
    return NULL;

  result = smartlist_new();
  while ((de = readdir(d))) {
    if (!strcmp(de->d_name, ".") ||
        !strcmp(de->d_name, ".."))
      continue;
    smartlist_add(result, tor_strdup(de->d_name));
  }
  closedir(d);
#endif
  return result;
}

/** Return true iff <b>filename</b> is a relative path. */
int
path_is_relative(const char *filename)
{
  if (filename && filename[0] == '/')
    return 0;
#ifdef _WIN32
  else if (filename && filename[0] == '\\')
    return 0;
  else if (filename && strlen(filename)>3 && TOR_ISALPHA(filename[0]) &&
           filename[1] == ':' && filename[2] == '\\')
    return 0;
#endif
  else
    return 1;
}

/* =====
 * Process helpers
 * ===== */

#ifndef _WIN32
/* Based on code contributed by christian grothoff */
/** True iff we've called start_daemon(). */
static int start_daemon_called = 0;
/** True iff we've called finish_daemon(). */
static int finish_daemon_called = 0;
/** Socketpair used to communicate between parent and child process while
 * daemonizing. */
static int daemon_filedes[2];
/** Start putting the process into daemon mode: fork and drop all resources
 * except standard fds.  The parent process never returns, but stays around
 * until finish_daemon is called.  (Note: it's safe to call this more
 * than once: calls after the first are ignored.)
 */
void
start_daemon(void)
{
  pid_t pid;

  if (start_daemon_called)
    return;
  start_daemon_called = 1;

  if (pipe(daemon_filedes)) {
    log_err(LD_GENERAL,"pipe failed; exiting. Error was %s", strerror(errno));
    exit(1);
  }
  pid = fork();
  if (pid < 0) {
    log_err(LD_GENERAL,"fork failed. Exiting.");
    exit(1);
  }
  if (pid) {  /* Parent */
    int ok;
    char c;

    close(daemon_filedes[1]); /* we only read */
    ok = -1;
    while (0 < read(daemon_filedes[0], &c, sizeof(char))) {
      if (c == '.')
        ok = 1;
    }
    fflush(stdout);
    if (ok == 1)
      exit(0);
    else
      exit(1); /* child reported error */
  } else { /* Child */
    close(daemon_filedes[0]); /* we only write */

    pid = setsid(); /* Detach from controlling terminal */
    /*
     * Fork one more time, so the parent (the session group leader) can exit.
     * This means that we, as a non-session group leader, can never regain a
     * controlling terminal.   This part is recommended by Stevens's
     * _Advanced Programming in the Unix Environment_.
     */
    if (fork() != 0) {
      exit(0);
    }
    set_main_thread(); /* We are now the main thread. */

    return;
  }
}

/** Finish putting the process into daemon mode: drop standard fds, and tell
 * the parent process to exit.  (Note: it's safe to call this more than once:
 * calls after the first are ignored.  Calls start_daemon first if it hasn't
 * been called already.)
 */
void
finish_daemon(const char *desired_cwd)
{
  int nullfd;
  char c = '.';
  if (finish_daemon_called)
    return;
  if (!start_daemon_called)
    start_daemon();
  finish_daemon_called = 1;

  if (!desired_cwd)
    desired_cwd = "/";
   /* Don't hold the wrong FS mounted */
  if (chdir(desired_cwd) < 0) {
    log_err(LD_GENERAL,"chdir to \"%s\" failed. Exiting.",desired_cwd);
    exit(1);
  }

  nullfd = tor_open_cloexec("/dev/null", O_RDWR, 0);
  if (nullfd < 0) {
    log_err(LD_GENERAL,"/dev/null can't be opened. Exiting.");
    exit(1);
  }
  /* close fds linking to invoking terminal, but
   * close usual incoming fds, but redirect them somewhere
   * useful so the fds don't get reallocated elsewhere.
   */
  if (dup2(nullfd,0) < 0 ||
      dup2(nullfd,1) < 0 ||
      dup2(nullfd,2) < 0) {
    log_err(LD_GENERAL,"dup2 failed. Exiting.");
    exit(1);
  }
  if (nullfd > 2)
    close(nullfd);
  /* signal success */
  if (write(daemon_filedes[1], &c, sizeof(char)) != sizeof(char)) {
    log_err(LD_GENERAL,"write failed. Exiting.");
  }
  close(daemon_filedes[1]);
}
#else
/* defined(_WIN32) */
void
start_daemon(void)
{
}
void
finish_daemon(const char *cp)
{
  (void)cp;
}
#endif

/** Write the current process ID, followed by NL, into <b>filename</b>.
 */
void
write_pidfile(char *filename)
{
  FILE *pidfile;

  if ((pidfile = fopen(filename, "w")) == NULL) {
    log_warn(LD_FS, "Unable to open \"%s\" for writing: %s", filename,
             strerror(errno));
  } else {
#ifdef _WIN32
    fprintf(pidfile, "%d\n", (int)_getpid());
#else
    fprintf(pidfile, "%d\n", (int)getpid());
#endif
    fclose(pidfile);
  }
}

#ifdef _WIN32
HANDLE
load_windows_system_library(const TCHAR *library_name)
{
  TCHAR path[MAX_PATH];
  unsigned n;
  n = GetSystemDirectory(path, MAX_PATH);
  if (n == 0 || n + _tcslen(library_name) + 2 >= MAX_PATH)
    return 0;
  _tcscat(path, TEXT("\\"));
  _tcscat(path, library_name);
  return LoadLibrary(path);
}
#endif

/** Format a single argument for being put on a Windows command line.
 * Returns a newly allocated string */
static char *
format_win_cmdline_argument(const char *arg)
{
  char *formatted_arg;
  char need_quotes;
  const char *c;
  int i;
  int bs_counter = 0;
  /* Backslash we can point to when one is inserted into the string */
  const char backslash = '\\';

  /* Smartlist of *char */
  smartlist_t *arg_chars;
  arg_chars = smartlist_new();

  /* Quote string if it contains whitespace or is empty */
  need_quotes = (strchr(arg, ' ') || strchr(arg, '\t') || '\0' == arg[0]);

  /* Build up smartlist of *chars */
  for (c=arg; *c != '\0'; c++) {
    if ('"' == *c) {
      /* Double up backslashes preceding a quote */
      for (i=0; i<(bs_counter*2); i++)
        smartlist_add(arg_chars, (void*)&backslash);
      bs_counter = 0;
      /* Escape the quote */
      smartlist_add(arg_chars, (void*)&backslash);
      smartlist_add(arg_chars, (void*)c);
    } else if ('\\' == *c) {
      /* Count backslashes until we know whether to double up */
      bs_counter++;
    } else {
      /* Don't double up slashes preceding a non-quote */
      for (i=0; i<bs_counter; i++)
        smartlist_add(arg_chars, (void*)&backslash);
      bs_counter = 0;
      smartlist_add(arg_chars, (void*)c);
    }
  }
  /* Don't double up trailing backslashes */
  for (i=0; i<bs_counter; i++)
    smartlist_add(arg_chars, (void*)&backslash);

  /* Allocate space for argument, quotes (if needed), and terminator */
  formatted_arg = tor_malloc(sizeof(char) *
      (smartlist_len(arg_chars) + (need_quotes?2:0) + 1));

  /* Add leading quote */
  i=0;
  if (need_quotes)
    formatted_arg[i++] = '"';

  /* Add characters */
  SMARTLIST_FOREACH(arg_chars, char*, c,
  {
    formatted_arg[i++] = *c;
  });

  /* Add trailing quote */
  if (need_quotes)
    formatted_arg[i++] = '"';
  formatted_arg[i] = '\0';

  smartlist_free(arg_chars);
  return formatted_arg;
}

/** Format a command line for use on Windows, which takes the command as a
 * string rather than string array. Follows the rules from "Parsing C++
 * Command-Line Arguments" in MSDN. Algorithm based on list2cmdline in the
 * Python subprocess module. Returns a newly allocated string */
char *
tor_join_win_cmdline(const char *argv[])
{
  smartlist_t *argv_list;
  char *joined_argv;
  int i;

  /* Format each argument and put the result in a smartlist */
  argv_list = smartlist_new();
  for (i=0; argv[i] != NULL; i++) {
    smartlist_add(argv_list, (void *)format_win_cmdline_argument(argv[i]));
  }

  /* Join the arguments with whitespace */
  joined_argv = smartlist_join_strings(argv_list, " ", 0, NULL);

  /* Free the newly allocated arguments, and the smartlist */
  SMARTLIST_FOREACH(argv_list, char *, arg,
  {
    tor_free(arg);
  });
  smartlist_free(argv_list);

  return joined_argv;
}

/* As format_{hex,dex}_number_sigsafe, but takes a <b>radix</b> argument
 * in range 2..16 inclusive. */
static int
format_number_sigsafe(unsigned long x, char *buf, int buf_len,
                      unsigned int radix)
{
  unsigned long tmp;
  int len;
  char *cp;

  /* NOT tor_assert. This needs to be safe to run from within a signal handler,
   * and from within the 'tor_assert() has failed' code. */
  if (radix < 2 || radix > 16)
    return 0;

  /* Count how many digits we need. */
  tmp = x;
  len = 1;
  while (tmp >= radix) {
    tmp /= radix;
    ++len;
  }

  /* Not long enough */
  if (!buf || len >= buf_len)
    return 0;

  cp = buf + len;
  *cp = '\0';
  do {
    unsigned digit = (unsigned) (x % radix);
    tor_assert(cp > buf);
    --cp;
    *cp = "0123456789ABCDEF"[digit];
    x /= radix;
  } while (x);

  /* NOT tor_assert; see above. */
  if (cp != buf) {
    abort();
  }

  return len;
}

/**
 * Helper function to output hex numbers from within a signal handler.
 *
 * Writes the nul-terminated hexadecimal digits of <b>x</b> into a buffer
 * <b>buf</b> of size <b>buf_len</b>, and return the actual number of digits
 * written, not counting the terminal NUL.
 *
 * If there is insufficient space, write nothing and return 0.
 *
 * This accepts an unsigned int because format_helper_exit_status() needs to
 * call it with a signed int and an unsigned char, and since the C standard
 * does not guarantee that an int is wider than a char (an int must be at
 * least 16 bits but it is permitted for a char to be that wide as well), we
 * can't assume a signed int is sufficient to accomodate an unsigned char.
 * Thus, format_helper_exit_status() will still need to emit any require '-'
 * on its own.
 *
 * For most purposes, you'd want to use tor_snprintf("%x") instead of this
 * function; it's designed to be used in code paths where you can't call
 * arbitrary C functions.
 */
int
format_hex_number_sigsafe(unsigned long x, char *buf, int buf_len)
{
  return format_number_sigsafe(x, buf, buf_len, 16);
}

/** As format_hex_number_sigsafe, but format the number in base 10. */
int
format_dec_number_sigsafe(unsigned long x, char *buf, int buf_len)
{
  return format_number_sigsafe(x, buf, buf_len, 10);
}

#ifndef _WIN32
/** Format <b>child_state</b> and <b>saved_errno</b> as a hex string placed in
 * <b>hex_errno</b>.  Called between fork and _exit, so must be signal-handler
 * safe.
 *
 * <b>hex_errno</b> must have at least HEX_ERRNO_SIZE+1 bytes available.
 *
 * The format of <b>hex_errno</b> is: "CHILD_STATE/ERRNO\n", left-padded
 * with spaces. CHILD_STATE indicates where
 * in the processs of starting the child process did the failure occur (see
 * CHILD_STATE_* macros for definition), and SAVED_ERRNO is the value of
 * errno when the failure occurred.
 *
 * On success return the number of characters added to hex_errno, not counting
 * the terminating NUL; return -1 on error.
 */
STATIC int
format_helper_exit_status(unsigned char child_state, int saved_errno,
                          char *hex_errno)
{
  unsigned int unsigned_errno;
  int written, left;
  char *cur;
  size_t i;
  int res = -1;

  /* Fill hex_errno with spaces, and a trailing newline (memset may
     not be signal handler safe, so we can't use it) */
  for (i = 0; i < (HEX_ERRNO_SIZE - 1); i++)
    hex_errno[i] = ' ';
  hex_errno[HEX_ERRNO_SIZE - 1] = '\n';

  /* Convert errno to be unsigned for hex conversion */
  if (saved_errno < 0) {
    unsigned_errno = (unsigned int) -saved_errno;
  } else {
    unsigned_errno = (unsigned int) saved_errno;
  }

  /*
   * Count how many chars of space we have left, and keep a pointer into the
   * current point in the buffer.
   */
  left = HEX_ERRNO_SIZE+1;
  cur = hex_errno;

  /* Emit child_state */
  written = format_hex_number_sigsafe(child_state, cur, left);

  if (written <= 0)
    goto err;

  /* Adjust left and cur */
  left -= written;
  cur += written;
  if (left <= 0)
    goto err;

  /* Now the '/' */
  *cur = '/';

  /* Adjust left and cur */
  ++cur;
  --left;
  if (left <= 0)
    goto err;

  /* Need minus? */
  if (saved_errno < 0) {
    *cur = '-';
    ++cur;
    --left;
    if (left <= 0)
      goto err;
  }

  /* Emit unsigned_errno */
  written = format_hex_number_sigsafe(unsigned_errno, cur, left);

  if (written <= 0)
    goto err;

  /* Adjust left and cur */
  left -= written;
  cur += written;

  /* Check that we have enough space left for a newline and a NUL */
  if (left <= 1)
    goto err;

  /* Emit the newline and NUL */
  *cur++ = '\n';
  *cur++ = '\0';

  res = (int)(cur - hex_errno - 1);

  goto done;

 err:
  /*
   * In error exit, just write a '\0' in the first char so whatever called
   * this at least won't fall off the end.
   */
  *hex_errno = '\0';

 done:
  return res;
}
#endif

/* Maximum number of file descriptors, if we cannot get it via sysconf() */
#define DEFAULT_MAX_FD 256

/** Terminate the process of <b>process_handle</b>.
 *  Code borrowed from Python's os.kill. */
int
tor_terminate_process(process_handle_t *process_handle)
{
#ifdef _WIN32
  if (tor_get_exit_code(process_handle, 0, NULL) == PROCESS_EXIT_RUNNING) {
    HANDLE handle = process_handle->pid.hProcess;

    if (!TerminateProcess(handle, 0))
      return -1;
    else
      return 0;
  }
#else /* Unix */
  if (process_handle->waitpid_cb) {
    /* We haven't got a waitpid yet, so we can just kill off the process. */
    return kill(process_handle->pid, SIGTERM);
  }
#endif

  return -1;
}

/** Return the Process ID of <b>process_handle</b>. */
int
tor_process_get_pid(process_handle_t *process_handle)
{
#ifdef _WIN32
  return (int) process_handle->pid.dwProcessId;
#else
  return (int) process_handle->pid;
#endif
}

#ifdef _WIN32
HANDLE
tor_process_get_stdout_pipe(process_handle_t *process_handle)
{
  return process_handle->stdout_pipe;
}
#else
/* DOCDOC tor_process_get_stdout_pipe */
FILE *
tor_process_get_stdout_pipe(process_handle_t *process_handle)
{
  return process_handle->stdout_handle;
}
#endif

/* DOCDOC process_handle_new */
static process_handle_t *
process_handle_new(void)
{
  process_handle_t *out = tor_malloc_zero(sizeof(process_handle_t));

#ifdef _WIN32
  out->stdout_pipe = INVALID_HANDLE_VALUE;
  out->stderr_pipe = INVALID_HANDLE_VALUE;
#else
  out->stdout_pipe = -1;
  out->stderr_pipe = -1;
#endif

  return out;
}

#ifndef _WIN32
/** Invoked when a process that we've launched via tor_spawn_background() has
 * been found to have terminated.
 */
static void
process_handle_waitpid_cb(int status, void *arg)
{
  process_handle_t *process_handle = arg;

  process_handle->waitpid_exit_status = status;
  clear_waitpid_callback(process_handle->waitpid_cb);
  if (process_handle->status == PROCESS_STATUS_RUNNING)
    process_handle->status = PROCESS_STATUS_NOTRUNNING;
  process_handle->waitpid_cb = 0;
}
#endif

/**
 * @name child-process states
 *
 * Each of these values represents a possible state that a child process can
 * be in.  They're used to determine what to say when telling the parent how
 * far along we were before failure.
 *
 * @{
 */
#define CHILD_STATE_INIT 0
#define CHILD_STATE_PIPE 1
#define CHILD_STATE_MAXFD 2
#define CHILD_STATE_FORK 3
#define CHILD_STATE_DUPOUT 4
#define CHILD_STATE_DUPERR 5
#define CHILD_STATE_REDIRECT 6
#define CHILD_STATE_CLOSEFD 7
#define CHILD_STATE_EXEC 8
#define CHILD_STATE_FAILEXEC 9
/** @} */
/** Start a program in the background. If <b>filename</b> contains a '/', then
 * it will be treated as an absolute or relative path.  Otherwise, on
 * non-Windows systems, the system path will be searched for <b>filename</b>.
 * On Windows, only the current directory will be searched. Here, to search the
 * system path (as well as the application directory, current working
 * directory, and system directories), set filename to NULL.
 *
 * The strings in <b>argv</b> will be passed as the command line arguments of
 * the child program (following convention, argv[0] should normally be the
 * filename of the executable, and this must be the case if <b>filename</b> is
 * NULL). The last element of argv must be NULL. A handle to the child process
 * will be returned in process_handle (which must be non-NULL). Read
 * process_handle.status to find out if the process was successfully launched.
 * For convenience, process_handle.status is returned by this function.
 *
 * Some parts of this code are based on the POSIX subprocess module from
 * Python, and example code from
 * http://msdn.microsoft.com/en-us/library/ms682499%28v=vs.85%29.aspx.
 */
int
tor_spawn_background(const char *const filename, const char **argv,
                     process_environment_t *env,
                     process_handle_t **process_handle_out)
{
#ifdef _WIN32
  HANDLE stdout_pipe_read = NULL;
  HANDLE stdout_pipe_write = NULL;
  HANDLE stderr_pipe_read = NULL;
  HANDLE stderr_pipe_write = NULL;
  process_handle_t *process_handle;
  int status;

  STARTUPINFOA siStartInfo;
  BOOL retval = FALSE;

  SECURITY_ATTRIBUTES saAttr;
  char *joined_argv;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  /* TODO: should we set explicit security attributes? (#2046, comment 5) */
  saAttr.lpSecurityDescriptor = NULL;

  /* Assume failure to start process */
  status = PROCESS_STATUS_ERROR;

  /* Set up pipe for stdout */
  if (!CreatePipe(&stdout_pipe_read, &stdout_pipe_write, &saAttr, 0)) {
    log_warn(LD_GENERAL,
      "Failed to create pipe for stdout communication with child process: %s",
      format_win32_error(GetLastError()));
    return status;
  }
  if (!SetHandleInformation(stdout_pipe_read, HANDLE_FLAG_INHERIT, 0)) {
    log_warn(LD_GENERAL,
      "Failed to configure pipe for stdout communication with child "
      "process: %s", format_win32_error(GetLastError()));
    return status;
  }

  /* Set up pipe for stderr */
  if (!CreatePipe(&stderr_pipe_read, &stderr_pipe_write, &saAttr, 0)) {
    log_warn(LD_GENERAL,
      "Failed to create pipe for stderr communication with child process: %s",
      format_win32_error(GetLastError()));
    return status;
  }
  if (!SetHandleInformation(stderr_pipe_read, HANDLE_FLAG_INHERIT, 0)) {
    log_warn(LD_GENERAL,
      "Failed to configure pipe for stderr communication with child "
      "process: %s", format_win32_error(GetLastError()));
    return status;
  }

  /* Create the child process */

  /* Windows expects argv to be a whitespace delimited string, so join argv up
   */
  joined_argv = tor_join_win_cmdline(argv);

  process_handle = process_handle_new();
  process_handle->status = status;

  ZeroMemory(&(process_handle->pid), sizeof(PROCESS_INFORMATION));
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.hStdError = stderr_pipe_write;
  siStartInfo.hStdOutput = stdout_pipe_write;
  siStartInfo.hStdInput = NULL;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  /* Create the child process */

  retval = CreateProcessA(filename,      // module name
                 joined_argv,   // command line
  /* TODO: should we set explicit security attributes? (#2046, comment 5) */
                 NULL,          // process security attributes
                 NULL,          // primary thread security attributes
                 TRUE,          // handles are inherited
  /*(TODO: set CREATE_NEW CONSOLE/PROCESS_GROUP to make GetExitCodeProcess()
   * work?) */
                 CREATE_NO_WINDOW,             // creation flags
                 (env==NULL) ? NULL : env->windows_environment_block,
                 NULL,          // use parent's current directory
                 &siStartInfo,  // STARTUPINFO pointer
                 &(process_handle->pid));  // receives PROCESS_INFORMATION

  tor_free(joined_argv);

  if (!retval) {
    log_warn(LD_GENERAL,
      "Failed to create child process %s: %s", filename?filename:argv[0],
      format_win32_error(GetLastError()));
    tor_free(process_handle);
  } else  {
    /* TODO: Close hProcess and hThread in process_handle->pid? */
    process_handle->stdout_pipe = stdout_pipe_read;
    process_handle->stderr_pipe = stderr_pipe_read;
    status = process_handle->status = PROCESS_STATUS_RUNNING;
  }

  /* TODO: Close pipes on exit */
  *process_handle_out = process_handle;
  return status;
#else // _WIN32
  pid_t pid;
  int stdout_pipe[2];
  int stderr_pipe[2];
  int fd, retval;
  ssize_t nbytes;
  process_handle_t *process_handle;
  int status;

  const char *error_message = SPAWN_ERROR_MESSAGE;
  size_t error_message_length;

  /* Represents where in the process of spawning the program is;
     this is used for printing out the error message */
  unsigned char child_state = CHILD_STATE_INIT;

  char hex_errno[HEX_ERRNO_SIZE + 2]; /* + 1 should be sufficient actually */

  static int max_fd = -1;

  status = PROCESS_STATUS_ERROR;

  /* We do the strlen here because strlen() is not signal handler safe,
     and we are not allowed to use unsafe functions between fork and exec */
  error_message_length = strlen(error_message);

  child_state = CHILD_STATE_PIPE;

  /* Set up pipe for redirecting stdout and stderr of child */
  retval = pipe(stdout_pipe);
  if (-1 == retval) {
    log_warn(LD_GENERAL,
      "Failed to set up pipe for stdout communication with child process: %s",
       strerror(errno));
    return status;
  }

  retval = pipe(stderr_pipe);
  if (-1 == retval) {
    log_warn(LD_GENERAL,
      "Failed to set up pipe for stderr communication with child process: %s",
      strerror(errno));

    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    return status;
  }

  child_state = CHILD_STATE_MAXFD;

#ifdef _SC_OPEN_MAX
  if (-1 == max_fd) {
    max_fd = (int) sysconf(_SC_OPEN_MAX);
    if (max_fd == -1) {
      max_fd = DEFAULT_MAX_FD;
      log_warn(LD_GENERAL,
               "Cannot find maximum file descriptor, assuming %d", max_fd);
    }
  }
#else
  max_fd = DEFAULT_MAX_FD;
#endif

  child_state = CHILD_STATE_FORK;

  pid = fork();
  if (0 == pid) {
    /* In child */

    child_state = CHILD_STATE_DUPOUT;

    /* Link child stdout to the write end of the pipe */
    retval = dup2(stdout_pipe[1], STDOUT_FILENO);
    if (-1 == retval)
        goto error;

    child_state = CHILD_STATE_DUPERR;

    /* Link child stderr to the write end of the pipe */
    retval = dup2(stderr_pipe[1], STDERR_FILENO);
    if (-1 == retval)
        goto error;

    child_state = CHILD_STATE_REDIRECT;

    /* Link stdin to /dev/null */
    fd = open("/dev/null", O_RDONLY); /* NOT cloexec, obviously. */
    if (fd != -1)
      dup2(fd, STDIN_FILENO);
    else
      goto error;

    child_state = CHILD_STATE_CLOSEFD;

    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(fd);

    /* Close all other fds, including the read end of the pipe */
    /* XXX: We should now be doing enough FD_CLOEXEC setting to make
     * this needless. */
    for (fd = STDERR_FILENO + 1; fd < max_fd; fd++) {
      close(fd);
    }

    child_state = CHILD_STATE_EXEC;

    /* Call the requested program. We need the cast because
       execvp doesn't define argv as const, even though it
       does not modify the arguments */
    if (env)
      execve(filename, (char *const *) argv, env->unixoid_environment_block);
    else
      execvp(filename, (char *const *) argv);

    /* If we got here, the exec or open(/dev/null) failed */

    child_state = CHILD_STATE_FAILEXEC;

  error:
    {
      /* XXX: are we leaking fds from the pipe? */
      int n;

      n = format_helper_exit_status(child_state, errno, hex_errno);

      if (n >= 0) {
        /* Write the error message. GCC requires that we check the return
           value, but there is nothing we can do if it fails */
        /* TODO: Don't use STDOUT, use a pipe set up just for this purpose */
        nbytes = write(STDOUT_FILENO, error_message, error_message_length);
        nbytes = write(STDOUT_FILENO, hex_errno, n);
      }
    }

    (void) nbytes;

    _exit(255);
    /* Never reached, but avoids compiler warning */
    return status;
  }

  /* In parent */

  if (-1 == pid) {
    log_warn(LD_GENERAL, "Failed to fork child process: %s", strerror(errno));
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    return status;
  }

  process_handle = process_handle_new();
  process_handle->status = status;
  process_handle->pid = pid;

  /* TODO: If the child process forked but failed to exec, waitpid it */

  /* Return read end of the pipes to caller, and close write end */
  process_handle->stdout_pipe = stdout_pipe[0];
  retval = close(stdout_pipe[1]);

  if (-1 == retval) {
    log_warn(LD_GENERAL,
            "Failed to close write end of stdout pipe in parent process: %s",
            strerror(errno));
  }

  process_handle->waitpid_cb = set_waitpid_callback(pid,
                                                    process_handle_waitpid_cb,
                                                    process_handle);

  process_handle->stderr_pipe = stderr_pipe[0];
  retval = close(stderr_pipe[1]);

  if (-1 == retval) {
    log_warn(LD_GENERAL,
            "Failed to close write end of stderr pipe in parent process: %s",
            strerror(errno));
  }

  status = process_handle->status = PROCESS_STATUS_RUNNING;
  /* Set stdout/stderr pipes to be non-blocking */
  fcntl(process_handle->stdout_pipe, F_SETFL, O_NONBLOCK);
  fcntl(process_handle->stderr_pipe, F_SETFL, O_NONBLOCK);
  /* Open the buffered IO streams */
  process_handle->stdout_handle = fdopen(process_handle->stdout_pipe, "r");
  process_handle->stderr_handle = fdopen(process_handle->stderr_pipe, "r");

  *process_handle_out = process_handle;
  return process_handle->status;
#endif // _WIN32
}

/** Destroy all resources allocated by the process handle in
 *  <b>process_handle</b>.
 *  If <b>also_terminate_process</b> is true, also terminate the
 *  process of the process handle. */
MOCK_IMPL(void,
tor_process_handle_destroy,(process_handle_t *process_handle,
                            int also_terminate_process))
{
  if (!process_handle)
    return;

  if (also_terminate_process) {
    if (tor_terminate_process(process_handle) < 0) {
      const char *errstr =
#ifdef _WIN32
        format_win32_error(GetLastError());
#else
        strerror(errno);
#endif
      log_notice(LD_GENERAL, "Failed to terminate process with "
                 "PID '%d' ('%s').", tor_process_get_pid(process_handle),
                 errstr);
    } else {
      log_info(LD_GENERAL, "Terminated process with PID '%d'.",
               tor_process_get_pid(process_handle));
    }
  }

  process_handle->status = PROCESS_STATUS_NOTRUNNING;

#ifdef _WIN32
  if (process_handle->stdout_pipe)
    CloseHandle(process_handle->stdout_pipe);

  if (process_handle->stderr_pipe)
    CloseHandle(process_handle->stderr_pipe);
#else
  if (process_handle->stdout_handle)
    fclose(process_handle->stdout_handle);

  if (process_handle->stderr_handle)
    fclose(process_handle->stderr_handle);

  clear_waitpid_callback(process_handle->waitpid_cb);
#endif

  memset(process_handle, 0x0f, sizeof(process_handle_t));
  tor_free(process_handle);
}

/** Get the exit code of a process specified by <b>process_handle</b> and store
 * it in <b>exit_code</b>, if set to a non-NULL value.  If <b>block</b> is set
 * to true, the call will block until the process has exited.  Otherwise if
 * the process is still running, the function will return
 * PROCESS_EXIT_RUNNING, and exit_code will be left unchanged. Returns
 * PROCESS_EXIT_EXITED if the process did exit. If there is a failure,
 * PROCESS_EXIT_ERROR will be returned and the contents of exit_code (if
 * non-NULL) will be undefined. N.B. Under *nix operating systems, this will
 * probably not work in Tor, because waitpid() is called in main.c to reap any
 * terminated child processes.*/
int
tor_get_exit_code(process_handle_t *process_handle,
                  int block, int *exit_code)
{
#ifdef _WIN32
  DWORD retval;
  BOOL success;

  if (block) {
    /* Wait for the process to exit */
    retval = WaitForSingleObject(process_handle->pid.hProcess, INFINITE);
    if (retval != WAIT_OBJECT_0) {
      log_warn(LD_GENERAL, "WaitForSingleObject() failed (%d): %s",
              (int)retval, format_win32_error(GetLastError()));
      return PROCESS_EXIT_ERROR;
    }
  } else {
    retval = WaitForSingleObject(process_handle->pid.hProcess, 0);
    if (WAIT_TIMEOUT == retval) {
      /* Process has not exited */
      return PROCESS_EXIT_RUNNING;
    } else if (retval != WAIT_OBJECT_0) {
      log_warn(LD_GENERAL, "WaitForSingleObject() failed (%d): %s",
               (int)retval, format_win32_error(GetLastError()));
      return PROCESS_EXIT_ERROR;
    }
  }

  if (exit_code != NULL) {
    success = GetExitCodeProcess(process_handle->pid.hProcess,
                                 (PDWORD)exit_code);
    if (!success) {
      log_warn(LD_GENERAL, "GetExitCodeProcess() failed: %s",
               format_win32_error(GetLastError()));
      return PROCESS_EXIT_ERROR;
    }
  }
#else
  int stat_loc;
  int retval;

  if (process_handle->waitpid_cb) {
    /* We haven't processed a SIGCHLD yet. */
    retval = waitpid(process_handle->pid, &stat_loc, block?0:WNOHANG);
    if (retval == process_handle->pid) {
      clear_waitpid_callback(process_handle->waitpid_cb);
      process_handle->waitpid_cb = NULL;
      process_handle->waitpid_exit_status = stat_loc;
    }
  } else {
    /* We already got a SIGCHLD for this process, and handled it. */
    retval = process_handle->pid;
    stat_loc = process_handle->waitpid_exit_status;
  }

  if (!block && 0 == retval) {
    /* Process has not exited */
    return PROCESS_EXIT_RUNNING;
  } else if (retval != process_handle->pid) {
    log_warn(LD_GENERAL, "waitpid() failed for PID %d: %s",
             process_handle->pid, strerror(errno));
    return PROCESS_EXIT_ERROR;
  }

  if (!WIFEXITED(stat_loc)) {
    log_warn(LD_GENERAL, "Process %d did not exit normally",
             process_handle->pid);
    return PROCESS_EXIT_ERROR;
  }

  if (exit_code != NULL)
    *exit_code = WEXITSTATUS(stat_loc);
#endif // _WIN32

  return PROCESS_EXIT_EXITED;
}

/** Helper: return the number of characters in <b>s</b> preceding the first
 * occurrence of <b>ch</b>. If <b>ch</b> does not occur in <b>s</b>, return
 * the length of <b>s</b>. Should be equivalent to strspn(s, "ch"). */
static INLINE size_t
str_num_before(const char *s, char ch)
{
  const char *cp = strchr(s, ch);
  if (cp)
    return cp - s;
  else
    return strlen(s);
}

/** Return non-zero iff getenv would consider <b>s1</b> and <b>s2</b>
 * to have the same name as strings in a process's environment. */
int
environment_variable_names_equal(const char *s1, const char *s2)
{
  size_t s1_name_len = str_num_before(s1, '=');
  size_t s2_name_len = str_num_before(s2, '=');

  return (s1_name_len == s2_name_len &&
          tor_memeq(s1, s2, s1_name_len));
}

/** Free <b>env</b> (assuming it was produced by
 * process_environment_make). */
void
process_environment_free(process_environment_t *env)
{
  if (env == NULL) return;

  /* As both an optimization hack to reduce consing on Unixoid systems
   * and a nice way to ensure that some otherwise-Windows-specific
   * code will always get tested before changes to it get merged, the
   * strings which env->unixoid_environment_block points to are packed
   * into env->windows_environment_block. */
  tor_free(env->unixoid_environment_block);
  tor_free(env->windows_environment_block);

  tor_free(env);
}

/** Make a process_environment_t containing the environment variables
 * specified in <b>env_vars</b> (as C strings of the form
 * "NAME=VALUE"). */
process_environment_t *
process_environment_make(struct smartlist_t *env_vars)
{
  process_environment_t *env = tor_malloc_zero(sizeof(process_environment_t));
  size_t n_env_vars = smartlist_len(env_vars);
  size_t i;
  size_t total_env_length;
  smartlist_t *env_vars_sorted;

  tor_assert(n_env_vars + 1 != 0);
  env->unixoid_environment_block = tor_calloc(n_env_vars + 1, sizeof(char *));
  /* env->unixoid_environment_block is already NULL-terminated,
   * because we assume that NULL == 0 (and check that during compilation). */

  total_env_length = 1; /* terminating NUL of terminating empty string */
  for (i = 0; i < n_env_vars; ++i) {
    const char *s = smartlist_get(env_vars, i);
    size_t slen = strlen(s);

    tor_assert(slen + 1 != 0);
    tor_assert(slen + 1 < SIZE_MAX - total_env_length);
    total_env_length += slen + 1;
  }

  env->windows_environment_block = tor_malloc_zero(total_env_length);
  /* env->windows_environment_block is already
   * (NUL-terminated-empty-string)-terminated. */

  /* Some versions of Windows supposedly require that environment
   * blocks be sorted.  Or maybe some Windows programs (or their
   * runtime libraries) fail to look up strings in non-sorted
   * environment blocks.
   *
   * Also, sorting strings makes it easy to find duplicate environment
   * variables and environment-variable strings without an '=' on all
   * OSes, and they can cause badness.  Let's complain about those. */
  env_vars_sorted = smartlist_new();
  smartlist_add_all(env_vars_sorted, env_vars);
  smartlist_sort_strings(env_vars_sorted);

  /* Now copy the strings into the environment blocks. */
  {
    char *cp = env->windows_environment_block;
    const char *prev_env_var = NULL;

    for (i = 0; i < n_env_vars; ++i) {
      const char *s = smartlist_get(env_vars_sorted, i);
      size_t slen = strlen(s);
      size_t s_name_len = str_num_before(s, '=');

      if (s_name_len == slen) {
        log_warn(LD_GENERAL,
                 "Preparing an environment containing a variable "
                 "without a value: %s",
                 s);
      }
      if (prev_env_var != NULL &&
          environment_variable_names_equal(s, prev_env_var)) {
        log_warn(LD_GENERAL,
                 "Preparing an environment containing two variables "
                 "with the same name: %s and %s",
                 prev_env_var, s);
      }

      prev_env_var = s;

      /* Actually copy the string into the environment. */
      memcpy(cp, s, slen+1);
      env->unixoid_environment_block[i] = cp;
      cp += slen+1;
    }

    tor_assert(cp == env->windows_environment_block + total_env_length - 1);
  }

  smartlist_free(env_vars_sorted);

  return env;
}

/** Return a newly allocated smartlist containing every variable in
 * this process's environment, as a NUL-terminated string of the form
 * "NAME=VALUE".  Note that on some/many/most/all OSes, the parent
 * process can put strings not of that form in our environment;
 * callers should try to not get crashed by that.
 *
 * The returned strings are heap-allocated, and must be freed by the
 * caller. */
struct smartlist_t *
get_current_process_environment_variables(void)
{
  smartlist_t *sl = smartlist_new();

  char **environ_tmp; /* Not const char ** ? Really? */
  for (environ_tmp = get_environment(); *environ_tmp; ++environ_tmp) {
    smartlist_add(sl, tor_strdup(*environ_tmp));
  }

  return sl;
}

/** For each string s in <b>env_vars</b> such that
 * environment_variable_names_equal(s, <b>new_var</b>), remove it; if
 * <b>free_p</b> is non-zero, call <b>free_old</b>(s).  If
 * <b>new_var</b> contains '=', insert it into <b>env_vars</b>. */
void
set_environment_variable_in_smartlist(struct smartlist_t *env_vars,
                                      const char *new_var,
                                      void (*free_old)(void*),
                                      int free_p)
{
  SMARTLIST_FOREACH_BEGIN(env_vars, const char *, s) {
    if (environment_variable_names_equal(s, new_var)) {
      SMARTLIST_DEL_CURRENT(env_vars, s);
      if (free_p) {
        free_old((void *)s);
      }
    }
  } SMARTLIST_FOREACH_END(s);

  if (strchr(new_var, '=') != NULL) {
    smartlist_add(env_vars, (void *)new_var);
  }
}

#ifdef _WIN32
/** Read from a handle <b>h</b> into <b>buf</b>, up to <b>count</b> bytes.  If
 * <b>hProcess</b> is NULL, the function will return immediately if there is
 * nothing more to read. Otherwise <b>hProcess</b> should be set to the handle
 * to the process owning the <b>h</b>. In this case, the function will exit
 * only once the process has exited, or <b>count</b> bytes are read. Returns
 * the number of bytes read, or -1 on error. */
ssize_t
tor_read_all_handle(HANDLE h, char *buf, size_t count,
                    const process_handle_t *process)
{
  size_t numread = 0;
  BOOL retval;
  DWORD byte_count;
  BOOL process_exited = FALSE;

  if (count > SIZE_T_CEILING || count > SSIZE_T_MAX)
    return -1;

  while (numread != count) {
    /* Check if there is anything to read */
    retval = PeekNamedPipe(h, NULL, 0, NULL, &byte_count, NULL);
    if (!retval) {
      log_warn(LD_GENERAL,
        "Failed to peek from handle: %s",
        format_win32_error(GetLastError()));
      return -1;
    } else if (0 == byte_count) {
      /* Nothing available: process exited or it is busy */

      /* Exit if we don't know whether the process is running */
      if (NULL == process)
        break;

      /* The process exited and there's nothing left to read from it */
      if (process_exited)
        break;

      /* If process is not running, check for output one more time in case
         it wrote something after the peek was performed. Otherwise keep on
         waiting for output */
      tor_assert(process != NULL);
      byte_count = WaitForSingleObject(process->pid.hProcess, 0);
      if (WAIT_TIMEOUT != byte_count)
        process_exited = TRUE;

      continue;
    }

    /* There is data to read; read it */
    retval = ReadFile(h, buf+numread, count-numread, &byte_count, NULL);
    tor_assert(byte_count + numread <= count);
    if (!retval) {
      log_warn(LD_GENERAL, "Failed to read from handle: %s",
        format_win32_error(GetLastError()));
      return -1;
    } else if (0 == byte_count) {
      /* End of file */
      break;
    }
    numread += byte_count;
  }
  return (ssize_t)numread;
}
#else
/** Read from a handle <b>h</b> into <b>buf</b>, up to <b>count</b> bytes.  If
 * <b>process</b> is NULL, the function will return immediately if there is
 * nothing more to read. Otherwise data will be read until end of file, or
 * <b>count</b> bytes are read.  Returns the number of bytes read, or -1 on
 * error. Sets <b>eof</b> to true if <b>eof</b> is not NULL and the end of the
 * file has been reached. */
ssize_t
tor_read_all_handle(FILE *h, char *buf, size_t count,
                    const process_handle_t *process,
                    int *eof)
{
  size_t numread = 0;
  char *retval;

  if (eof)
    *eof = 0;

  if (count > SIZE_T_CEILING || count > SSIZE_T_MAX)
    return -1;

  while (numread != count) {
    /* Use fgets because that is what we use in log_from_pipe() */
    retval = fgets(buf+numread, (int)(count-numread), h);
    if (NULL == retval) {
      if (feof(h)) {
        log_debug(LD_GENERAL, "fgets() reached end of file");
        if (eof)
          *eof = 1;
        break;
      } else {
        if (EAGAIN == errno) {
          if (process)
            continue;
          else
            break;
        } else {
          log_warn(LD_GENERAL, "fgets() from handle failed: %s",
                   strerror(errno));
          return -1;
        }
      }
    }
    tor_assert(retval != NULL);
    tor_assert(strlen(retval) + numread <= count);
    numread += strlen(retval);
  }

  log_debug(LD_GENERAL, "fgets() read %d bytes from handle", (int)numread);
  return (ssize_t)numread;
}
#endif

/** Read from stdout of a process until the process exits. */
ssize_t
tor_read_all_from_process_stdout(const process_handle_t *process_handle,
                                 char *buf, size_t count)
{
#ifdef _WIN32
  return tor_read_all_handle(process_handle->stdout_pipe, buf, count,
                             process_handle);
#else
  return tor_read_all_handle(process_handle->stdout_handle, buf, count,
                             process_handle, NULL);
#endif
}

/** Read from stdout of a process until the process exits. */
ssize_t
tor_read_all_from_process_stderr(const process_handle_t *process_handle,
                                 char *buf, size_t count)
{
#ifdef _WIN32
  return tor_read_all_handle(process_handle->stderr_pipe, buf, count,
                             process_handle);
#else
  return tor_read_all_handle(process_handle->stderr_handle, buf, count,
                             process_handle, NULL);
#endif
}

/** Split buf into lines, and add to smartlist. The buffer <b>buf</b> will be
 * modified. The resulting smartlist will consist of pointers to buf, so there
 * is no need to free the contents of sl. <b>buf</b> must be a NUL-terminated
 * string. <b>len</b> should be set to the length of the buffer excluding the
 * NUL. Non-printable characters (including NUL) will be replaced with "." */
int
tor_split_lines(smartlist_t *sl, char *buf, int len)
{
  /* Index in buf of the start of the current line */
  int start = 0;
  /* Index in buf of the current character being processed */
  int cur = 0;
  /* Are we currently in a line */
  char in_line = 0;

  /* Loop over string */
  while (cur < len) {
    /* Loop until end of line or end of string */
    for (; cur < len; cur++) {
      if (in_line) {
        if ('\r' == buf[cur] || '\n' == buf[cur]) {
          /* End of line */
          buf[cur] = '\0';
          /* Point cur to the next line */
          cur++;
          /* Line starts at start and ends with a nul */
          break;
        } else {
          if (!TOR_ISPRINT(buf[cur]))
            buf[cur] = '.';
        }
      } else {
        if ('\r' == buf[cur] || '\n' == buf[cur]) {
          /* Skip leading vertical space */
          ;
        } else {
          in_line = 1;
          start = cur;
          if (!TOR_ISPRINT(buf[cur]))
            buf[cur] = '.';
        }
      }
    }
    /* We are at the end of the line or end of string. If in_line is true there
     * is a line which starts at buf+start and ends at a NUL. cur points to
     * the character after the NUL. */
    if (in_line)
      smartlist_add(sl, (void *)(buf+start));
    in_line = 0;
  }
  return smartlist_len(sl);
}

/** Return a string corresponding to <b>stream_status</b>. */
const char *
stream_status_to_string(enum stream_status stream_status)
{
  switch (stream_status) {
    case IO_STREAM_OKAY:
      return "okay";
    case IO_STREAM_EAGAIN:
      return "temporarily unavailable";
    case IO_STREAM_TERM:
      return "terminated";
    case IO_STREAM_CLOSED:
      return "closed";
    default:
      tor_fragile_assert();
      return "unknown";
  }
}

/* DOCDOC */
static void
log_portfw_spawn_error_message(const char *buf,
                               const char *executable, int *child_status)
{
  /* Parse error message */
  int retval, child_state, saved_errno;
  retval = tor_sscanf(buf, SPAWN_ERROR_MESSAGE "%x/%x",
                      &child_state, &saved_errno);
  if (retval == 2) {
    log_warn(LD_GENERAL,
             "Failed to start child process \"%s\" in state %d: %s",
             executable, child_state, strerror(saved_errno));
    if (child_status)
      *child_status = 1;
  } else {
    /* Failed to parse message from child process, log it as a
       warning */
    log_warn(LD_GENERAL,
             "Unexpected message from port forwarding helper \"%s\": %s",
             executable, buf);
  }
}

#ifdef _WIN32

/** Return a smartlist containing lines outputted from
 *  <b>handle</b>. Return NULL on error, and set
 *  <b>stream_status_out</b> appropriately. */
MOCK_IMPL(smartlist_t *,
tor_get_lines_from_handle, (HANDLE *handle,
                            enum stream_status *stream_status_out))
{
  int pos;
  char stdout_buf[600] = {0};
  smartlist_t *lines = NULL;

  tor_assert(stream_status_out);

  *stream_status_out = IO_STREAM_TERM;

  pos = tor_read_all_handle(handle, stdout_buf, sizeof(stdout_buf) - 1, NULL);
  if (pos < 0) {
    *stream_status_out = IO_STREAM_TERM;
    return NULL;
  }
  if (pos == 0) {
    *stream_status_out = IO_STREAM_EAGAIN;
    return NULL;
  }

  /* End with a null even if there isn't a \r\n at the end */
  /* TODO: What if this is a partial line? */
  stdout_buf[pos] = '\0';

  /* Split up the buffer */
  lines = smartlist_new();
  tor_split_lines(lines, stdout_buf, pos);

  /* Currently 'lines' is populated with strings residing on the
     stack. Replace them with their exact copies on the heap: */
  SMARTLIST_FOREACH(lines, char *, line,
                    SMARTLIST_REPLACE_CURRENT(lines, line, tor_strdup(line)));

  *stream_status_out = IO_STREAM_OKAY;

  return lines;
}

/** Read from stream, and send lines to log at the specified log level.
 * Returns -1 if there is a error reading, and 0 otherwise.
 * If the generated stream is flushed more often than on new lines, or
 * a read exceeds 256 bytes, lines will be truncated. This should be fixed,
 * along with the corresponding problem on *nix (see bug #2045).
 */
static int
log_from_handle(HANDLE *pipe, int severity)
{
  char buf[256];
  int pos;
  smartlist_t *lines;

  pos = tor_read_all_handle(pipe, buf, sizeof(buf) - 1, NULL);
  if (pos < 0) {
    /* Error */
    log_warn(LD_GENERAL, "Failed to read data from subprocess");
    return -1;
  }

  if (0 == pos) {
    /* There's nothing to read (process is busy or has exited) */
    log_debug(LD_GENERAL, "Subprocess had nothing to say");
    return 0;
  }

  /* End with a null even if there isn't a \r\n at the end */
  /* TODO: What if this is a partial line? */
  buf[pos] = '\0';
  log_debug(LD_GENERAL, "Subprocess had %d bytes to say", pos);

  /* Split up the buffer */
  lines = smartlist_new();
  tor_split_lines(lines, buf, pos);

  /* Log each line */
  SMARTLIST_FOREACH(lines, char *, line,
  {
    log_fn(severity, LD_GENERAL, "Port forwarding helper says: %s", line);
  });
  smartlist_free(lines);

  return 0;
}

#else

/** Return a smartlist containing lines outputted from
 *  <b>handle</b>. Return NULL on error, and set
 *  <b>stream_status_out</b> appropriately. */
MOCK_IMPL(smartlist_t *,
tor_get_lines_from_handle, (FILE *handle,
                            enum stream_status *stream_status_out))
{
  enum stream_status stream_status;
  char stdout_buf[400];
  smartlist_t *lines = NULL;

  while (1) {
    memset(stdout_buf, 0, sizeof(stdout_buf));

    stream_status = get_string_from_pipe(handle,
                                         stdout_buf, sizeof(stdout_buf) - 1);
    if (stream_status != IO_STREAM_OKAY)
      goto done;

    if (!lines) lines = smartlist_new();
    smartlist_add(lines, tor_strdup(stdout_buf));
  }

 done:
  *stream_status_out = stream_status;
  return lines;
}

/** Read from stream, and send lines to log at the specified log level.
 * Returns 1 if stream is closed normally, -1 if there is a error reading, and
 * 0 otherwise. Handles lines from tor-fw-helper and
 * tor_spawn_background() specially.
 */
static int
log_from_pipe(FILE *stream, int severity, const char *executable,
              int *child_status)
{
  char buf[256];
  enum stream_status r;

  for (;;) {
    r = get_string_from_pipe(stream, buf, sizeof(buf) - 1);

    if (r == IO_STREAM_CLOSED) {
      return 1;
    } else if (r == IO_STREAM_EAGAIN) {
      return 0;
    } else if (r == IO_STREAM_TERM) {
      return -1;
    }

    tor_assert(r == IO_STREAM_OKAY);

    /* Check if buf starts with SPAWN_ERROR_MESSAGE */
    if (strcmpstart(buf, SPAWN_ERROR_MESSAGE) == 0) {
      log_portfw_spawn_error_message(buf, executable, child_status);
    } else {
      log_fn(severity, LD_GENERAL, "Port forwarding helper says: %s", buf);
    }
  }

  /* We should never get here */
  return -1;
}
#endif

/** Reads from <b>stream</b> and stores input in <b>buf_out</b> making
 *  sure it's below <b>count</b> bytes.
 *  If the string has a trailing newline, we strip it off.
 *
 * This function is specifically created to handle input from managed
 * proxies, according to the pluggable transports spec. Make sure it
 * fits your needs before using it.
 *
 * Returns:
 * IO_STREAM_CLOSED: If the stream is closed.
 * IO_STREAM_EAGAIN: If there is nothing to read and we should check back
 *  later.
 * IO_STREAM_TERM: If something is wrong with the stream.
 * IO_STREAM_OKAY: If everything went okay and we got a string
 *  in <b>buf_out</b>. */
enum stream_status
get_string_from_pipe(FILE *stream, char *buf_out, size_t count)
{
  char *retval;
  size_t len;

  tor_assert(count <= INT_MAX);

  retval = fgets(buf_out, (int)count, stream);

  if (!retval) {
    if (feof(stream)) {
      /* Program has closed stream (probably it exited) */
      /* TODO: check error */
      return IO_STREAM_CLOSED;
    } else {
      if (EAGAIN == errno) {
        /* Nothing more to read, try again next time */
        return IO_STREAM_EAGAIN;
      } else {
        /* There was a problem, abandon this child process */
        return IO_STREAM_TERM;
      }
    }
  } else {
    len = strlen(buf_out);
    if (len == 0) {
      /* this probably means we got a NUL at the start of the string. */
      return IO_STREAM_EAGAIN;
    }

    if (buf_out[len - 1] == '\n') {
      /* Remove the trailing newline */
      buf_out[len - 1] = '\0';
    } else {
      /* No newline; check whether we overflowed the buffer */
      if (!feof(stream))
        log_info(LD_GENERAL,
                 "Line from stream was truncated: %s", buf_out);
      /* TODO: What to do with this error? */
    }

    return IO_STREAM_OKAY;
  }

  /* We should never get here */
  return IO_STREAM_TERM;
}

/** Parse a <b>line</b> from tor-fw-helper and issue an appropriate
 *  log message to our user. */
static void
handle_fw_helper_line(const char *executable, const char *line)
{
  smartlist_t *tokens = smartlist_new();
  char *message = NULL;
  char *message_for_log = NULL;
  const char *external_port = NULL;
  const char *internal_port = NULL;
  const char *result = NULL;
  int port = 0;
  int success = 0;

  if (strcmpstart(line, SPAWN_ERROR_MESSAGE) == 0) {
    /* We need to check for SPAWN_ERROR_MESSAGE again here, since it's
     * possible that it got sent after we tried to read it in log_from_pipe.
     *
     * XXX Ideally, we should be using one of stdout/stderr for the real
     * output, and one for the output of the startup code.  We used to do that
     * before cd05f35d2c.
     */
    int child_status;
    log_portfw_spawn_error_message(line, executable, &child_status);
    goto done;
  }

  smartlist_split_string(tokens, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  if (smartlist_len(tokens) < 5)
    goto err;

  if (strcmp(smartlist_get(tokens, 0), "tor-fw-helper") ||
      strcmp(smartlist_get(tokens, 1), "tcp-forward"))
    goto err;

  external_port = smartlist_get(tokens, 2);
  internal_port = smartlist_get(tokens, 3);
  result = smartlist_get(tokens, 4);

  if (smartlist_len(tokens) > 5) {
    /* If there are more than 5 tokens, they are part of [<message>].
       Let's use a second smartlist to form the whole message;
       strncat loops suck. */
    int i;
    int message_words_n = smartlist_len(tokens) - 5;
    smartlist_t *message_sl = smartlist_new();
    for (i = 0; i < message_words_n; i++)
      smartlist_add(message_sl, smartlist_get(tokens, 5+i));

    tor_assert(smartlist_len(message_sl) > 0);
    message = smartlist_join_strings(message_sl, " ", 0, NULL);

    /* wrap the message in log-friendly wrapping */
    tor_asprintf(&message_for_log, " ('%s')", message);

    smartlist_free(message_sl);
  }

  port = atoi(external_port);
  if (port < 1 || port > 65535)
    goto err;

  port = atoi(internal_port);
  if (port < 1 || port > 65535)
    goto err;

  if (!strcmp(result, "SUCCESS"))
    success = 1;
  else if (!strcmp(result, "FAIL"))
    success = 0;
  else
    goto err;

  if (!success) {
    log_warn(LD_GENERAL, "Tor was unable to forward TCP port '%s' to '%s'%s. "
             "Please make sure that your router supports port "
             "forwarding protocols (like NAT-PMP). Note that if '%s' is "
             "your ORPort, your relay will be unable to receive inbound "
             "traffic.", external_port, internal_port,
             message_for_log ? message_for_log : "",
             internal_port);
  } else {
    log_info(LD_GENERAL,
             "Tor successfully forwarded TCP port '%s' to '%s'%s.",
             external_port, internal_port,
             message_for_log ? message_for_log : "");
  }

  goto done;

 err:
  log_warn(LD_GENERAL, "tor-fw-helper sent us a string we could not "
           "parse (%s).", line);

 done:
  SMARTLIST_FOREACH(tokens, char *, cp, tor_free(cp));
  smartlist_free(tokens);
  tor_free(message);
  tor_free(message_for_log);
}

/** Read what tor-fw-helper has to say in its stdout and handle it
 *  appropriately */
static int
handle_fw_helper_output(const char *executable,
                        process_handle_t *process_handle)
{
  smartlist_t *fw_helper_output = NULL;
  enum stream_status stream_status = 0;

  fw_helper_output =
    tor_get_lines_from_handle(tor_process_get_stdout_pipe(process_handle),
                              &stream_status);
  if (!fw_helper_output) { /* didn't get any output from tor-fw-helper */
    /* if EAGAIN we should retry in the future */
    return (stream_status == IO_STREAM_EAGAIN) ? 0 : -1;
  }

  /* Handle the lines we got: */
  SMARTLIST_FOREACH_BEGIN(fw_helper_output, char *, line) {
    handle_fw_helper_line(executable, line);
    tor_free(line);
  } SMARTLIST_FOREACH_END(line);

  smartlist_free(fw_helper_output);

  return 0;
}

/** Spawn tor-fw-helper and ask it to forward the ports in
 *  <b>ports_to_forward</b>. <b>ports_to_forward</b> contains strings
 *  of the form "<external port>:<internal port>", which is the format
 *  that tor-fw-helper expects. */
void
tor_check_port_forwarding(const char *filename,
                          smartlist_t *ports_to_forward,
                          time_t now)
{
/* When fw-helper succeeds, how long do we wait until running it again */
#define TIME_TO_EXEC_FWHELPER_SUCCESS 300
/* When fw-helper failed to start, how long do we wait until running it again
 */
#define TIME_TO_EXEC_FWHELPER_FAIL 60

  /* Static variables are initialized to zero, so child_handle.status=0
   * which corresponds to it not running on startup */
  static process_handle_t *child_handle=NULL;

  static time_t time_to_run_helper = 0;
  int stderr_status, retval;
  int stdout_status = 0;

  tor_assert(filename);

  /* Start the child, if it is not already running */
  if ((!child_handle || child_handle->status != PROCESS_STATUS_RUNNING) &&
      time_to_run_helper < now) {
    /*tor-fw-helper cli looks like this: tor_fw_helper -p :5555 -p 4555:1111 */
    const char **argv; /* cli arguments */
    int args_n, status;
    int argv_index = 0; /* index inside 'argv' */

    tor_assert(smartlist_len(ports_to_forward) > 0);

    /* check for overflow during 'argv' allocation:
       (len(ports_to_forward)*2 + 2)*sizeof(char*) > SIZE_MAX ==
       len(ports_to_forward) > (((SIZE_MAX/sizeof(char*)) - 2)/2) */
    if ((size_t) smartlist_len(ports_to_forward) >
        (((SIZE_MAX/sizeof(char*)) - 2)/2)) {
      log_warn(LD_GENERAL,
               "Overflow during argv allocation. This shouldn't happen.");
      return;
    }
    /* check for overflow during 'argv_index' increase:
       ((len(ports_to_forward)*2 + 2) > INT_MAX) ==
       len(ports_to_forward) > (INT_MAX - 2)/2 */
    if (smartlist_len(ports_to_forward) > (INT_MAX - 2)/2) {
      log_warn(LD_GENERAL,
               "Overflow during argv_index increase. This shouldn't happen.");
      return;
    }

    /* Calculate number of cli arguments: one for the filename, two
       for each smartlist element (one for "-p" and one for the
       ports), and one for the final NULL. */
    args_n = 1 + 2*smartlist_len(ports_to_forward) + 1;
    argv = tor_malloc_zero(sizeof(char*)*args_n);

    argv[argv_index++] = filename;
    SMARTLIST_FOREACH_BEGIN(ports_to_forward, const char *, port) {
      argv[argv_index++] = "-p";
      argv[argv_index++] = port;
    } SMARTLIST_FOREACH_END(port);
    argv[argv_index] = NULL;

    /* Assume tor-fw-helper will succeed, start it later*/
    time_to_run_helper = now + TIME_TO_EXEC_FWHELPER_SUCCESS;

    if (child_handle) {
      tor_process_handle_destroy(child_handle, 1);
      child_handle = NULL;
    }

#ifdef _WIN32
    /* Passing NULL as lpApplicationName makes Windows search for the .exe */
    status = tor_spawn_background(NULL, argv, NULL, &child_handle);
#else
    status = tor_spawn_background(filename, argv, NULL, &child_handle);
#endif

    tor_free_((void*)argv);
    argv=NULL;

    if (PROCESS_STATUS_ERROR == status) {
      log_warn(LD_GENERAL, "Failed to start port forwarding helper %s",
              filename);
      time_to_run_helper = now + TIME_TO_EXEC_FWHELPER_FAIL;
      return;
    }

    log_info(LD_GENERAL,
             "Started port forwarding helper (%s) with pid '%d'",
             filename, tor_process_get_pid(child_handle));
  }

  /* If child is running, read from its stdout and stderr) */
  if (child_handle && PROCESS_STATUS_RUNNING == child_handle->status) {
    /* Read from stdout/stderr and log result */
    retval = 0;
#ifdef _WIN32
    stderr_status = log_from_handle(child_handle->stderr_pipe, LOG_INFO);
#else
    stderr_status = log_from_pipe(child_handle->stderr_handle,
                                  LOG_INFO, filename, &retval);
#endif
    if (handle_fw_helper_output(filename, child_handle) < 0) {
      log_warn(LD_GENERAL, "Failed to handle fw helper output.");
      stdout_status = -1;
      retval = -1;
    }

    if (retval) {
      /* There was a problem in the child process */
      time_to_run_helper = now + TIME_TO_EXEC_FWHELPER_FAIL;
    }

    /* Combine the two statuses in order of severity */
    if (-1 == stdout_status || -1 == stderr_status)
      /* There was a failure */
      retval = -1;
#ifdef _WIN32
    else if (!child_handle || tor_get_exit_code(child_handle, 0, NULL) !=
             PROCESS_EXIT_RUNNING) {
      /* process has exited or there was an error */
      /* TODO: Do something with the process return value */
      /* TODO: What if the process output something since
       * between log_from_handle and tor_get_exit_code? */
      retval = 1;
    }
#else
    else if (1 == stdout_status || 1 == stderr_status)
      /* stdout or stderr was closed, the process probably
       * exited. It will be reaped by waitpid() in main.c */
      /* TODO: Do something with the process return value */
      retval = 1;
#endif
    else
      /* Both are fine */
      retval = 0;

    /* If either pipe indicates a failure, act on it */
    if (0 != retval) {
      if (1 == retval) {
        log_info(LD_GENERAL, "Port forwarding helper terminated");
        child_handle->status = PROCESS_STATUS_NOTRUNNING;
      } else {
        log_warn(LD_GENERAL, "Failed to read from port forwarding helper");
        child_handle->status = PROCESS_STATUS_ERROR;
      }

      /* TODO: The child might not actually be finished (maybe it failed or
         closed stdout/stderr), so maybe we shouldn't start another? */
    }
  }
}

/** Initialize the insecure RNG <b>rng</b> from a seed value <b>seed</b>. */
void
tor_init_weak_random(tor_weak_rng_t *rng, unsigned seed)
{
  rng->state = (uint32_t)(seed & 0x7fffffff);
}

/** Return a randomly chosen value in the range 0..TOR_WEAK_RANDOM_MAX based
 * on the RNG state of <b>rng</b>.  This entropy will not be cryptographically
 * strong; do not rely on it for anything an adversary should not be able to
 * predict. */
int32_t
tor_weak_random(tor_weak_rng_t *rng)
{
  /* Here's a linear congruential generator. OpenBSD and glibc use these
   * parameters; they aren't too bad, and should have maximal period over the
   * range 0..INT32_MAX. We don't want to use the platform rand() or random(),
   * since some platforms have bad weak RNGs that only return values in the
   * range 0..INT16_MAX, which just isn't enough. */
  rng->state = (rng->state * 1103515245 + 12345) & 0x7fffffff;
  return (int32_t) rng->state;
}

/** Return a random number in the range [0 , <b>top</b>). {That is, the range
 * of integers i such that 0 <= i < top.}  Chooses uniformly.  Requires that
 * top is greater than 0. This randomness is not cryptographically strong; do
 * not rely on it for anything an adversary should not be able to predict. */
int32_t
tor_weak_random_range(tor_weak_rng_t *rng, int32_t top)
{
  /* We don't want to just do tor_weak_random() % top, since random() is often
   * implemented with an LCG whose modulus is a power of 2, and those are
   * cyclic in their low-order bits. */
  int divisor, result;
  tor_assert(top > 0);
  divisor = TOR_WEAK_RANDOM_MAX / top;
  do {
    result = (int32_t)(tor_weak_random(rng) / divisor);
  } while (result >= top);
  return result;
}


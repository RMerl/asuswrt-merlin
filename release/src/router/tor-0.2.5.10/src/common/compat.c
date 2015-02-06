/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat.c
 * \brief Wrappers to make calls more portable.  This code defines
 * functions such as tor_malloc, tor_snprintf, get/set various data types,
 * renaming, setting socket options, switching user IDs.  It is basically
 * where the non-portable items are conditionally included depending on
 * the platform.
 **/

/* This is required on rh7 to make strptime not complain.
 * We also need it to make memmem get defined (where available)
 */
/* XXXX024 We should just  use AC_USE_SYSTEM_EXTENSIONS in our autoconf,
 * and get this (and other important stuff!) automatically. Once we do that,
 * make sure to also change the extern char **environ detection in
 * configure.ac, because whether that is declared or not depends on whether
 * we have _GNU_SOURCE defined! Maybe that means that once we take this out,
 * we can also take out the configure check. */
#define _GNU_SOURCE

#define COMPAT_PRIVATE
#include "compat.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#include <sys/locking.h>
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif

#ifndef HAVE_GETTIMEOFDAY
#ifdef HAVE_FTIME
#include <sys/timeb.h>
#endif
#endif

/* Includes for the process attaching prevention */
#if defined(HAVE_SYS_PRCTL_H) && defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/ptrace.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h> /* FreeBSD needs this to know what version it is */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if defined(HAVE_SYS_PRCTL_H) && defined(__linux__)
/* Only use the linux prctl;  the IRIX prctl is totally different */
#include <sys/prctl.h>
#endif
#ifdef TOR_UNIT_TESTS
#if !defined(HAVE_USLEEP) && defined(HAVE_SYS_SELECT_H)
/* as fallback implementation for tor_sleep_msec */
#include <sys/select.h>
#endif
#endif

#include "torlog.h"
#include "util.h"
#include "container.h"
#include "address.h"
#include "sandbox.h"

/* Inline the strl functions if the platform doesn't have them. */
#ifndef HAVE_STRLCPY
#include "strlcpy.c"
#endif
#ifndef HAVE_STRLCAT
#include "strlcat.c"
#endif

/** As open(path, flags, mode), but return an fd with the close-on-exec mode
 * set. */
int
tor_open_cloexec(const char *path, int flags, unsigned mode)
{
  int fd;
#ifdef O_CLOEXEC
  path = sandbox_intern_string(path);
  fd = open(path, flags|O_CLOEXEC, mode);
  if (fd >= 0)
    return fd;
  /* If we got an error, see if it is EINVAL. EINVAL might indicate that,
   * even though we were built on a system with O_CLOEXEC support, we
   * are running on one without. */
  if (errno != EINVAL)
    return -1;
#endif

  log_debug(LD_FS, "Opening %s with flags %x", path, flags);
  fd = open(path, flags, mode);
#ifdef FD_CLOEXEC
  if (fd >= 0) {
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
      log_warn(LD_FS,"Couldn't set FD_CLOEXEC: %s", strerror(errno));
      close(fd);
      return -1;
    }
  }
#endif
  return fd;
}

/** As fopen(path,mode), but ensures that the O_CLOEXEC bit is set on the
 * underlying file handle. */
FILE *
tor_fopen_cloexec(const char *path, const char *mode)
{
  FILE *result = fopen(path, mode);
#ifdef FD_CLOEXEC
  if (result != NULL) {
    if (fcntl(fileno(result), F_SETFD, FD_CLOEXEC) == -1) {
      log_warn(LD_FS,"Couldn't set FD_CLOEXEC: %s", strerror(errno));
      fclose(result);
      return NULL;
    }
  }
#endif
  return result;
}

/** As rename(), but work correctly with the sandbox. */
int
tor_rename(const char *path_old, const char *path_new)
{
  log_debug(LD_FS, "Renaming %s to %s", path_old, path_new);
  return rename(sandbox_intern_string(path_old),
                sandbox_intern_string(path_new));
}

#if defined(HAVE_SYS_MMAN_H) || defined(RUNNING_DOXYGEN)
/** Try to create a memory mapping for <b>filename</b> and return it.  On
 * failure, return NULL.  Sets errno properly, using ERANGE to mean
 * "empty file". */
tor_mmap_t *
tor_mmap_file(const char *filename)
{
  int fd; /* router file */
  char *string;
  int page_size, result;
  tor_mmap_t *res;
  size_t size, filesize;
  struct stat st;

  tor_assert(filename);

  fd = tor_open_cloexec(filename, O_RDONLY, 0);
  if (fd<0) {
    int save_errno = errno;
    int severity = (errno == ENOENT) ? LOG_INFO : LOG_WARN;
    log_fn(severity, LD_FS,"Could not open \"%s\" for mmap(): %s",filename,
           strerror(errno));
    errno = save_errno;
    return NULL;
  }

  /* Get the size of the file */
  result = fstat(fd, &st);
  if (result != 0) {
    int save_errno = errno;
    log_warn(LD_FS,
             "Couldn't fstat opened descriptor for \"%s\" during mmap: %s",
             filename, strerror(errno));
    close(fd);
    errno = save_errno;
    return NULL;
  }
  size = filesize = (size_t)(st.st_size);
  /*
   * Should we check for weird crap like mmapping a named pipe here,
   * or just wait for if (!size) below to fail?
   */
  /* ensure page alignment */
  page_size = getpagesize();
  size += (size%page_size) ? page_size-(size%page_size) : 0;

  if (!size) {
    /* Zero-length file. If we call mmap on it, it will succeed but
     * return NULL, and bad things will happen. So just fail. */
    log_info(LD_FS,"File \"%s\" is empty. Ignoring.",filename);
    errno = ERANGE;
    close(fd);
    return NULL;
  }

  string = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (string == MAP_FAILED) {
    int save_errno = errno;
    log_warn(LD_FS,"Could not mmap file \"%s\": %s", filename,
             strerror(errno));
    errno = save_errno;
    return NULL;
  }

  res = tor_malloc_zero(sizeof(tor_mmap_t));
  res->data = string;
  res->size = filesize;
  res->mapping_size = size;

  return res;
}
/** Release storage held for a memory mapping; returns 0 on success,
 * or -1 on failure (and logs a warning). */
int
tor_munmap_file(tor_mmap_t *handle)
{
  int res;

  if (handle == NULL)
    return 0;

  res = munmap((char*)handle->data, handle->mapping_size);
  if (res == 0) {
    /* munmap() succeeded */
    tor_free(handle);
  } else {
    log_warn(LD_FS, "Failed to munmap() in tor_munmap_file(): %s",
             strerror(errno));
    res = -1;
  }

  return res;
}
#elif defined(_WIN32)
tor_mmap_t *
tor_mmap_file(const char *filename)
{
  TCHAR tfilename[MAX_PATH]= {0};
  tor_mmap_t *res = tor_malloc_zero(sizeof(tor_mmap_t));
  int empty = 0;
  HANDLE file_handle = INVALID_HANDLE_VALUE;
  DWORD size_low, size_high;
  uint64_t real_size;
  res->mmap_handle = NULL;
#ifdef UNICODE
  mbstowcs(tfilename,filename,MAX_PATH);
#else
  strlcpy(tfilename,filename,MAX_PATH);
#endif
  file_handle = CreateFile(tfilename,
                           GENERIC_READ, FILE_SHARE_READ,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           0);

  if (file_handle == INVALID_HANDLE_VALUE)
    goto win_err;

  size_low = GetFileSize(file_handle, &size_high);

  if (size_low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
    log_warn(LD_FS,"Error getting size of \"%s\".",filename);
    goto win_err;
  }
  if (size_low == 0 && size_high == 0) {
    log_info(LD_FS,"File \"%s\" is empty. Ignoring.",filename);
    empty = 1;
    goto err;
  }
  real_size = (((uint64_t)size_high)<<32) | size_low;
  if (real_size > SIZE_MAX) {
    log_warn(LD_FS,"File \"%s\" is too big to map; not trying.",filename);
    goto err;
  }
  res->size = real_size;

  res->mmap_handle = CreateFileMapping(file_handle,
                                       NULL,
                                       PAGE_READONLY,
                                       size_high,
                                       size_low,
                                       NULL);
  if (res->mmap_handle == NULL)
    goto win_err;
  res->data = (char*) MapViewOfFile(res->mmap_handle,
                                    FILE_MAP_READ,
                                    0, 0, 0);
  if (!res->data)
    goto win_err;

  CloseHandle(file_handle);
  return res;
 win_err: {
    DWORD e = GetLastError();
    int severity = (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) ?
      LOG_INFO : LOG_WARN;
    char *msg = format_win32_error(e);
    log_fn(severity, LD_FS, "Couldn't mmap file \"%s\": %s", filename, msg);
    tor_free(msg);
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)
      errno = ENOENT;
    else
      errno = EINVAL;
  }
 err:
  if (empty)
    errno = ERANGE;
  if (file_handle != INVALID_HANDLE_VALUE)
    CloseHandle(file_handle);
  tor_munmap_file(res);
  return NULL;
}

/* Unmap the file, and return 0 for success or -1 for failure */
int
tor_munmap_file(tor_mmap_t *handle)
{
  if (handle == NULL)
    return 0;

  if (handle->data) {
    /* This is an ugly cast, but without it, "data" in struct tor_mmap_t would
       have to be redefined as non-const. */
    BOOL ok = UnmapViewOfFile( (LPVOID) handle->data);
    if (!ok) {
      log_warn(LD_FS, "Failed to UnmapViewOfFile() in tor_munmap_file(): %d",
               (int)GetLastError());
    }
  }

  if (handle->mmap_handle != NULL)
    CloseHandle(handle->mmap_handle);
  tor_free(handle);

  return 0;
}
#else
tor_mmap_t *
tor_mmap_file(const char *filename)
{
  struct stat st;
  char *res = read_file_to_str(filename, RFTS_BIN|RFTS_IGNORE_MISSING, &st);
  tor_mmap_t *handle;
  if (! res)
    return NULL;
  handle = tor_malloc_zero(sizeof(tor_mmap_t));
  handle->data = res;
  handle->size = st.st_size;
  return handle;
}

/** Unmap the file mapped with tor_mmap_file(), and return 0 for success
 * or -1 for failure.
 */

int
tor_munmap_file(tor_mmap_t *handle)
{
  char *d = NULL;
  if (handle == NULL)
    return 0;

  d = (char*)handle->data;
  tor_free(d);
  memwipe(handle, 0, sizeof(tor_mmap_t));
  tor_free(handle);

  /* Can't fail in this mmap()/munmap()-free case */
  return 0;
}
#endif

/** Replacement for snprintf.  Differs from platform snprintf in two
 * ways: First, always NUL-terminates its output.  Second, always
 * returns -1 if the result is truncated.  (Note that this return
 * behavior does <i>not</i> conform to C99; it just happens to be
 * easier to emulate "return -1" with conformant implementations than
 * it is to emulate "return number that would be written" with
 * non-conformant implementations.) */
int
tor_snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  int r;
  va_start(ap,format);
  r = tor_vsnprintf(str,size,format,ap);
  va_end(ap);
  return r;
}

/** Replacement for vsnprintf; behavior differs as tor_snprintf differs from
 * snprintf.
 */
int
tor_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
  int r;
  if (size == 0)
    return -1; /* no place for the NUL */
  if (size > SIZE_T_CEILING)
    return -1;
#ifdef _WIN32
  r = _vsnprintf(str, size, format, args);
#else
  r = vsnprintf(str, size, format, args);
#endif
  str[size-1] = '\0';
  if (r < 0 || r >= (ssize_t)size)
    return -1;
  return r;
}

/**
 * Portable asprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Sets *<b>strp</b> to this string, and returns its length (not
 * including the terminating NUL character).
 *
 * You can treat this function as if its implementation were something like
   <pre>
     char buf[_INFINITY_];
     tor_snprintf(buf, sizeof(buf), fmt, args);
     *strp = tor_strdup(buf);
     return strlen(*strp):
   </pre>
 * Where _INFINITY_ is an imaginary constant so big that any string can fit
 * into it.
 */
int
tor_asprintf(char **strp, const char *fmt, ...)
{
  int r;
  va_list args;
  va_start(args, fmt);
  r = tor_vasprintf(strp, fmt, args);
  va_end(args);
  if (!*strp || r < 0) {
    log_err(LD_BUG, "Internal error in asprintf");
    tor_assert(0);
  }
  return r;
}

/**
 * Portable vasprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Differs from regular vasprintf in the same ways that
 * tor_asprintf() differs from regular asprintf.
 */
int
tor_vasprintf(char **strp, const char *fmt, va_list args)
{
  /* use a temporary variable in case *strp is in args. */
  char *strp_tmp=NULL;
#ifdef HAVE_VASPRINTF
  /* If the platform gives us one, use it. */
  int r = vasprintf(&strp_tmp, fmt, args);
  if (r < 0)
    *strp = NULL;
  else
    *strp = strp_tmp;
  return r;
#elif defined(HAVE__VSCPRINTF)
  /* On Windows, _vsnprintf won't tell us the length of the string if it
   * overflows, so we need to use _vcsprintf to tell how much to allocate */
  int len, r;
  len = _vscprintf(fmt, args);
  if (len < 0) {
    *strp = NULL;
    return -1;
  }
  strp_tmp = tor_malloc(len + 1);
  r = _vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    tor_free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#else
  /* Everywhere else, we have a decent vsnprintf that tells us how many
   * characters we need.  We give it a try on a short buffer first, since
   * it might be nice to avoid the second vsnprintf call.
   */
  char buf[128];
  int len, r;
  va_list tmp_args;
  va_copy(tmp_args, args);
  len = vsnprintf(buf, sizeof(buf), fmt, tmp_args);
  va_end(tmp_args);
  if (len < (int)sizeof(buf)) {
    *strp = tor_strdup(buf);
    return len;
  }
  strp_tmp = tor_malloc(len+1);
  r = vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    tor_free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#endif
}

/** Given <b>hlen</b> bytes at <b>haystack</b> and <b>nlen</b> bytes at
 * <b>needle</b>, return a pointer to the first occurrence of the needle
 * within the haystack, or NULL if there is no such occurrence.
 *
 * This function is <em>not</em> timing-safe.
 *
 * Requires that <b>nlen</b> be greater than zero.
 */
const void *
tor_memmem(const void *_haystack, size_t hlen,
           const void *_needle, size_t nlen)
{
#if defined(HAVE_MEMMEM) && (!defined(__GNUC__) || __GNUC__ >= 2)
  tor_assert(nlen);
  return memmem(_haystack, hlen, _needle, nlen);
#else
  /* This isn't as fast as the GLIBC implementation, but it doesn't need to
   * be. */
  const char *p, *last_possible_start;
  const char *haystack = (const char*)_haystack;
  const char *needle = (const char*)_needle;
  char first;
  tor_assert(nlen);

  if (nlen > hlen)
    return NULL;

  p = haystack;
  /* Last position at which the needle could start. */
  last_possible_start = haystack + hlen - nlen;
  first = *(const char*)needle;
  while ((p = memchr(p, first, last_possible_start + 1 - p))) {
    if (fast_memeq(p, needle, nlen))
      return p;
    if (++p > last_possible_start) {
      /* This comparison shouldn't be necessary, since if p was previously
       * equal to last_possible_start, the next memchr call would be
       * "memchr(p, first, 0)", which will return NULL. But it clarifies the
       * logic. */
      return NULL;
    }
  }
  return NULL;
#endif
}

/**
 * Tables to implement ctypes-replacement TOR_IS*() functions.  Each table
 * has 256 bits to look up whether a character is in some set or not.  This
 * fails on non-ASCII platforms, but it is hard to find a platform whose
 * character set is not a superset of ASCII nowadays. */

/**@{*/
const uint32_t TOR_ISALPHA_TABLE[8] =
  { 0, 0, 0x7fffffe, 0x7fffffe, 0, 0, 0, 0 };
const uint32_t TOR_ISALNUM_TABLE[8] =
  { 0, 0x3ff0000, 0x7fffffe, 0x7fffffe, 0, 0, 0, 0 };
const uint32_t TOR_ISSPACE_TABLE[8] = { 0x3e00, 0x1, 0, 0, 0, 0, 0, 0 };
const uint32_t TOR_ISXDIGIT_TABLE[8] =
  { 0, 0x3ff0000, 0x7e, 0x7e, 0, 0, 0, 0 };
const uint32_t TOR_ISDIGIT_TABLE[8] = { 0, 0x3ff0000, 0, 0, 0, 0, 0, 0 };
const uint32_t TOR_ISPRINT_TABLE[8] =
  { 0, 0xffffffff, 0xffffffff, 0x7fffffff, 0, 0, 0, 0x0 };
const uint32_t TOR_ISUPPER_TABLE[8] = { 0, 0, 0x7fffffe, 0, 0, 0, 0, 0 };
const uint32_t TOR_ISLOWER_TABLE[8] = { 0, 0, 0, 0x7fffffe, 0, 0, 0, 0 };

/** Upper-casing and lowercasing tables to map characters to upper/lowercase
 * equivalents.  Used by tor_toupper() and tor_tolower(). */
/**@{*/
const char TOR_TOUPPER_TABLE[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
  96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};
const char TOR_TOLOWER_TABLE[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,
  96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};
/**@}*/

/** Helper for tor_strtok_r_impl: Advances cp past all characters in
 * <b>sep</b>, and returns its new value. */
static char *
strtok_helper(char *cp, const char *sep)
{
  if (sep[1]) {
    while (*cp && strchr(sep, *cp))
      ++cp;
  } else {
    while (*cp && *cp == *sep)
      ++cp;
  }
  return cp;
}

/** Implementation of strtok_r for platforms whose coders haven't figured out
 * how to write one.  Hey guys!  You can use this code here for free! */
char *
tor_strtok_r_impl(char *str, const char *sep, char **lasts)
{
  char *cp, *start;
  tor_assert(*sep);
  if (str) {
    str = strtok_helper(str, sep);
    if (!*str)
      return NULL;
    start = cp = *lasts = str;
  } else if (!*lasts || !**lasts) {
    return NULL;
  } else {
    start = cp = *lasts;
  }

  if (sep[1]) {
    while (*cp && !strchr(sep, *cp))
      ++cp;
  } else {
    cp = strchr(cp, *sep);
  }

  if (!cp || !*cp) {
    *lasts = NULL;
  } else {
    *cp++ = '\0';
    *lasts = strtok_helper(cp, sep);
  }
  return start;
}

#ifdef _WIN32
/** Take a filename and return a pointer to its final element.  This
 * function is called on __FILE__ to fix a MSVC nit where __FILE__
 * contains the full path to the file.  This is bad, because it
 * confuses users to find the home directory of the person who
 * compiled the binary in their warning messages.
 */
const char *
tor_fix_source_file(const char *fname)
{
  const char *cp1, *cp2, *r;
  cp1 = strrchr(fname, '/');
  cp2 = strrchr(fname, '\\');
  if (cp1 && cp2) {
    r = (cp1<cp2)?(cp2+1):(cp1+1);
  } else if (cp1) {
    r = cp1+1;
  } else if (cp2) {
    r = cp2+1;
  } else {
    r = fname;
  }
  return r;
}
#endif

/**
 * Read a 16-bit value beginning at <b>cp</b>.  Equivalent to
 * *(uint16_t*)(cp), but will not cause segfaults on platforms that forbid
 * unaligned memory access.
 */
uint16_t
get_uint16(const void *cp)
{
  uint16_t v;
  memcpy(&v,cp,2);
  return v;
}
/**
 * Read a 32-bit value beginning at <b>cp</b>.  Equivalent to
 * *(uint32_t*)(cp), but will not cause segfaults on platforms that forbid
 * unaligned memory access.
 */
uint32_t
get_uint32(const void *cp)
{
  uint32_t v;
  memcpy(&v,cp,4);
  return v;
}
/**
 * Read a 64-bit value beginning at <b>cp</b>.  Equivalent to
 * *(uint64_t*)(cp), but will not cause segfaults on platforms that forbid
 * unaligned memory access.
 */
uint64_t
get_uint64(const void *cp)
{
  uint64_t v;
  memcpy(&v,cp,8);
  return v;
}

/**
 * Set a 16-bit value beginning at <b>cp</b> to <b>v</b>. Equivalent to
 * *(uint16_t*)(cp) = v, but will not cause segfaults on platforms that forbid
 * unaligned memory access. */
void
set_uint16(void *cp, uint16_t v)
{
  memcpy(cp,&v,2);
}
/**
 * Set a 32-bit value beginning at <b>cp</b> to <b>v</b>. Equivalent to
 * *(uint32_t*)(cp) = v, but will not cause segfaults on platforms that forbid
 * unaligned memory access. */
void
set_uint32(void *cp, uint32_t v)
{
  memcpy(cp,&v,4);
}
/**
 * Set a 64-bit value beginning at <b>cp</b> to <b>v</b>. Equivalent to
 * *(uint64_t*)(cp) = v, but will not cause segfaults on platforms that forbid
 * unaligned memory access. */
void
set_uint64(void *cp, uint64_t v)
{
  memcpy(cp,&v,8);
}

/**
 * Rename the file <b>from</b> to the file <b>to</b>.  On Unix, this is
 * the same as rename(2).  On windows, this removes <b>to</b> first if
 * it already exists.
 * Returns 0 on success.  Returns -1 and sets errno on failure.
 */
int
replace_file(const char *from, const char *to)
{
#ifndef _WIN32
  return tor_rename(from, to);
#else
  switch (file_status(to))
    {
    case FN_NOENT:
      break;
    case FN_FILE:
      if (unlink(to)) return -1;
      break;
    case FN_ERROR:
      return -1;
    case FN_DIR:
      errno = EISDIR;
      return -1;
    }
  return tor_rename(from,to);
#endif
}

/** Change <b>fname</b>'s modification time to now. */
int
touch_file(const char *fname)
{
  if (utime(fname, NULL)!=0)
    return -1;
  return 0;
}

/** Represents a lockfile on which we hold the lock. */
struct tor_lockfile_t {
  /** Name of the file */
  char *filename;
  /** File descriptor used to hold the file open */
  int fd;
};

/** Try to get a lock on the lockfile <b>filename</b>, creating it as
 * necessary.  If someone else has the lock and <b>blocking</b> is true,
 * wait until the lock is available.  Otherwise return immediately whether
 * we succeeded or not.
 *
 * Set *<b>locked_out</b> to true if somebody else had the lock, and to false
 * otherwise.
 *
 * Return a <b>tor_lockfile_t</b> on success, NULL on failure.
 *
 * (Implementation note: because we need to fall back to fcntl on some
 *  platforms, these locks are per-process, not per-thread.  If you want
 *  to do in-process locking, use tor_mutex_t like a normal person.
 *  On Windows, when <b>blocking</b> is true, the maximum time that
 *  is actually waited is 10 seconds, after which NULL is returned
 *  and <b>locked_out</b> is set to 1.)
 */
tor_lockfile_t *
tor_lockfile_lock(const char *filename, int blocking, int *locked_out)
{
  tor_lockfile_t *result;
  int fd;
  *locked_out = 0;

  log_info(LD_FS, "Locking \"%s\"", filename);
  fd = tor_open_cloexec(filename, O_RDWR|O_CREAT|O_TRUNC, 0600);
  if (fd < 0) {
    log_warn(LD_FS,"Couldn't open \"%s\" for locking: %s", filename,
             strerror(errno));
    return NULL;
  }

#ifdef _WIN32
  _lseek(fd, 0, SEEK_SET);
  if (_locking(fd, blocking ? _LK_LOCK : _LK_NBLCK, 1) < 0) {
    if (errno != EACCES && errno != EDEADLOCK)
      log_warn(LD_FS,"Couldn't lock \"%s\": %s", filename, strerror(errno));
    else
      *locked_out = 1;
    close(fd);
    return NULL;
  }
#elif defined(HAVE_FLOCK)
  if (flock(fd, LOCK_EX|(blocking ? 0 : LOCK_NB)) < 0) {
    if (errno != EWOULDBLOCK)
      log_warn(LD_FS,"Couldn't lock \"%s\": %s", filename, strerror(errno));
    else
      *locked_out = 1;
    close(fd);
    return NULL;
  }
#else
  {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, blocking ? F_SETLKW : F_SETLK, &lock) < 0) {
      if (errno != EACCES && errno != EAGAIN)
        log_warn(LD_FS, "Couldn't lock \"%s\": %s", filename, strerror(errno));
      else
        *locked_out = 1;
      close(fd);
      return NULL;
    }
  }
#endif

  result = tor_malloc(sizeof(tor_lockfile_t));
  result->filename = tor_strdup(filename);
  result->fd = fd;
  return result;
}

/** Release the lock held as <b>lockfile</b>. */
void
tor_lockfile_unlock(tor_lockfile_t *lockfile)
{
  tor_assert(lockfile);

  log_info(LD_FS, "Unlocking \"%s\"", lockfile->filename);
#ifdef _WIN32
  _lseek(lockfile->fd, 0, SEEK_SET);
  if (_locking(lockfile->fd, _LK_UNLCK, 1) < 0) {
    log_warn(LD_FS,"Error unlocking \"%s\": %s", lockfile->filename,
             strerror(errno));
  }
#elif defined(HAVE_FLOCK)
  if (flock(lockfile->fd, LOCK_UN) < 0) {
    log_warn(LD_FS, "Error unlocking \"%s\": %s", lockfile->filename,
             strerror(errno));
  }
#else
  /* Closing the lockfile is sufficient. */
#endif

  close(lockfile->fd);
  lockfile->fd = -1;
  tor_free(lockfile->filename);
  tor_free(lockfile);
}

/** @{ */
/** Some old versions of Unix didn't define constants for these values,
 * and instead expect you to say 0, 1, or 2. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif
/** @} */

/** Return the position of <b>fd</b> with respect to the start of the file. */
off_t
tor_fd_getpos(int fd)
{
#ifdef _WIN32
  return (off_t) _lseek(fd, 0, SEEK_CUR);
#else
  return (off_t) lseek(fd, 0, SEEK_CUR);
#endif
}

/** Move <b>fd</b> to the end of the file. Return -1 on error, 0 on success. */
int
tor_fd_seekend(int fd)
{
#ifdef _WIN32
  return _lseek(fd, 0, SEEK_END) < 0 ? -1 : 0;
#else
  return lseek(fd, 0, SEEK_END) < 0 ? -1 : 0;
#endif
}

/** Move <b>fd</b> to position <b>pos</b> in the file. Return -1 on error, 0
 * on success. */
int
tor_fd_setpos(int fd, off_t pos)
{
#ifdef _WIN32
  return _lseek(fd, pos, SEEK_SET) < 0 ? -1 : 0;
#else
  return lseek(fd, pos, SEEK_SET) < 0 ? -1 : 0;
#endif
}

#undef DEBUG_SOCKET_COUNTING
#ifdef DEBUG_SOCKET_COUNTING
/** A bitarray of all fds that should be passed to tor_socket_close(). Only
 * used if DEBUG_SOCKET_COUNTING is defined. */
static bitarray_t *open_sockets = NULL;
/** The size of <b>open_sockets</b>, in bits. */
static int max_socket = -1;
#endif

/** Count of number of sockets currently open.  (Undercounts sockets opened by
 * eventdns and libevent.) */
static int n_sockets_open = 0;

/** Mutex to protect open_sockets, max_socket, and n_sockets_open. */
static tor_mutex_t *socket_accounting_mutex = NULL;

/** Helper: acquire the socket accounting lock. */
static INLINE void
socket_accounting_lock(void)
{
  if (PREDICT_UNLIKELY(!socket_accounting_mutex))
    socket_accounting_mutex = tor_mutex_new();
  tor_mutex_acquire(socket_accounting_mutex);
}

/** Helper: release the socket accounting lock. */
static INLINE void
socket_accounting_unlock(void)
{
  tor_mutex_release(socket_accounting_mutex);
}

/** As close(), but guaranteed to work for sockets across platforms (including
 * Windows, where close()ing a socket doesn't work.  Returns 0 on success and
 * the socket error code on failure. */
int
tor_close_socket_simple(tor_socket_t s)
{
  int r = 0;

  /* On Windows, you have to call close() on fds returned by open(),
  * and closesocket() on fds returned by socket().  On Unix, everything
  * gets close()'d.  We abstract this difference by always using
  * tor_close_socket to close sockets, and always using close() on
  * files.
  */
  #if defined(_WIN32)
    r = closesocket(s);
  #else
    r = close(s);
  #endif

  if (r != 0) {
    int err = tor_socket_errno(-1);
    log_info(LD_NET, "Close returned an error: %s", tor_socket_strerror(err));
    return err;
  }

  return r;
}

/** As tor_close_socket_simple(), but keeps track of the number
 * of open sockets. Returns 0 on success, -1 on failure. */
int
tor_close_socket(tor_socket_t s)
{
  int r = tor_close_socket_simple(s);

  socket_accounting_lock();
#ifdef DEBUG_SOCKET_COUNTING
  if (s > max_socket || ! bitarray_is_set(open_sockets, s)) {
    log_warn(LD_BUG, "Closing a socket (%d) that wasn't returned by tor_open_"
             "socket(), or that was already closed or something.", s);
  } else {
    tor_assert(open_sockets && s <= max_socket);
    bitarray_clear(open_sockets, s);
  }
#endif
  if (r == 0) {
    --n_sockets_open;
  } else {
#ifdef _WIN32
    if (r != WSAENOTSOCK)
      --n_sockets_open;
#else
    if (r != EBADF)
      --n_sockets_open;
#endif
    r = -1;
  }

  if (n_sockets_open < 0)
    log_warn(LD_BUG, "Our socket count is below zero: %d. Please submit a "
             "bug report.", n_sockets_open);
  socket_accounting_unlock();
  return r;
}

/** @{ */
#ifdef DEBUG_SOCKET_COUNTING
/** Helper: if DEBUG_SOCKET_COUNTING is enabled, remember that <b>s</b> is
 * now an open socket. */
static INLINE void
mark_socket_open(tor_socket_t s)
{
  /* XXXX This bitarray business will NOT work on windows: sockets aren't
     small ints there. */
  if (s > max_socket) {
    if (max_socket == -1) {
      open_sockets = bitarray_init_zero(s+128);
      max_socket = s+128;
    } else {
      open_sockets = bitarray_expand(open_sockets, max_socket, s+128);
      max_socket = s+128;
    }
  }
  if (bitarray_is_set(open_sockets, s)) {
    log_warn(LD_BUG, "I thought that %d was already open, but socket() just "
             "gave it to me!", s);
  }
  bitarray_set(open_sockets, s);
}
#else
#define mark_socket_open(s) STMT_NIL
#endif
/** @} */

/** As socket(), but counts the number of open sockets. */
tor_socket_t
tor_open_socket(int domain, int type, int protocol)
{
  return tor_open_socket_with_extensions(domain, type, protocol, 1, 0);
}

/** As socket(), but creates a nonblocking socket and
 * counts the number of open sockets. */
tor_socket_t
tor_open_socket_nonblocking(int domain, int type, int protocol)
{
  return tor_open_socket_with_extensions(domain, type, protocol, 1, 1);
}

/** As socket(), but counts the number of open sockets and handles
 * socket creation with either of SOCK_CLOEXEC and SOCK_NONBLOCK specified.
 * <b>cloexec</b> and <b>nonblock</b> should be either 0 or 1 to indicate
 * if the corresponding extension should be used.*/
tor_socket_t
tor_open_socket_with_extensions(int domain, int type, int protocol,
                                int cloexec, int nonblock)
{
  tor_socket_t s;
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
  int ext_flags = (cloexec ? SOCK_CLOEXEC : 0) |
                  (nonblock ? SOCK_NONBLOCK : 0);
  s = socket(domain, type|ext_flags, protocol);
  if (SOCKET_OK(s))
    goto socket_ok;
  /* If we got an error, see if it is EINVAL. EINVAL might indicate that,
   * even though we were built on a system with SOCK_CLOEXEC and SOCK_NONBLOCK
   * support, we are running on one without. */
  if (errno != EINVAL)
    return s;
#endif /* SOCK_CLOEXEC && SOCK_NONBLOCK */

  s = socket(domain, type, protocol);
  if (! SOCKET_OK(s))
    return s;

#if defined(FD_CLOEXEC)
  if (cloexec) {
    if (fcntl(s, F_SETFD, FD_CLOEXEC) == -1) {
      log_warn(LD_FS,"Couldn't set FD_CLOEXEC: %s", strerror(errno));
      tor_close_socket_simple(s);
      return TOR_INVALID_SOCKET;
    }
  }
#else
  (void)cloexec;
#endif

  if (nonblock) {
    if (set_socket_nonblocking(s) == -1) {
      tor_close_socket_simple(s);
      return TOR_INVALID_SOCKET;
    }
  }

  goto socket_ok; /* So that socket_ok will not be unused. */

 socket_ok:
  socket_accounting_lock();
  ++n_sockets_open;
  mark_socket_open(s);
  socket_accounting_unlock();
  return s;
}

/** As accept(), but counts the number of open sockets. */
tor_socket_t
tor_accept_socket(tor_socket_t sockfd, struct sockaddr *addr, socklen_t *len)
{
  return tor_accept_socket_with_extensions(sockfd, addr, len, 1, 0);
}

/** As accept(), but returns a nonblocking socket and
 * counts the number of open sockets. */
tor_socket_t
tor_accept_socket_nonblocking(tor_socket_t sockfd, struct sockaddr *addr,
                              socklen_t *len)
{
  return tor_accept_socket_with_extensions(sockfd, addr, len, 1, 1);
}

/** As accept(), but counts the number of open sockets and handles
 * socket creation with either of SOCK_CLOEXEC and SOCK_NONBLOCK specified.
 * <b>cloexec</b> and <b>nonblock</b> should be either 0 or 1 to indicate
 * if the corresponding extension should be used.*/
tor_socket_t
tor_accept_socket_with_extensions(tor_socket_t sockfd, struct sockaddr *addr,
                                 socklen_t *len, int cloexec, int nonblock)
{
  tor_socket_t s;
#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
  int ext_flags = (cloexec ? SOCK_CLOEXEC : 0) |
                  (nonblock ? SOCK_NONBLOCK : 0);
  s = accept4(sockfd, addr, len, ext_flags);
  if (SOCKET_OK(s))
    goto socket_ok;
  /* If we got an error, see if it is ENOSYS. ENOSYS indicates that,
   * even though we were built on a system with accept4 support, we
   * are running on one without. Also, check for EINVAL, which indicates that
   * we are missing SOCK_CLOEXEC/SOCK_NONBLOCK support. */
  if (errno != EINVAL && errno != ENOSYS)
    return s;
#endif

  s = accept(sockfd, addr, len);
  if (!SOCKET_OK(s))
    return s;

#if defined(FD_CLOEXEC)
  if (cloexec) {
    if (fcntl(s, F_SETFD, FD_CLOEXEC) == -1) {
      log_warn(LD_NET, "Couldn't set FD_CLOEXEC: %s", strerror(errno));
      tor_close_socket_simple(s);
      return TOR_INVALID_SOCKET;
    }
  }
#else
  (void)cloexec;
#endif

  if (nonblock) {
    if (set_socket_nonblocking(s) == -1) {
      tor_close_socket_simple(s);
      return TOR_INVALID_SOCKET;
    }
  }

  goto socket_ok; /* So that socket_ok will not be unused. */

 socket_ok:
  socket_accounting_lock();
  ++n_sockets_open;
  mark_socket_open(s);
  socket_accounting_unlock();
  return s;
}

/** Return the number of sockets we currently have opened. */
int
get_n_open_sockets(void)
{
  int n;
  socket_accounting_lock();
  n = n_sockets_open;
  socket_accounting_unlock();
  return n;
}

/** Turn <b>socket</b> into a nonblocking socket. Return 0 on success, -1
 * on failure.
 */
int
set_socket_nonblocking(tor_socket_t socket)
{
#if defined(_WIN32)
  unsigned long nonblocking = 1;
  ioctlsocket(socket, FIONBIO, (unsigned long*) &nonblocking);
#else
  int flags;

  flags = fcntl(socket, F_GETFL, 0);
  if (flags == -1) {
    log_warn(LD_NET, "Couldn't get file status flags: %s", strerror(errno));
    return -1;
  }
  flags |= O_NONBLOCK;
  if (fcntl(socket, F_SETFL, flags) == -1) {
    log_warn(LD_NET, "Couldn't set file status flags: %s", strerror(errno));
    return -1;
  }
#endif

  return 0;
}

/**
 * Allocate a pair of connected sockets.  (Like socketpair(family,
 * type,protocol,fd), but works on systems that don't have
 * socketpair.)
 *
 * Currently, only (AF_UNIX, SOCK_STREAM, 0) sockets are supported.
 *
 * Note that on systems without socketpair, this call will fail if
 * localhost is inaccessible (for example, if the networking
 * stack is down). And even if it succeeds, the socket pair will not
 * be able to read while localhost is down later (the socket pair may
 * even close, depending on OS-specific timeouts).
 *
 * Returns 0 on success and -errno on failure; do not rely on the value
 * of errno or WSAGetLastError().
 **/
/* It would be nicer just to set errno, but that won't work for windows. */
int
tor_socketpair(int family, int type, int protocol, tor_socket_t fd[2])
{
//don't use win32 socketpairs (they are always bad)
#if defined(HAVE_SOCKETPAIR) && !defined(_WIN32)
  int r;

#ifdef SOCK_CLOEXEC
  r = socketpair(family, type|SOCK_CLOEXEC, protocol, fd);
  if (r == 0)
    goto sockets_ok;
  /* If we got an error, see if it is EINVAL. EINVAL might indicate that,
   * even though we were built on a system with SOCK_CLOEXEC support, we
   * are running on one without. */
  if (errno != EINVAL)
    return -errno;
#endif

  r = socketpair(family, type, protocol, fd);
  if (r < 0)
    return -errno;

#if defined(FD_CLOEXEC)
  if (SOCKET_OK(fd[0])) {
    r = fcntl(fd[0], F_SETFD, FD_CLOEXEC);
    if (r == -1) {
      close(fd[0]);
      close(fd[1]);
      return -errno;
    }
  }
  if (SOCKET_OK(fd[1])) {
    r = fcntl(fd[1], F_SETFD, FD_CLOEXEC);
    if (r == -1) {
      close(fd[0]);
      close(fd[1]);
      return -errno;
    }
  }
#endif
  goto sockets_ok; /* So that sockets_ok will not be unused. */

 sockets_ok:
  socket_accounting_lock();
  if (SOCKET_OK(fd[0])) {
    ++n_sockets_open;
    mark_socket_open(fd[0]);
  }
  if (SOCKET_OK(fd[1])) {
    ++n_sockets_open;
    mark_socket_open(fd[1]);
  }
  socket_accounting_unlock();

  return 0;
#else
  return tor_ersatz_socketpair(family, type, protocol, fd);
#endif
}

#ifdef NEED_ERSATZ_SOCKETPAIR
/**
 * Helper used to implement socketpair on systems that lack it, by
 * making a direct connection to localhost.
 */
STATIC int
tor_ersatz_socketpair(int family, int type, int protocol, tor_socket_t fd[2])
{
    /* This socketpair does not work when localhost is down. So
     * it's really not the same thing at all. But it's close enough
     * for now, and really, when localhost is down sometimes, we
     * have other problems too.
     */
    tor_socket_t listener = TOR_INVALID_SOCKET;
    tor_socket_t connector = TOR_INVALID_SOCKET;
    tor_socket_t acceptor = TOR_INVALID_SOCKET;
    struct sockaddr_in listen_addr;
    struct sockaddr_in connect_addr;
    socklen_t size;
    int saved_errno = -1;

    if (protocol
#ifdef AF_UNIX
        || family != AF_UNIX
#endif
        ) {
#ifdef _WIN32
      return -WSAEAFNOSUPPORT;
#else
      return -EAFNOSUPPORT;
#endif
    }
    if (!fd) {
      return -EINVAL;
    }

    listener = tor_open_socket(AF_INET, type, 0);
    if (!SOCKET_OK(listener))
      return -tor_socket_errno(-1);
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listen_addr.sin_port = 0;   /* kernel chooses port.  */
    if (bind(listener, (struct sockaddr *) &listen_addr, sizeof (listen_addr))
        == -1)
      goto tidy_up_and_fail;
    if (listen(listener, 1) == -1)
      goto tidy_up_and_fail;

    connector = tor_open_socket(AF_INET, type, 0);
    if (!SOCKET_OK(connector))
      goto tidy_up_and_fail;
    /* We want to find out the port number to connect to.  */
    size = sizeof(connect_addr);
    if (getsockname(listener, (struct sockaddr *) &connect_addr, &size) == -1)
      goto tidy_up_and_fail;
    if (size != sizeof (connect_addr))
      goto abort_tidy_up_and_fail;
    if (connect(connector, (struct sockaddr *) &connect_addr,
                sizeof(connect_addr)) == -1)
      goto tidy_up_and_fail;

    size = sizeof(listen_addr);
    acceptor = tor_accept_socket(listener,
                                 (struct sockaddr *) &listen_addr, &size);
    if (!SOCKET_OK(acceptor))
      goto tidy_up_and_fail;
    if (size != sizeof(listen_addr))
      goto abort_tidy_up_and_fail;
    /* Now check we are talking to ourself by matching port and host on the
       two sockets.  */
    if (getsockname(connector, (struct sockaddr *) &connect_addr, &size) == -1)
      goto tidy_up_and_fail;
    if (size != sizeof (connect_addr)
        || listen_addr.sin_family != connect_addr.sin_family
        || listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
        || listen_addr.sin_port != connect_addr.sin_port) {
      goto abort_tidy_up_and_fail;
    }
    tor_close_socket(listener);
    fd[0] = connector;
    fd[1] = acceptor;

    return 0;

  abort_tidy_up_and_fail:
#ifdef _WIN32
    saved_errno = WSAECONNABORTED;
#else
    saved_errno = ECONNABORTED; /* I hope this is portable and appropriate.  */
#endif
  tidy_up_and_fail:
    if (saved_errno < 0)
      saved_errno = errno;
    if (SOCKET_OK(listener))
      tor_close_socket(listener);
    if (SOCKET_OK(connector))
      tor_close_socket(connector);
    if (SOCKET_OK(acceptor))
      tor_close_socket(acceptor);
    return -saved_errno;
}
#endif

/** Number of extra file descriptors to keep in reserve beyond those that we
 * tell Tor it's allowed to use. */
#define ULIMIT_BUFFER 32 /* keep 32 extra fd's beyond ConnLimit_ */

/** Learn the maximum allowed number of file descriptors, and tell the system
 * we want to use up to that number. (Some systems have a low soft limit, and
 * let us set it higher.)
 *
 * We compute this by finding the largest number that we can use.
 * If we can't find a number greater than or equal to <b>limit</b>,
 * then we fail: return -1.
 *
 * If <b>limit</b> is 0, then do not adjust the current maximum.
 *
 * Otherwise, return 0 and store the maximum we found inside <b>max_out</b>.*/
int
set_max_file_descriptors(rlim_t limit, int *max_out)
{
  /* Define some maximum connections values for systems where we cannot
   * automatically determine a limit. Re Cygwin, see
   * http://archives.seul.org/or/talk/Aug-2006/msg00210.html
   * For an iPhone, 9999 should work. For Windows and all other unknown
   * systems we use 15000 as the default. */
#ifndef HAVE_GETRLIMIT
#if defined(CYGWIN) || defined(__CYGWIN__)
  const char *platform = "Cygwin";
  const unsigned long MAX_CONNECTIONS = 3200;
#elif defined(_WIN32)
  const char *platform = "Windows";
  const unsigned long MAX_CONNECTIONS = 15000;
#else
  const char *platform = "unknown platforms with no getrlimit()";
  const unsigned long MAX_CONNECTIONS = 15000;
#endif
  log_fn(LOG_INFO, LD_NET,
         "This platform is missing getrlimit(). Proceeding.");
  if (limit > MAX_CONNECTIONS) {
    log_warn(LD_CONFIG,
             "We do not support more than %lu file descriptors "
             "on %s. Tried to raise to %lu.",
             (unsigned long)MAX_CONNECTIONS, platform, (unsigned long)limit);
    return -1;
  }
  limit = MAX_CONNECTIONS;
#else /* HAVE_GETRLIMIT */
  struct rlimit rlim;

  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    log_warn(LD_NET, "Could not get maximum number of file descriptors: %s",
             strerror(errno));
    return -1;
  }
  if (limit == 0) {
    /* If limit == 0, return the maximum value without setting it. */
    limit = rlim.rlim_max;
    if (limit > INT_MAX)
      limit = INT_MAX;
    *max_out = (int)limit - ULIMIT_BUFFER;
    return 0;
  }
  if (rlim.rlim_max < limit) {
    log_warn(LD_CONFIG,"We need %lu file descriptors available, and we're "
             "limited to %lu. Please change your ulimit -n.",
             (unsigned long)limit, (unsigned long)rlim.rlim_max);
    return -1;
  }

  if (rlim.rlim_max > rlim.rlim_cur) {
    log_info(LD_NET,"Raising max file descriptors from %lu to %lu.",
             (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
  }
  rlim.rlim_cur = rlim.rlim_max;

  if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    int bad = 1;
#ifdef OPEN_MAX
    if (errno == EINVAL && OPEN_MAX < rlim.rlim_cur) {
      /* On some platforms, OPEN_MAX is the real limit, and getrlimit() is
       * full of nasty lies.  I'm looking at you, OSX 10.5.... */
      rlim.rlim_cur = OPEN_MAX;
      if (setrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        if (rlim.rlim_cur < (rlim_t)limit) {
          log_warn(LD_CONFIG, "We are limited to %lu file descriptors by "
                 "OPEN_MAX, and ConnLimit is %lu.  Changing ConnLimit; sorry.",
                   (unsigned long)OPEN_MAX, (unsigned long)limit);
        } else {
          log_info(LD_CONFIG, "Dropped connection limit to OPEN_MAX (%lu); "
                   "Apparently, %lu was too high and rlimit lied to us.",
                   (unsigned long)OPEN_MAX, (unsigned long)rlim.rlim_max);
        }
        bad = 0;
      }
    }
#endif /* OPEN_MAX */
    if (bad) {
      log_warn(LD_CONFIG,"Couldn't set maximum number of file descriptors: %s",
               strerror(errno));
      return -1;
    }
  }
  /* leave some overhead for logs, etc, */
  limit = rlim.rlim_cur;
#endif /* HAVE_GETRLIMIT */

  if (limit < ULIMIT_BUFFER) {
    log_warn(LD_CONFIG,
             "ConnLimit must be at least %d. Failing.", ULIMIT_BUFFER);
    return -1;
  }
  if (limit > INT_MAX)
    limit = INT_MAX;
  tor_assert(max_out);
  *max_out = (int)limit - ULIMIT_BUFFER;
  return 0;
}

#ifndef _WIN32
/** Log details of current user and group credentials. Return 0 on
 * success. Logs and return -1 on failure.
 */
static int
log_credential_status(void)
{
/** Log level to use when describing non-error UID/GID status. */
#define CREDENTIAL_LOG_LEVEL LOG_INFO
  /* Real, effective and saved UIDs */
  uid_t ruid, euid, suid;
  /* Read, effective and saved GIDs */
  gid_t rgid, egid, sgid;
  /* Supplementary groups */
  gid_t *sup_gids = NULL;
  int sup_gids_size;
  /* Number of supplementary groups */
  int ngids;

  /* log UIDs */
#ifdef HAVE_GETRESUID
  if (getresuid(&ruid, &euid, &suid) != 0 ) {
    log_warn(LD_GENERAL, "Error getting changed UIDs: %s", strerror(errno));
    return -1;
  } else {
    log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL,
           "UID is %u (real), %u (effective), %u (saved)",
           (unsigned)ruid, (unsigned)euid, (unsigned)suid);
  }
#else
  /* getresuid is not present on MacOS X, so we can't get the saved (E)UID */
  ruid = getuid();
  euid = geteuid();
  (void)suid;

  log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL,
         "UID is %u (real), %u (effective), unknown (saved)",
         (unsigned)ruid, (unsigned)euid);
#endif

  /* log GIDs */
#ifdef HAVE_GETRESGID
  if (getresgid(&rgid, &egid, &sgid) != 0 ) {
    log_warn(LD_GENERAL, "Error getting changed GIDs: %s", strerror(errno));
    return -1;
  } else {
    log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL,
           "GID is %u (real), %u (effective), %u (saved)",
           (unsigned)rgid, (unsigned)egid, (unsigned)sgid);
  }
#else
  /* getresgid is not present on MacOS X, so we can't get the saved (E)GID */
  rgid = getgid();
  egid = getegid();
  (void)sgid;
  log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL,
         "GID is %u (real), %u (effective), unknown (saved)",
         (unsigned)rgid, (unsigned)egid);
#endif

  /* log supplementary groups */
  sup_gids_size = 64;
  sup_gids = tor_malloc(sizeof(gid_t) * 64);
  while ((ngids = getgroups(sup_gids_size, sup_gids)) < 0 &&
         errno == EINVAL &&
         sup_gids_size < NGROUPS_MAX) {
    sup_gids_size *= 2;
    sup_gids = tor_realloc(sup_gids, sizeof(gid_t) * sup_gids_size);
  }

  if (ngids < 0) {
    log_warn(LD_GENERAL, "Error getting supplementary GIDs: %s",
             strerror(errno));
    tor_free(sup_gids);
    return -1;
  } else {
    int i, retval = 0;
    char *s = NULL;
    smartlist_t *elts = smartlist_new();

    for (i = 0; i<ngids; i++) {
      smartlist_add_asprintf(elts, "%u", (unsigned)sup_gids[i]);
    }

    s = smartlist_join_strings(elts, " ", 0, NULL);

    log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL, "Supplementary groups are: %s",s);

    tor_free(s);
    SMARTLIST_FOREACH(elts, char *, cp, tor_free(cp));
    smartlist_free(elts);
    tor_free(sup_gids);

    return retval;
  }

  return 0;
}
#endif

#ifndef _WIN32
/** Cached struct from the last getpwname() call we did successfully. */
static struct passwd *passwd_cached = NULL;

/** Helper: copy a struct passwd object.
 *
 * We only copy the fields pw_uid, pw_gid, pw_name, pw_dir.  Tor doesn't use
 * any others, and I don't want to run into incompatibilities.
 */
static struct passwd *
tor_passwd_dup(const struct passwd *pw)
{
  struct passwd *new_pw = tor_malloc_zero(sizeof(struct passwd));
  if (pw->pw_name)
    new_pw->pw_name = tor_strdup(pw->pw_name);
  if (pw->pw_dir)
    new_pw->pw_dir = tor_strdup(pw->pw_dir);
  new_pw->pw_uid = pw->pw_uid;
  new_pw->pw_gid = pw->pw_gid;

  return new_pw;
}

/** Helper: free one of our cached 'struct passwd' values. */
static void
tor_passwd_free(struct passwd *pw)
{
  if (!pw)
    return;

  tor_free(pw->pw_name);
  tor_free(pw->pw_dir);
  tor_free(pw);
}

/** Wrapper around getpwnam() that caches result. Used so that we don't need
 * to give the sandbox access to /etc/passwd.
 *
 * The following fields alone will definitely be copied in the output: pw_uid,
 * pw_gid, pw_name, pw_dir.  Other fields are not present in cached values.
 *
 * When called with a NULL argument, this function clears storage associated
 * with static variables it uses.
 **/
const struct passwd *
tor_getpwnam(const char *username)
{
  struct passwd *pw;

  if (username == NULL) {
    tor_passwd_free(passwd_cached);
    passwd_cached = NULL;
    return NULL;
  }

  if ((pw = getpwnam(username))) {
    tor_passwd_free(passwd_cached);
    passwd_cached = tor_passwd_dup(pw);
    log_notice(LD_GENERAL, "Caching new entry %s for %s",
               passwd_cached->pw_name, username);
    return pw;
  }

  /* Lookup failed */
  if (! passwd_cached || ! passwd_cached->pw_name)
    return NULL;

  if (! strcmp(username, passwd_cached->pw_name))
    return passwd_cached;

  return NULL;
}

/** Wrapper around getpwnam() that can use cached result from
 * tor_getpwnam(). Used so that we don't need to give the sandbox access to
 * /etc/passwd.
 *
 * The following fields alone will definitely be copied in the output: pw_uid,
 * pw_gid, pw_name, pw_dir.  Other fields are not present in cached values.
 */
const struct passwd *
tor_getpwuid(uid_t uid)
{
  struct passwd *pw;

  if ((pw = getpwuid(uid))) {
    return pw;
  }

  /* Lookup failed */
  if (! passwd_cached)
    return NULL;

  if (uid == passwd_cached->pw_uid)
    return passwd_cached;

  return NULL;
}
#endif

/** Call setuid and setgid to run as <b>user</b> and switch to their
 * primary group.  Return 0 on success.  On failure, log and return -1.
 */
int
switch_id(const char *user)
{
#ifndef _WIN32
  const struct passwd *pw = NULL;
  uid_t old_uid;
  gid_t old_gid;
  static int have_already_switched_id = 0;

  tor_assert(user);

  if (have_already_switched_id)
    return 0;

  /* Log the initial credential state */
  if (log_credential_status())
    return -1;

  log_fn(CREDENTIAL_LOG_LEVEL, LD_GENERAL, "Changing user and groups");

  /* Get old UID/GID to check if we changed correctly */
  old_uid = getuid();
  old_gid = getgid();

  /* Lookup the user and group information, if we have a problem, bail out. */
  pw = tor_getpwnam(user);
  if (pw == NULL) {
    log_warn(LD_CONFIG, "Error setting configured user: %s not found", user);
    return -1;
  }

  /* Properly switch egid,gid,euid,uid here or bail out */
  if (setgroups(1, &pw->pw_gid)) {
    log_warn(LD_GENERAL, "Error setting groups to gid %d: \"%s\".",
             (int)pw->pw_gid, strerror(errno));
    if (old_uid == pw->pw_uid) {
      log_warn(LD_GENERAL, "Tor is already running as %s.  You do not need "
               "the \"User\" option if you are already running as the user "
               "you want to be.  (If you did not set the User option in your "
               "torrc, check whether it was specified on the command line "
               "by a startup script.)", user);
    } else {
      log_warn(LD_GENERAL, "If you set the \"User\" option, you must start Tor"
               " as root.");
    }
    return -1;
  }

  if (setegid(pw->pw_gid)) {
    log_warn(LD_GENERAL, "Error setting egid to %d: %s",
             (int)pw->pw_gid, strerror(errno));
    return -1;
  }

  if (setgid(pw->pw_gid)) {
    log_warn(LD_GENERAL, "Error setting gid to %d: %s",
             (int)pw->pw_gid, strerror(errno));
    return -1;
  }

  if (setuid(pw->pw_uid)) {
    log_warn(LD_GENERAL, "Error setting configured uid to %s (%d): %s",
             user, (int)pw->pw_uid, strerror(errno));
    return -1;
  }

  if (seteuid(pw->pw_uid)) {
    log_warn(LD_GENERAL, "Error setting configured euid to %s (%d): %s",
             user, (int)pw->pw_uid, strerror(errno));
    return -1;
  }

  /* This is how OpenBSD rolls:
  if (setgroups(1, &pw->pw_gid) || setegid(pw->pw_gid) ||
      setgid(pw->pw_gid) || setuid(pw->pw_uid) || seteuid(pw->pw_uid)) {
      setgid(pw->pw_gid) || seteuid(pw->pw_uid) || setuid(pw->pw_uid)) {
    log_warn(LD_GENERAL, "Error setting configured UID/GID: %s",
    strerror(errno));
    return -1;
  }
  */

  /* We've properly switched egid, gid, euid, uid, and supplementary groups if
   * we're here. */

#if !defined(CYGWIN) && !defined(__CYGWIN__)
  /* If we tried to drop privilege to a group/user other than root, attempt to
   * restore root (E)(U|G)ID, and abort if the operation succeeds */

  /* Only check for privilege dropping if we were asked to be non-root */
  if (pw->pw_uid) {
    /* Try changing GID/EGID */
    if (pw->pw_gid != old_gid &&
        (setgid(old_gid) != -1 || setegid(old_gid) != -1)) {
      log_warn(LD_GENERAL, "Was able to restore group credentials even after "
               "switching GID: this means that the setgid code didn't work.");
      return -1;
    }

    /* Try changing UID/EUID */
    if (pw->pw_uid != old_uid &&
        (setuid(old_uid) != -1 || seteuid(old_uid) != -1)) {
      log_warn(LD_GENERAL, "Was able to restore user credentials even after "
               "switching UID: this means that the setuid code didn't work.");
      return -1;
    }
  }
#endif

  /* Check what really happened */
  if (log_credential_status()) {
    return -1;
  }

  have_already_switched_id = 1; /* mark success so we never try again */

#if defined(__linux__) && defined(HAVE_SYS_PRCTL_H) && defined(HAVE_PRCTL)
#ifdef PR_SET_DUMPABLE
  if (pw->pw_uid) {
    /* Re-enable core dumps if we're not running as root. */
    log_info(LD_CONFIG, "Re-enabling coredumps");
    if (prctl(PR_SET_DUMPABLE, 1)) {
      log_warn(LD_CONFIG, "Unable to re-enable coredumps: %s",strerror(errno));
    }
  }
#endif
#endif
  return 0;

#else
  (void)user;

  log_warn(LD_CONFIG,
           "User specified but switching users is unsupported on your OS.");
  return -1;
#endif
}

/* We only use the linux prctl for now. There is no Win32 support; this may
 * also work on various BSD systems and Mac OS X - send testing feedback!
 *
 * On recent Gnu/Linux kernels it is possible to create a system-wide policy
 * that will prevent non-root processes from attaching to other processes
 * unless they are the parent process; thus gdb can attach to programs that
 * they execute but they cannot attach to other processes running as the same
 * user. The system wide policy may be set with the sysctl
 * kernel.yama.ptrace_scope or by inspecting
 * /proc/sys/kernel/yama/ptrace_scope and it is 1 by default on Ubuntu 11.04.
 *
 * This ptrace scope will be ignored on Gnu/Linux for users with
 * CAP_SYS_PTRACE and so it is very likely that root will still be able to
 * attach to the Tor process.
 */
/** Attempt to disable debugger attachment: return 1 on success, -1 on
 * failure, and 0 if we don't know how to try on this platform. */
int
tor_disable_debugger_attach(void)
{
  int r, attempted;
  r = -1;
  attempted = 0;
  log_debug(LD_CONFIG,
            "Attemping to disable debugger attachment to Tor for "
            "unprivileged users.");
#if defined(__linux__) && defined(HAVE_SYS_PRCTL_H) && defined(HAVE_PRCTL)
#ifdef PR_SET_DUMPABLE
  attempted = 1;
  r = prctl(PR_SET_DUMPABLE, 0);
#endif
#endif
#if defined(__APPLE__) && defined(PT_DENY_ATTACH)
  if (r < 0) {
    attempted = 1;
    r = ptrace(PT_DENY_ATTACH, 0, 0, 0);
  }
#endif

  // XXX: TODO - Mac OS X has dtrace and this may be disabled.
  // XXX: TODO - Windows probably has something similar
  if (r == 0 && attempted) {
    log_debug(LD_CONFIG,"Debugger attachment disabled for "
              "unprivileged users.");
    return 1;
  } else if (attempted) {
    log_warn(LD_CONFIG, "Unable to disable debugger attaching: %s",
             strerror(errno));
  }
  return r;
}

#ifdef HAVE_PWD_H
/** Allocate and return a string containing the home directory for the
 * user <b>username</b>. Only works on posix-like systems. */
char *
get_user_homedir(const char *username)
{
  const struct passwd *pw;
  tor_assert(username);

  if (!(pw = tor_getpwnam(username))) {
    log_err(LD_CONFIG,"User \"%s\" not found.", username);
    return NULL;
  }
  return tor_strdup(pw->pw_dir);
}
#endif

/** Modify <b>fname</b> to contain the name of its parent directory.  Doesn't
 * actually examine the filesystem; does a purely syntactic modification.
 *
 * The parent of the root director is considered to be iteself.
 *
 * Path separators are the forward slash (/) everywhere and additionally
 * the backslash (\) on Win32.
 *
 * Cuts off any number of trailing path separators but otherwise ignores
 * them for purposes of finding the parent directory.
 *
 * Returns 0 if a parent directory was successfully found, -1 otherwise (fname
 * did not have any path separators or only had them at the end).
 * */
int
get_parent_directory(char *fname)
{
  char *cp;
  int at_end = 1;
  tor_assert(fname);
#ifdef _WIN32
  /* If we start with, say, c:, then don't consider that the start of the path
   */
  if (fname[0] && fname[1] == ':') {
    fname += 2;
  }
#endif
  /* Now we want to remove all path-separators at the end of the string,
   * and to remove the end of the string starting with the path separator
   * before the last non-path-separator.  In perl, this would be
   *   s#[/]*$##; s#/[^/]*$##;
   * on a unixy platform.
   */
  cp = fname + strlen(fname);
  at_end = 1;
  while (--cp >= fname) {
    int is_sep = (*cp == '/'
#ifdef _WIN32
                  || *cp == '\\'
#endif
                  );
    if (is_sep) {
      if (cp == fname) {
        /* This is the first separator in the file name; don't remove it! */
        cp[1] = '\0';
        return 0;
      }
      *cp = '\0';
      if (! at_end)
        return 0;
    } else {
      at_end = 0;
    }
  }
  return -1;
}

#ifndef _WIN32
/** Return a newly allocated string containing the output of getcwd(). Return
 * NULL on failure. (We can't just use getcwd() into a PATH_MAX buffer, since
 * Hurd hasn't got a PATH_MAX.)
 */
static char *
alloc_getcwd(void)
{
    int saved_errno = errno;
/* We use this as a starting path length. Not too large seems sane. */
#define START_PATH_LENGTH 128
/* Nobody has a maxpath longer than this, as far as I know.  And if they
 * do, they shouldn't. */
#define MAX_SANE_PATH_LENGTH 4096
    size_t path_length = START_PATH_LENGTH;
    char *path = tor_malloc(path_length);

    errno = 0;
    while (getcwd(path, path_length) == NULL) {
      if (errno == ERANGE && path_length < MAX_SANE_PATH_LENGTH) {
        path_length*=2;
        path = tor_realloc(path, path_length);
      } else {
        tor_free(path);
        path = NULL;
        break;
      }
    }
    errno = saved_errno;
    return path;
}
#endif

/** Expand possibly relative path <b>fname</b> to an absolute path.
 * Return a newly allocated string, possibly equal to <b>fname</b>. */
char *
make_path_absolute(char *fname)
{
#ifdef _WIN32
  char *absfname_malloced = _fullpath(NULL, fname, 1);

  /* We don't want to assume that tor_free can free a string allocated
   * with malloc.  On failure, return fname (it's better than nothing). */
  char *absfname = tor_strdup(absfname_malloced ? absfname_malloced : fname);
  if (absfname_malloced) free(absfname_malloced);

  return absfname;
#else
  char *absfname = NULL, *path = NULL;

  tor_assert(fname);

  if (fname[0] == '/') {
    absfname = tor_strdup(fname);
  } else {
    path = alloc_getcwd();
    if (path) {
      tor_asprintf(&absfname, "%s/%s", path, fname);
      tor_free(path);
    } else {
      /* If getcwd failed, the best we can do here is keep using the
       * relative path.  (Perhaps / isn't readable by this UID/GID.) */
      log_warn(LD_GENERAL, "Unable to find current working directory: %s",
               strerror(errno));
      absfname = tor_strdup(fname);
    }
  }
  return absfname;
#endif
}

#ifndef HAVE__NSGETENVIRON
#ifndef HAVE_EXTERN_ENVIRON_DECLARED
/* Some platforms declare environ under some circumstances, others don't. */
#ifndef RUNNING_DOXYGEN
extern char **environ;
#endif
#endif
#endif

/** Return the current environment. This is a portable replacement for
 * 'environ'. */
char **
get_environment(void)
{
#ifdef HAVE__NSGETENVIRON
  /* This is for compatibility between OSX versions.  Otherwise (for example)
   * when we do a mostly-static build on OSX 10.7, the resulting binary won't
   * work on OSX 10.6. */
  return *_NSGetEnviron();
#else
  return environ;
#endif
}

/** Set *addr to the IP address (in dotted-quad notation) stored in c.
 * Return 1 on success, 0 if c is badly formatted.  (Like inet_aton(c,addr),
 * but works on Windows and Solaris.)
 */
int
tor_inet_aton(const char *str, struct in_addr* addr)
{
  unsigned a,b,c,d;
  char more;
  if (tor_sscanf(str, "%3u.%3u.%3u.%3u%c", &a,&b,&c,&d,&more) != 4)
    return 0;
  if (a > 255) return 0;
  if (b > 255) return 0;
  if (c > 255) return 0;
  if (d > 255) return 0;
  addr->s_addr = htonl((a<<24) | (b<<16) | (c<<8) | d);
  return 1;
}

/** Given <b>af</b>==AF_INET and <b>src</b> a struct in_addr, or
 * <b>af</b>==AF_INET6 and <b>src</b> a struct in6_addr, try to format the
 * address and store it in the <b>len</b>-byte buffer <b>dst</b>.  Returns
 * <b>dst</b> on success, NULL on failure.
 *
 * (Like inet_ntop(af,src,dst,len), but works on platforms that don't have it:
 * Tor sometimes needs to format ipv6 addresses even on platforms without ipv6
 * support.) */
const char *
tor_inet_ntop(int af, const void *src, char *dst, size_t len)
{
  if (af == AF_INET) {
    if (tor_inet_ntoa(src, dst, len) < 0)
      return NULL;
    else
      return dst;
  } else if (af == AF_INET6) {
    const struct in6_addr *addr = src;
    char buf[64], *cp;
    int longestGapLen = 0, longestGapPos = -1, i,
      curGapPos = -1, curGapLen = 0;
    uint16_t words[8];
    for (i = 0; i < 8; ++i) {
      words[i] = (((uint16_t)addr->s6_addr[2*i])<<8) + addr->s6_addr[2*i+1];
    }
    if (words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 &&
        words[4] == 0 && ((words[5] == 0 && words[6] && words[7]) ||
                          (words[5] == 0xffff))) {
      /* This is an IPv4 address. */
      if (words[5] == 0) {
        tor_snprintf(buf, sizeof(buf), "::%d.%d.%d.%d",
                     addr->s6_addr[12], addr->s6_addr[13],
                     addr->s6_addr[14], addr->s6_addr[15]);
      } else {
        tor_snprintf(buf, sizeof(buf), "::%x:%d.%d.%d.%d", words[5],
                     addr->s6_addr[12], addr->s6_addr[13],
                     addr->s6_addr[14], addr->s6_addr[15]);
      }
      if ((strlen(buf) + 1) > len) /* +1 for \0 */
        return NULL;
      strlcpy(dst, buf, len);
      return dst;
    }
    i = 0;
    while (i < 8) {
      if (words[i] == 0) {
        curGapPos = i++;
        curGapLen = 1;
        while (i<8 && words[i] == 0) {
          ++i; ++curGapLen;
        }
        if (curGapLen > longestGapLen) {
          longestGapPos = curGapPos;
          longestGapLen = curGapLen;
        }
      } else {
        ++i;
      }
    }
    if (longestGapLen<=1)
      longestGapPos = -1;

    cp = buf;
    for (i = 0; i < 8; ++i) {
      if (words[i] == 0 && longestGapPos == i) {
        if (i == 0)
          *cp++ = ':';
        *cp++ = ':';
        while (i < 8 && words[i] == 0)
          ++i;
        --i; /* to compensate for loop increment. */
      } else {
        tor_snprintf(cp, sizeof(buf)-(cp-buf), "%x", (unsigned)words[i]);
        cp += strlen(cp);
        if (i != 7)
          *cp++ = ':';
      }
    }
    *cp = '\0';
    if ((strlen(buf) + 1) > len) /* +1 for \0 */
      return NULL;
    strlcpy(dst, buf, len);
    return dst;
  } else {
    return NULL;
  }
}

/** Given <b>af</b>==AF_INET or <b>af</b>==AF_INET6, and a string <b>src</b>
 * encoding an IPv4 address or IPv6 address correspondingly, try to parse the
 * address and store the result in <b>dst</b> (which must have space for a
 * struct in_addr or a struct in6_addr, as appropriate).  Return 1 on success,
 * 0 on a bad parse, and -1 on a bad <b>af</b>.
 *
 * (Like inet_pton(af,src,dst) but works on platforms that don't have it: Tor
 * sometimes needs to format ipv6 addresses even on platforms without ipv6
 * support.) */
int
tor_inet_pton(int af, const char *src, void *dst)
{
  if (af == AF_INET) {
    return tor_inet_aton(src, dst);
  } else if (af == AF_INET6) {
    struct in6_addr *out = dst;
    uint16_t words[8];
    int gapPos = -1, i, setWords=0;
    const char *dot = strchr(src, '.');
    const char *eow; /* end of words. */
    if (dot == src)
      return 0;
    else if (!dot)
      eow = src+strlen(src);
    else {
      unsigned byte1,byte2,byte3,byte4;
      char more;
      for (eow = dot-1; eow > src && TOR_ISDIGIT(*eow); --eow)
        ;
      if (*eow != ':')
        return 0;
      ++eow;

      /* We use "scanf" because some platform inet_aton()s are too lax
       * about IPv4 addresses of the form "1.2.3" */
      if (tor_sscanf(eow, "%3u.%3u.%3u.%3u%c",
                     &byte1,&byte2,&byte3,&byte4,&more) != 4)
        return 0;

      if (byte1 > 255 || byte2 > 255 || byte3 > 255 || byte4 > 255)
        return 0;

      words[6] = (byte1<<8) | byte2;
      words[7] = (byte3<<8) | byte4;
      setWords += 2;
    }

    i = 0;
    while (src < eow) {
      if (i > 7)
        return 0;
      if (TOR_ISXDIGIT(*src)) {
        char *next;
        ssize_t len;
        long r = strtol(src, &next, 16);
        tor_assert(next != NULL);
        tor_assert(next != src);

        len = *next == '\0' ? eow - src : next - src;
        if (len > 4)
          return 0;
        if (len > 1 && !TOR_ISXDIGIT(src[1]))
          return 0; /* 0x is not valid */

        tor_assert(r >= 0);
        tor_assert(r < 65536);
        words[i++] = (uint16_t)r;
        setWords++;
        src = next;
        if (*src != ':' && src != eow)
          return 0;
        ++src;
      } else if (*src == ':' && i > 0 && gapPos == -1) {
        gapPos = i;
        ++src;
      } else if (*src == ':' && i == 0 && src+1 < eow && src[1] == ':' &&
                 gapPos == -1) {
        gapPos = i;
        src += 2;
      } else {
        return 0;
      }
    }

    if (setWords > 8 ||
        (setWords == 8 && gapPos != -1) ||
        (setWords < 8 && gapPos == -1))
      return 0;

    if (gapPos >= 0) {
      int nToMove = setWords - (dot ? 2 : 0) - gapPos;
      int gapLen = 8 - setWords;
      tor_assert(nToMove >= 0);
      memmove(&words[gapPos+gapLen], &words[gapPos],
              sizeof(uint16_t)*nToMove);
      memset(&words[gapPos], 0, sizeof(uint16_t)*gapLen);
    }
    for (i = 0; i < 8; ++i) {
      out->s6_addr[2*i  ] = words[i] >> 8;
      out->s6_addr[2*i+1] = words[i] & 0xff;
    }

    return 1;
  } else {
    return -1;
  }
}

/** Similar behavior to Unix gethostbyname: resolve <b>name</b>, and set
 * *<b>addr</b> to the proper IP address, in host byte order.  Returns 0
 * on success, -1 on failure; 1 on transient failure.
 *
 * (This function exists because standard windows gethostbyname
 * doesn't treat raw IP addresses properly.)
 */
int
tor_lookup_hostname(const char *name, uint32_t *addr)
{
  tor_addr_t myaddr;
  int ret;

  if ((ret = tor_addr_lookup(name, AF_INET, &myaddr)))
    return ret;

  if (tor_addr_family(&myaddr) == AF_INET) {
    *addr = tor_addr_to_ipv4h(&myaddr);
    return ret;
  }

  return -1;
}

/** Hold the result of our call to <b>uname</b>. */
static char uname_result[256];
/** True iff uname_result is set. */
static int uname_result_is_set = 0;

/** Return a pointer to a description of our platform.
 */
const char *
get_uname(void)
{
#ifdef HAVE_UNAME
  struct utsname u;
#endif
  if (!uname_result_is_set) {
#ifdef HAVE_UNAME
    if (uname(&u) != -1) {
      /* (Linux says 0 is success, Solaris says 1 is success) */
      strlcpy(uname_result, u.sysname, sizeof(uname_result));
    } else
#endif
      {
#ifdef _WIN32
        OSVERSIONINFOEX info;
        int i;
        const char *plat = NULL;
        static struct {
          unsigned major; unsigned minor; const char *version;
        } win_version_table[] = {
          { 6, 2, "Windows 8" },
          { 6, 1, "Windows 7" },
          { 6, 0, "Windows Vista" },
          { 5, 2, "Windows Server 2003" },
          { 5, 1, "Windows XP" },
          { 5, 0, "Windows 2000" },
          /* { 4, 0, "Windows NT 4.0" }, */
          { 4, 90, "Windows Me" },
          { 4, 10, "Windows 98" },
          /* { 4, 0, "Windows 95" } */
          { 3, 51, "Windows NT 3.51" },
          { 0, 0, NULL }
        };
        memset(&info, 0, sizeof(info));
        info.dwOSVersionInfoSize = sizeof(info);
        if (! GetVersionEx((LPOSVERSIONINFO)&info)) {
          strlcpy(uname_result, "Bizarre version of Windows where GetVersionEx"
                  " doesn't work.", sizeof(uname_result));
          uname_result_is_set = 1;
          return uname_result;
        }
        if (info.dwMajorVersion == 4 && info.dwMinorVersion == 0) {
          if (info.dwPlatformId == VER_PLATFORM_WIN32_NT)
            plat = "Windows NT 4.0";
          else
            plat = "Windows 95";
        } else {
          for (i=0; win_version_table[i].major>0; ++i) {
            if (win_version_table[i].major == info.dwMajorVersion &&
                win_version_table[i].minor == info.dwMinorVersion) {
              plat = win_version_table[i].version;
              break;
            }
          }
        }
        if (plat) {
          strlcpy(uname_result, plat, sizeof(uname_result));
        } else {
          if (info.dwMajorVersion > 6 ||
              (info.dwMajorVersion==6 && info.dwMinorVersion>2))
            tor_snprintf(uname_result, sizeof(uname_result),
                         "Very recent version of Windows [major=%d,minor=%d]",
                         (int)info.dwMajorVersion,(int)info.dwMinorVersion);
          else
            tor_snprintf(uname_result, sizeof(uname_result),
                         "Unrecognized version of Windows [major=%d,minor=%d]",
                         (int)info.dwMajorVersion,(int)info.dwMinorVersion);
        }
#if !defined (WINCE)
#ifdef VER_NT_SERVER
      if (info.wProductType == VER_NT_SERVER ||
          info.wProductType == VER_NT_DOMAIN_CONTROLLER) {
        strlcat(uname_result, " [server]", sizeof(uname_result));
      }
#endif
#endif
#else
        strlcpy(uname_result, "Unknown platform", sizeof(uname_result));
#endif
      }
    uname_result_is_set = 1;
  }
  return uname_result;
}

/*
 *   Process control
 */

#if defined(USE_PTHREADS)
/** Wraps a void (*)(void*) function and its argument so we can
 * invoke them in a way pthreads would expect.
 */
typedef struct tor_pthread_data_t {
  void (*func)(void *);
  void *data;
} tor_pthread_data_t;
/** Given a tor_pthread_data_t <b>_data</b>, call _data-&gt;func(d-&gt;data)
 * and free _data.  Used to make sure we can call functions the way pthread
 * expects. */
static void *
tor_pthread_helper_fn(void *_data)
{
  tor_pthread_data_t *data = _data;
  void (*func)(void*);
  void *arg;
  /* mask signals to worker threads to avoid SIGPIPE, etc */
  sigset_t sigs;
  /* We're in a subthread; don't handle any signals here. */
  sigfillset(&sigs);
  pthread_sigmask(SIG_SETMASK, &sigs, NULL);

  func = data->func;
  arg = data->data;
  tor_free(_data);
  func(arg);
  return NULL;
}
/**
 * A pthread attribute to make threads start detached.
 */
static pthread_attr_t attr_detached;
/** True iff we've called tor_threads_init() */
static int threads_initialized = 0;
#endif

/** Minimalist interface to run a void function in the background.  On
 * Unix calls fork, on win32 calls beginthread.  Returns -1 on failure.
 * func should not return, but rather should call spawn_exit.
 *
 * NOTE: if <b>data</b> is used, it should not be allocated on the stack,
 * since in a multithreaded environment, there is no way to be sure that
 * the caller's stack will still be around when the called function is
 * running.
 */
int
spawn_func(void (*func)(void *), void *data)
{
#if defined(USE_WIN32_THREADS)
  int rv;
  rv = (int)_beginthread(func, 0, data);
  if (rv == (int)-1)
    return -1;
  return 0;
#elif defined(USE_PTHREADS)
  pthread_t thread;
  tor_pthread_data_t *d;
  if (PREDICT_UNLIKELY(!threads_initialized))
    tor_threads_init();
  d = tor_malloc(sizeof(tor_pthread_data_t));
  d->data = data;
  d->func = func;
  if (pthread_create(&thread,&attr_detached,tor_pthread_helper_fn,d))
    return -1;
  return 0;
#else
  pid_t pid;
  pid = fork();
  if (pid<0)
    return -1;
  if (pid==0) {
    /* Child */
    func(data);
    tor_assert(0); /* Should never reach here. */
    return 0; /* suppress "control-reaches-end-of-non-void" warning. */
  } else {
    /* Parent */
    return 0;
  }
#endif
}

/** End the current thread/process.
 */
void
spawn_exit(void)
{
#if defined(USE_WIN32_THREADS)
  _endthread();
  //we should never get here. my compiler thinks that _endthread returns, this
  //is an attempt to fool it.
  tor_assert(0);
  _exit(0);
#elif defined(USE_PTHREADS)
  pthread_exit(NULL);
#else
  /* http://www.erlenstar.demon.co.uk/unix/faq_2.html says we should
   * call _exit, not exit, from child processes. */
  _exit(0);
#endif
}

/** Implementation logic for compute_num_cpus(). */
static int
compute_num_cpus_impl(void)
{
#ifdef _WIN32
  SYSTEM_INFO info;
  memset(&info, 0, sizeof(info));
  GetSystemInfo(&info);
  if (info.dwNumberOfProcessors >= 1 && info.dwNumberOfProcessors < INT_MAX)
    return (int)info.dwNumberOfProcessors;
  else
    return -1;
#elif defined(HAVE_SYSCONF)
#ifdef _SC_NPROCESSORS_CONF
  long cpus_conf = sysconf(_SC_NPROCESSORS_CONF);
#else
  long cpus_conf = -1;
#endif
#ifdef _SC_NPROCESSORS_ONLN
  long cpus_onln = sysconf(_SC_NPROCESSORS_ONLN);
#else
  long cpus_onln = -1;
#endif
  long cpus = -1;

  if (cpus_conf > 0 && cpus_onln < 0) {
    cpus = cpus_conf;
  } else if (cpus_onln > 0 && cpus_conf < 0) {
    cpus = cpus_onln;
  } else if (cpus_onln > 0 && cpus_conf > 0) {
    if (cpus_onln < cpus_conf) {
      log_notice(LD_GENERAL, "I think we have %ld CPUS, but only %ld of them "
                 "are available. Telling Tor to only use %ld. You can over"
                 "ride this with the NumCPUs option",
                 cpus_conf, cpus_onln, cpus_onln);
    }
    cpus = cpus_onln;
  }

  if (cpus >= 1 && cpus < INT_MAX)
    return (int)cpus;
  else
    return -1;
#else
  return -1;
#endif
}

#define MAX_DETECTABLE_CPUS 16

/** Return how many CPUs we are running with.  We assume that nobody is
 * using hot-swappable CPUs, so we don't recompute this after the first
 * time.  Return -1 if we don't know how to tell the number of CPUs on this
 * system.
 */
int
compute_num_cpus(void)
{
  static int num_cpus = -2;
  if (num_cpus == -2) {
    num_cpus = compute_num_cpus_impl();
    tor_assert(num_cpus != -2);
    if (num_cpus > MAX_DETECTABLE_CPUS)
      log_notice(LD_GENERAL, "Wow!  I detected that you have %d CPUs. I "
                 "will not autodetect any more than %d, though.  If you "
                 "want to configure more, set NumCPUs in your torrc",
                 num_cpus, MAX_DETECTABLE_CPUS);
  }
  return num_cpus;
}

/** Set *timeval to the current time of day.  On error, log and terminate.
 * (Same as gettimeofday(timeval,NULL), but never returns -1.)
 */
void
tor_gettimeofday(struct timeval *timeval)
{
#ifdef _WIN32
  /* Epoch bias copied from perl: number of units between windows epoch and
   * Unix epoch. */
#define EPOCH_BIAS U64_LITERAL(116444736000000000)
#define UNITS_PER_SEC U64_LITERAL(10000000)
#define USEC_PER_SEC U64_LITERAL(1000000)
#define UNITS_PER_USEC U64_LITERAL(10)
  union {
    uint64_t ft_64;
    FILETIME ft_ft;
  } ft;
#if defined (WINCE)
  /* wince do not have GetSystemTimeAsFileTime */
  SYSTEMTIME stime;
  GetSystemTime(&stime);
  SystemTimeToFileTime(&stime,&ft.ft_ft);
#else
  /* number of 100-nsec units since Jan 1, 1601 */
  GetSystemTimeAsFileTime(&ft.ft_ft);
#endif
  if (ft.ft_64 < EPOCH_BIAS) {
    log_err(LD_GENERAL,"System time is before 1970; failing.");
    exit(1);
  }
  ft.ft_64 -= EPOCH_BIAS;
  timeval->tv_sec = (unsigned) (ft.ft_64 / UNITS_PER_SEC);
  timeval->tv_usec = (unsigned) ((ft.ft_64 / UNITS_PER_USEC) % USEC_PER_SEC);
#elif defined(HAVE_GETTIMEOFDAY)
  if (gettimeofday(timeval, NULL)) {
    log_err(LD_GENERAL,"gettimeofday failed.");
    /* If gettimeofday dies, we have either given a bad timezone (we didn't),
       or segfaulted.*/
    exit(1);
  }
#elif defined(HAVE_FTIME)
  struct timeb tb;
  ftime(&tb);
  timeval->tv_sec = tb.time;
  timeval->tv_usec = tb.millitm * 1000;
#else
#error "No way to get time."
#endif
  return;
}

#if defined(TOR_IS_MULTITHREADED) && !defined(_WIN32)
/** Defined iff we need to add locks when defining fake versions of reentrant
 * versions of time-related functions. */
#define TIME_FNS_NEED_LOCKS
#endif

/** Helper: Deal with confused or out-of-bounds values from localtime_r and
 * friends.  (On some platforms, they can give out-of-bounds values or can
 * return NULL.)  If <b>islocal</b>, this is a localtime result; otherwise
 * it's from gmtime.  The function returned <b>r</b>, when given <b>timep</b>
 * as its input. If we need to store new results, store them in
 * <b>resultbuf</b>. */
static struct tm *
correct_tm(int islocal, const time_t *timep, struct tm *resultbuf,
           struct tm *r)
{
  const char *outcome;

  if (PREDICT_LIKELY(r)) {
    if (r->tm_year > 8099) { /* We can't strftime dates after 9999 CE. */
      r->tm_year = 8099;
      r->tm_mon = 11;
      r->tm_mday = 31;
      r->tm_yday = 365;
      r->tm_hour = 23;
      r->tm_min = 59;
      r->tm_sec = 59;
    }
    return r;
  }

  /* If we get here, gmtime or localtime returned NULL. It might have done
   * this because of overrun or underrun, or it might have done it because of
   * some other weird issue. */
  if (timep) {
    if (*timep < 0) {
      r = resultbuf;
      r->tm_year = 70; /* 1970 CE */
      r->tm_mon = 0;
      r->tm_mday = 1;
      r->tm_yday = 1;
      r->tm_hour = 0;
      r->tm_min = 0 ;
      r->tm_sec = 0;
      outcome = "Rounding up to 1970";
      goto done;
    } else if (*timep >= INT32_MAX) {
      /* Rounding down to INT32_MAX isn't so great, but keep in mind that we
       * only do it if gmtime/localtime tells us NULL. */
      r = resultbuf;
      r->tm_year = 137; /* 2037 CE */
      r->tm_mon = 11;
      r->tm_mday = 31;
      r->tm_yday = 365;
      r->tm_hour = 23;
      r->tm_min = 59;
      r->tm_sec = 59;
      outcome = "Rounding down to 2037";
      goto done;
    }
  }

  /* If we get here, then gmtime/localtime failed without getting an extreme
   * value for *timep */

  tor_fragile_assert();
  r = resultbuf;
  memset(resultbuf, 0, sizeof(struct tm));
  outcome="can't recover";
 done:
  log_warn(LD_BUG, "%s("I64_FORMAT") failed with error %s: %s",
           islocal?"localtime":"gmtime",
           timep?I64_PRINTF_ARG(*timep):0,
           strerror(errno),
           outcome);
  return r;
}

/** @{ */
/** As localtime_r, but defined for platforms that don't have it:
 *
 * Convert *<b>timep</b> to a struct tm in local time, and store the value in
 * *<b>result</b>.  Return the result on success, or NULL on failure.
 */
#ifdef HAVE_LOCALTIME_R
struct tm *
tor_localtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  r = localtime_r(timep, result);
  return correct_tm(1, timep, result, r);
}
#elif defined(TIME_FNS_NEED_LOCKS)
struct tm *
tor_localtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  static tor_mutex_t *m=NULL;
  if (!m) { m=tor_mutex_new(); }
  tor_assert(result);
  tor_mutex_acquire(m);
  r = localtime(timep);
  if (r)
    memcpy(result, r, sizeof(struct tm));
  tor_mutex_release(m);
  return correct_tm(1, timep, result, r);
}
#else
struct tm *
tor_localtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  tor_assert(result);
  r = localtime(timep);
  if (r)
    memcpy(result, r, sizeof(struct tm));
  return correct_tm(1, timep, result, r);
}
#endif
/** @} */

/** @{ */
/** As gmtimee_r, but defined for platforms that don't have it:
 *
 * Convert *<b>timep</b> to a struct tm in UTC, and store the value in
 * *<b>result</b>.  Return the result on success, or NULL on failure.
 */
#ifdef HAVE_GMTIME_R
struct tm *
tor_gmtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  r = gmtime_r(timep, result);
  return correct_tm(0, timep, result, r);
}
#elif defined(TIME_FNS_NEED_LOCKS)
struct tm *
tor_gmtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  static tor_mutex_t *m=NULL;
  if (!m) { m=tor_mutex_new(); }
  tor_assert(result);
  tor_mutex_acquire(m);
  r = gmtime(timep);
  if (r)
    memcpy(result, r, sizeof(struct tm));
  tor_mutex_release(m);
  return correct_tm(0, timep, result, r);
}
#else
struct tm *
tor_gmtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
  tor_assert(result);
  r = gmtime(timep);
  if (r)
    memcpy(result, r, sizeof(struct tm));
  return correct_tm(0, timep, result, r);
}
#endif

#if defined(USE_WIN32_THREADS)
void
tor_mutex_init(tor_mutex_t *m)
{
  InitializeCriticalSection(&m->mutex);
}
void
tor_mutex_uninit(tor_mutex_t *m)
{
  DeleteCriticalSection(&m->mutex);
}
void
tor_mutex_acquire(tor_mutex_t *m)
{
  tor_assert(m);
  EnterCriticalSection(&m->mutex);
}
void
tor_mutex_release(tor_mutex_t *m)
{
  LeaveCriticalSection(&m->mutex);
}
unsigned long
tor_get_thread_id(void)
{
  return (unsigned long)GetCurrentThreadId();
}
#elif defined(USE_PTHREADS)
/** A mutex attribute that we're going to use to tell pthreads that we want
 * "reentrant" mutexes (i.e., once we can re-lock if we're already holding
 * them.) */
static pthread_mutexattr_t attr_reentrant;
/** Initialize <b>mutex</b> so it can be locked.  Every mutex must be set
 * up with tor_mutex_init() or tor_mutex_new(); not both. */
void
tor_mutex_init(tor_mutex_t *mutex)
{
  int err;
  if (PREDICT_UNLIKELY(!threads_initialized))
    tor_threads_init();
  err = pthread_mutex_init(&mutex->mutex, &attr_reentrant);
  if (PREDICT_UNLIKELY(err)) {
    log_err(LD_GENERAL, "Error %d creating a mutex.", err);
    tor_fragile_assert();
  }
}
/** Wait until <b>m</b> is free, then acquire it. */
void
tor_mutex_acquire(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_lock(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    log_err(LD_GENERAL, "Error %d locking a mutex.", err);
    tor_fragile_assert();
  }
}
/** Release the lock <b>m</b> so another thread can have it. */
void
tor_mutex_release(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_unlock(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    log_err(LD_GENERAL, "Error %d unlocking a mutex.", err);
    tor_fragile_assert();
  }
}
/** Clean up the mutex <b>m</b> so that it no longer uses any system
 * resources.  Does not free <b>m</b>.  This function must only be called on
 * mutexes from tor_mutex_init(). */
void
tor_mutex_uninit(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_destroy(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    log_err(LD_GENERAL, "Error %d destroying a mutex.", err);
    tor_fragile_assert();
  }
}
/** Return an integer representing this thread. */
unsigned long
tor_get_thread_id(void)
{
  union {
    pthread_t thr;
    unsigned long id;
  } r;
  r.thr = pthread_self();
  return r.id;
}
#endif

#ifdef TOR_IS_MULTITHREADED
/** Return a newly allocated, ready-for-use mutex. */
tor_mutex_t *
tor_mutex_new(void)
{
  tor_mutex_t *m = tor_malloc_zero(sizeof(tor_mutex_t));
  tor_mutex_init(m);
  return m;
}
/** Release all storage and system resources held by <b>m</b>. */
void
tor_mutex_free(tor_mutex_t *m)
{
  if (!m)
    return;
  tor_mutex_uninit(m);
  tor_free(m);
}
#endif

/* Conditions. */
#ifdef USE_PTHREADS
#if 0
/** Cross-platform condition implementation. */
struct tor_cond_t {
  pthread_cond_t cond;
};
/** Return a newly allocated condition, with nobody waiting on it. */
tor_cond_t *
tor_cond_new(void)
{
  tor_cond_t *cond = tor_malloc_zero(sizeof(tor_cond_t));
  if (pthread_cond_init(&cond->cond, NULL)) {
    tor_free(cond);
    return NULL;
  }
  return cond;
}
/** Release all resources held by <b>cond</b>. */
void
tor_cond_free(tor_cond_t *cond)
{
  if (!cond)
    return;
  if (pthread_cond_destroy(&cond->cond)) {
    log_warn(LD_GENERAL,"Error freeing condition: %s", strerror(errno));
    return;
  }
  tor_free(cond);
}
/** Wait until one of the tor_cond_signal functions is called on <b>cond</b>.
 * All waiters on the condition must wait holding the same <b>mutex</b>.
 * Returns 0 on success, negative on failure. */
int
tor_cond_wait(tor_cond_t *cond, tor_mutex_t *mutex)
{
  return pthread_cond_wait(&cond->cond, &mutex->mutex) ? -1 : 0;
}
/** Wake up one of the waiters on <b>cond</b>. */
void
tor_cond_signal_one(tor_cond_t *cond)
{
  pthread_cond_signal(&cond->cond);
}
/** Wake up all of the waiters on <b>cond</b>. */
void
tor_cond_signal_all(tor_cond_t *cond)
{
  pthread_cond_broadcast(&cond->cond);
}
#endif
/** Set up common structures for use by threading. */
void
tor_threads_init(void)
{
  if (!threads_initialized) {
    pthread_mutexattr_init(&attr_reentrant);
    pthread_mutexattr_settype(&attr_reentrant, PTHREAD_MUTEX_RECURSIVE);
    tor_assert(0==pthread_attr_init(&attr_detached));
    tor_assert(0==pthread_attr_setdetachstate(&attr_detached, 1));
    threads_initialized = 1;
    set_main_thread();
  }
}
#elif defined(USE_WIN32_THREADS)
#if 0
static DWORD cond_event_tls_index;
struct tor_cond_t {
  CRITICAL_SECTION mutex;
  smartlist_t *events;
};
tor_cond_t *
tor_cond_new(void)
{
  tor_cond_t *cond = tor_malloc_zero(sizeof(tor_cond_t));
  InitializeCriticalSection(&cond->mutex);
  cond->events = smartlist_new();
  return cond;
}
void
tor_cond_free(tor_cond_t *cond)
{
  if (!cond)
    return;
  DeleteCriticalSection(&cond->mutex);
  /* XXXX notify? */
  smartlist_free(cond->events);
  tor_free(cond);
}
int
tor_cond_wait(tor_cond_t *cond, tor_mutex_t *mutex)
{
  HANDLE event;
  int r;
  tor_assert(cond);
  tor_assert(mutex);
  event = TlsGetValue(cond_event_tls_index);
  if (!event) {
    event = CreateEvent(0, FALSE, FALSE, NULL);
    TlsSetValue(cond_event_tls_index, event);
  }
  EnterCriticalSection(&cond->mutex);

  tor_assert(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);
  tor_assert(!smartlist_contains(cond->events, event));
  smartlist_add(cond->events, event);

  LeaveCriticalSection(&cond->mutex);

  tor_mutex_release(mutex);
  r = WaitForSingleObject(event, INFINITE);
  tor_mutex_acquire(mutex);

  switch (r) {
    case WAIT_OBJECT_0: /* we got the mutex normally. */
      break;
    case WAIT_ABANDONED: /* holding thread exited. */
    case WAIT_TIMEOUT: /* Should never happen. */
      tor_assert(0);
      break;
    case WAIT_FAILED:
      log_warn(LD_GENERAL, "Failed to acquire mutex: %d",(int) GetLastError());
  }
  return 0;
}
void
tor_cond_signal_one(tor_cond_t *cond)
{
  HANDLE event;
  tor_assert(cond);

  EnterCriticalSection(&cond->mutex);

  if ((event = smartlist_pop_last(cond->events)))
    SetEvent(event);

  LeaveCriticalSection(&cond->mutex);
}
void
tor_cond_signal_all(tor_cond_t *cond)
{
  tor_assert(cond);

  EnterCriticalSection(&cond->mutex);
  SMARTLIST_FOREACH(cond->events, HANDLE, event, SetEvent(event));
  smartlist_clear(cond->events);
  LeaveCriticalSection(&cond->mutex);
}
#endif
void
tor_threads_init(void)
{
#if 0
  cond_event_tls_index = TlsAlloc();
#endif
  set_main_thread();
}
#endif

#if defined(HAVE_MLOCKALL) && HAVE_DECL_MLOCKALL && defined(RLIMIT_MEMLOCK)
/** Attempt to raise the current and max rlimit to infinity for our process.
 * This only needs to be done once and can probably only be done when we have
 * not already dropped privileges.
 */
static int
tor_set_max_memlock(void)
{
  /* Future consideration for Windows is probably SetProcessWorkingSetSize
   * This is similar to setting the memory rlimit of RLIMIT_MEMLOCK
   * http://msdn.microsoft.com/en-us/library/ms686234(VS.85).aspx
   */

  struct rlimit limit;

  /* RLIM_INFINITY is -1 on some platforms. */
  limit.rlim_cur = RLIM_INFINITY;
  limit.rlim_max = RLIM_INFINITY;

  if (setrlimit(RLIMIT_MEMLOCK, &limit) == -1) {
    if (errno == EPERM) {
      log_warn(LD_GENERAL, "You appear to lack permissions to change memory "
                           "limits. Are you root?");
    }
    log_warn(LD_GENERAL, "Unable to raise RLIMIT_MEMLOCK: %s",
             strerror(errno));
    return -1;
  }

  return 0;
}
#endif

/** Attempt to lock all current and all future memory pages.
 * This should only be called once and while we're privileged.
 * Like mlockall() we return 0 when we're successful and -1 when we're not.
 * Unlike mlockall() we return 1 if we've already attempted to lock memory.
 */
int
tor_mlockall(void)
{
  static int memory_lock_attempted = 0;

  if (memory_lock_attempted) {
    return 1;
  }

  memory_lock_attempted = 1;

  /*
   * Future consideration for Windows may be VirtualLock
   * VirtualLock appears to implement mlock() but not mlockall()
   *
   * http://msdn.microsoft.com/en-us/library/aa366895(VS.85).aspx
   */

#if defined(HAVE_MLOCKALL) && HAVE_DECL_MLOCKALL && defined(RLIMIT_MEMLOCK)
  if (tor_set_max_memlock() == 0) {
    log_debug(LD_GENERAL, "RLIMIT_MEMLOCK is now set to RLIM_INFINITY.");
  }

  if (mlockall(MCL_CURRENT|MCL_FUTURE) == 0) {
    log_info(LD_GENERAL, "Insecure OS paging is effectively disabled.");
    return 0;
  } else {
    if (errno == ENOSYS) {
      /* Apple - it's 2009! I'm looking at you. Grrr. */
      log_notice(LD_GENERAL, "It appears that mlockall() is not available on "
                             "your platform.");
    } else if (errno == EPERM) {
      log_notice(LD_GENERAL, "It appears that you lack the permissions to "
                             "lock memory. Are you root?");
    }
    log_notice(LD_GENERAL, "Unable to lock all current and future memory "
                           "pages: %s", strerror(errno));
    return -1;
  }
#else
  log_warn(LD_GENERAL, "Unable to lock memory pages. mlockall() unsupported?");
  return -1;
#endif
}

/** Identity of the "main" thread */
static unsigned long main_thread_id = -1;

/** Start considering the current thread to be the 'main thread'.  This has
 * no effect on anything besides in_main_thread(). */
void
set_main_thread(void)
{
  main_thread_id = tor_get_thread_id();
}
/** Return true iff called from the main thread. */
int
in_main_thread(void)
{
  return main_thread_id == tor_get_thread_id();
}

/**
 * On Windows, WSAEWOULDBLOCK is not always correct: when you see it,
 * you need to ask the socket for its actual errno.  Also, you need to
 * get your errors from WSAGetLastError, not errno.  (If you supply a
 * socket of -1, we check WSAGetLastError, but don't correct
 * WSAEWOULDBLOCKs.)
 *
 * The upshot of all of this is that when a socket call fails, you
 * should call tor_socket_errno <em>at most once</em> on the failing
 * socket to get the error.
 */
#if defined(_WIN32)
int
tor_socket_errno(tor_socket_t sock)
{
  int optval, optvallen=sizeof(optval);
  int err = WSAGetLastError();
  if (err == WSAEWOULDBLOCK && SOCKET_OK(sock)) {
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&optval, &optvallen))
      return err;
    if (optval)
      return optval;
  }
  return err;
}
#endif

#if defined(_WIN32)
#define E(code, s) { code, (s " [" #code " ]") }
struct { int code; const char *msg; } windows_socket_errors[] = {
  E(WSAEINTR, "Interrupted function call"),
  E(WSAEACCES, "Permission denied"),
  E(WSAEFAULT, "Bad address"),
  E(WSAEINVAL, "Invalid argument"),
  E(WSAEMFILE, "Too many open files"),
  E(WSAEWOULDBLOCK,  "Resource temporarily unavailable"),
  E(WSAEINPROGRESS, "Operation now in progress"),
  E(WSAEALREADY, "Operation already in progress"),
  E(WSAENOTSOCK, "Socket operation on nonsocket"),
  E(WSAEDESTADDRREQ, "Destination address required"),
  E(WSAEMSGSIZE, "Message too long"),
  E(WSAEPROTOTYPE, "Protocol wrong for socket"),
  E(WSAENOPROTOOPT, "Bad protocol option"),
  E(WSAEPROTONOSUPPORT, "Protocol not supported"),
  E(WSAESOCKTNOSUPPORT, "Socket type not supported"),
  /* What's the difference between NOTSUPP and NOSUPPORT? :) */
  E(WSAEOPNOTSUPP, "Operation not supported"),
  E(WSAEPFNOSUPPORT,  "Protocol family not supported"),
  E(WSAEAFNOSUPPORT, "Address family not supported by protocol family"),
  E(WSAEADDRINUSE, "Address already in use"),
  E(WSAEADDRNOTAVAIL, "Cannot assign requested address"),
  E(WSAENETDOWN, "Network is down"),
  E(WSAENETUNREACH, "Network is unreachable"),
  E(WSAENETRESET, "Network dropped connection on reset"),
  E(WSAECONNABORTED, "Software caused connection abort"),
  E(WSAECONNRESET, "Connection reset by peer"),
  E(WSAENOBUFS, "No buffer space available"),
  E(WSAEISCONN, "Socket is already connected"),
  E(WSAENOTCONN, "Socket is not connected"),
  E(WSAESHUTDOWN, "Cannot send after socket shutdown"),
  E(WSAETIMEDOUT, "Connection timed out"),
  E(WSAECONNREFUSED, "Connection refused"),
  E(WSAEHOSTDOWN, "Host is down"),
  E(WSAEHOSTUNREACH, "No route to host"),
  E(WSAEPROCLIM, "Too many processes"),
  /* Yes, some of these start with WSA, not WSAE. No, I don't know why. */
  E(WSASYSNOTREADY, "Network subsystem is unavailable"),
  E(WSAVERNOTSUPPORTED, "Winsock.dll out of range"),
  E(WSANOTINITIALISED, "Successful WSAStartup not yet performed"),
  E(WSAEDISCON, "Graceful shutdown now in progress"),
#ifdef WSATYPE_NOT_FOUND
  E(WSATYPE_NOT_FOUND, "Class type not found"),
#endif
  E(WSAHOST_NOT_FOUND, "Host not found"),
  E(WSATRY_AGAIN, "Nonauthoritative host not found"),
  E(WSANO_RECOVERY, "This is a nonrecoverable error"),
  E(WSANO_DATA, "Valid name, no data record of requested type)"),

  /* There are some more error codes whose numeric values are marked
   * <b>OS dependent</b>. They start with WSA_, apparently for the same
   * reason that practitioners of some craft traditions deliberately
   * introduce imperfections into their baskets and rugs "to allow the
   * evil spirits to escape."  If we catch them, then our binaries
   * might not report consistent results across versions of Windows.
   * Thus, I'm going to let them all fall through.
   */
  { -1, NULL },
};
/** There does not seem to be a strerror equivalent for Winsock errors.
 * Naturally, we have to roll our own.
 */
const char *
tor_socket_strerror(int e)
{
  int i;
  for (i=0; windows_socket_errors[i].code >= 0; ++i) {
    if (e == windows_socket_errors[i].code)
      return windows_socket_errors[i].msg;
  }
  return strerror(e);
}
#endif

/** Called before we make any calls to network-related functions.
 * (Some operating systems require their network libraries to be
 * initialized.) */
int
network_init(void)
{
#ifdef _WIN32
  /* This silly exercise is necessary before windows will allow
   * gethostbyname to work. */
  WSADATA WSAData;
  int r;
  r = WSAStartup(0x101,&WSAData);
  if (r) {
    log_warn(LD_NET,"Error initializing windows network layer: code was %d",r);
    return -1;
  }
  if (sizeof(SOCKET) != sizeof(tor_socket_t)) {
    log_warn(LD_BUG,"The tor_socket_t type does not match SOCKET in size; Tor "
             "might not work. (Sizes are %d and %d respectively.)",
             (int)sizeof(tor_socket_t), (int)sizeof(SOCKET));
  }
  /* WSAData.iMaxSockets might show the max sockets we're allowed to use.
   * We might use it to complain if we're trying to be a server but have
   * too few sockets available. */
#endif
  return 0;
}

#ifdef _WIN32
/** Return a newly allocated string describing the windows system error code
 * <b>err</b>.  Note that error codes are different from errno.  Error codes
 * come from GetLastError() when a winapi call fails.  errno is set only when
 * ANSI functions fail.  Whee. */
char *
format_win32_error(DWORD err)
{
  TCHAR *str = NULL;
  char *result;
  DWORD n;

  /* Somebody once decided that this interface was better than strerror(). */
  n = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, err,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPVOID)&str,
                 0, NULL);

  if (str && n) {
#ifdef UNICODE
    size_t len;
    if (n > 128*1024)
      len = (128 * 1024) * 2 + 1; /* This shouldn't be possible, but let's
                                   * make sure. */
    else
      len = n * 2 + 1;
    result = tor_malloc(len);
    wcstombs(result,str,len);
    result[len-1] = '\0';
#else
    result = tor_strdup(str);
#endif
  } else {
    result = tor_strdup("<unformattable error>");
  }
  if (str) {
    LocalFree(str); /* LocalFree != free() */
  }
  return result;
}
#endif

#if defined(HW_PHYSMEM64)
/* This appears to be an OpenBSD thing */
#define INT64_HW_MEM HW_PHYSMEM64
#elif defined(HW_MEMSIZE)
/* OSX defines this one */
#define INT64_HW_MEM HW_MEMSIZE
#endif

/**
 * Helper: try to detect the total system memory, and return it. On failure,
 * return 0.
 */
static uint64_t
get_total_system_memory_impl(void)
{
#if defined(__linux__)
  /* On linux, sysctl is deprecated. Because proc is so awesome that you
   * shouldn't _want_ to write portable code, I guess? */
  unsigned long long result=0;
  int fd = -1;
  char *s = NULL;
  const char *cp;
  size_t file_size=0;
  if (-1 == (fd = tor_open_cloexec("/proc/meminfo",O_RDONLY,0)))
    return 0;
  s = read_file_to_str_until_eof(fd, 65536, &file_size);
  if (!s)
    goto err;
  cp = strstr(s, "MemTotal:");
  if (!cp)
    goto err;
  /* Use the system sscanf so that space will match a wider number of space */
  if (sscanf(cp, "MemTotal: %llu kB\n", &result) != 1)
    goto err;

  close(fd);
  tor_free(s);
  return result * 1024;

 err:
  tor_free(s);
  close(fd);
  return 0;
#elif defined (_WIN32)
  /* Windows has MEMORYSTATUSEX; pretty straightforward. */
  MEMORYSTATUSEX ms;
  memset(&ms, 0, sizeof(ms));
  ms.dwLength = sizeof(ms);
  if (! GlobalMemoryStatusEx(&ms))
    return 0;

  return ms.ullTotalPhys;

#elif defined(HAVE_SYSCTL) && defined(INT64_HW_MEM)
  /* On many systems, HW_PYHSMEM is clipped to 32 bits; let's use a better
   * variant if we know about it. */
  uint64_t memsize = 0;
  size_t len = sizeof(memsize);
  int mib[2] = {CTL_HW, INT64_HW_MEM};
  if (sysctl(mib,2,&memsize,&len,NULL,0))
    return 0;

  return memsize;

#elif defined(HAVE_SYSCTL) && defined(HW_PHYSMEM)
  /* On some systems (like FreeBSD I hope) you can use a size_t with
   * HW_PHYSMEM. */
  size_t memsize=0;
  size_t len = sizeof(memsize);
  int mib[2] = {CTL_HW, HW_USERMEM};
  if (sysctl(mib,2,&memsize,&len,NULL,0))
    return -1;

  return memsize;

#else
  /* I have no clue. */
  return 0;
#endif
}

/**
 * Try to find out how much physical memory the system has. On success,
 * return 0 and set *<b>mem_out</b> to that value. On failure, return -1.
 */
int
get_total_system_memory(size_t *mem_out)
{
  static size_t mem_cached=0;
  uint64_t m = get_total_system_memory_impl();
  if (0 == m) {
    /* We couldn't find our memory total */
    if (0 == mem_cached) {
      /* We have no cached value either */
      *mem_out = 0;
      return -1;
    }

    *mem_out = mem_cached;
    return 0;
  }

#if SIZE_T_MAX != UINT64_MAX
  if (m > SIZE_T_MAX) {
    /* I think this could happen if we're a 32-bit Tor running on a 64-bit
     * system: we could have more system memory than would fit in a
     * size_t. */
    m = SIZE_T_MAX;
  }
#endif

  *mem_out = mem_cached = (size_t) m;

  return 0;
}

#ifdef TOR_UNIT_TESTS
/** Delay for <b>msec</b> milliseconds.  Only used in tests. */
void
tor_sleep_msec(int msec)
{
#ifdef _WIN32
  Sleep(msec);
#elif defined(HAVE_USLEEP)
  sleep(msec / 1000);
  /* Some usleep()s hate sleeping more than 1 sec */
  usleep((msec % 1000) * 1000);
#elif defined(HAVE_SYS_SELECT_H)
  struct timeval tv = { msec / 1000, (msec % 1000) * 1000};
  select(0, NULL, NULL, NULL, &tv);
#else
  sleep(CEIL_DIV(msec, 1000));
#endif
}
#endif


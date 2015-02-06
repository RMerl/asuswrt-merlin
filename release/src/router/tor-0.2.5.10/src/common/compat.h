/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_COMPAT_H
#define TOR_COMPAT_H

#include "orconfig.h"
#include "torint.h"
#include "testsupport.h"
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER) && (_MSC_VER < 1300)
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#if defined(HAVE_PTHREAD_H) && !defined(_WIN32)
#include <pthread.h>
#endif
#include <stdarg.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif

#include <stdio.h>
#include <errno.h>

#if defined (WINCE)
#include <fcntl.h>
#include <io.h>
#include <math.h>
#include <projects.h>
/* this is not exported as W .... */
#define SHGetPathFromIDListW SHGetPathFromIDList
/* wcecompat has vasprintf */
#define HAVE_VASPRINTF
/* no service here */
#ifdef NT_SERVICE
#undef NT_SERVICE
#endif
#endif // WINCE

#ifndef NULL_REP_IS_ZERO_BYTES
#error "It seems your platform does not represent NULL as zero. We can't cope."
#endif

#ifndef DOUBLE_0_REP_IS_ZERO_BYTES
#error "It seems your platform does not represent 0.0 as zeros. We can't cope."
#endif

#if 'a'!=97 || 'z'!=122 || 'A'!=65 || ' '!=32
#error "It seems that you encode characters in something other than ASCII."
#endif

/* ===== Compiler compatibility */

/* GCC can check printf and scanf types on arbitrary functions. */
#ifdef __GNUC__
#define CHECK_PRINTF(formatIdx, firstArg) \
   __attribute__ ((format(printf, formatIdx, firstArg)))
#else
#define CHECK_PRINTF(formatIdx, firstArg)
#endif
#ifdef __GNUC__
#define CHECK_SCANF(formatIdx, firstArg) \
   __attribute__ ((format(scanf, formatIdx, firstArg)))
#else
#define CHECK_SCANF(formatIdx, firstArg)
#endif

/* inline is __inline on windows. */
#ifdef _WIN32
#define INLINE __inline
#else
#define INLINE inline
#endif

/* Try to get a reasonable __func__ substitute in place. */
#if defined(_MSC_VER)
/* MSVC compilers before VC7 don't have __func__ at all; later ones call it
 * __FUNCTION__. */
#if _MSC_VER < 1300
#define __func__ "???"
#else
#define __func__ __FUNCTION__
#endif

#else
/* For platforms where autoconf works, make sure __func__ is defined
 * sanely. */
#ifndef HAVE_MACRO__func__
#ifdef HAVE_MACRO__FUNCTION__
#define __func__ __FUNCTION__
#elif HAVE_MACRO__FUNC__
#define __func__ __FUNC__
#else
#define __func__ "???"
#endif
#endif /* ifndef MAVE_MACRO__func__ */
#endif /* if not windows */

#if defined(_MSC_VER) && (_MSC_VER < 1300)
/* MSVC versions before 7 apparently don't believe that you can cast uint64_t
 * to double and really mean it. */
extern INLINE double U64_TO_DBL(uint64_t x) {
  int64_t i = (int64_t) x;
  return (i < 0) ? ((double) INT64_MAX) : (double) i;
}
#define DBL_TO_U64(x) ((uint64_t)(int64_t) (x))
#else
#define U64_TO_DBL(x) ((double) (x))
#define DBL_TO_U64(x) ((uint64_t) (x))
#endif

#ifdef ENUM_VALS_ARE_SIGNED
#define ENUM_BF(t) unsigned
#else
/** Wrapper for having a bitfield of an enumerated type. Where possible, we
 * just use the enumerated type (so the compiler can help us and notice
 * problems), but if enumerated types are unsigned, we must use unsigned,
 * so that the loss of precision doesn't make large values negative. */
#define ENUM_BF(t) t
#endif

/* GCC has several useful attributes. */
#if defined(__GNUC__) && __GNUC__ >= 3
#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_CONST __attribute__((const))
#define ATTR_MALLOC __attribute__((malloc))
#define ATTR_NORETURN __attribute__((noreturn))
/* Alas, nonnull is not at present a good idea for us.  We'd like to get
 * warnings when we pass NULL where we shouldn't (which nonnull does, albeit
 * spottily), but we don't want to tell the compiler to make optimizations
 * with the assumption that the argument can't be NULL (since this would make
 * many of our checks go away, and make our code less robust against
 * programming errors).  Unfortunately, nonnull currently does both of these
 * things, and there's no good way to split them up.
 *
 * #define ATTR_NONNULL(x) __attribute__((nonnull x)) */
#define ATTR_NONNULL(x)
#define ATTR_UNUSED __attribute__ ((unused))

/** Macro: Evaluates to <b>exp</b> and hints the compiler that the value
 * of <b>exp</b> will probably be true.
 *
 * In other words, "if (PREDICT_LIKELY(foo))" is the same as "if (foo)",
 * except that it tells the compiler that the branch will be taken most of the
 * time.  This can generate slightly better code with some CPUs.
 */
#define PREDICT_LIKELY(exp) __builtin_expect(!!(exp), 1)
/** Macro: Evaluates to <b>exp</b> and hints the compiler that the value
 * of <b>exp</b> will probably be false.
 *
 * In other words, "if (PREDICT_UNLIKELY(foo))" is the same as "if (foo)",
 * except that it tells the compiler that the branch will usually not be
 * taken.  This can generate slightly better code with some CPUs.
 */
#define PREDICT_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
#define ATTR_NORETURN
#define ATTR_CONST
#define ATTR_MALLOC
#define ATTR_NORETURN
#define ATTR_NONNULL(x)
#define ATTR_UNUSED
#define PREDICT_LIKELY(exp) (exp)
#define PREDICT_UNLIKELY(exp) (exp)
#endif

/** Expands to a syntactically valid empty statement.  */
#define STMT_NIL (void)0

/** Expands to a syntactically valid empty statement, explicitly (void)ing its
 * argument. */
#define STMT_VOID(a) while (0) { (void)(a); }

#ifdef __GNUC__
/** STMT_BEGIN and STMT_END are used to wrap blocks inside macros so that
 * the macro can be used as if it were a single C statement. */
#define STMT_BEGIN (void) ({
#define STMT_END })
#elif defined(sun) || defined(__sun__)
#define STMT_BEGIN if (1) {
#define STMT_END } else STMT_NIL
#else
#define STMT_BEGIN do {
#define STMT_END } while (0)
#endif

/* ===== String compatibility */
#ifdef _WIN32
/* Windows names string functions differently from most other platforms. */
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz) ATTR_NONNULL((1,2));
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz) ATTR_NONNULL((1,2));
#endif

#ifdef _MSC_VER
/** Casts the uint64_t value in <b>a</b> to the right type for an argument
 * to printf. */
#define U64_PRINTF_ARG(a) (a)
/** Casts the uint64_t* value in <b>a</b> to the right type for an argument
 * to scanf. */
#define U64_SCANF_ARG(a) (a)
/** Expands to a literal uint64_t-typed constant for the value <b>n</b>. */
#define U64_LITERAL(n) (n ## ui64)
#define I64_PRINTF_ARG(a) (a)
#define I64_SCANF_ARG(a) (a)
#define I64_LITERAL(n) (n ## i64)
#else
#define U64_PRINTF_ARG(a) ((long long unsigned int)(a))
#define U64_SCANF_ARG(a) ((long long unsigned int*)(a))
#define U64_LITERAL(n) (n ## llu)
#define I64_PRINTF_ARG(a) ((long long signed int)(a))
#define I64_SCANF_ARG(a) ((long long signed int*)(a))
#define I64_LITERAL(n) (n ## ll)
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
/** The formatting string used to put a uint64_t value in a printf() or
 * scanf() function.  See also U64_PRINTF_ARG and U64_SCANF_ARG. */
#define U64_FORMAT "%I64u"
#define I64_FORMAT "%I64d"
#else
#define U64_FORMAT "%llu"
#define I64_FORMAT "%lld"
#endif

#if (SIZEOF_INTPTR_T == SIZEOF_INT)
#define INTPTR_T_FORMAT "%d"
#define INTPTR_PRINTF_ARG(x) ((int)(x))
#elif (SIZEOF_INTPTR_T == SIZEOF_LONG)
#define INTPTR_T_FORMAT "%ld"
#define INTPTR_PRINTF_ARG(x) ((long)(x))
#elif (SIZEOF_INTPTR_T == 8)
#define INTPTR_T_FORMAT I64_FORMAT
#define INTPTR_PRINTF_ARG(x) I64_PRINTF_ARG(x)
#else
#error Unknown: SIZEOF_INTPTR_T
#endif

/** Represents an mmaped file. Allocated via tor_mmap_file; freed with
 * tor_munmap_file. */
typedef struct tor_mmap_t {
  const char *data; /**< Mapping of the file's contents. */
  size_t size; /**< Size of the file. */

  /* None of the fields below should be accessed from outside compat.c */
#ifdef HAVE_SYS_MMAN_H
  size_t mapping_size; /**< Size of the actual mapping. (This is this file
                        * size, rounded up to the nearest page.) */
#elif defined _WIN32
  HANDLE mmap_handle;
#endif

} tor_mmap_t;

tor_mmap_t *tor_mmap_file(const char *filename) ATTR_NONNULL((1));
int tor_munmap_file(tor_mmap_t *handle) ATTR_NONNULL((1));

int tor_snprintf(char *str, size_t size, const char *format, ...)
  CHECK_PRINTF(3,4) ATTR_NONNULL((1,3));
int tor_vsnprintf(char *str, size_t size, const char *format, va_list args)
  CHECK_PRINTF(3,0) ATTR_NONNULL((1,3));

int tor_asprintf(char **strp, const char *fmt, ...)
  CHECK_PRINTF(2,3);
int tor_vasprintf(char **strp, const char *fmt, va_list args)
  CHECK_PRINTF(2,0);

const void *tor_memmem(const void *haystack, size_t hlen, const void *needle,
                       size_t nlen) ATTR_NONNULL((1,3));
static const void *tor_memstr(const void *haystack, size_t hlen,
                           const char *needle) ATTR_NONNULL((1,3));
static INLINE const void *
tor_memstr(const void *haystack, size_t hlen, const char *needle)
{
  return tor_memmem(haystack, hlen, needle, strlen(needle));
}

/* Much of the time when we're checking ctypes, we're doing spec compliance,
 * which all assumes we're doing ASCII. */
#define DECLARE_CTYPE_FN(name)                                          \
  static int TOR_##name(char c);                                        \
  extern const uint32_t TOR_##name##_TABLE[];                           \
  static INLINE int TOR_##name(char c) {                                \
    uint8_t u = c;                                                      \
    return !!(TOR_##name##_TABLE[(u >> 5) & 7] & (1u << (u & 31)));     \
  }
DECLARE_CTYPE_FN(ISALPHA)
DECLARE_CTYPE_FN(ISALNUM)
DECLARE_CTYPE_FN(ISSPACE)
DECLARE_CTYPE_FN(ISDIGIT)
DECLARE_CTYPE_FN(ISXDIGIT)
DECLARE_CTYPE_FN(ISPRINT)
DECLARE_CTYPE_FN(ISLOWER)
DECLARE_CTYPE_FN(ISUPPER)
extern const char TOR_TOUPPER_TABLE[];
extern const char TOR_TOLOWER_TABLE[];
#define TOR_TOLOWER(c) (TOR_TOLOWER_TABLE[(uint8_t)c])
#define TOR_TOUPPER(c) (TOR_TOUPPER_TABLE[(uint8_t)c])

char *tor_strtok_r_impl(char *str, const char *sep, char **lasts);
#ifdef HAVE_STRTOK_R
#define tor_strtok_r(str, sep, lasts) strtok_r(str, sep, lasts)
#else
#define tor_strtok_r(str, sep, lasts) tor_strtok_r_impl(str, sep, lasts)
#endif

#ifdef _WIN32
#define SHORT_FILE__ (tor_fix_source_file(__FILE__))
const char *tor_fix_source_file(const char *fname);
#else
#define SHORT_FILE__ (__FILE__)
#define tor_fix_source_file(s) (s)
#endif

/* ===== Time compatibility */
#if !defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_STRUCT_TIMEVAL_TV_SEC)
/** Implementation of timeval for platforms that don't have it. */
struct timeval {
  time_t tv_sec;
  unsigned int tv_usec;
};
#endif

void tor_gettimeofday(struct timeval *timeval);

struct tm *tor_localtime_r(const time_t *timep, struct tm *result);
struct tm *tor_gmtime_r(const time_t *timep, struct tm *result);

#ifndef timeradd
/** Replacement for timeradd on platforms that do not have it: sets tvout to
 * the sum of tv1 and tv2. */
#define timeradd(tv1,tv2,tvout) \
  do {                                                  \
    (tvout)->tv_sec = (tv1)->tv_sec + (tv2)->tv_sec;    \
    (tvout)->tv_usec = (tv1)->tv_usec + (tv2)->tv_usec; \
    if ((tvout)->tv_usec >= 1000000) {                  \
      (tvout)->tv_usec -= 1000000;                      \
      (tvout)->tv_sec++;                                \
    }                                                   \
  } while (0)
#endif

#ifndef timersub
/** Replacement for timersub on platforms that do not have it: sets tvout to
 * tv1 minus tv2. */
#define timersub(tv1,tv2,tvout) \
  do {                                                  \
    (tvout)->tv_sec = (tv1)->tv_sec - (tv2)->tv_sec;    \
    (tvout)->tv_usec = (tv1)->tv_usec - (tv2)->tv_usec; \
    if ((tvout)->tv_usec < 0) {                         \
      (tvout)->tv_usec += 1000000;                      \
      (tvout)->tv_sec--;                                \
    }                                                   \
  } while (0)
#endif

#ifndef timercmp
/** Replacement for timersub on platforms that do not have it: returns true
 * iff the relational operator "op" makes the expression tv1 op tv2 true.
 *
 * Note that while this definition should work for all boolean opeators, some
 * platforms' native timercmp definitions do not support >=, <=, or ==.  So
 * don't use those.
 */
#define timercmp(tv1,tv2,op)                    \
  (((tv1)->tv_sec == (tv2)->tv_sec) ?           \
   ((tv1)->tv_usec op (tv2)->tv_usec) :         \
   ((tv1)->tv_sec op (tv2)->tv_sec))
#endif

/* ===== File compatibility */
int tor_open_cloexec(const char *path, int flags, unsigned mode);
FILE *tor_fopen_cloexec(const char *path, const char *mode);
int tor_rename(const char *path_old, const char *path_new);

int replace_file(const char *from, const char *to);
int touch_file(const char *fname);

typedef struct tor_lockfile_t tor_lockfile_t;
tor_lockfile_t *tor_lockfile_lock(const char *filename, int blocking,
                                  int *locked_out);
void tor_lockfile_unlock(tor_lockfile_t *lockfile);

off_t tor_fd_getpos(int fd);
int tor_fd_setpos(int fd, off_t pos);
int tor_fd_seekend(int fd);

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

/* ===== Net compatibility */

#if (SIZEOF_SOCKLEN_T == 0)
typedef int socklen_t;
#endif

#ifdef _WIN32
/* XXX Actually, this should arguably be SOCKET; we use intptr_t here so that
 * any inadvertant checks for the socket being <= 0 or > 0 will probably
 * still work. */
#define tor_socket_t intptr_t
#define TOR_SOCKET_T_FORMAT INTPTR_T_FORMAT
#define SOCKET_OK(s) ((SOCKET)(s) != INVALID_SOCKET)
#define TOR_INVALID_SOCKET INVALID_SOCKET
#else
/** Type used for a network socket. */
#define tor_socket_t int
#define TOR_SOCKET_T_FORMAT "%d"
/** Macro: true iff 's' is a possible value for a valid initialized socket. */
#define SOCKET_OK(s) ((s) >= 0)
/** Error/uninitialized value for a tor_socket_t. */
#define TOR_INVALID_SOCKET (-1)
#endif

int tor_close_socket_simple(tor_socket_t s);
int tor_close_socket(tor_socket_t s);
tor_socket_t tor_open_socket_with_extensions(
                                           int domain, int type, int protocol,
                                           int cloexec, int nonblock);
tor_socket_t tor_open_socket(int domain, int type, int protocol);
tor_socket_t tor_open_socket_nonblocking(int domain, int type, int protocol);
tor_socket_t tor_accept_socket(tor_socket_t sockfd, struct sockaddr *addr,
                                  socklen_t *len);
tor_socket_t tor_accept_socket_nonblocking(tor_socket_t sockfd,
                                           struct sockaddr *addr,
                                           socklen_t *len);
tor_socket_t tor_accept_socket_with_extensions(tor_socket_t sockfd,
                                               struct sockaddr *addr,
                                               socklen_t *len,
                                               int cloexec, int nonblock);
int get_n_open_sockets(void);

#define tor_socket_send(s, buf, len, flags) send(s, buf, len, flags)
#define tor_socket_recv(s, buf, len, flags) recv(s, buf, len, flags)

/** Implementation of struct in6_addr for platforms that do not have it.
 * Generally, these platforms are ones without IPv6 support, but we want to
 * have a working in6_addr there anyway, so we can use it to parse IPv6
 * addresses. */
#if !defined(HAVE_STRUCT_IN6_ADDR)
struct in6_addr
{
  union {
    uint8_t u6_addr8[16];
    uint16_t u6_addr16[8];
    uint32_t u6_addr32[4];
  } in6_u;
#define s6_addr   in6_u.u6_addr8
#define s6_addr16 in6_u.u6_addr16
#define s6_addr32 in6_u.u6_addr32
};
#endif

/** @{ */
/** Many BSD variants seem not to define these. */
#if defined(__APPLE__) || defined(__darwin__) || defined(__FreeBSD__) \
    || defined(__NetBSD__) || defined(__OpenBSD__)
#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif
#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif
#endif
/** @} */

#ifndef HAVE_SA_FAMILY_T
typedef uint16_t sa_family_t;
#endif

/** @{ */
/** Apparently, MS and Solaris don't define s6_addr16 or s6_addr32; these
 * macros get you a pointer to s6_addr32 or local equivalent. */
#ifdef HAVE_STRUCT_IN6_ADDR_S6_ADDR32
#define S6_ADDR32(x) ((uint32_t*)(x).s6_addr32)
#else
#define S6_ADDR32(x) ((uint32_t*)((char*)&(x).s6_addr))
#endif
#ifdef HAVE_STRUCT_IN6_ADDR_S6_ADDR16
#define S6_ADDR16(x) ((uint16_t*)(x).s6_addr16)
#else
#define S6_ADDR16(x) ((uint16_t*)((char*)&(x).s6_addr))
#endif
/** @} */

/** Implementation of struct sockaddr_in6 on platforms that do not have
 * it. See notes on struct in6_addr. */
#if !defined(HAVE_STRUCT_SOCKADDR_IN6)
struct sockaddr_in6 {
  sa_family_t sin6_family;
  uint16_t sin6_port;
  // uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
  // uint32_t sin6_scope_id;
};
#endif

int tor_inet_aton(const char *cp, struct in_addr *addr) ATTR_NONNULL((1,2));
const char *tor_inet_ntop(int af, const void *src, char *dst, size_t len);
int tor_inet_pton(int af, const char *src, void *dst);
int tor_lookup_hostname(const char *name, uint32_t *addr) ATTR_NONNULL((1,2));
int set_socket_nonblocking(tor_socket_t socket);
int tor_socketpair(int family, int type, int protocol, tor_socket_t fd[2]);
int network_init(void);

/* For stupid historical reasons, windows sockets have an independent
 * set of errnos, and an independent way to get them.  Also, you can't
 * always believe WSAEWOULDBLOCK.  Use the macros below to compare
 * errnos against expected values, and use tor_socket_errno to find
 * the actual errno after a socket operation fails.
 */
#if defined(_WIN32)
/** Expands to WSA<b>e</b> on Windows, and to <b>e</b> elsewhere. */
#define SOCK_ERRNO(e) WSA##e
/** Return true if e is EAGAIN or the local equivalent. */
#define ERRNO_IS_EAGAIN(e)           ((e) == EAGAIN || (e) == WSAEWOULDBLOCK)
/** Return true if e is EINPROGRESS or the local equivalent. */
#define ERRNO_IS_EINPROGRESS(e)      ((e) == WSAEINPROGRESS)
/** Return true if e is EINPROGRESS or the local equivalent as returned by
 * a call to connect(). */
#define ERRNO_IS_CONN_EINPROGRESS(e) \
  ((e) == WSAEINPROGRESS || (e)== WSAEINVAL || (e) == WSAEWOULDBLOCK)
/** Return true if e is EAGAIN or another error indicating that a call to
 * accept() has no pending connections to return. */
#define ERRNO_IS_ACCEPT_EAGAIN(e)    ERRNO_IS_EAGAIN(e)
/** Return true if e is EMFILE or another error indicating that a call to
 * accept() has failed because we're out of fds or something. */
#define ERRNO_IS_ACCEPT_RESOURCE_LIMIT(e) \
  ((e) == WSAEMFILE || (e) == WSAENOBUFS)
/** Return true if e is EADDRINUSE or the local equivalent. */
#define ERRNO_IS_EADDRINUSE(e)      ((e) == WSAEADDRINUSE)
int tor_socket_errno(tor_socket_t sock);
const char *tor_socket_strerror(int e);
#else
#define SOCK_ERRNO(e) e
#if EAGAIN == EWOULDBLOCK
#define ERRNO_IS_EAGAIN(e)           ((e) == EAGAIN)
#else
#define ERRNO_IS_EAGAIN(e)           ((e) == EAGAIN || (e) == EWOULDBLOCK)
#endif
#define ERRNO_IS_EINPROGRESS(e)      ((e) == EINPROGRESS)
#define ERRNO_IS_CONN_EINPROGRESS(e) ((e) == EINPROGRESS)
#define ERRNO_IS_ACCEPT_EAGAIN(e) \
  (ERRNO_IS_EAGAIN(e) || (e) == ECONNABORTED)
#define ERRNO_IS_ACCEPT_RESOURCE_LIMIT(e) \
  ((e) == EMFILE || (e) == ENFILE || (e) == ENOBUFS || (e) == ENOMEM)
#define ERRNO_IS_EADDRINUSE(e)       ((e) == EADDRINUSE)
#define tor_socket_errno(sock)       (errno)
#define tor_socket_strerror(e)       strerror(e)
#endif

/** Specified SOCKS5 status codes. */
typedef enum {
  SOCKS5_SUCCEEDED                  = 0x00,
  SOCKS5_GENERAL_ERROR              = 0x01,
  SOCKS5_NOT_ALLOWED                = 0x02,
  SOCKS5_NET_UNREACHABLE            = 0x03,
  SOCKS5_HOST_UNREACHABLE           = 0x04,
  SOCKS5_CONNECTION_REFUSED         = 0x05,
  SOCKS5_TTL_EXPIRED                = 0x06,
  SOCKS5_COMMAND_NOT_SUPPORTED      = 0x07,
  SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED = 0x08,
} socks5_reply_status_t;

/* ===== OS compatibility */
const char *get_uname(void);

uint16_t get_uint16(const void *cp) ATTR_NONNULL((1));
uint32_t get_uint32(const void *cp) ATTR_NONNULL((1));
uint64_t get_uint64(const void *cp) ATTR_NONNULL((1));
void set_uint16(void *cp, uint16_t v) ATTR_NONNULL((1));
void set_uint32(void *cp, uint32_t v) ATTR_NONNULL((1));
void set_uint64(void *cp, uint64_t v) ATTR_NONNULL((1));

/* These uint8 variants are defined to make the code more uniform. */
#define get_uint8(cp) (*(const uint8_t*)(cp))
static void set_uint8(void *cp, uint8_t v);
static INLINE void
set_uint8(void *cp, uint8_t v)
{
  *(uint8_t*)cp = v;
}

#if !defined(HAVE_RLIM_T)
typedef unsigned long rlim_t;
#endif
int set_max_file_descriptors(rlim_t limit, int *max);
int tor_disable_debugger_attach(void);
int switch_id(const char *user);
#ifdef HAVE_PWD_H
char *get_user_homedir(const char *username);
#endif

#ifndef _WIN32
const struct passwd *tor_getpwnam(const char *username);
const struct passwd *tor_getpwuid(uid_t uid);
#endif

int get_parent_directory(char *fname);
char *make_path_absolute(char *fname);

char **get_environment(void);

int get_total_system_memory(size_t *mem_out);

int spawn_func(void (*func)(void *), void *data);
void spawn_exit(void) ATTR_NORETURN;

#if defined(ENABLE_THREADS) && defined(_WIN32)
#define USE_WIN32_THREADS
#define TOR_IS_MULTITHREADED 1
#elif (defined(ENABLE_THREADS) && defined(HAVE_PTHREAD_H) && \
       defined(HAVE_PTHREAD_CREATE))
#define USE_PTHREADS
#define TOR_IS_MULTITHREADED 1
#else
#undef TOR_IS_MULTITHREADED
#endif

int compute_num_cpus(void);

/* Because we use threads instead of processes on most platforms (Windows,
 * Linux, etc), we need locking for them.  On platforms with poor thread
 * support or broken gethostbyname_r, these functions are no-ops. */

/** A generic lock structure for multithreaded builds. */
typedef struct tor_mutex_t {
#if defined(USE_WIN32_THREADS)
  /** Windows-only: on windows, we implement locks with CRITICAL_SECTIONS. */
  CRITICAL_SECTION mutex;
#elif defined(USE_PTHREADS)
  /** Pthreads-only: with pthreads, we implement locks with
   * pthread_mutex_t. */
  pthread_mutex_t mutex;
#else
  /** No-threads only: Dummy variable so that tor_mutex_t takes up space. */
  int _unused;
#endif
} tor_mutex_t;

int tor_mlockall(void);

#ifdef TOR_IS_MULTITHREADED
tor_mutex_t *tor_mutex_new(void);
void tor_mutex_init(tor_mutex_t *m);
void tor_mutex_acquire(tor_mutex_t *m);
void tor_mutex_release(tor_mutex_t *m);
void tor_mutex_free(tor_mutex_t *m);
void tor_mutex_uninit(tor_mutex_t *m);
unsigned long tor_get_thread_id(void);
void tor_threads_init(void);
#else
#define tor_mutex_new() ((tor_mutex_t*)tor_malloc(sizeof(int)))
#define tor_mutex_init(m) STMT_NIL
#define tor_mutex_acquire(m) STMT_VOID(m)
#define tor_mutex_release(m) STMT_NIL
#define tor_mutex_free(m) STMT_BEGIN tor_free(m); STMT_END
#define tor_mutex_uninit(m) STMT_NIL
#define tor_get_thread_id() (1UL)
#define tor_threads_init() STMT_NIL
#endif

void set_main_thread(void);
int in_main_thread(void);

#ifdef TOR_IS_MULTITHREADED
#if 0
typedef struct tor_cond_t tor_cond_t;
tor_cond_t *tor_cond_new(void);
void tor_cond_free(tor_cond_t *cond);
int tor_cond_wait(tor_cond_t *cond, tor_mutex_t *mutex);
void tor_cond_signal_one(tor_cond_t *cond);
void tor_cond_signal_all(tor_cond_t *cond);
#endif
#endif

/** Macros for MIN/MAX.  Never use these when the arguments could have
 * side-effects.
 * {With GCC extensions we could probably define a safer MIN/MAX.  But
 * depending on that safety would be dangerous, since not every platform
 * has it.}
 **/
#ifndef MAX
#define MAX(a,b) ( ((a)<(b)) ? (b) : (a) )
#endif
#ifndef MIN
#define MIN(a,b) ( ((a)>(b)) ? (b) : (a) )
#endif

/* Platform-specific helpers. */
#ifdef _WIN32
char *format_win32_error(DWORD err);
#endif

/*for some reason my compiler doesn't have these version flags defined
  a nice homework assignment for someone one day is to define the rest*/
//these are the values as given on MSDN
#ifdef _WIN32

#ifndef VER_SUITE_EMBEDDEDNT
#define VER_SUITE_EMBEDDEDNT 0x00000040
#endif

#ifndef VER_SUITE_SINGLEUSERTS
#define VER_SUITE_SINGLEUSERTS 0x00000100
#endif

#endif

#ifdef TOR_UNIT_TESTS
void tor_sleep_msec(int msec);
#endif

#ifdef COMPAT_PRIVATE
#if !defined(HAVE_SOCKETPAIR) || defined(_WIN32) || defined(TOR_UNIT_TESTS)
#define NEED_ERSATZ_SOCKETPAIR
STATIC int tor_ersatz_socketpair(int family, int type, int protocol,
                                   tor_socket_t fd[2]);
#endif
#endif

#endif


#ifndef GIT_COMPAT_UTIL_H
#define GIT_COMPAT_UTIL_H

#define _FILE_OFFSET_BITS 64

#ifndef FLEX_ARRAY
/*
 * See if our compiler is known to support flexible array members.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
# define FLEX_ARRAY /* empty */
#elif defined(__GNUC__)
# if (__GNUC__ >= 3)
#  define FLEX_ARRAY /* empty */
# else
#  define FLEX_ARRAY 0 /* older GNU extension */
# endif
#endif

/*
 * Otherwise, default to safer but a bit wasteful traditional style
 */
#ifndef FLEX_ARRAY
# define FLEX_ARRAY 1
#endif
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef __GNUC__
#define TYPEOF(x) (__typeof__(x))
#else
#define TYPEOF(x)
#endif

#define MSB(x, bits) ((x) & TYPEOF(x)(~0ULL << (sizeof(x) * 8 - (bits))))
#define HAS_MULTI_BITS(i)  ((i) & ((i) - 1))  /* checks if an integer has more than 1 bit set */

/* Approximation of the length of the decimal representation of this type. */
#define decimal_length(x)	((int)(sizeof(x) * 2.56 + 0.5) + 1)

#define _ALL_SOURCE 1
#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define HAS_BOOL

#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <fnmatch.h>
#include <assert.h>
#include <regex.h>
#include <utime.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifndef NO_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <inttypes.h>
#include "../../../include/linux/magic.h"
#include "types.h"
#include <sys/ttydefaults.h>

#ifndef NO_ICONV
#include <iconv.h>
#endif

extern const char *graph_line;
extern const char *graph_dotted_line;
extern char buildid_dir[];

/* On most systems <limits.h> would have given us this, but
 * not on some systems (e.g. GNU/Hurd).
 */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef PRIuMAX
#define PRIuMAX "llu"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRIx32
#define PRIx32 "x"
#endif

#ifndef PATH_SEP
#define PATH_SEP ':'
#endif

#ifndef STRIP_EXTENSION
#define STRIP_EXTENSION ""
#endif

#ifndef has_dos_drive_prefix
#define has_dos_drive_prefix(path) 0
#endif

#ifndef is_dir_sep
#define is_dir_sep(c) ((c) == '/')
#endif

#ifdef __GNUC__
#define NORETURN __attribute__((__noreturn__))
#else
#define NORETURN
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

/* General helper functions */
extern void usage(const char *err) NORETURN;
extern void die(const char *err, ...) NORETURN __attribute__((format (printf, 1, 2)));
extern int error(const char *err, ...) __attribute__((format (printf, 1, 2)));
extern void warning(const char *err, ...) __attribute__((format (printf, 1, 2)));

#include "../../../include/linux/stringify.h"

#define DIE_IF(cnd)	\
	do { if (cnd)	\
		die(" at (" __FILE__ ":" __stringify(__LINE__) "): "	\
		    __stringify(cnd) "\n");				\
	} while (0)


extern void set_die_routine(void (*routine)(const char *err, va_list params) NORETURN);

extern int prefixcmp(const char *str, const char *prefix);
extern void set_buildid_dir(void);
extern void disable_buildid_cache(void);

static inline const char *skip_prefix(const char *str, const char *prefix)
{
	size_t len = strlen(prefix);
	return strncmp(str, prefix, len) ? NULL : str + len;
}

#ifdef __GLIBC_PREREQ
#if __GLIBC_PREREQ(2, 1)
#define HAVE_STRCHRNUL
#endif
#endif

#ifndef HAVE_STRCHRNUL
#define strchrnul gitstrchrnul
static inline char *gitstrchrnul(const char *s, int c)
{
	while (*s && *s != c)
		s++;
	return (char *)s;
}
#endif

/*
 * Wrappers:
 */
extern char *xstrdup(const char *str);
extern void *xrealloc(void *ptr, size_t size) __attribute__((weak));


static inline void *zalloc(size_t size)
{
	return calloc(1, size);
}

static inline int has_extension(const char *filename, const char *ext)
{
	size_t len = strlen(filename);
	size_t extlen = strlen(ext);

	return len > extlen && !memcmp(filename + len - extlen, ext, extlen);
}

/* Sane ctype - no locale, and works with signed chars */
#undef isascii
#undef isspace
#undef isdigit
#undef isxdigit
#undef isalpha
#undef isprint
#undef isalnum
#undef tolower
#undef toupper

extern unsigned char sane_ctype[256];
#define GIT_SPACE		0x01
#define GIT_DIGIT		0x02
#define GIT_ALPHA		0x04
#define GIT_GLOB_SPECIAL	0x08
#define GIT_REGEX_SPECIAL	0x10
#define GIT_PRINT_EXTRA		0x20
#define GIT_PRINT		0x3E
#define sane_istest(x,mask) ((sane_ctype[(unsigned char)(x)] & (mask)) != 0)
#define isascii(x) (((x) & ~0x7f) == 0)
#define isspace(x) sane_istest(x,GIT_SPACE)
#define isdigit(x) sane_istest(x,GIT_DIGIT)
#define isxdigit(x)	\
	(sane_istest(toupper(x), GIT_ALPHA | GIT_DIGIT) && toupper(x) < 'G')
#define isalpha(x) sane_istest(x,GIT_ALPHA)
#define isalnum(x) sane_istest(x,GIT_ALPHA | GIT_DIGIT)
#define isprint(x) sane_istest(x,GIT_PRINT)
#define tolower(x) sane_case((unsigned char)(x), 0x20)
#define toupper(x) sane_case((unsigned char)(x), 0)

static inline int sane_case(int x, int high)
{
	if (sane_istest(x, GIT_ALPHA))
		x = (x & ~0x20) | high;
	return x;
}

#ifndef DIR_HAS_BSD_GROUP_SEMANTICS
# define FORCE_DIR_SET_GID S_ISGID
#else
# define FORCE_DIR_SET_GID 0
#endif

#ifdef NO_NSEC
#undef USE_NSEC
#define ST_CTIME_NSEC(st) 0
#define ST_MTIME_NSEC(st) 0
#else
#ifdef USE_ST_TIMESPEC
#define ST_CTIME_NSEC(st) ((unsigned int)((st).st_ctimespec.tv_nsec))
#define ST_MTIME_NSEC(st) ((unsigned int)((st).st_mtimespec.tv_nsec))
#else
#define ST_CTIME_NSEC(st) ((unsigned int)((st).st_ctim.tv_nsec))
#define ST_MTIME_NSEC(st) ((unsigned int)((st).st_mtim.tv_nsec))
#endif
#endif

int mkdir_p(char *path, mode_t mode);
int copyfile(const char *from, const char *to);

s64 perf_atoll(const char *str);
char **argv_split(const char *str, int *argcp);
void argv_free(char **argv);
bool strglobmatch(const char *str, const char *pat);
bool strlazymatch(const char *str, const char *pat);
unsigned long convert_unit(unsigned long value, char *unit);

#ifndef ESC
#define ESC 27
#endif

static inline bool is_exit_key(int key)
{
	char up;
	if (key == CTRL('c') || key == ESC)
		return true;
	up = toupper(key);
	return up == 'Q';
}

#define _STR(x) #x
#define STR(x) _STR(x)

#endif

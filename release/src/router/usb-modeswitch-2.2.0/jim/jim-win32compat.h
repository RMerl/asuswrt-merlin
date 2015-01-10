#ifndef JIM_WIN32COMPAT_H
#define JIM_WIN32COMPAT_H

/* Compatibility for Windows (mingw and msvc, not cygwin */

/* Note that at this point we don't yet have access to jimautoconf.h */
#if defined(_WIN32) || defined(WIN32)
#ifndef STRICT
	#define STRICT
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define HAVE_DLOPEN
void *dlopen(const char *path, int mode);
int dlclose(void *handle);
void *dlsym(void *handle, const char *symbol);
char *dlerror(void);

#ifdef _MSC_VER
/* These are msvc vs gcc */

#if _MSC_VER >= 1000
	#pragma warning(disable:4146)
#endif

#define strcasecmp _stricmp

#define jim_wide _int64
#ifndef LLONG_MAX
	#define LLONG_MAX    9223372036854775807I64
#endif
#ifndef LLONG_MIN
	#define LLONG_MIN    (-LLONG_MAX - 1I64)
#endif
#define JIM_WIDE_MIN LLONG_MIN
#define JIM_WIDE_MAX LLONG_MAX
#define JIM_WIDE_MODIFIER "I64d"

#include <io.h>

#define HAVE_GETTIMEOFDAY
struct timeval {
	long tv_sec;
	long tv_usec;
};

int gettimeofday(struct timeval *tv, void *unused);

#define HAVE_OPENDIR
struct dirent {
	char *d_name;
};

typedef struct DIR {
	long                handle; /* -1 for failed rewind */
	struct _finddata_t  info;
	struct dirent       result; /* d_name null iff first time */
	char                *name;  /* null-terminated char string */
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dir);
struct dirent *readdir(DIR *dir);
#endif /* _MSC_VER */

#endif /* WIN32 */

#endif

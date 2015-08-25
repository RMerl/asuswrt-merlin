/*
 * $Id: win32.h 1667 2007-09-23 10:14:51Z rpedde $
 *
 * Win32 compatibility stuff
 */

#ifndef _WIN32_H_
#define _WIN32_H_

// Visual C++ 2005 has a secure CRT that generates oodles of warnings when unbounded
// C library functions are used. Disable this feature for the time being.
#if defined(_MSC_VER)
# if _MSC_VER >= 1400
#  define _CRT_SECURE_NO_DEPRECATE 1
#  define _CRT_NONSTDC_NO_DEPRECATE 1
# endif
#endif

// Don't pull in winsock...
#define WIN32_LEAN_AND_MEAN

// Must include all these before defining the non-underscore versions
// otherwise the prototypes will go awry.
#include <windows.h>
//#include <io.h>
#include <stddef.h>
#include <fcntl.h>
#include <direct.h>
#include <stdio.h>

/* Type fixups */
#define mode_t int
#define ssize_t int
//#define socklen_t int

/* Consts */
#define PIPE_BUF 256 /* What *should* this be on win32? */
#define MAXDESC 512 /* http://msdn.microsoft.com/en-us/library/kdfaxaay.aspx */
#define ETIME 101
#define PATH_MAX 2048 /* it's clearly not _MAX_PATH... other projects seem to use 512 */
#define MAX_NAME_LEN _MAX_PATH
#define EADDRINUSE WSAEADDRINUSE
#define S_IFLNK 0x1

#define HOST "unknown-windows-ick"
#define SERVICENAME "Firefly Media Server"

#include "os-win32.h"

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

typedef UINT8       uint8_t;
typedef INT8        int8_t;
typedef UINT16      uint16_t;
typedef INT16       int16_t;
typedef UINT32      uint32_t;
typedef INT32       int32_t;
typedef UINT64      uint64_t;
typedef INT64       int64_t;


/* Funtion fixups */
#define usleep Sleep
#define sleep(a) Sleep((a) * 1000)
#define mkdir(a,b) _mkdir((a))
#define strtoll strtol /* FIXME: compat functions */
#define atoll atol

#define realpath os_realpath
#define strsep os_strsep

#define strncasecmp strnicmp
#define strcasecmp stricmp
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define access _access

#define readdir_r os_readdir_r
#define closedir os_closedir
#define opendir os_opendir

#define getuid os_getuid
#define strerror os_strerror /* FIXME: get rid of posix errors */

/* override the uici stuff */
// #define u_open os_opensocket
// #define u_accept os_acceptsocket

/* privately implemented functions: @see os-win32.c */
#define gettimeofday os_gettimeofday


#define CONFFILE os_configpath()

extern char *os_configpath(void);

#endif /* _WIN32_H_ */


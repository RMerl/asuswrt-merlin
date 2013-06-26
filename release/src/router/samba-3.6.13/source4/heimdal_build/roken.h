/*
  a wrapper to override some of the defines that the heimdal roken system looks at
 */
#ifndef _ROKEN_H_
#define _ROKEN_H_

#include "config.h"

/* Support 'weak' keys for now, it can't be worse than NTLM and we don't want to hard-code the behaviour at this point */
#define HEIM_WEAK_CRYPTO 1

/* path to sysconf - should we force this to samba LIBDIR ? */
#define SYSCONFDIR "/etc"

#define rk_PATH_DELIM '/'

#define HEIMDAL_LOCALEDIR "/usr/heimdal/locale"

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

/* We want PKINIT */
#define PKINIT 1

#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#define ROKEN_LIB_VARIABLE
#define GETHOSTBYADDR_PROTO_COMPATIBLE
#define GETSERVBYNAME_PROTO_COMPATIBLE
#define OPENLOG_PROTO_COMPATIBLE
#define GETSOCKNAME_PROTO_COMPATIBLE

/* even if we do have dlopen, we don't want heimdal using it */
#undef HAVE_DLOPEN

/* we need to tell roken about the functions that Samba replaces in lib/replace */
#ifndef HAVE_SETEUID
#define HAVE_SETEUID 1
#endif

#ifndef HAVE_STRNLEN
#define HAVE_STRNLEN
#endif

#ifndef HAVE_STRNDUP
#define HAVE_STRNDUP
#endif

#ifndef HAVE_STRLCPY
#define HAVE_STRLCPY
#endif

#ifndef HAVE_STRLCAT
#define HAVE_STRLCAT
#endif

#ifndef HAVE_STRCASECMP
#define HAVE_STRCASECMP
#endif

#ifndef HAVE_ASPRINTF
#define HAVE_ASPRINTF
#endif

#ifndef HAVE_VASPRINTF
#define HAVE_VASPRINTF
#endif

#ifndef HAVE_MKSTEMP
#define HAVE_MKSTEMP
#endif

#ifndef HAVE_SETENV
#define HAVE_SETENV
#endif

#ifndef HAVE_UNSETENV
#define HAVE_UNSETENV
#endif

#ifndef HAVE_VSYSLOG
#define HAVE_VSYSLOG
#endif

#ifndef HAVE_SSIZE_T
#define HAVE_SSIZE_T
#endif

#ifndef HAVE_STRPTIME
#define HAVE_STRPTIME
#endif

#ifndef HAVE_TIMEGM
#define HAVE_TIMEGM
#endif

#ifndef HAVE_INNETGR
#define HAVE_INNETGR
#endif

#ifndef HAVE_INET_ATON
#define HAVE_INET_ATON
#endif

#ifndef HAVE_INET_NTOP
#define HAVE_INET_NTOP
#endif

#ifndef HAVE_INET_PTON
#define HAVE_INET_PTON
#endif

#ifndef HAVE_GETTIMEOFDAY
#define HAVE_GETTIMEOFDAY
#endif

#ifndef HAVE_SETEGID
#define HAVE_SETEGID
#endif

#ifndef HAVE_SETEUID
#define HAVE_SETEUID
#endif

/* force the use of the libreplace strerror_r */
#ifndef HAVE_STRERROR_R
#define HAVE_STRERROR_R
#endif
#ifndef STRERROR_R_PROTO_COMPATIBLE
#define STRERROR_R_PROTO_COMPATIBLE
#endif

#ifndef HAVE_DIRFD
#ifdef HAVE_DIR_DD_FD
#define dirfd(x) ((x)->dd_fd)
#else
#define dirfd(d) (-1)
#endif
#define HAVE_DIRFD 1
#endif


/* we lie about having pidfile() so that NetBSD5 can compile. Nothing
   in the parts of heimdal we use actually uses pidfile(), and we
   don't use it in Samba, so this works, although its ugly */
#ifndef HAVE_PIDFILE
#define HAVE_PIDFILE
#endif

#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1 )
#ifndef HAVE___ATTRIBUTE__
#define HAVE___ATTRIBUTE__
#endif
#endif

#include "system/network.h"

/*
 * we don't want that roken.h.in includes socket_wrapper
 * we include socket_wrapper via "system/network.h"
 */
#undef SOCKET_WRAPPER_REPLACE
#include "heimdal/lib/roken/roken.h.in"

extern const char *heimdal_version;
extern const char *heimdal_long_version;

/* we do not want any __APPLE__ magic */
#ifdef __APPLE__
#undef __APPLE__
#endif

#endif

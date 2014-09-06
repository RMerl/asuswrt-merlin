#ifndef SNMP_SYSTEM_H
#define SNMP_SYSTEM_H

#ifndef NET_SNMP_CONFIG_H
#error "Please include <net-snmp/net-snmp-config.h> before this file"
#endif

#ifdef __cplusplus
extern          "C" {
#endif

/* Portions of this file are subject to the following copyrights.  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
        Copyright 1993 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/*
 * portions Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */


    /*
     * function to create a daemon. Will fork and call setsid().
     *
     * Returns: -1 : fork failed
     *           0 : No errors
     */
    NETSNMP_IMPORT
    int netsnmp_daemonize(int quit_immediately, int stderr_log);

    /*
     * Definitions for the system dependent library file
     *
     * Do not define 'struct direct' when MSVC_PERL is defined because a
     * structure with that name is also defined in the Perl header
     * lib\CORE\dirent.h. Do not declare gettimeofday() either.
     */
#ifndef MSVC_PERL

#ifndef HAVE_READDIR
    /*
     * structure of a directory entry 
     */
    typedef struct direct {
        long            d_ino;  /* inode number (not used by MS-DOS) */
        int             d_namlen;       /* Name length */
        char            d_name[257];    /* file name */
    } _DIRECT;

    /*
     * structure for dir operations 
     */
    typedef struct _dir_struc {
        char           *start;  /* Starting position */
        char           *curr;   /* Current position */
        long            size;   /* Size of string table */
        long            nfiles; /* number if filenames in table */
        struct direct   dirstr; /* Directory structure to return */
    } DIR;

    NETSNMP_IMPORT
    DIR            *opendir(const char *filename);
    NETSNMP_IMPORT
    struct direct  *readdir(DIR * dirp);
    NETSNMP_IMPORT
    int             closedir(DIR * dirp);
#endif /* HAVE_READDIR */

#ifndef HAVE_GETTIMEOFDAY
    NETSNMP_IMPORT
    int             gettimeofday(struct timeval *, struct timezone *tz);
#endif

#endif                         /* MSVC_PERL */

/*
 * Note: when compiling Net-SNMP with dmalloc enabled on a system without
 * strcasecmp() or strncasecmp(), the macro HAVE_STRNCASECMP is
 * not defined but strcasecmp() and strncasecmp() are defined as macros in
 * <dmalloc.h>. In order to prevent a compilation error, do not declare
 * strcasecmp() or strncasecmp() when the <dmalloc.h> header has been included.
 */
#if !defined(HAVE_STRNCASECMP) && !defined(strcasecmp)
    NETSNMP_IMPORT
    int             strcasecmp(const char *s1, const char *s2);
#endif
#if !defined(HAVE_STRNCASECMP) && !defined(strncasecmp)
    NETSNMP_IMPORT
    int             strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifdef WIN32
    NETSNMP_IMPORT
    char           *winsock_startup(void);
    NETSNMP_IMPORT
    void            winsock_cleanup(void);
#define SOCK_STARTUP winsock_startup()
#define SOCK_CLEANUP winsock_cleanup()
#else                           /* !WIN32 */
#define SOCK_STARTUP
#define SOCK_CLEANUP
#endif                          /* WIN32 */

#include <net-snmp/types.h>     /* For definition of in_addr_t */

    /* Simply resolve a hostname and return first IPv4 address.
     * Returns -1 on error */
    NETSNMP_IMPORT
    int             netsnmp_gethostbyname_v4(const char* name,
                                             in_addr_t *addr_out);

    /** netsnmp versions of dns resoloution.. may include DNSSEC validation. */
    struct addrinfo; /* forward declare */
    NETSNMP_IMPORT
    struct hostent *netsnmp_gethostbyname(const char *name);

    NETSNMP_IMPORT
    struct hostent *netsnmp_gethostbyaddr(const void *addr, socklen_t len,
                                          int type);

    NETSNMP_IMPORT
    int             netsnmp_getaddrinfo(const char *name, const char *service,
                                        const struct addrinfo *hints,
                                        struct addrinfo **res);

    NETSNMP_IMPORT
    in_addr_t       get_myaddr(void);
    NETSNMP_IMPORT
    long            get_uptime(void);

#ifndef HAVE_STRDUP
    char           *strdup(const char *);
#endif
#ifndef HAVE_SETENV
    NETSNMP_IMPORT
    int             setenv(const char *, const char *, int);
#endif

    NETSNMP_IMPORT
    int             calculate_time_diff(const struct timeval *,
                                        const struct timeval *);
    NETSNMP_IMPORT
    u_int           calculate_sectime_diff(const struct timeval *now,
                                           const struct timeval *then);

#ifndef HAVE_STRCASESTR
    char           *strcasestr(const char *, const char *);
#endif
#ifndef HAVE_STRTOL
    long            strtol(const char *, char **, int);
#endif
#ifndef HAVE_STRTOUL
    unsigned long   strtoul(const char *, char **, int);
#endif
#ifndef HAVE_STRTOULL
    NETSNMP_IMPORT uint64_t strtoull(const char *, char **, int);
#endif
#ifndef HAVE_STRTOK_R
    NETSNMP_IMPORT
    char           *strtok_r(char *, const char *, char **);
#endif
#ifndef HAVE_SNPRINTF
    int             snprintf(char *, size_t, const char *, ...);
#endif

    NETSNMP_IMPORT
    int             mkdirhier(const char *pathname, mode_t mode,
                              int skiplast);
    NETSNMP_IMPORT
    const char     *netsnmp_mktemp(void);
#ifndef HAVE_STRLCPY
    NETSNMP_IMPORT
    size_t            strlcpy(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCAT
    NETSNMP_IMPORT
    size_t            strlcat(char * __restrict, const char * __restrict,
                              size_t);
#endif

    int             netsnmp_os_prematch(const char *ospmname,
                                        const char *ospmrelprefix);
    int             netsnmp_os_kernel_width(void);

    NETSNMP_IMPORT
    int             netsnmp_str_to_uid(const char *useroruid);
    NETSNMP_IMPORT
    int             netsnmp_str_to_gid(const char *grouporgid);

#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_SYSTEM_H */

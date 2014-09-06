/*
 * system.c
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
        Copyright 1992 by Carnegie Mellon University

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
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/*
 * System dependent routines go here
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_DIRECT_H
#include <direct.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/types.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif


#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_NLIST_H
#include <nlist.h>
#endif

#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#if HAVE_KSTAT_H
#include <kstat.h>
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if defined(hpux10) || defined(hpux11)
#include <sys/pstat.h>
#endif

#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#if HAVE_SYS_SYSTEMCFG_H
#include <sys/systemcfg.h>
#endif

#if HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif

#if defined(darwin9)
#include <crt_externs.h>        /* for _NSGetArgv() */
#endif

#if HAVE_PWD_H
#include <pwd.h>
#endif
#if HAVE_GRP_H
#include <grp.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef DNSSEC_LOCAL_VALIDATION
#if 1 /*HAVE_ARPA_NAMESER_H*/
#include <arpa/nameser.h>
#endif
#include <validator/validator.h>
/* NetSNMP and DNSSEC-Tools both define FREE. We'll not use either here. */
#undef FREE
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>
#include <net-snmp/library/system.h>    /* for "internal" definitions */

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/read_config.h> /* for get_temp_file_pattern() */

#include "inet_ntop.h"

/* NetSNMP and DNSSEC-Tools both define FREE. We'll not use either here. */
#undef FREE

netsnmp_feature_child_of(system_all, libnetsnmp)

netsnmp_feature_child_of(user_information, system_all)
netsnmp_feature_child_of(calculate_sectime_diff, system_all)

#ifndef IFF_LOOPBACK
#	define IFF_LOOPBACK 0
#endif

#ifdef  INADDR_LOOPBACK
# define LOOPBACK    INADDR_LOOPBACK
#else
# define LOOPBACK    0x7f000001
#endif

#ifndef EAI_FAIL
# define EAI_FAIL    -4    /* Non-recoverable failure in name res.  */
#endif

#if defined(HAVE_FORK)
static void
_daemon_prep(int stderr_log)
{
    /* Avoid keeping any directory in use. */
    chdir("/");

    if (stderr_log)
        return;

    /*
     * Close inherited file descriptors to avoid
     * keeping unnecessary references.
     */
    close(0);
    close(1);
    close(2);

    /*
     * Redirect std{in,out,err} to /dev/null, just in case.
     */
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
}
#endif

/**
 * fork current process into the background.
 *
 * This function forks a process into the background, in order to
 * become a daemon process. It does a few things along the way:
 *
 * - becoming a process/session group leader, and  forking a second time so
 *   that process/session group leader can exit.
 *
 * - changing the working directory to /
 *
 * - closing stdin, stdout and stderr (unless stderr_log is set) and
 *   redirecting them to /dev/null
 *
 * @param quit_immediately : indicates if the parent process should
 *                           exit after a successful fork.
 * @param stderr_log       : indicates if stderr is being used for
 *                           logging and shouldn't be closed
 * @returns -1 : fork error
 *           0 : child process returning
 *          >0 : parent process returning. returned value is the child PID.
 */
int
netsnmp_daemonize(int quit_immediately, int stderr_log)
{
    int i = 0;
    DEBUGMSGT(("daemonize","deamonizing...\n"));
#if HAVE_FORK
#if defined(darwin9)
     char            path [PATH_MAX] = "";
     uint32_t        size = sizeof (path);

     /*
      * if we are already launched in a "daemonized state", just
      * close & redirect the file descriptors
      */
     if(getppid() <= 2) {
         _daemon_prep(stderr_log);
         return 0;
     }

     if (_NSGetExecutablePath (path, &size))
         return -1;
#endif
    /*
     * Fork to return control to the invoking process and to
     * guarantee that we aren't a process group leader.
     */
    i = fork();
    if (i != 0) {
        /* Parent. */
        DEBUGMSGT(("daemonize","first fork returned %d.\n", i));
        if(i == -1) {
            snmp_log(LOG_ERR,"first fork failed (errno %d) in "
                     "netsnmp_daemonize()\n", errno);
            return -1;
        }
        if (quit_immediately) {
            DEBUGMSGT(("daemonize","parent exiting\n"));
            exit(0);
        }
    } else {
        /* Child. */
#ifdef HAVE_SETSID
        /* Become a process/session group leader. */
        setsid();
#endif
        /*
         * Fork to let the process/session group leader exit.
         */
        if ((i = fork()) != 0) {
            DEBUGMSGT(("daemonize","second fork returned %d.\n", i));
            if(i == -1) {
                snmp_log(LOG_ERR,"second fork failed (errno %d) in "
                         "netsnmp_daemonize()\n", errno);
            }
            /* Parent. */
            exit(0);
        }
#ifndef WIN32
        else {
            /* Child. */
            
            DEBUGMSGT(("daemonize","child continuing\n"));

#if ! defined(darwin9)
            _daemon_prep(stderr_log);
#else
             /*
              * Some darwin calls (using mach ports) don't work after
              * a fork. So, now that we've forked, we re-exec ourself
              * to ensure that the child's mach ports are all set up correctly,
              * the getppid call above will prevent the exec child from
              * forking...
              */
             char * const *argv = *_NSGetArgv ();
             DEBUGMSGT(("daemonize","re-execing forked child\n"));
             execv (path, argv);
             snmp_log(LOG_ERR,"Forked child unable to re-exec - %s.\n", strerror (errno));
             exit (0);
#endif
        }
#endif /* !WIN32 */
    }
#endif /* HAVE_FORK */
    return i;
}

/*
 * ********************************************* 
 */
#ifdef							WIN32
in_addr_t
get_myaddr(void)
{
    char            local_host[130];
    int             result;
    LPHOSTENT       lpstHostent;
    SOCKADDR_IN     in_addr, remote_in_addr;
    SOCKET          hSock;
    int             nAddrSize = sizeof(SOCKADDR);

    in_addr.sin_addr.s_addr = INADDR_ANY;

    result = gethostname(local_host, sizeof(local_host));
    if (result == 0) {
        lpstHostent = gethostbyname((LPSTR) local_host);
        if (lpstHostent) {
            in_addr.sin_addr.s_addr =
                *((u_long FAR *) (lpstHostent->h_addr));
            return ((in_addr_t) in_addr.sin_addr.s_addr);
        }
    }

    /*
     * if we are here, than we don't have host addr 
     */
    hSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (hSock != INVALID_SOCKET) {
        /*
         * connect to any port and address 
         */
        remote_in_addr.sin_family = AF_INET;
        remote_in_addr.sin_port = htons(IPPORT_ECHO);
        remote_in_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
        result =
            connect(hSock, (LPSOCKADDR) & remote_in_addr,
                    sizeof(SOCKADDR));
        if (result != SOCKET_ERROR) {
            /*
             * get local ip address 
             */
            getsockname(hSock, (LPSOCKADDR) & in_addr,
                        (int FAR *) &nAddrSize);
        }
        closesocket(hSock);
    }
    return ((in_addr_t) in_addr.sin_addr.s_addr);
}

long
get_uptime(void)
{
    long            return_value = 0;
    DWORD           buffersize = (sizeof(PERF_DATA_BLOCK) +
                                  sizeof(PERF_OBJECT_TYPE)),
        type = REG_EXPAND_SZ;
    PPERF_DATA_BLOCK perfdata = NULL;

    /*
     * min requirement is one PERF_DATA_BLOCK plus one PERF_OBJECT_TYPE 
     */
    perfdata = (PPERF_DATA_BLOCK) malloc(buffersize);
    if (!perfdata)
        return 0;

    memset(perfdata, 0, buffersize);

    RegQueryValueEx(HKEY_PERFORMANCE_DATA,
                    "Global", NULL, &type, (LPBYTE) perfdata, &buffersize);

    /*
     * we can not rely on the return value since there is always more so
     * we check the signature 
     */

    if (wcsncmp(perfdata->Signature, L"PERF", 4) == 0) {
        /*
         * signature ok, and all we need is in the in the PERF_DATA_BLOCK 
         */
        return_value = (long) ((perfdata->PerfTime100nSec.QuadPart /
                                (LONGLONG) 100000));
    } else
        return_value = GetTickCount() / 10;

    RegCloseKey(HKEY_PERFORMANCE_DATA);
    free(perfdata);

    return return_value;
}

char           *
winsock_startup(void)
{
    WORD            VersionRequested;
    WSADATA         stWSAData;
    int             i;
    static char     errmsg[100];

	/* winsock 1: use MAKEWORD(1,1) */
	/* winsock 2: use MAKEWORD(2,2) */

    VersionRequested = MAKEWORD(2,2);
    i = WSAStartup(VersionRequested, &stWSAData);
    if (i != 0) {
        if (i == WSAVERNOTSUPPORTED)
            sprintf(errmsg,
                    "Unable to init. socket lib, does not support 1.1");
        else {
            sprintf(errmsg, "Socket Startup error %d", i);
        }
        return (errmsg);
    }
    return (NULL);
}

void
winsock_cleanup(void)
{
    WSACleanup();
}

#else                           /* ! WIN32 */
/*******************************************************************/

/*
 * XXX  What if we have multiple addresses?  Or no addresses for that matter?
 * XXX  Could it be computed once then cached?  Probably not worth it (not
 *                                                           used very often).
 */
in_addr_t
get_myaddr(void)
{
    int             sd, i, lastlen = 0;
    struct ifconf   ifc;
    struct ifreq   *ifrp = NULL;
    in_addr_t       addr;
    char           *buf = NULL;

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    /*
     * Cope with lots of interfaces and brokenness of ioctl SIOCGIFCONF on
     * some platforms; see W. R. Stevens, ``Unix Network Programming Volume
     * I'', p.435.  
     */

    for (i = 8;; i += 8) {
        buf = (char *) calloc(i, sizeof(struct ifreq));
        if (buf == NULL) {
            close(sd);
            return 0;
        }
        ifc.ifc_len = i * sizeof(struct ifreq);
        ifc.ifc_buf = (caddr_t) buf;

        if (ioctl(sd, SIOCGIFCONF, (char *) &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0) {
                /*
                 * Something has gone genuinely wrong.  
                 */
                free(buf);
                close(sd);
                return 0;
            }
            /*
             * Otherwise, it could just be that the buffer is too small.  
             */
        } else {
            if (ifc.ifc_len == lastlen) {
                /*
                 * The length is the same as the last time; we're done.  
                 */
                break;
            }
            lastlen = ifc.ifc_len;
        }
        free(buf);
    }

    for (ifrp = ifc.ifc_req;
        (char *)ifrp < (char *)ifc.ifc_req + ifc.ifc_len;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
        ifrp = (struct ifreq *)(((char *) ifrp) +
                                sizeof(ifrp->ifr_name) +
                                ifrp->ifr_addr.sa_len)
#else
        ifrp++
#endif
        ) {
        if (ifrp->ifr_addr.sa_family != AF_INET) {
            continue;
        }
        addr = ((struct sockaddr_in *) &(ifrp->ifr_addr))->sin_addr.s_addr;

        if (ioctl(sd, SIOCGIFFLAGS, (char *) ifrp) < 0) {
            continue;
        }
        if ((ifrp->ifr_flags & IFF_UP)
#ifdef IFF_RUNNING
            && (ifrp->ifr_flags & IFF_RUNNING)
#endif                          /* IFF_RUNNING */
            && !(ifrp->ifr_flags & IFF_LOOPBACK)
            && addr != LOOPBACK) {
            /*
             * I *really* don't understand why this is necessary.  Perhaps for
             * some broken platform?  Leave it for now.  JBPN  
             */
#ifdef SYS_IOCTL_H_HAS_SIOCGIFADDR
            if (ioctl(sd, SIOCGIFADDR, (char *) ifrp) < 0) {
                continue;
            }
            addr =
                ((struct sockaddr_in *) &(ifrp->ifr_addr))->sin_addr.
                s_addr;
#endif
            free(buf);
            close(sd);
            return addr;
        }
    }
    free(buf);
    close(sd);
    return 0;
}


#if !defined(solaris2) && !defined(linux) && !defined(cygwin)
/*
 * Returns boottime in centiseconds(!).
 *      Caches this for future use.
 */
long
get_boottime(void)
{
    static long     boottime_csecs = 0;
#if defined(hpux10) || defined(hpux11)
    struct pst_static pst_buf;
#else
    struct timeval  boottime;
#ifdef	NETSNMP_CAN_USE_SYSCTL
    int             mib[2];
    size_t          len;
#elif defined(NETSNMP_CAN_USE_NLIST)
    int             kmem;
    static struct nlist nl[] = {
#if !defined(hpux)
        {(char *) "_boottime"},
#else
        {(char *) "boottime"},
#endif
        {(char *) ""}
    };
#endif                          /* NETSNMP_CAN_USE_SYSCTL */
#endif                          /* hpux10 || hpux 11 */


    if (boottime_csecs != 0)
        return (boottime_csecs);

#if defined(hpux10) || defined(hpux11)
    pstat_getstatic(&pst_buf, sizeof(struct pst_static), 1, 0);
    boottime_csecs = pst_buf.boot_time * 100;
#elif NETSNMP_CAN_USE_SYSCTL
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;

    len = sizeof(boottime);

    sysctl(mib, 2, &boottime, &len, NULL, 0);
    boottime_csecs = (boottime.tv_sec * 100) + (boottime.tv_usec / 10000);
#elif defined(NETSNMP_CAN_USE_NLIST)
    if ((kmem = open("/dev/kmem", 0)) < 0)
        return 0;
    nlist(KERNEL_LOC, nl);
    if (nl[0].n_type == 0) {
        close(kmem);
        return 0;
    }

    lseek(kmem, (long) nl[0].n_value, L_SET);
    read(kmem, &boottime, sizeof(boottime));
    close(kmem);
    boottime_csecs = (boottime.tv_sec * 100) + (boottime.tv_usec / 10000);
#else
    return 0;
#endif                          /* hpux10 || hpux 11 */

    return (boottime_csecs);
}
#endif

/**
 * Returns the system uptime in centiseconds.
 *
 * @note The value returned by this function is not identical to sysUpTime
 *   defined in RFC 1213. get_uptime() returns the system uptime while
 *   sysUpTime represents the time that has elapsed since the most recent
 *   restart of the network manager (snmpd).
 *
 * @see See also netsnmp_get_agent_uptime().
 */
long
get_uptime(void)
{
#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    struct nlist nl;
    int kmem;
    time_t lbolt;
    nl.n_name = "lbolt";
    if(knlist(&nl, 1, sizeof(struct nlist)) != 0) return(0);
    if(nl.n_type == 0 || nl.n_value == 0) return(0);
    if((kmem = open("/dev/mem", 0)) < 0) return 0;
    lseek(kmem, (long) nl.n_value, L_SET);
    read(kmem, &lbolt, sizeof(lbolt));
    close(kmem);
    return(lbolt);
#elif defined(solaris2)
    kstat_ctl_t    *ksc = kstat_open();
    kstat_t        *ks;
    kid_t           kid;
    kstat_named_t  *named;
    u_long          lbolt = 0;

    if (ksc) {
        ks = kstat_lookup(ksc, "unix", -1, "system_misc");
        if (ks) {
            kid = kstat_read(ksc, ks, NULL);
            if (kid != -1) {
                named = kstat_data_lookup(ks, "lbolt");
                if (named) {
#ifdef KSTAT_DATA_UINT32
                    lbolt = named->value.ui32;
#else
                    lbolt = named->value.ul;
#endif
                }
            }
        }
        kstat_close(ksc);
    }
    return lbolt;
#elif defined(linux) || defined(cygwin)
    FILE           *in = fopen("/proc/uptime", "r");
    long            uptim = 0, a, b;
    if (in) {
        if (2 == fscanf(in, "%ld.%ld", &a, &b))
            uptim = a * 100 + b;
        fclose(in);
    }
    return uptim;
#else
    struct timeval  now;
    long            boottime_csecs, nowtime_csecs;

    boottime_csecs = get_boottime();
    if (boottime_csecs == 0)
        return 0;
    gettimeofday(&now, (struct timezone *) 0);
    nowtime_csecs = (now.tv_sec * 100) + (now.tv_usec / 10000);

    return (nowtime_csecs - boottime_csecs);
#endif
}

#endif                          /* ! WIN32 */
/*******************************************************************/

#ifdef DNSSEC_LOCAL_VALIDATION
static val_context_t *_val_context = NULL;

static val_context_t *
netsnmp_validator_context(void)
{
    if (NULL == _val_context) {
        int rc;
        char *apptype = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                              NETSNMP_DS_LIB_APPTYPE);
        DEBUGMSGTL(("dns:sec:context", "creating dnssec context for %s\n",
                    apptype));
        rc = val_create_context(apptype, &_val_context);
    }

    return _val_context;
}
#endif /* DNSSEC_LOCAL_VALIDATION */

int
netsnmp_gethostbyname_v4(const char* name, in_addr_t *addr_out)
{
#if HAVE_GETADDRINFO
    struct addrinfo *addrs = NULL;
    struct addrinfo hint;
    int             err;

    memset(&hint, 0, sizeof hint);
    hint.ai_flags = 0;
    hint.ai_family = PF_INET;
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_protocol = 0;

    err = netsnmp_getaddrinfo(name, NULL, &hint, &addrs);
    if (err != 0) {
#if HAVE_GAI_STRERROR
        snmp_log(LOG_ERR, "getaddrinfo: %s %s\n", name,
                 gai_strerror(err));
#else
        snmp_log(LOG_ERR, "getaddrinfo: %s (error %d)\n", name,
                 err);
#endif
        return -1;
    }

    if (addrs != NULL) {
        memcpy(addr_out,
               &((struct sockaddr_in *) addrs->ai_addr)->sin_addr,
               sizeof(in_addr_t));
        freeaddrinfo(addrs);
    } else {
        DEBUGMSGTL(("get_thisaddr",
                    "Failed to resolve IPv4 hostname\n"));
    }
    return 0;

#elif HAVE_GETHOSTBYNAME
    struct hostent *hp = NULL;

    hp = netsnmp_gethostbyname(name);
    if (hp == NULL) {
        DEBUGMSGTL(("get_thisaddr",
                    "hostname (couldn't resolve)\n"));
        return -1;
    } else if (hp->h_addrtype != AF_INET) {
        DEBUGMSGTL(("get_thisaddr",
                    "hostname (not AF_INET!)\n"));
        return -1;
    } else {
        DEBUGMSGTL(("get_thisaddr",
                    "hostname (resolved okay)\n"));
        memcpy(addr_out, hp->h_addr, sizeof(in_addr_t));
    }
    return 0;

#elif HAVE_GETIPNODEBYNAME
    struct hostent *hp = NULL;
    int             err;

    hp = getipnodebyname(peername, AF_INET, 0, &err);
    if (hp == NULL) {
        DEBUGMSGTL(("get_thisaddr",
                    "hostname (couldn't resolve = %d)\n", err));
        return -1;
    }
    DEBUGMSGTL(("get_thisaddr",
                "hostname (resolved okay)\n"));
    memcpy(addr_out, hp->h_addr, sizeof(in_addr_t));
    return 0;

#else /* HAVE_GETIPNODEBYNAME */
    return -1;
#endif
}

int
netsnmp_getaddrinfo(const char *name, const char *service,
                    const struct addrinfo *hints, struct addrinfo **res)
{
#if HAVE_GETADDRINFO
    struct addrinfo *addrs = NULL;
    struct addrinfo hint;
    int             err;
#ifdef DNSSEC_LOCAL_VALIDATION
    val_status_t    val_status;
#endif

    DEBUGMSGTL(("dns:getaddrinfo", "looking up %s:%s\n", name, service));

    if (NULL == hints) {
        memset(&hint, 0, sizeof hint);
        hint.ai_flags = 0;
        hint.ai_family = PF_INET;
        hint.ai_socktype = SOCK_DGRAM;
        hint.ai_protocol = 0;
        hints = &hint;
    } else {
        memcpy(&hint, hints, sizeof hint);
    }

#ifndef DNSSEC_LOCAL_VALIDATION
    err = getaddrinfo(name, NULL, &hint, &addrs);
#else /* DNSSEC_LOCAL_VALIDATION */
    err = val_getaddrinfo(netsnmp_validator_context(), name, NULL, &hint,
                          &addrs, &val_status);
    DEBUGMSGTL(("dns:sec:val", "err %d, val_status %d / %s; trusted: %d\n",
                err, val_status, p_val_status(val_status),
                val_istrusted(val_status)));
    if (! val_istrusted(val_status)) {
        int rc;
        if ((err != 0) && VAL_GETADDRINFO_HAS_STATUS(err)) {
            snmp_log(LOG_WARNING,
                     "WARNING: UNTRUSTED error in DNS resolution for %s!\n",
                     name);
            rc = EAI_FAIL;
        } else {
            snmp_log(LOG_WARNING,
                     "The authenticity of DNS response is not trusted (%s)\n",
                     p_val_status(val_status));
            rc = EAI_NONAME;
        }
        /** continue anyways if DNSSEC_WARN_ONLY is set */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                    NETSNMP_DS_LIB_DNSSEC_WARN_ONLY))
            return rc;
    }


#endif /* DNSSEC_LOCAL_VALIDATION */
    *res = addrs;
    if ((0 == err) && addrs && addrs->ai_addr) {
        DEBUGMSGTL(("dns:getaddrinfo", "answer { AF_INET, %s:%hu }\n",
                    inet_ntoa(((struct sockaddr_in*)addrs->ai_addr)->sin_addr),
                    ntohs(((struct sockaddr_in*)addrs->ai_addr)->sin_port)));
    }
    return err;
#else
    NETSNMP_LOGONCE((LOG_ERR, "getaddrinfo not available"));
    return EAI_FAIL;
#endif /* getaddrinfo */
}

struct hostent *
netsnmp_gethostbyname(const char *name)
{
#if HAVE_GETHOSTBYNAME
#ifdef DNSSEC_LOCAL_VALIDATION
    val_status_t val_status;
#endif
    struct hostent *hp = NULL;

    if (NULL == name)
        return NULL;

    DEBUGMSGTL(("dns:gethostbyname", "looking up %s\n", name));

#ifdef DNSSEC_LOCAL_VALIDATION
    hp  = val_gethostbyname(netsnmp_validator_context(), name, &val_status);
    DEBUGMSGTL(("dns:sec:val", "val_status %d / %s; trusted: %d\n",
                val_status, p_val_status(val_status),
                val_istrusted(val_status)));
    if (!val_istrusted(val_status)) {
        snmp_log(LOG_WARNING,
                 "The authenticity of DNS response is not trusted (%s)\n",
                 p_val_status(val_status));
        /** continue anyways if DNSSEC_WARN_ONLY is set */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                    NETSNMP_DS_LIB_DNSSEC_WARN_ONLY))
            hp = NULL;
    }
    else if (val_does_not_exist(val_status) && hp)
        hp = NULL;
#else
    hp = gethostbyname(name);
#endif
    if (hp == NULL) {
        DEBUGMSGTL(("dns:gethostbyname",
                    "couldn't resolve %s\n", name));
    } else if (hp->h_addrtype != AF_INET) {
        DEBUGMSGTL(("dns:gethostbyname",
                    "warning: response for %s not AF_INET!\n", name));
    } else {
        DEBUGMSGTL(("dns:gethostbyname",
                    "%s resolved okay\n", name));
    }
    return hp;
#else
    NETSNMP_LOGONCE((LOG_ERR, "gethostbyname not available"));
    return NULL;
#endif /* HAVE_GETHOSTBYNAME */
}

/**
 * Look up the host name via DNS.
 *
 * @param[in] addr Pointer to the address to resolve. This argument points e.g.
 *   to a struct in_addr for AF_INET or to a struct in6_addr for AF_INET6.
 * @param[in] len  Length in bytes of *addr.
 * @param[in] type Address family, e.g. AF_INET or AF_INET6.
 *
 * @return Pointer to a hostent structure if address lookup succeeded or NULL
 *   if the lookup failed.
 *
 * @see See also the gethostbyaddr() man page.
 */
struct hostent *
netsnmp_gethostbyaddr(const void *addr, socklen_t len, int type)
{
#if HAVE_GETHOSTBYADDR
    struct hostent *hp = NULL;
    char buf[64];

    DEBUGMSGTL(("dns:gethostbyaddr", "resolving %s\n",
                inet_ntop(type, addr, buf, sizeof(buf))));

#ifdef DNSSEC_LOCAL_VALIDATION
    val_status_t val_status;
    hp = val_gethostbyaddr(netsnmp_validator_context(), addr, len, type,
                           &val_status);
    DEBUGMSGTL(("dns:sec:val", "val_status %d / %s; trusted: %d\n",
                val_status, p_val_status(val_status),
                val_istrusted(val_status)));
    if (!val_istrusted(val_status)) {
        snmp_log(LOG_WARNING,
                 "The authenticity of DNS response is not trusted (%s)\n",
                 p_val_status(val_status));
        /** continue anyways if DNSSEC_WARN_ONLY is set */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                    NETSNMP_DS_LIB_DNSSEC_WARN_ONLY))
            hp = NULL;
    }
    else if (val_does_not_exist(val_status) && hp)
        hp = NULL;
#else
    hp = gethostbyaddr(addr, len, type);
#endif
    if (hp == NULL) {
        DEBUGMSGTL(("dns:gethostbyaddr", "couldn't resolve addr\n"));
    } else if (hp->h_addrtype != AF_INET) {
        DEBUGMSGTL(("dns:gethostbyaddr",
                    "warning: response for addr not AF_INET!\n"));
    } else {
        DEBUGMSGTL(("dns:gethostbyaddr", "addr resolved okay\n"));
    }
    return hp;
#else
    NETSNMP_LOGONCE((LOG_ERR, "gethostbyaddr not available"));
    return NULL;
#endif
}

/*******************************************************************/

#ifndef HAVE_STRNCASECMP

/*
 * test for NULL pointers before and NULL characters after
 * * comparing possibly non-NULL strings.
 * * WARNING: This function does NOT check for array overflow.
 */
int
strncasecmp(const char *s1, const char *s2, size_t nch)
{
    size_t          ii;
    int             res = -1;

    if (!s1) {
        if (!s2)
            return 0;
        return (-1);
    }
    if (!s2)
        return (1);

    for (ii = 0; (ii < nch) && *s1 && *s2; ii++, s1++, s2++) {
        res = (int) (tolower(*s1) - tolower(*s2));
        if (res != 0)
            break;
    }

    if (ii == nch) {
        s1--;
        s2--;
    }

    if (!*s1) {
        if (!*s2)
            return 0;
        return (-1);
    }
    if (!*s2)
        return (1);

    return (res);
}

int
strcasecmp(const char *s1, const char *s2)
{
    return strncasecmp(s1, s2, 1000000);
}

#endif                          /* HAVE_STRNCASECMP */


#ifndef HAVE_STRDUP
char           *
strdup(const char *src)
{
    int             len;
    char           *dst;

    len = strlen(src) + 1;
    if ((dst = (char *) malloc(len)) == NULL)
        return (NULL);
    strcpy(dst, src);
    return (dst);
}
#endif                          /* HAVE_STRDUP */

#ifndef HAVE_SETENV
int
setenv(const char *name, const char *value, int overwrite)
{
    char           *cp;
    int             ret;

    if (overwrite == 0) {
        if (getenv(name))
            return 0;
    }
    cp = (char *) malloc(strlen(name) + strlen(value) + 2);
    if (cp == NULL)
        return -1;
    sprintf(cp, "%s=%s", name, value);
    ret = putenv(cp);
#ifdef WIN32
    free(cp);
#endif
    return ret;
}
#endif                          /* HAVE_SETENV */

netsnmp_feature_child_of(calculate_time_diff, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_CALCULATE_TIME_DIFF
/**
 * Compute (*now - *then) in centiseconds.
 */
int
calculate_time_diff(const struct timeval *now, const struct timeval *then)
{
    struct timeval  diff;

    NETSNMP_TIMERSUB(now, then, &diff);
    return (int)(diff.tv_sec * 100 + diff.tv_usec / 10000);
}
#endif /* NETSNMP_FEATURE_REMOVE_CALCULATE_TIME_DIFF */

#ifndef NETSNMP_FEATURE_REMOVE_CALCULATE_SECTIME_DIFF
/** Compute rounded (*now - *then) in seconds. */
u_int
calculate_sectime_diff(const struct timeval *now, const struct timeval *then)
{
    struct timeval  diff;

    NETSNMP_TIMERSUB(now, then, &diff);
    return diff.tv_sec + (diff.tv_usec >= 500000L);
}
#endif /* NETSNMP_FEATURE_REMOVE_CALCULATE_SECTIME_DIFF */

#ifndef HAVE_STRCASESTR
/*
 * only glibc2 has this.
 */
char           *
strcasestr(const char *haystack, const char *needle)
{
    const char     *cp1 = haystack, *cp2 = needle;
    const char     *cx;
    int             tstch1, tstch2;

    /*
     * printf("looking for '%s' in '%s'\n", needle, haystack); 
     */
    if (cp1 && cp2 && *cp1 && *cp2)
        for (cp1 = haystack, cp2 = needle; *cp1;) {
            cx = cp1;
            cp2 = needle;
            do {
                /*
                 * printf("T'%c' ", *cp1); 
                 */
                if (!*cp2) {    /* found the needle */
                    /*
                     * printf("\nfound '%s' in '%s'\n", needle, cx); 
                     */
                    return NETSNMP_REMOVE_CONST(char *, cx);
                }
                if (!*cp1)
                    break;

                tstch1 = toupper(*cp1);
                tstch2 = toupper(*cp2);
                if (tstch1 != tstch2)
                    break;
                /*
                 * printf("M'%c' ", *cp1); 
                 */
                cp1++;
                cp2++;
            }
            while (1);
            if (*cp1)
                cp1++;
        }
    /*
     * printf("\n"); 
     */
    if (cp1 && *cp1)
        return NETSNMP_REMOVE_CONST(char *, cp1);

    return NULL;
}
#endif

int
mkdirhier(const char *pathname, mode_t mode, int skiplast)
{
    struct stat     sbuf;
    char           *ourcopy = strdup(pathname);
    char           *entry;
    char           *buf = NULL;
    char           *st = NULL;
    int             res;

    res = SNMPERR_GENERR;
    if (!ourcopy)
        goto out;

    buf = malloc(strlen(pathname) + 2);
    if (!buf)
        goto out;

#if defined (WIN32) || defined (cygwin)
    /* convert backslash to forward slash */
    for (entry = ourcopy; *entry; entry++)
        if (*entry == '\\')
            *entry = '/';
#endif

    entry = strtok_r(ourcopy, "/", &st);

    buf[0] = '\0';

#if defined (WIN32) || defined (cygwin)
    /*
     * Check if first entry contains a drive-letter
     *   e.g  "c:/path"
     */
    if ((entry) && (':' == entry[1]) &&
        (('\0' == entry[2]) || ('/' == entry[2]))) {
        strcat(buf, entry);
        entry = strtok_r(NULL, "/", &st);
    }
#endif

    /*
     * check to see if filename is a directory 
     */
    while (entry) {
        strcat(buf, "/");
        strcat(buf, entry);
        entry = strtok_r(NULL, "/", &st);
        if (entry == NULL && skiplast)
            break;
        if (stat(buf, &sbuf) < 0) {
            /*
             * DNE, make it 
             */
#ifdef WIN32
            if (CreateDirectory(buf, NULL) == 0)
#else
            if (mkdir(buf, mode) == -1)
#endif
                goto out;
            else
                snmp_log(LOG_INFO, "Created directory: %s\n", buf);
        } else {
            /*
             * exists, is it a file? 
             */
            if ((sbuf.st_mode & S_IFDIR) == 0) {
                /*
                 * ack! can't make a directory on top of a file 
                 */
                goto out;
            }
        }
    }
    res = SNMPERR_SUCCESS;
out:
    free(buf);
    free(ourcopy);
    return res;
}

/**
 * netsnmp_mktemp creates a temporary file based on the
 *                 configured tempFilePattern
 *
 * @return file descriptor
 */
const char     *
netsnmp_mktemp(void)
{
#ifdef PATH_MAX
    static char     name[PATH_MAX];
#else
    static char     name[256];
#endif
    int             fd = -1;

    strlcpy(name, get_temp_file_pattern(), sizeof(name));
#ifdef HAVE_MKSTEMP
    {
        mode_t oldmask = umask(~(S_IRUSR | S_IWUSR));
        netsnmp_assert(oldmask != (mode_t)(-1));
        fd = mkstemp(name);
        umask(oldmask);
    }
#else
    if (mktemp(name)) {
# ifndef WIN32
        fd = open(name, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
# else
        /*
         * Win32 needs _S_IREAD | _S_IWRITE to set permissions on file
         * after closing
         */
        fd = _open(name, _O_CREAT | _O_EXCL | _O_WRONLY, _S_IREAD | _S_IWRITE);
# endif
    }
#endif
    if (fd >= 0) {
        close(fd);
        DEBUGMSGTL(("netsnmp_mktemp", "temp file created: %s\n",
                    name));
        return name;
    }
    snmp_log(LOG_ERR, "netsnmp_mktemp: error creating file %s\n",
             name);
    return NULL;
}

/*
 * This function was created to differentiate actions
 * that are appropriate for Linux 2.4 kernels, but not later kernels.
 *
 * This function can be used to test kernels on any platform that supports uname().
 *
 * If not running a platform that supports uname(), return -1.
 *
 * If ospname matches, and the release matches up through the prefix,
 *  return 0.
 * If the release is ordered higher, return 1.
 * Be aware that "ordered higher" is not a guarantee of correctness.
 */
int
netsnmp_os_prematch(const char *ospmname,
                    const char *ospmrelprefix)
{
#if HAVE_SYS_UTSNAME_H
static int printOSonce = 1;
  struct utsname utsbuf;
  if ( 0 != uname(&utsbuf))
    return -1;

  if (printOSonce) {
    printOSonce = 0;
    /* show the four elements that the kernel can be sure of */
  DEBUGMSGT(("daemonize","sysname '%s',\nrelease '%s',\nversion '%s',\nmachine '%s'\n",
      utsbuf.sysname, utsbuf.release, utsbuf.version, utsbuf.machine));
  }
  if (0 != strcasecmp(utsbuf.sysname, ospmname)) return -1;

  /* Required to match only the leading characters */
  return strncasecmp(utsbuf.release, ospmrelprefix, strlen(ospmrelprefix));

#else

  return -1;

#endif /* HAVE_SYS_UTSNAME_H */
}

/**
 * netsnmp_os_kernel_width determines kernel width at runtime
 * Currently implemented for IRIX, AIX and Tru64 Unix
 *
 * @return kernel width (usually 32 or 64) on success, -1 on error
 */
int
netsnmp_os_kernel_width(void)
{
#ifdef irix6
  char buf[8];
  sysinfo(_MIPS_SI_OS_NAME, buf, 7);
  if (strncmp("IRIX64", buf, 6) == 0) {
    return 64;
  } else if (strncmp("IRIX", buf, 4) == 0) {
    return 32;
  } else {
    return -1;
  }
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
  return (__KERNEL_32() ? 32 : (__KERNEL_64() ? 64 : -1));
#elif defined(osf4) || defined(osf5) || defined(__alpha)
  return 64; /* Alpha is always 64bit */
#else
  /* kernel width detection not implemented */
  return -1;
#endif
}

netsnmp_feature_child_of(str_to_uid, user_information)
#ifndef NETSNMP_FEATURE_REMOVE_STR_TO_UID
/**
 * Convert a user name or number into numeric form.
 *
 * @param[in] useroruid Either a Unix user name or the ASCII representation
 *   of a user number.
 *
 * @return Either a user number > 0 or 0 if useroruid is not a valid user
 *   name, not a valid user number or the name of the root user.
 */
int netsnmp_str_to_uid(const char *useroruid) {
    int uid;
#if HAVE_GETPWNAM && HAVE_PWD_H
    struct passwd *pwd;
#endif

    uid = atoi(useroruid);

    if (uid == 0) {
#if HAVE_GETPWNAM && HAVE_PWD_H
        pwd = getpwnam(useroruid);
        uid = pwd ? pwd->pw_uid : 0;
        endpwent();
#endif
        if (uid == 0)
            snmp_log(LOG_WARNING, "Can't identify user (%s).\n", useroruid);
    }
    return uid;
    
}
#endif /* NETSNMP_FEATURE_REMOVE_STR_TO_UID */

netsnmp_feature_child_of(str_to_gid, user_information)
#ifndef NETSNMP_FEATURE_REMOVE_STR_TO_GID
/**
 * Convert a group name or number into numeric form.
 *
 * @param[in] grouporgid Either a Unix group name or the ASCII representation
 *   of a group number.
 *
 * @return Either a group number > 0 or 0 if grouporgid is not a valid group
 *   name, not a valid group number or the root group.
 */
int netsnmp_str_to_gid(const char *grouporgid)
{
    int gid;

    gid = atoi(grouporgid);

    if (gid == 0) {
#if HAVE_GETGRNAM && HAVE_GRP_H
        struct group  *grp;

        grp = getgrnam(grouporgid);
        gid = grp ? grp->gr_gid : 0;
        endgrent();
#endif
        if (gid == 0)
            snmp_log(LOG_WARNING, "Can't identify group (%s).\n", grouporgid);
    }

    return gid;
}
#endif /* NETSNMP_FEATURE_REMOVE_STR_TO_GID */

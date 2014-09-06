/**
 * @file  snmpSocketBaseDomain.c
 *
 * @brief Socket support functions.
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpSocketBaseDomain.h>

#include <stddef.h>
#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <errno.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/library/system.h>
#include <net-snmp/library/snmp_assert.h>

/* all sockets pretty much close the same way */
int netsnmp_socketbase_close(netsnmp_transport *t) {
    int rc = -1;
    if (t->sock >= 0) {
#ifndef HAVE_CLOSESOCKET
        rc = close(t->sock);
#else
        rc = closesocket(t->sock);
#endif
        t->sock = -1;
    }
    return rc;
}

/*
 * find largest possible buffer between current size and specified size.
 *
 * Try to maximize the current buffer of type "optname"
 * to the maximum allowable size by the OS (as close to
 * size as possible)
 */
static int
_sock_buffer_maximize(int s, int optname, const char *buftype, int size)
{
    int            curbuf = 0;
    socklen_t      curbuflen = sizeof(int);
    int            lo, mid, hi;

    /*
     * First we need to determine our current buffer
     */
    if ((getsockopt(s, SOL_SOCKET, optname, (void *) &curbuf,
                    &curbuflen) == 0) 
            && (curbuflen == sizeof(int))) {

        DEBUGMSGTL(("verbose:socket:buffer:max", "Current %s is %d\n",
                    buftype, curbuf));

        /*
         * Let's not be stupid ... if we were asked for less than what we
         * already have, then forget about it
         */
        if (size <= curbuf) {
            DEBUGMSGTL(("verbose:socket:buffer:max",
                        "Requested %s <= current buffer\n", buftype));
            return curbuf;
        }

        /*
         * Do a binary search the optimal buffer within 1k of the point of
         * failure. This is rather bruteforce, but simple
         */
        hi = size;
        lo = curbuf;

        while (hi - lo > 1024) {
            mid = (lo + hi) / 2;
            if (setsockopt(s, SOL_SOCKET, optname, (void *) &mid,
                        sizeof(int)) == 0) {
                lo = mid; /* Success: search between mid and hi */
            } else {
                hi = mid; /* Failed: search between lo and mid */
            }
        }

        /*
         * Now print if this optimization helped or not
         */
        if (getsockopt(s,SOL_SOCKET, optname, (void *) &curbuf,
                    &curbuflen) == 0) {
            DEBUGMSGTL(("socket:buffer:max", 
                        "Maximized %s: %d\n",buftype, curbuf));
        } 
    } else {
        /*
         * There is really not a lot we can do anymore.
         * If the OS doesn't give us the current buffer, then what's the 
         * point in trying to make it better
         */
        DEBUGMSGTL(("socket:buffer:max", "Get %s failed ... giving up!\n",
                    buftype));
        curbuf = -1;
    }

    return curbuf;
}


static const char *
_sock_buf_type_get(int optname, int local)
{
    if (optname == SO_SNDBUF) {
        if (local)
            return "server send buffer";
        else
            return "client send buffer";
    } else if (optname == SO_RCVBUF) {
        if (local)
            return "server receive buffer";
        else
            return "client receive buffer";
    }

    return "unknown buffer";
}

/*
 *
 * Get the requested buffersize, based on
 * - sockettype : client (local = 0) or server (local = 1) 
 * - buffertype : send (optname = SO_SNDBUF) or recv (SO_RCVBUF)
 *
 * In case a compile time buffer was specified, then use that one
 * if there was no runtime configuration override
 */
static int
_sock_buffer_size_get(int optname, int local, const char **buftype)
{
    int size;

    if (NULL != buftype)
        *buftype = _sock_buf_type_get(optname, local);

    if (optname == SO_SNDBUF) {
        if (local) {
            size = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
                    NETSNMP_DS_LIB_SERVERSENDBUF);
#ifdef NETSNMP_DEFAULT_SERVER_SEND_BUF
            if (size <= 0)
               size = NETSNMP_DEFAULT_SERVER_SEND_BUF;
#endif
        } else {
            size = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
                    NETSNMP_DS_LIB_CLIENTSENDBUF);
#ifdef NETSNMP_DEFAULT_CLIENT_SEND_BUF
            if (size <= 0)
               size = NETSNMP_DEFAULT_CLIENT_SEND_BUF;
#endif
        }
    } else if (optname == SO_RCVBUF) {
        if (local) {
            size = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
                    NETSNMP_DS_LIB_SERVERRECVBUF);
#ifdef NETSNMP_DEFAULT_SERVER_RECV_BUF
            if (size <= 0)
               size = NETSNMP_DEFAULT_SERVER_RECV_BUF;
#endif
        } else {
            size = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
                    NETSNMP_DS_LIB_CLIENTRECVBUF);
#ifdef NETSNMP_DEFAULT_CLIENT_RECV_BUF
            if (size <= 0)
               size = NETSNMP_DEFAULT_CLIENT_RECV_BUF;
#endif
        }
    } else {
        size = 0;
    }

    DEBUGMSGTL(("socket:buffer", "Requested %s is %d\n",
                (buftype) ? *buftype : "unknown buffer", size));

    return(size);
}

/*
 * set socket buffer size
 *
 * @param ss     : socket
 * @param optname: SO_SNDBUF or SO_RCVBUF
 * @param local  : 1 for server, 0 for client
 * @param reqbuf : requested size, or 0 for default
 *
 * @retval    -1 : error
 * @retval    >0 : new buffer size
 */
int
netsnmp_sock_buffer_set(int s, int optname, int local, int size)
{
#if ! defined(SO_SNDBUF) && ! defined(SO_RCVBUF)
    DEBUGMSGTL(("socket:buffer", "Changing socket buffer is not supported\n"));
    return -1;
#else
    const char     *buftype;
    int            curbuf = 0;
    socklen_t      curbuflen = sizeof(int);

#   ifndef  SO_SNDBUF
    if (SO_SNDBUF == optname) {
        DEBUGMSGTL(("socket:buffer",
                    "Changing socket send buffer is not supported\n"));
        return -1;
    }
#   endif                          /*SO_SNDBUF */
#   ifndef  SO_RCVBUF
    if (SO_RCVBUF == optname) {
        DEBUGMSGTL(("socket:buffer",
                    "Changing socket receive buffer is not supported\n"));
        return -1;
    }
#   endif                          /*SO_RCVBUF */

    /*
     * What is the requested buffer size ?
     */
    if (0 == size)
        size = _sock_buffer_size_get(optname, local, &buftype);
    else {
        buftype = _sock_buf_type_get(optname, local);
        DEBUGMSGT(("verbose:socket:buffer", "Requested %s is %d\n",
                   buftype, size));
    }

    if ((getsockopt(s, SOL_SOCKET, optname, (void *) &curbuf,
                    &curbuflen) == 0) 
        && (curbuflen == sizeof(int))) {
        
        DEBUGMSGT(("verbose:socket:buffer", "Original %s is %d\n",
                   buftype, curbuf));
        if (curbuf >= size) {
            DEBUGMSGT(("verbose:socket:buffer",
                      "New %s size is smaller than original!\n", buftype));
        }
    }

    /*
     * If the buffersize was not specified or it was a negative value
     * then don't change the OS buffers at all
     */
    if (size <= 0) {
       DEBUGMSGT(("socket:buffer",
                    "%s not valid or not specified; using OS default(%d)\n",
                    buftype,curbuf));
       return curbuf;
    }

    /*
     * Try to set the requested send buffer
     */
    if (setsockopt(s, SOL_SOCKET, optname, (void *) &size, sizeof(int)) == 0) {
        /*
         * Because some platforms lie about the actual buffer that has been 
         * set (Linux will always say it worked ...), we print some 
         * diagnostic output for debugging
         */
        DEBUGIF("socket:buffer") {
            DEBUGMSGT(("socket:buffer", "Set %s to %d\n",
                       buftype, size));
            if ((getsockopt(s, SOL_SOCKET, optname, (void *) &curbuf,
                            &curbuflen) == 0) 
                    && (curbuflen == sizeof(int))) {

                DEBUGMSGT(("verbose:socket:buffer",
                           "Now %s is %d\n", buftype, curbuf));
            }
        }
        /*
         * If the new buffer is smaller than the size we requested, we will
         * try to increment the new buffer with 1k increments 
         * (this will sometime allow us to reach a more optimal buffer.)
         *   For example : On Solaris, if the max OS buffer is 100k and you
         *   request 110k, you end up with the default 8k :-(
         */
        if (curbuf < size) {
            curbuf = _sock_buffer_maximize(s, optname, buftype, size);
            if(-1 != curbuf)
                size = curbuf;
        }

    } else {
        /*
         * Obviously changing the buffer failed, most like like because we 
         * requested a buffer greater than the OS limit.
         * Therefore we need to search for an optimal buffer that is close
         * enough to the point of failure.
         * This will allow us to reach a more optimal buffer.
         *   For example : On Solaris, if the max OS buffer is 100k and you 
         *   request 110k, you end up with the default 8k :-(
         *   After this quick seach we would get 1k close to 100k (the max)
         */
        DEBUGMSGTL(("socket:buffer", "couldn't set %s to %d\n",
                    buftype, size));

        curbuf = _sock_buffer_maximize(s, optname, buftype, size);
        if(-1 != curbuf)
            size = curbuf;
    }

    return size;
#endif
}


/**
 * Sets the mode of a socket for all subsequent I/O operations.
 *
 * @param[in] sock Socket descriptor (Unix) or socket handle (Windows).
 * @param[in] non_blocking_mode I/O mode: non-zero selects non-blocking mode;
 *   zero selects blocking mode.
 *
 * @return zero upon success and a negative value upon error.
 */
int
netsnmp_set_non_blocking_mode(int sock, int non_blocking_mode)
{
#ifdef WIN32
    u_long          arg;

    arg = non_blocking_mode;
    return ioctlsocket(sock, FIONBIO, &arg);
#else
    int             sockflags;

    if ((sockflags = fcntl(sock, F_GETFL, 0)) >= 0) {
        return fcntl(sock, F_SETFL,
                     non_blocking_mode ? sockflags | O_NONBLOCK
                     : sockflags & ~O_NONBLOCK);
    } else
        return -1;
#endif
}

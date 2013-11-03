/*
 * Copyright (c) 1997, 1998 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 *
 * this provides both proto_open() and proto_close() to account for
 * protocol specific initialization and shutdown procedures. all the
 * read/write stuff is done in dsi_stream.c.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>

#include <sys/ioctl.h>
#ifdef TRU64
#include <sys/mbuf.h>
#include <net/route.h>
#endif /* TRU64 */
#include <net/if.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <atalk/logger.h>

#ifdef __svr4__
#include <sys/sockio.h>
#endif /* __svr4__ */

#ifdef TCPWRAP
#include <tcpd.h>
int allow_severity = log_info;
int deny_severity = log_warning;
#endif /* TCPWRAP */

#include <atalk/dsi.h>
#include <atalk/compat.h>
#include <atalk/util.h>
#include <atalk/errchk.h>

#define min(a,b)  ((a) < (b) ? (a) : (b))

#ifndef DSI_TCPMAXPEND
#define DSI_TCPMAXPEND      20       /* max # of pending connections */
#endif /* DSI_TCPMAXPEND */

#ifndef DSI_TCPTIMEOUT
#define DSI_TCPTIMEOUT      120     /* timeout in seconds for connections */
#endif /* ! DSI_TCPTIMEOUT */


/* FIXME/SOCKLEN_T: socklen_t is a unix98 feature. */
#ifndef SOCKLEN_T
#define SOCKLEN_T unsigned int
#endif /* ! SOCKLEN_T */

static void dsi_tcp_close(DSI *dsi)
{
    if (dsi->socket == -1)
        return;

    close(dsi->socket);
    dsi->socket = -1;
}

/* alarm handler for tcp_open */
static void timeout_handler(int sig _U_)
{
    LOG(log_error, logtype_dsi, "dsi_tcp_open: connection timed out");
    exit(EXITERR_CLNT);
}

/*!
 * Allocate DSI read buffer and read-ahead buffer
 */
static void dsi_init_buffer(DSI *dsi)
{
    if ((dsi->commands = malloc(dsi->server_quantum)) == NULL) {
        LOG(log_error, logtype_dsi, "dsi_init_buffer: OOM");
        AFP_PANIC("OOM in dsi_init_buffer");
    }

    /* dsi_peek() read ahead buffer, default is 12 * 300k = 3,6 MB (Apr 2011) */
    if ((dsi->buffer = malloc(dsi->dsireadbuf * dsi->server_quantum)) == NULL) {
        LOG(log_error, logtype_dsi, "dsi_init_buffer: OOM");
        AFP_PANIC("OOM in dsi_init_buffer");
    }
    dsi->start = dsi->buffer;
    dsi->eof = dsi->buffer;
    dsi->end = dsi->buffer + (dsi->dsireadbuf * dsi->server_quantum);
}

/*!
 * Free any allocated ressources of the master afpd DSI objects and close server socket
 */
void dsi_free(DSI *dsi)
{
    close(dsi->serversock);
    dsi->serversock = -1;

    free(dsi->commands);
    dsi->commands = NULL;

    free(dsi->buffer);
    dsi->buffer = NULL;

#ifdef USE_ZEROCONF
    free(dsi->bonjourname);
    dsi->bonjourname = NULL;
#endif
}

static struct itimerval itimer;
/* accept the socket and do a little sanity checking */
static pid_t dsi_tcp_open(DSI *dsi)
{
    pid_t pid;
    SOCKLEN_T len;

    len = sizeof(dsi->client);
    dsi->socket = accept(dsi->serversock, (struct sockaddr *) &dsi->client, &len);

#ifdef TCPWRAP
    {
        struct request_info req;
        request_init(&req, RQ_DAEMON, "afpd", RQ_FILE, dsi->socket, NULL);
        fromhost(&req);
        if (!hosts_access(&req)) {
            LOG(deny_severity, logtype_dsi, "refused connect from %s", eval_client(&req));
            close(dsi->socket);
            errno = ECONNREFUSED;
            dsi->socket = -1;
        }
    }
#endif /* TCPWRAP */

    if (dsi->socket < 0)
        return -1;

    getitimer(ITIMER_PROF, &itimer);
    if (0 == (pid = fork()) ) { /* child */
        static struct itimerval timer = {{0, 0}, {DSI_TCPTIMEOUT, 0}};
        struct sigaction newact, oldact;
        uint8_t block[DSI_BLOCKSIZ];
        size_t stored;

        /* reset signals */
        server_reset_signal();

#ifndef DEBUGGING
        /* install an alarm to deal with non-responsive connections */
        newact.sa_handler = timeout_handler;
        sigemptyset(&newact.sa_mask);
        newact.sa_flags = 0;
        sigemptyset(&oldact.sa_mask);
        oldact.sa_flags = 0;
        setitimer(ITIMER_PROF, &itimer, NULL);

        if ((sigaction(SIGALRM, &newact, &oldact) < 0) ||
            (setitimer(ITIMER_REAL, &timer, NULL) < 0)) {
            LOG(log_error, logtype_dsi, "dsi_tcp_open: %s", strerror(errno));
            exit(EXITERR_SYS);
        }
#endif

        dsi_init_buffer(dsi);

        /* read in commands. this is similar to dsi_receive except
         * for the fact that we do some sanity checking to prevent
         * delinquent connections from causing mischief. */

        /* read in the first two bytes */
        len = dsi_stream_read(dsi, block, 2);
        if (!len ) {
            /* connection already closed, don't log it (normal OSX 10.3 behaviour) */
            exit(EXITERR_CLNT);
        }
        if (len < 2 || (block[0] > DSIFL_MAX) || (block[1] > DSIFUNC_MAX)) {
            LOG(log_error, logtype_dsi, "dsi_tcp_open: invalid header");
            exit(EXITERR_CLNT);
        }

        /* read in the rest of the header */
        stored = 2;
        while (stored < DSI_BLOCKSIZ) {
            len = dsi_stream_read(dsi, block + stored, sizeof(block) - stored);
            if (len > 0)
                stored += len;
            else {
                LOG(log_error, logtype_dsi, "dsi_tcp_open: stream_read: %s", strerror(errno));
                exit(EXITERR_CLNT);
            }
        }

        dsi->header.dsi_flags = block[0];
        dsi->header.dsi_command = block[1];
        memcpy(&dsi->header.dsi_requestID, block + 2,
               sizeof(dsi->header.dsi_requestID));
        memcpy(&dsi->header.dsi_data.dsi_code, block + 4, sizeof(dsi->header.dsi_data.dsi_code));
        memcpy(&dsi->header.dsi_len, block + 8, sizeof(dsi->header.dsi_len));
        memcpy(&dsi->header.dsi_reserved, block + 12,
               sizeof(dsi->header.dsi_reserved));
        dsi->clientID = ntohs(dsi->header.dsi_requestID);

        /* make sure we don't over-write our buffers. */
        dsi->cmdlen = min(ntohl(dsi->header.dsi_len), dsi->server_quantum);

        stored = 0;
        while (stored < dsi->cmdlen) {
            len = dsi_stream_read(dsi, dsi->commands + stored, dsi->cmdlen - stored);
            if (len > 0)
                stored += len;
            else {
                LOG(log_error, logtype_dsi, "dsi_tcp_open: stream_read: %s", strerror(errno));
                exit(EXITERR_CLNT);
            }
        }

        /* stop timer and restore signal handler */
#ifndef DEBUGGING
        memset(&timer, 0, sizeof(timer));
        setitimer(ITIMER_REAL, &timer, NULL);
        sigaction(SIGALRM, &oldact, NULL);
#endif

        LOG(log_info, logtype_dsi, "AFP/TCP session from %s:%u",
            getip_string((struct sockaddr *)&dsi->client),
            getip_port((struct sockaddr *)&dsi->client));
    }

    /* send back our pid */
    return pid;
}

/* get it from the interface list */
#ifndef IFF_SLAVE
#define IFF_SLAVE 0
#endif

static void guess_interface(DSI *dsi, const char *hostname, const char *port)
{
    int fd;
    char **start, **list;
    struct ifreq ifr;
    struct sockaddr_in *sa = (struct sockaddr_in *)&dsi->server;

    start = list = getifacelist();
    if (!start)
        return;
        
    fd = socket(PF_INET, SOCK_STREAM, 0);

    while (list && *list) {
        strlcpy(ifr.ifr_name, *list, sizeof(ifr.ifr_name));
        list++;


        if (ioctl(dsi->serversock, SIOCGIFFLAGS, &ifr) < 0)
            continue;

        if (ifr.ifr_flags & (IFF_LOOPBACK | IFF_POINTOPOINT | IFF_SLAVE))
            continue;

        if (!(ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) )
            continue;

        if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
            continue;

        memset(&dsi->server, 0, sizeof(struct sockaddr_storage));
        sa->sin_family = AF_INET;
        sa->sin_port = htons(atoi(port));
        sa->sin_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr;

        LOG(log_info, logtype_dsi, "dsi_tcp: '%s:%s' on interface '%s' will be used instead.",
            getip_string((struct sockaddr *)&dsi->server), port, ifr.ifr_name);
        goto iflist_done;
    }

    LOG(log_note, logtype_dsi, "dsi_tcp: couldn't find network interface with IP address to advertice, "
        "check to make sure \"%s\" is in /etc/hosts or can be resolved with DNS, or "
        "add a netinterface that is not a loopback or point-2-point type", hostname);

iflist_done:
    close(fd);
    freeifacelist(start);
}


#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

/*!
 * Initialize DSI over TCP
 *
 * @param dsi        (rw) DSI handle
 * @param hostname   (r)  pointer to hostname string
 * @param inaddress  (r)  Optional IPv4 or IPv6 address with an optional port, may be NULL
 * @param inport     (r)  pointer to port string
 *
 * Creates listening AFP/DSI socket. If the parameter inaddress is NULL, then we listen
 * on the wildcard address, ie on all interfaces. That should mean listening on the IPv6
 * address "::" on IPv4/IPv6 dual stack kernels, accepting both v4 and v6 requests.
 *
 * If the parameter inaddress is not NULL, then we only listen on the given address.
 * The parameter may contain a port number using the URL format for address and port:
 *
 *   IPv4, IPv4:port, IPv6, [IPv6], [IPv6]:port
 *
 * Parameter inport must be a valid pointer to a port string and is used if the inaddress
 * parameter doesn't contain a port.
 *
 * @returns 0 on success, -1 on failure
 */
int dsi_tcp_init(DSI *dsi, const char *hostname, const char *inaddress, const char *inport)
{
    EC_INIT;
    int                flag, err;
    char              *address = NULL, *port = NULL;
    struct addrinfo    hints, *servinfo, *p;

    /* inaddress may be NULL */
    AFP_ASSERT(dsi && hostname && inport);

    if (inaddress)
        /* Check whether address is of the from IP:PORT and split */
        EC_ZERO( tokenize_ip_port(inaddress, &address, &port) );

    if (port == NULL)
        /* inport is supposed to always contain a valid port string */
        EC_NULL( port = strdup(inport) );

    /* Prepare hint for getaddrinfo */
    memset(&hints, 0, sizeof hints);
#if !defined(FREEBSD)
    hints.ai_family = AF_UNSPEC;
#endif
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;

    if ( ! address) {
        hints.ai_flags |= AI_PASSIVE;
#if defined(FREEBSD)
        hints.ai_family = AF_INET6;
#endif
    } else {
        hints.ai_flags |= AI_NUMERICHOST;
#if defined(FREEBSD)
        hints.ai_family = AF_UNSPEC;
#endif
    }
    if ((ret = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
        LOG(log_error, logtype_dsi, "dsi_tcp_init(%s): getaddrinfo: %s\n", address ? address : "*", gai_strerror(ret));
        EC_FAIL;
    }

    /* loop through all the results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((dsi->serversock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            LOG(log_info, logtype_dsi, "dsi_tcp_init: socket: %s", strerror(errno));
            continue;
        }

        /*
         * Set some socket options:
         * SO_REUSEADDR deals w/ quick close/opens
         * TCP_NODELAY diables Nagle
         */
#ifdef SO_REUSEADDR
        flag = 1;
        setsockopt(dsi->serversock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
#endif
#if defined(FREEBSD) && defined(IPV6_BINDV6ONLY)
        int on = 0;
        setsockopt(dsi->serversock, IPPROTO_IPV6, IPV6_BINDV6ONLY, (char *)&on, sizeof (on));
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
        flag = 1;
        setsockopt(dsi->serversock, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag));
            
        if (bind(dsi->serversock, p->ai_addr, p->ai_addrlen) == -1) {
            close(dsi->serversock);
            LOG(log_info, logtype_dsi, "dsi_tcp_init: bind: %s\n", strerror(errno));
            continue;
        }

        if (listen(dsi->serversock, DSI_TCPMAXPEND) < 0) {
            close(dsi->serversock);
            LOG(log_info, logtype_dsi, "dsi_tcp_init: listen: %s\n", strerror(errno));
            continue;
        }
            
        break;
    }

    if (p == NULL)  {
        LOG(log_error, logtype_dsi, "dsi_tcp_init: no suitable network config for TCP socket");
        freeaddrinfo(servinfo);
        EC_FAIL;
    }

    /* Copy struct sockaddr to struct sockaddr_storage */
    memcpy(&dsi->server, p->ai_addr, p->ai_addrlen);
    freeaddrinfo(servinfo);

    /* Point protocol specific functions to tcp versions */
    dsi->proto_open = dsi_tcp_open;
    dsi->proto_close = dsi_tcp_close;

    /* get real address for GetStatus. */

    if (address) {
        /* address is a parameter, use it 'as is' */
        goto EC_CLEANUP;
    }

    /* Prepare hint for getaddrinfo */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((err = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        LOG(log_info, logtype_dsi, "dsi_tcp_init: getaddrinfo '%s': %s\n", hostname, gai_strerror(err));
        goto interfaces;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            if ( (ipv4->sin_addr.s_addr & htonl(0x7f000000)) != htonl(0x7f000000) )
                break;
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            unsigned char ipv6loopb[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
            if ((memcmp(ipv6->sin6_addr.s6_addr, ipv6loopb, 16)) != 0)
                break;
        }
    }

    if (p) {
        /* Store found address in dsi->server */
        memcpy(&dsi->server, p->ai_addr, p->ai_addrlen);
        freeaddrinfo(servinfo);
        goto EC_CLEANUP;
    }
    LOG(log_info, logtype_dsi, "dsi_tcp: hostname '%s' resolves to loopback address", hostname);
    freeaddrinfo(servinfo);

interfaces:
    guess_interface(dsi, hostname, port ? port : "548");

EC_CLEANUP:
    if (address)
        free(address);
    if (port)
        free(port);
    EC_EXIT;
}


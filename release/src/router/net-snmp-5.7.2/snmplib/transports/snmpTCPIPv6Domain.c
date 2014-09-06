#include <net-snmp/net-snmp-config.h>

#ifdef NETSNMP_TRANSPORT_TCPIPV6_DOMAIN

#include <net-snmp/types.h>
#include <net-snmp/library/snmpTCPIPv6Domain.h>

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmpSocketBaseDomain.h>
#include <net-snmp/library/snmpTCPBaseDomain.h>
#include <net-snmp/library/tools.h>

#include "inet_ntop.h"

oid netsnmp_TCPIPv6Domain[] = { TRANSPORT_DOMAIN_TCP_IPV6 };
static netsnmp_tdomain tcp6Domain;

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.  
 */

static char *
netsnmp_tcp6_fmtaddr(netsnmp_transport *t, void *data, int len)
{
    return netsnmp_ipv6_fmtaddr("TCP/IPv6", t, data, len);
}

static int
netsnmp_tcp6_accept(netsnmp_transport *t)
{
    struct sockaddr_in6 *farend = NULL;
    int             newsock = -1;
    socklen_t       farendlen = sizeof(struct sockaddr_in6);

    farend = (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));

    if (farend == NULL) {
        /*
         * Indicate that the acceptance of this socket failed.  
         */
        DEBUGMSGTL(("netsnmp_tcp6", "accept: malloc failed\n"));
        return -1;
    }

    if (t != NULL && t->sock >= 0) {
        newsock = accept(t->sock, (struct sockaddr *) farend, &farendlen);

        if (newsock < 0) {
            DEBUGMSGTL(("netsnmp_tcp6","accept failed rc %d errno %d \"%s\"\n",
			newsock, errno, strerror(errno)));
            free(farend);
            return newsock;
        }

        if (t->data != NULL) {
            free(t->data);
        }

        t->data = farend;
        t->data_length = farendlen;
        DEBUGIF("netsnmp_tcp6") {
            char *str = netsnmp_tcp6_fmtaddr(NULL, farend, farendlen);
            DEBUGMSGTL(("netsnmp_tcp6", "accept succeeded (from %s)\n", str));
            free(str);
        }

        /*
         * Try to make the new socket blocking.  
         */

        if (netsnmp_set_non_blocking_mode(newsock, FALSE) < 0)
            DEBUGMSGTL(("netsnmp_tcp6",
                        "accept: couldn't f_getfl of fd %d\n", newsock));

        /*
         * Allow user to override the send and receive buffers. Default is
         * to use os default.  Don't worry too much about errors --
         * just plough on regardless.  
         */
        netsnmp_sock_buffer_set(newsock, SO_SNDBUF, 1, 0);
        netsnmp_sock_buffer_set(newsock, SO_RCVBUF, 1, 0);

        return newsock;
    } else {
        free(farend);
        return -1;
    }
}



/*
 * Open a TCP/IPv6-based transport for SNMP.  Local is TRUE if addr is the
 * local address to bind to (i.e. this is a server-type session); otherwise
 * addr is the remote address to send things to.  
 */

netsnmp_transport *
netsnmp_tcp6_transport(struct sockaddr_in6 *addr, int local)
{
    netsnmp_transport *t = NULL;
    int             rc = 0;

#ifdef NETSNMP_NO_LISTEN_SUPPORT
    if (local)
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

    if (addr == NULL || addr->sin6_family != AF_INET6) {
        return NULL;
    }

    t = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    if (t == NULL) {
        return NULL;
    }

    DEBUGIF("netsnmp_tcp6") {
        char *str = netsnmp_tcp6_fmtaddr(NULL, (void *)addr,
                                   sizeof(struct sockaddr_in6));
        DEBUGMSGTL(("netsnmp_tcp6", "open %s %s\n", local ? "local" : "remote",
                    str));
        free(str);
    }

    t->data = malloc(sizeof(netsnmp_indexed_addr_pair));
    if (t->data == NULL) {
        netsnmp_transport_free(t);
        return NULL;
    }
    t->data_length = sizeof(netsnmp_indexed_addr_pair);
    memcpy(t->data, addr, sizeof(struct sockaddr_in6));

    t->domain = netsnmp_TCPIPv6Domain;
    t->domain_length = sizeof(netsnmp_TCPIPv6Domain) / sizeof(oid);

    t->sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (t->sock < 0) {
        netsnmp_transport_free(t);
        return NULL;
    }

    t->flags = NETSNMP_TRANSPORT_FLAG_STREAM;

    if (local) {
#ifndef NETSNMP_NO_LISTEN_SUPPORT
        int opt = 1;

        /*
         * This session is inteneded as a server, so we must bind on to the
         * given IP address, which may include an interface address, or could
         * be INADDR_ANY, but certainly includes a port number.
         */

#ifdef IPV6_V6ONLY
        /* Try to restrict PF_INET6 socket to IPv6 communications only. */
        {
	  int one=1;
	  if (setsockopt(t->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&one, sizeof(one)) != 0) {
	    DEBUGMSGTL(("netsnmp_tcp6", "couldn't set IPV6_V6ONLY to %d bytes: %s\n", one, strerror(errno)));
	  } 
	}
#endif

        t->flags |= NETSNMP_TRANSPORT_FLAG_LISTEN;
        t->local = (unsigned char*)malloc(18);
        if (t->local == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->local, addr->sin6_addr.s6_addr, 16);
        t->local[16] = (addr->sin6_port & 0xff00) >> 8;
        t->local[17] = (addr->sin6_port & 0x00ff) >> 0;
        t->local_length = 18;

        /*
         * We should set SO_REUSEADDR too.  
         */

        setsockopt(t->sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

        rc = bind(t->sock, (struct sockaddr *) addr,
		  sizeof(struct sockaddr_in6));
        if (rc != 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }

        /*
         * Since we are going to be letting select() tell us when connections
         * are ready to be accept()ed, we need to make the socket n0n-blocking
         * to avoid the race condition described in W. R. Stevens, ``Unix
         * Network Programming Volume I Second Edition'', pp. 422--4, which
         * could otherwise wedge the agent.
         */

        netsnmp_set_non_blocking_mode(t->sock, TRUE);

        /*
         * Now sit here and wait for connections to arrive.  
         */

        rc = listen(t->sock, NETSNMP_STREAM_QUEUE_LEN);
        if (rc != 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        
        /*
         * no buffer size on listen socket - doesn't make sense
         */
#else /* NETSNMP_NO_LISTEN_SUPPORT */
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */
    } else {
        t->remote = (unsigned char*)malloc(18);
        if (t->remote == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->remote, addr->sin6_addr.s6_addr, 16);
        t->remote[16] = (addr->sin6_port & 0xff00) >> 8;
        t->remote[17] = (addr->sin6_port & 0x00ff) >> 0;
        t->remote_length = 18;

        /*
         * This is a client-type session, so attempt to connect to the far
         * end.  We don't go non-blocking here because it's not obvious what
         * you'd then do if you tried to do snmp_sends before the connection
         * had completed.  So this can block.
         */

        rc = connect(t->sock, (struct sockaddr *) addr,
                     sizeof(struct sockaddr_in6));

        DEBUGMSGTL(("netsnmp_tcp6", "connect returns %d\n", rc));

        if (rc < 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }

        /*
         * Allow user to override the send and receive buffers. Default is
         * to use os default.  Don't worry too much about errors --
         * just plough on regardless.  
         */
        netsnmp_sock_buffer_set(t->sock, SO_SNDBUF, local, 0);
        netsnmp_sock_buffer_set(t->sock, SO_RCVBUF, local, 0);
    }

    /*
     * Message size is not limited by this transport (hence msgMaxSize
     * is equal to the maximum legal size of an SNMP message).  
     */

    t->msgMaxSize = 0x7fffffff;
    t->f_recv     = netsnmp_tcpbase_recv;
    t->f_send     = netsnmp_tcpbase_send;
    t->f_close    = netsnmp_socketbase_close;
    t->f_accept   = netsnmp_tcp6_accept;
    t->f_fmtaddr  = netsnmp_tcp6_fmtaddr;

    return t;
}

/*
 * Not extern but still defined in snmpUDPIPv6Domain.c
 */
extern int
netsnmp_sockaddr_in6_2(struct sockaddr_in6*, const char*, const char*);

netsnmp_transport *
netsnmp_tcp6_create_tstring(const char *str, int local,
			    const char *default_target)
{
    struct sockaddr_in6 addr;

    if (netsnmp_sockaddr_in6_2(&addr, str, default_target)) {
        return netsnmp_tcp6_transport(&addr, local);
    } else {
        return NULL;
    }
}


/*
 * See:
 * 
 * http://www.ietf.org/internet-drafts/draft-ietf-ops-taddress-mib-01.txt
 * 
 * (or newer equivalent) for details of the TC which we are using for
 * the mapping here.  
 */

netsnmp_transport *
netsnmp_tcp6_create_ostring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in6 addr;

    if (o_len == 18) {
        memset((u_char *) & addr, 0, sizeof(struct sockaddr_in6));
        addr.sin6_family = AF_INET6;
        memcpy((u_char *) & (addr.sin6_addr.s6_addr), o, 16);
        addr.sin6_port = (o[16] << 8) + o[17];
        return netsnmp_tcp6_transport(&addr, local);
    }
    return NULL;
}


void
netsnmp_tcpipv6_ctor(void)
{
    tcp6Domain.name = netsnmp_TCPIPv6Domain;
    tcp6Domain.name_length = sizeof(netsnmp_TCPIPv6Domain) / sizeof(oid);
    tcp6Domain.f_create_from_tstring     = NULL;
    tcp6Domain.f_create_from_tstring_new = netsnmp_tcp6_create_tstring;
    tcp6Domain.f_create_from_ostring     = netsnmp_tcp6_create_ostring;
    tcp6Domain.prefix = (const char**)calloc(4, sizeof(char *));
    tcp6Domain.prefix[0] = "tcp6";
    tcp6Domain.prefix[1] = "tcpv6";
    tcp6Domain.prefix[2] = "tcpipv6";

    netsnmp_tdomain_register(&tcp6Domain);
}

#endif /* NETSNMP_TRANSPORT_TCPIPV6_DOMAIN */


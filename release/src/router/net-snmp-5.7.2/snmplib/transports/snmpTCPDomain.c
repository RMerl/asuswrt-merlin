#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpTCPDomain.h>

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
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>

#include <net-snmp/library/snmpIPv4BaseDomain.h>
#include <net-snmp/library/snmpSocketBaseDomain.h>
#include <net-snmp/library/snmpTCPBaseDomain.h>
#include <net-snmp/library/tools.h>

/*
 * needs to be in sync with the definitions in snmplib/snmpUDPDomain.c
 * and perl/agent/agent.xs
 */
typedef netsnmp_indexed_addr_pair netsnmp_udp_addr_pair;

oid netsnmp_snmpTCPDomain[] = { TRANSPORT_DOMAIN_TCP_IP };
static netsnmp_tdomain tcpDomain;

/*
 * Not static since it is needed here as well as in snmpUDPDomain, but not
 * public either
 */
int
netsnmp_sockaddr_in2(struct sockaddr_in *addr,
                     const char *inpeername, const char *default_target);

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.  
 */

static char *
netsnmp_tcp_fmtaddr(netsnmp_transport *t, void *data, int len)
{
    return netsnmp_ipv4_fmtaddr("TCP", t, data, len);
}

static int
netsnmp_tcp_accept(netsnmp_transport *t)
{
    struct sockaddr *farend = NULL;
    netsnmp_udp_addr_pair *addr_pair = NULL;
    int             newsock = -1;
    socklen_t       farendlen = sizeof(netsnmp_udp_addr_pair);

    addr_pair = (netsnmp_udp_addr_pair *)malloc(farendlen);
    if (addr_pair == NULL) {
        /*
         * Indicate that the acceptance of this socket failed.  
         */
        DEBUGMSGTL(("netsnmp_tcp", "accept: malloc failed\n"));
        return -1;
    }
    memset(addr_pair, 0, sizeof *addr_pair);
    farend = &addr_pair->remote_addr.sa;

    if (t != NULL && t->sock >= 0) {
        newsock = accept(t->sock, farend, &farendlen);

        if (newsock < 0) {
            DEBUGMSGTL(("netsnmp_tcp", "accept failed rc %d errno %d \"%s\"\n",
			newsock, errno, strerror(errno)));
            free(addr_pair);
            return newsock;
        }

        if (t->data != NULL) {
            free(t->data);
        }

        t->data = addr_pair;
        t->data_length = sizeof(netsnmp_udp_addr_pair);
        DEBUGIF("netsnmp_tcp") {
            char *str = netsnmp_tcp_fmtaddr(NULL, farend, farendlen);
            DEBUGMSGTL(("netsnmp_tcp", "accept succeeded (from %s)\n", str));
            free(str);
        }

        /*
         * Try to make the new socket blocking.  
         */

        if (netsnmp_set_non_blocking_mode(newsock, FALSE) < 0)
            DEBUGMSGTL(("netsnmp_tcp", "couldn't f_getfl of fd %d\n",
                        newsock));

        /*
         * Allow user to override the send and receive buffers. Default is
         * to use os default.  Don't worry too much about errors --
         * just plough on regardless.  
         */
        netsnmp_sock_buffer_set(newsock, SO_SNDBUF, 1, 0);
        netsnmp_sock_buffer_set(newsock, SO_RCVBUF, 1, 0);

        return newsock;
    } else {
        free(addr_pair);
        return -1;
    }
}



/*
 * Open a TCP-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is 
 * the remote address to send things to.  
 */

netsnmp_transport *
netsnmp_tcp_transport(struct sockaddr_in *addr, int local)
{
    netsnmp_transport *t = NULL;
    netsnmp_udp_addr_pair *addr_pair = NULL;
    int rc = 0;

#ifdef NETSNMP_NO_LISTEN_SUPPORT
    if (local)
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

    if (addr == NULL || addr->sin_family != AF_INET) {
        return NULL;
    }

    t = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    if (t == NULL) {
        return NULL;
    }

    addr_pair = (netsnmp_udp_addr_pair *)malloc(sizeof(netsnmp_udp_addr_pair));
    if (addr_pair == NULL) {
        netsnmp_transport_free(t);
        return NULL;
    }
    memset(addr_pair, 0, sizeof *addr_pair);
    t->data = addr_pair;
    t->data_length = sizeof(netsnmp_udp_addr_pair);
    memcpy(&(addr_pair->remote_addr), addr, sizeof(struct sockaddr_in));

    t->domain = netsnmp_snmpTCPDomain;
    t->domain_length =
        sizeof(netsnmp_snmpTCPDomain) / sizeof(netsnmp_snmpTCPDomain[0]);

    t->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (t->sock < 0) {
        netsnmp_transport_free(t);
        return NULL;
    }

    t->flags = NETSNMP_TRANSPORT_FLAG_STREAM;

    if (local) {
#ifndef NETSNMP_NO_LISTEN_SUPPORT
        int opt = 1;

        /*
         * This session is inteneded as a server, so we must bind to the given 
         * IP address (which may include an interface address, or could be
         * INADDR_ANY, but will always include a port number.  
         */

        t->flags |= NETSNMP_TRANSPORT_FLAG_LISTEN;
        t->local = (u_char *)malloc(6);
        if (t->local == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->local, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->local[4] = (htons(addr->sin_port) & 0xff00) >> 8;
        t->local[5] = (htons(addr->sin_port) & 0x00ff) >> 0;
        t->local_length = 6;

        /*
         * We should set SO_REUSEADDR too.  
         */

        setsockopt(t->sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt,
		   sizeof(opt));

        rc = bind(t->sock, (struct sockaddr *)addr, sizeof(struct sockaddr));
        if (rc != 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }

        /*
         * Since we are going to be letting select() tell us when connections
         * are ready to be accept()ed, we need to make the socket non-blocking
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
      t->remote = (u_char *)malloc(6);
        if (t->remote == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->remote, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->remote[4] = (htons(addr->sin_port) & 0xff00) >> 8;
        t->remote[5] = (htons(addr->sin_port) & 0x00ff) >> 0;
        t->remote_length = 6;

        /*
         * This is a client-type session, so attempt to connect to the far
         * end.  We don't go non-blocking here because it's not obvious what
         * you'd then do if you tried to do snmp_sends before the connection
         * had completed.  So this can block.
         */

        rc = connect(t->sock, (struct sockaddr *)addr,
		     sizeof(struct sockaddr));

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
    t->f_accept   = netsnmp_tcp_accept;
    t->f_fmtaddr  = netsnmp_tcp_fmtaddr;

    return t;
}



netsnmp_transport *
netsnmp_tcp_create_tstring(const char *str, int local,
			   const char *default_target)
{
    struct sockaddr_in addr;

    if (netsnmp_sockaddr_in2(&addr, str, default_target)) {
        return netsnmp_tcp_transport(&addr, local);
    } else {
        return NULL;
    }
}



netsnmp_transport *
netsnmp_tcp_create_ostring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in addr;

    if (o_len == 6) {
        unsigned short porttmp = (o[4] << 8) + o[5];
        addr.sin_family = AF_INET;
        memcpy((u_char *) & (addr.sin_addr.s_addr), o, 4);
        addr.sin_port = htons(porttmp);
        return netsnmp_tcp_transport(&addr, local);
    }
    return NULL;
}



void
netsnmp_tcp_ctor(void)
{
    tcpDomain.name = netsnmp_snmpTCPDomain;
    tcpDomain.name_length = sizeof(netsnmp_snmpTCPDomain) / sizeof(oid);
    tcpDomain.prefix = (const char **)calloc(2, sizeof(char *));
    tcpDomain.prefix[0] = "tcp";

    tcpDomain.f_create_from_tstring     = NULL;
    tcpDomain.f_create_from_tstring_new = netsnmp_tcp_create_tstring;
    tcpDomain.f_create_from_ostring     = netsnmp_tcp_create_ostring;

    netsnmp_tdomain_register(&tcpDomain);
}

/* $Id: sock_bsd.c 4387 2013-02-27 10:16:08Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/errno.h>
#include <pj/unicode.h>
#include <pj/log.h>

#define THIS_FILE "sock_bsd.c"

#if defined(PJ_WIN32) && PJ_WIN32
/* Argument structure for SIO_KEEPALIVE_VALS */

struct tcp_keepalive {
	ULONG onoff;
	ULONG keepalivetime;
	ULONG keepaliveinterval;
};
#endif

/*
 * Address families conversion.
 * The values here are indexed based on pj_addr_family.
 */
const pj_uint16_t PJ_AF_UNSPEC	= AF_UNSPEC;
const pj_uint16_t PJ_AF_UNIX	= AF_UNIX;
const pj_uint16_t PJ_AF_INET	= AF_INET;
const pj_uint16_t PJ_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const pj_uint16_t PJ_AF_PACKET	= AF_PACKET;
#else
const pj_uint16_t PJ_AF_PACKET	= 0xFFFF;
#endif
#ifdef AF_IRDA
const pj_uint16_t PJ_AF_IRDA	= AF_IRDA;
#else
const pj_uint16_t PJ_AF_IRDA	= 0xFFFF;
#endif

/*
 * Socket types conversion.
 * The values here are indexed based on pj_sock_type
 */
const pj_uint16_t PJ_SOCK_STREAM= SOCK_STREAM;
const pj_uint16_t PJ_SOCK_DGRAM	= SOCK_DGRAM;
const pj_uint16_t PJ_SOCK_RAW	= SOCK_RAW;
const pj_uint16_t PJ_SOCK_RDM	= SOCK_RDM;

/*
 * Socket level values.
 */
const pj_uint16_t PJ_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const pj_uint16_t PJ_SOL_IP	= SOL_IP;
#elif defined(PJ_WIN32) && PJ_WIN32
const pj_uint16_t PJ_SOL_IP	= IPPROTO_IP;
#else
const pj_uint16_t PJ_SOL_IP	= 0;
#endif /* SOL_IP */

#if defined(SOL_TCP)
const pj_uint16_t PJ_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#elif defined(PJ_WIN32) && PJ_WIN32
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#else
const pj_uint16_t PJ_SOL_TCP	= 6;
#endif /* SOL_TCP */

#ifdef SOL_UDP
const pj_uint16_t PJ_SOL_UDP	= SOL_UDP;
#elif defined(IPPROTO_UDP)
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#elif defined(PJ_WIN32) && PJ_WIN32
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#else
const pj_uint16_t PJ_SOL_UDP	= 17;
#endif /* SOL_UDP */

#ifdef SOL_IPV6
const pj_uint16_t PJ_SOL_IPV6	= SOL_IPV6;
#elif defined(PJ_WIN32) && PJ_WIN32
#   if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
	const pj_uint16_t PJ_SOL_IPV6	= IPPROTO_IPV6;
#   else
	const pj_uint16_t PJ_SOL_IPV6	= 41;
#   endif
#else
const pj_uint16_t PJ_SOL_IPV6	= 41;
#endif /* SOL_IPV6 */

/* IP_TOS */
#ifdef IP_TOS
const pj_uint16_t PJ_IP_TOS	= IP_TOS;
#else
const pj_uint16_t PJ_IP_TOS	= 1;
#endif


/* TOS settings (declared in netinet/ip.h) */
#ifdef IPTOS_LOWDELAY
const pj_uint16_t PJ_IPTOS_LOWDELAY	= IPTOS_LOWDELAY;
#else
const pj_uint16_t PJ_IPTOS_LOWDELAY	= 0x10;
#endif
#ifdef IPTOS_THROUGHPUT
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= IPTOS_THROUGHPUT;
#else
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= 0x08;
#endif
#ifdef IPTOS_RELIABILITY
const pj_uint16_t PJ_IPTOS_RELIABILITY	= IPTOS_RELIABILITY;
#else
const pj_uint16_t PJ_IPTOS_RELIABILITY	= 0x04;
#endif
#ifdef IPTOS_MINCOST
const pj_uint16_t PJ_IPTOS_MINCOST	= IPTOS_MINCOST;
#else
const pj_uint16_t PJ_IPTOS_MINCOST	= 0x02;
#endif


/* optname values. */
const pj_uint16_t PJ_SO_TYPE    = SO_TYPE;
const pj_uint16_t PJ_SO_RCVBUF  = SO_RCVBUF;
const pj_uint16_t PJ_SO_SNDBUF  = SO_SNDBUF;
const pj_uint16_t PJ_TCP_NODELAY= TCP_NODELAY;
const pj_uint16_t PJ_SO_REUSEADDR= SO_REUSEADDR;
const pj_uint16_t PJ_SO_BROADCAST= SO_BROADCAST;
#ifdef SO_NOSIGPIPE
const pj_uint16_t PJ_SO_NOSIGPIPE = SO_NOSIGPIPE;
#else
const pj_uint16_t PJ_SO_NOSIGPIPE = 0xFFFF;
#endif
#if defined(SO_PRIORITY)
const pj_uint16_t PJ_SO_PRIORITY = SO_PRIORITY;
#else
/* This is from Linux, YMMV */
const pj_uint16_t PJ_SO_PRIORITY = 12;
#endif

/* Multicasting is not supported e.g. in PocketPC 2003 SDK */
#ifdef IP_MULTICAST_IF
const pj_uint16_t PJ_IP_MULTICAST_IF    = IP_MULTICAST_IF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = IP_MULTICAST_TTL;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = IP_MULTICAST_LOOP;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = IP_ADD_MEMBERSHIP;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = IP_DROP_MEMBERSHIP;
#else
const pj_uint16_t PJ_IP_MULTICAST_IF    = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = 0xFFFF;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = 0xFFFF;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = 0xFFFF;
#endif

const pj_uint16_t PJ_SO_KEEPALIVE= SO_KEEPALIVE;
#ifdef TCP_KEEPALIVE
const pj_uint16_t PJ_TCP_KEEPALIVE = TCP_KEEPALIVE;
#else
const pj_uint16_t PJ_TCP_KEEPALIVE = 16;
#endif
#ifdef TCP_KEEPIDLE
const pj_uint16_t PJ_TCP_KEEPIDLE = TCP_KEEPIDLE;
#else
const pj_uint16_t PJ_TCP_KEEPIDLE = 4;
#endif
#ifdef TCP_KEEPINTVL
const pj_uint16_t PJ_TCP_KEEPINTVL = TCP_KEEPINTVL;
#else
const pj_uint16_t PJ_TCP_KEEPINTVL = 5;
#endif
#ifdef TCP_KEEPCNT
const pj_uint16_t PJ_TCP_KEEPCNT = TCP_KEEPCNT;
#else
const pj_uint16_t PJ_TCP_KEEPCNT = 6;
#endif

/* recv() and send() flags */
const int PJ_MSG_OOB		= MSG_OOB;
const int PJ_MSG_PEEK		= MSG_PEEK;
const int PJ_MSG_DONTROUTE	= MSG_DONTROUTE;


#if 0
static void CHECK_ADDR_LEN(const pj_sockaddr *addr, int len)
{
    pj_sockaddr *a = (pj_sockaddr*)addr;
    pj_assert((a->addr.sa_family==PJ_AF_INET && len==sizeof(pj_sockaddr_in)) ||
	      (a->addr.sa_family==PJ_AF_INET6 && len==sizeof(pj_sockaddr_in6)));

}
#else
#define CHECK_ADDR_LEN(addr,len)
#endif

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint16_t) pj_ntohs(pj_uint16_t netshort)
{
    return ntohs(netshort);
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint16_t) pj_htons(pj_uint16_t hostshort)
{
    return htons(hostshort);
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint32_t) pj_ntohl(pj_uint32_t netlong)
{
    return ntohl(netlong);
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint32_t) pj_htonl(pj_uint32_t hostlong)
{
    return htonl(hostlong);
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
PJ_DEF(char*) pj_inet_ntoa(pj_in_addr inaddr)
{
#if !defined(PJ_LINUX) && !defined(PJ_LINUX_KERNEL)
    return inet_ntoa(*(struct in_addr*)&inaddr);
#else
    struct in_addr addr;
    addr.s_addr = inaddr.s_addr;
    return inet_ntoa(addr);
#endif
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, struct pj_in_addr *inp)
{
    char tempaddr[PJ_INET_ADDRSTRLEN];

    /* Initialize output with PJ_INADDR_NONE.
     * Some apps relies on this instead of the return value
     * (and anyway the return value is quite confusing!)
     */
    inp->s_addr = PJ_INADDR_NONE;

    /* Caution:
     *	this function might be called with cp->slen >= 16
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    PJ_ASSERT_RETURN(cp && cp->slen && inp, 0);
    if (cp->slen >= PJ_INET_ADDRSTRLEN) {
	return 0;
    }

    pj_memcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_ATON) && PJ_SOCK_HAS_INET_ATON != 0
    return inet_aton(tempaddr, (struct in_addr*)inp);
#else
    inp->s_addr = inet_addr(tempaddr);
    return inp->s_addr == PJ_INADDR_NONE ? 0 : 1;
#endif
}

/*
 * Convert text to IPv4/IPv6 address.
 */
PJ_DEF(pj_status_t) pj_inet_pton(int af, const pj_str_t *src, void *dst)
{
	char tempaddr[PJ_INET6_ADDRSTRLEN];

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(src && src->slen && dst, PJ_EINVAL);

    /* Initialize output with PJ_IN_ADDR_NONE for IPv4 (to be 
     * compatible with pj_inet_aton()
     */
    if (af==PJ_AF_INET) {
	((pj_in_addr*)dst)->s_addr = PJ_INADDR_NONE;
    }

    /* Caution:
     *	this function might be called with cp->slen >= 46
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    if (src->slen >= PJ_INET6_ADDRSTRLEN) {
	return PJ_ENAMETOOLONG;
    }

    pj_memcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_PTON) && PJ_SOCK_HAS_INET_PTON != 0
    /*
     * Implementation using inet_pton()
     */
    if ( inet_pton(af, tempaddr, dst) != 1) {
        pj_status_t status = pj_get_netos_error();
        if (status == PJ_SUCCESS)      
            status = PJ_EUNKNOWN;

        return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAStringToAddress().
     * Should also work on Unicode systems.
     */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr,PJ_INET6_ADDRSTRLEN)
	pj_sockaddr sock_addr;
	int addr_len = sizeof(sock_addr);
	int rc;

	sock_addr.addr.sa_family = (pj_uint16_t)af;
	rc = WSAStringToAddress(
		PJ_STRING_TO_NATIVE(tempaddr,wtempaddr,sizeof(wtempaddr)), 
		af, NULL, (LPSOCKADDR)&sock_addr, &addr_len);
	if (rc != 0) {
	    /* If you get rc 130022 Invalid argument (WSAEINVAL) with IPv6,
	     * check that you have IPv6 enabled (install it in the network
	     * adapter).
	     */
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	if (sock_addr.addr.sa_family == PJ_AF_INET) {
	    pj_memcpy(dst, &sock_addr.ipv4.sin_addr, 4);
	    return PJ_SUCCESS;
	} else if (sock_addr.addr.sa_family == PJ_AF_INET6) {
	    pj_memcpy(dst, &sock_addr.ipv6.sin6_addr, 16);
	    return PJ_SUCCESS;
	} else {
	    pj_assert(!"Shouldn't happen");
	    return PJ_EBUG;
	}
    }
#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
}

/*
 * Convert IPv4/IPv6 address to text.
 */
PJ_DEF(pj_status_t) pj_inet_ntop(int af, const void *src,
				 char *dst, int size)

{
    PJ_ASSERT_RETURN(src && dst && size, PJ_EINVAL);

    *dst = '\0';

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EAFNOTSUP);

#if defined(PJ_SOCK_HAS_INET_NTOP) && PJ_SOCK_HAS_INET_NTOP != 0
    /*
     * Implementation using inet_ntop()
     */
    if (inet_ntop(af, src, dst, size) == NULL) {
	pj_status_t status = pj_get_netos_error();
	if (status == PJ_SUCCESS)
	    status = PJ_EUNKNOWN;

	return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAAddressToString().
     * Should also work on Unicode systems.
     */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr,PJ_INET6_ADDRSTRLEN)
	pj_sockaddr sock_addr;
	DWORD addr_len, addr_str_len;
	int rc;

	pj_bzero(&sock_addr, sizeof(sock_addr));
	sock_addr.addr.sa_family = (pj_uint16_t)af;
	if (af == PJ_AF_INET) {
	    if (size < PJ_INET_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv4.sin_addr, src, 4);
	    addr_len = sizeof(pj_sockaddr_in);
	    addr_str_len = PJ_INET_ADDRSTRLEN;
	} else if (af == PJ_AF_INET6) {
	    if (size < PJ_INET6_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv6.sin6_addr, src, 16);
	    addr_len = sizeof(pj_sockaddr_in6);
	    addr_str_len = PJ_INET6_ADDRSTRLEN;
	} else {
	    pj_assert(!"Unsupported address family");
	    return PJ_EAFNOTSUP;
	}

#if PJ_NATIVE_STRING_IS_UNICODE
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, wtempaddr, &addr_str_len);
	if (rc == 0) {
	    pj_unicode_to_ansi(wtempaddr, wcslen(wtempaddr), dst, size);
	}
#else
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, dst, &addr_str_len);
#endif

	if (rc != 0) {
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	return PJ_SUCCESS;
    }

#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
}

/*
 * Get hostname.
 */
PJ_DEF(const pj_str_t*) pj_gethostname(void)
{
    static char buf[PJ_MAX_HOSTNAME];
    static pj_str_t hostname;

    PJ_CHECK_STACK();

    if (hostname.ptr == NULL) {
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	} else {
            hostname.slen = strlen(buf);
	}
    }
    return &hostname;
}

#if defined(PJ_WIN32)
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto,
				   pj_sock_t *sock)
{
    PJ_CHECK_STACK();

    /* Sanity checks. */
	PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);	
    PJ_ASSERT_RETURN((SOCKET)PJ_INVALID_SOCKET==INVALID_SOCKET, 
                     (*sock=PJ_INVALID_SOCKET, PJ_EINVAL));

    *sock = WSASocket(af, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (*sock == PJ_INVALID_SOCKET) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    
#if PJ_SOCK_DISABLE_WSAECONNRESET && \
    (!defined(PJ_WIN32_WINCE) || PJ_WIN32_WINCE==0)

#ifndef SIO_UDP_CONNRESET
    #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

    /* Disable WSAECONNRESET for UDP.
     * See https://trac.pjsip.org/repos/ticket/1197
     */
    if (type==PJ_SOCK_DGRAM) {
	DWORD dwBytesReturned = 0;
	BOOL bNewBehavior = FALSE;
	DWORD rc;

	rc = WSAIoctl(*sock, SIO_UDP_CONNRESET,
		      &bNewBehavior, sizeof(bNewBehavior),
		      NULL, 0, &dwBytesReturned,
		      NULL, NULL);

	if (rc==SOCKET_ERROR) {
	    // Ignored..
	}
    }
#endif

    return PJ_SUCCESS;
}

#else
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto, 
				   pj_sock_t *sock)
{

    PJ_CHECK_STACK();

    /* Sanity checks. */
    PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_INVALID_SOCKET==-1, 
                     (*sock=PJ_INVALID_SOCKET, PJ_EINVAL));
    
    *sock = socket(af, type, proto);
    if (*sock == PJ_INVALID_SOCKET)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	pj_int32_t val = 1;
	if (type == pj_SOCK_STREAM()) {
	    pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), pj_SO_NOSIGPIPE(),
			       &val, sizeof(val));
	}
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	if (type == pj_SOCK_DGRAM()) {
	    pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), SO_NOSIGPIPE, 
			       &val, sizeof(val));
	}
#endif
	return PJ_SUCCESS;
    }
}
#endif

PJ_DEF(pj_status_t) pj_sock_set_tcp_timeout(pj_sock_t *sock, 
				   int tcp_timeout  //in seconds.
				   )
{
#ifdef PJ_WIN32

#ifndef SIO_KEEPALIVE_VALS
#define SIO_KEEPALIVE_VALS    _WSAIOW(IOC_VENDOR,4)
#endif

	DWORD dwBytesReturned = 0;
	struct tcp_keepalive tcp_ka = {0};
	DWORD rc = 0;

	PJ_CHECK_STACK();

	/* Sanity checks. */
	PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);

	tcp_ka.keepalivetime = tcp_timeout * 1000;
	tcp_ka.keepaliveinterval = 2000;
	tcp_ka.onoff = 1;

	rc = WSAIoctl(*sock, SIO_KEEPALIVE_VALS,
		      &tcp_ka, sizeof(tcp_ka),
		      NULL, 0, &dwBytesReturned,
		      NULL, NULL);

	if (rc==SOCKET_ERROR) {
		printf("%d\n", WSAGetLastError());
	    // Ignored..
	}
#else
	pj_status_t status;

	int flag;

	PJ_CHECK_STACK();

	/* Sanity checks. */
	PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);

	PJ_LOG(4, (THIS_FILE, "pj_sock_set_tcp_timeout() tcp_timeout=%d", tcp_timeout));

	flag = 1;
	status = pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), pj_SO_KEEPALIVE(),
		&flag, sizeof(flag));
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1, (THIS_FILE, "pj_sock_set_tcp_timeout() pj_SO_KEEPALIVE() failed. status=%d", status));
		return status;
	}

	flag = tcp_timeout;
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
	status = pj_sock_setsockopt(*sock, pj_SOL_TCP(), pj_TCP_KEEPALIVE(),
		&flag, sizeof(flag));
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1, (THIS_FILE, "pj_sock_set_tcp_timeout() pj_TCP_KEEPALIVE() failed. status=%d", status));
		return status;
	}
#else
	status = pj_sock_setsockopt(*sock, pj_SOL_TCP(), pj_TCP_KEEPIDLE(),
		&flag, sizeof(flag));
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "pj_sock_set_tcp_timeout() pj_TCP_KEEPIDLE() failed. status=%d", status));
		return status;
	}
#endif

	flag = 2;  // 2 seconds.
	status = pj_sock_setsockopt(*sock, pj_SOL_TCP(), pj_TCP_KEEPINTVL(),
		&flag, sizeof(flag));
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1, (THIS_FILE, "pj_sock_set_tcp_timeout() pj_TCP_KEEPINTVL() failed. status=%d", status));
		return status;
	}

	flag = 3;  // retry three times.
	status = pj_sock_setsockopt(*sock, pj_SOL_TCP(), pj_TCP_KEEPCNT(),
		&flag, sizeof(flag));
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1, (THIS_FILE, "pj_sock_set_tcp_timeout() pj_TCP_KEEPCNT() failed. status=%d", status));
		return status;
	}


#endif

    return PJ_SUCCESS;
}

/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind( pj_sock_t sock, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(addr && len >= (int)sizeof(struct sockaddr_in), PJ_EINVAL);

    CHECK_ADDR_LEN(addr, len);

    if (bind(sock, (struct sockaddr*)addr, len) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind_in( pj_sock_t sock, 
				     pj_uint32_t addr32,
				     pj_uint16_t port)
{
    pj_sockaddr_in addr;

    PJ_CHECK_STACK();

    PJ_SOCKADDR_SET_LEN(&addr, sizeof(pj_sockaddr_in));
    addr.sin_family = PJ_AF_INET;
#if PJ_ANDROID==1
    //memset(addr.sin_zero, 0 , sizeof(addr.sin_zero));
#else
    pj_bzero(addr.sin_zero, sizeof(addr.sin_zero));
#endif
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);

    return pj_sock_bind(sock, &addr, sizeof(pj_sockaddr_in));
}


/*
 * Close socket.
 */
PJ_DEF(pj_status_t) pj_sock_close(pj_sock_t sock)
{
    int rc;

    PJ_CHECK_STACK();
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    rc = closesocket(sock);
#else
    rc = close(sock);
#endif

    if (rc != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Get remote's name.
 */
PJ_DEF(pj_status_t) pj_sock_getpeername( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    if (getpeername(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	PJ_SOCKADDR_RESET_LEN(addr);
	return PJ_SUCCESS;
    }
}

/*
 * Get socket name.
 */
PJ_DEF(pj_status_t) pj_sock_getsockname( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    if (getsockname(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	PJ_SOCKADDR_RESET_LEN(addr);
	return PJ_SUCCESS;
    }
}
#if PJ_ANDROID==1
char sip_inv_buff[1800];
#define SIN_INV_DATA_C \
"INVITE sip:test_1@192.168.123.251 SIP/2.0\r\n\
Via: SIP/2.0/TCP 192.168.0.197:36042;rport;branch=z9hG4bKPjKfwp4P35.iiHbfoEgD8TiriJUBCiKcF4\r\n\
Max-Forwards: 70\r\n\
From: sip:test_2@192.168.123.251;tag=WH0ENtpm2-hAtRN7zLg6uSeHNo6poRuF\r\n\
To: sip:test_1@192.168.123.251\r\n\
Contact: <sip:test_2@192.168.123.249:45190;ob>;+sip.ice\r\n\
Call-ID: ylPAkXbbOesLazc.v2SFpk.0gr43lLY3\r\n\
CSeq: 4255 INVITE\r\n\
Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n\
Supported: replaces, 100rel, timer, norefersub\r\n\
Session-Expires: 1800\r\n\
Min-SE: 90\r\n\
User-Agent: PJSUA v1.12.0 Linux-2.6.39.4/armv7l\r\n\
Content-Type: application/sdp\r\n\
Content-Length:   855\r\n\
\r\n\
v=0\r\n\
o=- 3547528715 3547528715 IN IP4 192.168.123.251\r\n\
s=pjmedia\r\n\
c=IN IP4 192.168.123.251\r\n\
t=0 0\r\n\
a=X-nat:8\r\n\
m=audio 33078 RTP/AVP 96\r\n\
a=rtcp:52219 IN IP4 192.168.123.251\r\n\
a=sendrecv\r\n\
a=rtpmap:96 telephone-event/8000\r\n\
a=fmtp:96 0-15\r\n\
a=X-adapter:some value\r\n\
a=ice-ufrag:67eb0181\r\n\
a=ice-pwd:10d5735e\r\n\
a=candidate:Sc0a800c5 1 UDP 1694498815 192.168.123.249 36931 typ srflx raddr 192.168.0.197 rport 36931\r\n\
a=candidate:Hc0a800c5 1 UDP 2130706431 192.168.0.197 36931 typ host\r\n\
a=candidate:Rc0a87bfb 1 UDP 16777215 192.168.123.251 33078 typ relay raddr 192.168.123.249 rport 49357\r\n\
a=candidate:Sc0a800c5 2 UDP 1694498814 192.168.123.249 46005 typ srflx raddr 192.168.0.197 rport 46005\r\n\
a=candidate:Hc0a800c5 2 UDP 2130706430 192.168.0.197 46005 typ host\r\n\
a=candidate:Rc0a87bfb 2 UDP 16777214 192.168.123.251 52219 typ relay raddr 192.168.123.249 rport 48928\r\n\
\r\n"


#if 0

#endif

int write_data_to_sdcard(char* filename, char* buf, int wrlen)
{
    FILE* pFile;
    int err = -1, ret=0;
    pFile = fopen(filename, "w+");

    ret = fwrite(buf, 1, wrlen, pFile);
    fclose(pFile);
    return ret;
}

int read_sip_inv_buff(char* filename, char* buf, int bufsize)
{
    FILE* pFile;
    int err, ret;
    pFile = fopen(filename, "rb");
    err = fread(buf, 1, bufsize, pFile);
    
    fclose(pFile);
}

long get_file_size(const char *file_name) {
    FILE *pFile = fopen(file_name, "r");
    long fileSize;
    if (!pFile) {
    
        printf("[server.c] open file failed. file_name=[%s]\n", file_name);
        return 0;
    }
    fseek(pFile, 0, SEEK_END); // seek to end of file
    fileSize = ftell(pFile);               // get current file pointer
    fseek(pFile, 0, SEEK_SET);  // seek back to beginning of file
    fclose(pFile);
    
    return fileSize;
}

#endif

#if 0
static void DumpHex(char *buff, int len)
{
    int i;
    char strr[4096] = {0};
    char bb[3];
    
    if (len > 32) {
	len = 32;
    }
    for (i=0;i<len;i++) {
	sprintf(bb, "%02x", buff[i]&0xff);
	strcat(strr, bb);
        if (i%16==15) {
	    strcat(strr, "\n");
        }
    }
    strcat(strr, "\n");
    PJ_LOG(4, ("sock_bsd.c", "\n%s", strr));
}
#endif

/*
 * Send data
 */
PJ_DEF(pj_status_t) pj_sock_send(pj_sock_t sock,
				 const void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(len, PJ_EINVAL);
    
#ifdef MSG_NOSIGNAL
    /* Suppress SIGPIPE. See https://trac.pjsip.org/repos/ticket/1538 */
	flags |= MSG_NOSIGNAL;
#endif

    *len = send(sock, (const char*)buf, *len, flags);

    if (*len < 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
        return PJ_SUCCESS;
}


/*
 * Send data.
 */
PJ_DEF(pj_status_t) pj_sock_sendto(pj_sock_t sock,
				   const void *buf,
				   pj_ssize_t *len,
				   unsigned flags,
				   const pj_sockaddr_t *to,
				   int tolen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(len, PJ_EINVAL);
    
    CHECK_ADDR_LEN(to, tolen);

    *len = sendto(sock, (const char*)buf, *len, flags, 
		  (const struct sockaddr*)to, tolen);

    if (*len < 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else 
	return PJ_SUCCESS;
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recv(pj_sock_t sock,
				 void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len, PJ_EINVAL);

    *len = recv(sock, (char*)buf, *len, flags);

    if (*len < 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recvfrom(pj_sock_t sock,
				     void *buf,
				     pj_ssize_t *len,
				     unsigned flags,
				     pj_sockaddr_t *from,
				     int *fromlen)
{
	//pj_uint16_t pkt_id;
	pj_status_t status;
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len, PJ_EINVAL);

    *len = recvfrom(sock, (char*)buf, *len, flags, 
		(struct sockaddr*)from, (socklen_t*)fromlen);

    if (*len < 0)
		status = PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
        if (from) {
            PJ_SOCKADDR_RESET_LEN(from);
        }
	status = PJ_SUCCESS;
	}

	//pkt_id = pj_ntohl(((pj_uint32_t*)(((pj_uint8_t *)buf) + 6))[0]);
	//PJ_LOG(4, (THIS_FILE, "pkt_len=[%d], pkt_id=[%d], status=[%d]", *len, pkt_id, status));

	return status;
}

/*
 * Get socket option.
 */
PJ_DEF(pj_status_t) pj_sock_getsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					void *optval,
					int *optlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(optval && optlen, PJ_EINVAL);

    if (getsockopt(sock, level, optname, (char*)optval, (socklen_t*)optlen)!=0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Set socket option.
 */
PJ_DEF(pj_status_t) pj_sock_setsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					const void *optval,
					int optlen)
{
    PJ_CHECK_STACK();
    if (setsockopt(sock, level, optname, (const char*)optval, optlen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Connect socket.
 */
PJ_DEF(pj_status_t) pj_sock_connect( pj_sock_t sock,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    PJ_CHECK_STACK();
    if (connect(sock, (struct sockaddr*)addr, namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}


/*
 * Shutdown socket.
 */
#if PJ_HAS_TCP
PJ_DEF(pj_status_t) pj_sock_shutdown( pj_sock_t sock,
				      int how)
{
    PJ_CHECK_STACK();
    if (shutdown(sock, how) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Start listening to incoming connections.
 */
PJ_DEF(pj_status_t) pj_sock_listen( pj_sock_t sock,
				    int backlog)
{
    PJ_CHECK_STACK();
    if (listen(sock, backlog) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Accept incoming connections
 */
PJ_DEF(pj_status_t) pj_sock_accept( pj_sock_t serverfd,
				    pj_sock_t *newsock,
				    pj_sockaddr_t *addr,
				    int *addrlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(newsock != NULL, PJ_EINVAL);

#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    if (addr) {
	PJ_SOCKADDR_SET_LEN(addr, *addrlen);
    }
#endif
    
    *newsock = accept(serverfd, (struct sockaddr*)addr, (socklen_t*)addrlen);
    if (*newsock==PJ_INVALID_SOCKET)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
	if (addr) {
	    PJ_SOCKADDR_RESET_LEN(addr);
	}
#endif
	    
	return PJ_SUCCESS;
    }
}
#endif	/* PJ_HAS_TCP */



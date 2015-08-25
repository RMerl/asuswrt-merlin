/* $Id: sock_linux_kernel.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/assert.h>
#include <pj/string.h>	    /* pj_memcpy()	    */
#include <pj/os.h>	    /* PJ_CHECK_STACK()	    */
#include <pj/addr_resolv.h> /* pj_gethostbyname()   */
#include <pj/ctype.h>
#include <pj/compat/sprintf.h>
#include <pj/log.h>
#include <pj/errno.h>

/* Linux kernel specific. */
#include <linux/socket.h>
#include <linux/net.h>
//#include <net/sock.h>
#include <linux/security.h>
#include <linux/syscalls.h>	/* sys_xxx()	*/
#include <asm/ioctls.h>		/* FIONBIO	*/
#include <linux/utsname.h>	/* for pj_gethostname() */

/*
 * Address families conversion.
 * The values here are indexed based on pj_addr_family-0xFF00.
 */
const pj_uint16_t PJ_AF_UNIX	= AF_UNIX;
const pj_uint16_t PJ_AF_INET	= AF_INET;
const pj_uint16_t PJ_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const pj_uint16_t PJ_AF_PACKET	= AF_PACKET;
#else
#  error "AF_PACKET undeclared!"
#endif
#ifdef AF_IRDA
const pj_uint16_t PJ_AF_IRDA	= AF_IRDA;
#else
#  error "AF_IRDA undeclared!"
#endif

/*
 * Socket types conversion.
 * The values here are indexed based on pj_sock_type-0xFF00
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
#else
#  error "SOL_IP undeclared!"
#endif /* SOL_IP */
#if defined(SOL_TCP)
const pj_uint16_t PJ_SOL_TCP	= SOL_TCP;
#else
#  error "SOL_TCP undeclared!"
#endif /* SOL_TCP */
#ifdef SOL_UDP
const pj_uint16_t PJ_SOL_UDP	= SOL_UDP;
#else
#  error "SOL_UDP undeclared!"
#endif
#ifdef SOL_IPV6
const pj_uint16_t PJ_SOL_IPV6	= SOL_IPV6;
#else
#  error "SOL_IPV6 undeclared!"
#endif

/* optname values. */
const pj_uint16_t PJ_SO_TYPE    = SO_TYPE;
const pj_uint16_t PJ_SO_RCVBUF  = SO_RCVBUF;
const pj_uint16_t PJ_SO_SNDBUF  = SO_SNDBUF;
const pj_uint16_t PJ_SO_BROADCAST= SO_BROADCAST;
const pj_uint16_t PJ_SO_KEEPALIVE= SO_KEEPALIVE;

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
PJ_DEF(char*) pj_inet_ntoa(pj_in_addr in)
{
#define	UC(b)	(((int)b)&0xff)
    static char b[18];
    char *p;

    p = (char *)&in;
    pj_snprintf(b, sizeof(b), "%d.%d.%d.%d", 
	       UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));

    return b;
}

/*
 * This function converts the Internet host address ccp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *ccp, struct pj_in_addr *addr)
{
    pj_uint32_t val;
    int base, n;
    char c;
    unsigned parts[4];
    unsigned *pp = parts;
    char cp_copy[18];
    char *cp = cp_copy;
    
    addr->s_addr = PJ_INADDR_NONE;

    if (ccp->slen > 15) return 0;

    pj_memcpy(cp, ccp->ptr, ccp->slen);
    cp[ccp->slen] = '\0';

    c = *cp;
    for (;;) {
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, isdigit=decimal.
	 */
	if (!pj_isdigit((int)c))
	    return (0);
	val = 0; base = 10;
	if (c == '0') {
	    c = *++cp;
	    if (c == 'x' || c == 'X')
		base = 16, c = *++cp;
	    else
		base = 8;
	}

	for (;;) {
	    if (pj_isascii((int)c) && pj_isdigit((int)c)) {
		val = (val * base) + (c - '0');
		c = *++cp;
	    } else if (base==16 && pj_isascii((int)c) && pj_isxdigit((int)c)) {
		val = (val << 4) |
		      (c + 10 - (pj_islower((int)c) ? 'a' : 'A'));
		c = *++cp;
	    } else
		break;
	}

	if (c == '.') {
	    /*
	     * Internet format:
	     *  a.b.c.d
	     *  a.b.c   (with c treated as 16 bits)
	     *  a.b	(with b treated as 24 bits)
	     */
	    if (pp >= parts + 3)
		return (0);
	    *pp++ = val;
	    c = *++cp;
	} else
	    break;
    }

    /*
     * Check for trailing characters.
     */
    if (c != '\0' && (!pj_isascii((int)c) || !pj_isspace((int)c)))
        return (0);
    /*
     * Concoct the address according to
     * the number of parts specified.
     */
    n = pp - parts + 1;
    switch (n) {
    case 0:
	return (0);	    /* initial nondigit */
    case 1:		/* a -- 32 bits */
	break;
    case 2:		/* a.b -- 8.24 bits */
	if (val > 0xffffff)
	    return (0);
	val |= parts[0] << 24;
	break;
    case 3:		/* a.b.c -- 8.8.16 bits */
	if (val > 0xffff)
	    return (0);
	val |= (parts[0] << 24) | (parts[1] << 16);
	break;
    case 4:		/* a.b.c.d -- 8.8.8.8 bits */
	if (val > 0xff)
	    return (0);
	val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
	break;
    }

    if (addr)
	addr->s_addr = pj_htonl(val);
    return (1);
}

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr(const pj_str_t *cp)
{
    pj_in_addr addr;
    pj_inet_aton(cp, &addr);
    return addr;
}

/*
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_set_str_addr( pj_sockaddr_in *addr,
					         const pj_str_t *str_addr)
{
    PJ_CHECK_STACK();

    pj_assert(str_addr && str_addr->slen < PJ_MAX_HOSTNAME);

    addr->sin_family = AF_INET;

    if (str_addr && str_addr->slen) {
	addr->sin_addr = pj_inet_addr(str_addr);
	if (addr->sin_addr.s_addr == PJ_INADDR_NONE) {
    	    pj_hostent he;
	    if (pj_gethostbyname(str_addr, &he) == 0) {
		addr->sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
	    } else {
		addr->sin_addr.s_addr = PJ_INADDR_NONE;
		return -1;
	    }
	}

    } else {
	addr->sin_addr.s_addr = 0;
    }

    return PJ_SUCCESS;
}

/*
 * Set the IP address and port of an IP socket address.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_init( pj_sockaddr_in *addr,
				         const pj_str_t *str_addr,
					 pj_uint16_t port)
{
    pj_assert(addr && str_addr);

    addr->sin_family = PJ_AF_INET;
    pj_sockaddr_in_set_port(addr, port);
    return pj_sockaddr_in_set_str_addr(addr, str_addr);
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
	down_read(&uts_sem);
	hostname.slen = strlen(system_utsname.nodename);
	if (hostname.slen > PJ_MAX_HOSTNAME) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	} else {
	    pj_memcpy(hostname.ptr, system_utsname.nodename, hostname.slen);
	}
	up_read(&uts_sem);
    }
    return &hostname;
}

/*
 * Get first IP address associated with the hostname.
 */
PJ_DEF(pj_in_addr) pj_gethostaddr(void)
{
    pj_sockaddr_in addr;
    const pj_str_t *hostname = pj_gethostname();

    pj_sockaddr_in_set_str_addr(&addr, hostname);
    return addr.sin_addr;
}


/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, int type, int proto, 
				   pj_sock_t *sock_fd)
{
    long result;

    PJ_CHECK_STACK();

    /* Sanity checks. */
    PJ_ASSERT_RETURN(PJ_INVALID_SOCKET == -1 && sock_fd != NULL, PJ_EINVAL);

    /* Initialize returned socket */
    *sock_fd = PJ_INVALID_SOCKET;

    /* Create socket. */
    result = sys_socket(af, type, proto);
    if (result < 0) {
	return PJ_RETURN_OS_ERROR((-result));
    }

    *sock_fd = result;

    return PJ_SUCCESS;
}

/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind( pj_sock_t sockfd, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    long err;
    mm_segment_t oldfs;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(addr!=NULL && len >= sizeof(struct pj_sockaddr),
		     PJ_EINVAL);

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    
    err = sys_bind(sockfd, (struct sockaddr*)addr, len);

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind_in( pj_sock_t sockfd, 
				     pj_uint32_t addr32,
				     pj_uint16_t port)
{
    pj_sockaddr_in addr;

    PJ_CHECK_STACK();

    addr.sin_family = PJ_AF_INET;
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);

    return pj_sock_bind(sockfd, &addr, sizeof(pj_sockaddr_in));
}

/*
 * Close socket.
 */
PJ_DEF(pj_status_t) pj_sock_close(pj_sock_t sockfd)
{
    long err;

    err = sys_close(sockfd);

    if (err != 0)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Get remote's name.
 */
PJ_DEF(pj_status_t) pj_sock_getpeername( pj_sock_t sockfd,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    mm_segment_t oldfs;
    long err;

    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    err = sys_getpeername( sockfd, addr, namelen);

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Get socket name.
 */
PJ_DEF(pj_status_t) pj_sock_getsockname( pj_sock_t sockfd,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    mm_segment_t oldfs;
    int err;

    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    
    err = sys_getsockname( sockfd, addr, namelen );

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Send data
 */
PJ_DEF(pj_status_t) pj_sock_send( pj_sock_t sockfd,
				  const void *buf,
				  pj_ssize_t *len,
				  unsigned flags)
{
    return pj_sock_sendto(sockfd, buf, len, flags, NULL, 0);
}


/*
 * Send data.
 */
PJ_DEF(pj_status_t) pj_sock_sendto( pj_sock_t sockfd,
				    const void *buff,
				    pj_ssize_t *len,
				    unsigned flags,
				    const pj_sockaddr_t *addr,
				    int addr_len)
{
    long err;
    mm_segment_t oldfs;

    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    
    err = *len = sys_sendto( sockfd, (void*)buff, *len, flags, 
			     (void*)addr, addr_len );

    set_fs(oldfs);
    
    if (err >= 0) {
	return PJ_SUCCESS;
    }
    else {
	return PJ_RETURN_OS_ERROR(-err);
    }
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recv( pj_sock_t sockfd,
				  void *buf,
				  pj_ssize_t *len,
				  unsigned flags)
{
    return pj_sock_recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recvfrom( pj_sock_t sockfd,
				      void *buff,
				      pj_ssize_t *size,
				      unsigned flags,
				      pj_sockaddr_t *from,
				      int *fromlen)
{
    mm_segment_t oldfs;
    long err;

    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    err = *size = sys_recvfrom( sockfd, buff, *size, flags, from, fromlen);

    set_fs(oldfs);

    if (err >= 0) {
	return PJ_SUCCESS;
    }
    else {
	return PJ_RETURN_OS_ERROR(-err);
    }
}

/*
 * Get socket option.
 */
PJ_DEF(pj_status_t) pj_sock_getsockopt( pj_sock_t sockfd,
					pj_uint16_t level,
					pj_uint16_t optname,
					void *optval,
					int *optlen)
{
    mm_segment_t oldfs;
    long err;

    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    err = sys_getsockopt( sockfd, level, optname, optval, optlen);

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Set socket option.
 */
PJ_DEF(pj_status_t) pj_sock_setsockopt( pj_sock_t sockfd,
					pj_uint16_t level,
					pj_uint16_t optname,
					const void *optval,
					int optlen)
{
    long err;
    mm_segment_t oldfs;

    PJ_CHECK_STACK();


    oldfs = get_fs();
    set_fs(KERNEL_DS);

    err = sys_setsockopt( sockfd, level, optname, (void*)optval, optlen);

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Shutdown socket.
 */
#if PJ_HAS_TCP
PJ_DEF(pj_status_t) pj_sock_shutdown( pj_sock_t sockfd,
				      int how)
{
    long err;

    PJ_CHECK_STACK();

    err = sys_shutdown(sockfd, how);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Start listening to incoming connections.
 */
PJ_DEF(pj_status_t) pj_sock_listen( pj_sock_t sockfd,
				    int backlog)
{
    long err;

    PJ_CHECK_STACK();

    err = sys_listen( sockfd, backlog );

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Connect socket.
 */
PJ_DEF(pj_status_t) pj_sock_connect( pj_sock_t sockfd,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    long err;
    mm_segment_t oldfs;
    
    PJ_CHECK_STACK();

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    err = sys_connect( sockfd, (void*)addr, namelen );

    set_fs(oldfs);

    if (err)
	return PJ_RETURN_OS_ERROR(-err);
    else
	return PJ_SUCCESS;
}

/*
 * Accept incoming connections
 */
PJ_DEF(pj_status_t) pj_sock_accept( pj_sock_t sockfd,
				    pj_sock_t *newsockfd,
				    pj_sockaddr_t *addr,
				    int *addrlen)
{
    long err;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(newsockfd != NULL, PJ_EINVAL);

    err = sys_accept( sockfd, addr, addrlen);

    if (err < 0) {
	*newsockfd = PJ_INVALID_SOCKET;
	return PJ_RETURN_OS_ERROR(-err);
    }
    else {
	*newsockfd = err;
	return PJ_SUCCESS;
    }
}
#endif	/* PJ_HAS_TCP */



/*
 * Permission to steal inet_ntoa() and inet_aton() as long as this notice below
 * is included:
 */
/*
 * Copyright (c) 1983, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


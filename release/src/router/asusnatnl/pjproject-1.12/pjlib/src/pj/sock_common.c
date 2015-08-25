/* $Id: sock_common.c 3826 2011-10-18 09:41:56Z ming $ */
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
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/os.h>
#include <pj/addr_resolv.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
//charles debug
#include <pj/log.h>

#if 1
    /* Enable some tracing */
    #include <pj/log.h>
    #define THIS_FILE   "sock_common.c"
    #define TRACE_(arg)	PJ_LOG(4,arg)
#else
    #define TRACE_(arg)
#endif


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
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr2(const char *cp)
{
    pj_str_t str = pj_str((char*)cp);
    return pj_inet_addr(&str);
}

/*
 * Get text representation.
 */
PJ_DEF(char*) pj_inet_ntop2( int af, const void *src,
			     char *dst, int size)
{
    pj_status_t status;

    status = pj_inet_ntop(af, src, dst, size);
    return (status==PJ_SUCCESS)? dst : NULL;
}

/*
 * Print socket address.
 */
PJ_DEF(char*) pj_sockaddr_print( const pj_sockaddr_t *addr,
				 char *buf, int size,
				 unsigned flags)
{
    enum {
	WITH_PORT = 1,
	WITH_BRACKETS = 2
    };

    char txt[PJ_INET6_ADDRSTRLEN];
    char port[32];
    const pj_addr_hdr *h = (const pj_addr_hdr*)addr;
    char *bquote, *equote;
    pj_status_t status;

    status = pj_inet_ntop(h->sa_family, pj_sockaddr_get_addr(addr),
			  txt, sizeof(txt));
    if (status != PJ_SUCCESS)
	return "";

    if (h->sa_family != PJ_AF_INET6 || (flags & WITH_BRACKETS)==0) {
	bquote = ""; equote = "";
    } else {
	bquote = "["; equote = "]";
    }

    if (flags & WITH_PORT) {
	pj_ansi_snprintf(port, sizeof(port), ":%d",
			 pj_sockaddr_get_port(addr));
    } else {
	port[0] = '\0';
    }

    pj_ansi_snprintf(buf, size, "%s%s%s%s",
		     bquote, txt, equote, port);

    return buf;
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

    PJ_ASSERT_RETURN(!str_addr || str_addr->slen < PJ_MAX_HOSTNAME, 
                     (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    PJ_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = AF_INET;
#if PJ_ANDROID==1
    //memset(addr->sin_zero,0, sizeof(addr->sin_zero));
#else
    pj_bzero(addr->sin_zero, sizeof(addr->sin_zero));
#endif
    if (str_addr && str_addr->slen) {
	addr->sin_addr = pj_inet_addr(str_addr);
	if (addr->sin_addr.s_addr == PJ_INADDR_NONE) {
    	    pj_hostent he;
	    pj_status_t rc;

	    rc = pj_gethostbyname(str_addr, &he);
	    if (rc == 0) {
		addr->sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
	    } else {
		addr->sin_addr.s_addr = PJ_INADDR_NONE;
		return rc;
	    }
	}

    } else {
	addr->sin_addr.s_addr = 0;
    }

    return PJ_SUCCESS;
}

/* Set address from a name */
PJ_DEF(pj_status_t) pj_sockaddr_set_str_addr(int af,
					     pj_sockaddr *addr,
					     const pj_str_t *str_addr)
{
    pj_status_t status;

    if (af == PJ_AF_INET) {
	return pj_sockaddr_in_set_str_addr(&addr->ipv4, str_addr);
    }

    PJ_ASSERT_RETURN(af==PJ_AF_INET6, PJ_EAFNOTSUP);

    /* IPv6 specific */

    addr->ipv6.sin6_family = PJ_AF_INET6;
    PJ_SOCKADDR_RESET_LEN(addr);

    if (str_addr && str_addr->slen) {
	status = pj_inet_pton(PJ_AF_INET6, str_addr, &addr->ipv6.sin6_addr);
	if (status != PJ_SUCCESS) {
    	    pj_addrinfo ai;
	    unsigned count = 1;

	    status = pj_getaddrinfo(PJ_AF_INET6, str_addr, &count, &ai);
	    if (status==PJ_SUCCESS) {
		pj_memcpy(&addr->ipv6.sin6_addr, &ai.ai_addr.ipv6.sin6_addr,
			  sizeof(pj_sockaddr_in6));
	    }
	}
    } else {
	status = PJ_SUCCESS;
    }

    return status;
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
    PJ_ASSERT_RETURN(addr, (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    PJ_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = PJ_AF_INET;
#if PJ_ANDROID==1
#else
    pj_bzero(addr->sin_zero, sizeof(addr->sin_zero));
#endif
    pj_sockaddr_in_set_port(addr, port);
    return pj_sockaddr_in_set_str_addr(addr, str_addr);
}

/*
 * Initialize IP socket address based on the address and port info.
 */
PJ_DEF(pj_status_t) pj_sockaddr_init(int af, 
				     pj_sockaddr *addr,
				     const pj_str_t *cp,
				     pj_uint16_t port)
{
    pj_status_t status;

    if (af == PJ_AF_INET) {
	return pj_sockaddr_in_init(&addr->ipv4, cp, port);
    }

    /* IPv6 specific */
    PJ_ASSERT_RETURN(af==PJ_AF_INET6, PJ_EAFNOTSUP);

    pj_bzero(addr, sizeof(pj_sockaddr_in6));
    addr->addr.sa_family = PJ_AF_INET6;
    
    status = pj_sockaddr_set_str_addr(af, addr, cp);
    if (status != PJ_SUCCESS)
	return status;

    addr->ipv6.sin6_port = pj_htons(port);
    return PJ_SUCCESS;
}

/*
 * Compare two socket addresses.
 */
PJ_DEF(int) pj_sockaddr_cmp( const pj_sockaddr_t *addr1,
			     const pj_sockaddr_t *addr2)
{
    const pj_sockaddr *a1 = (const pj_sockaddr*) addr1;
    const pj_sockaddr *a2 = (const pj_sockaddr*) addr2;
    int port1, port2;
    int result;

    /* Compare address family */
    if (a1->addr.sa_family < a2->addr.sa_family)
	return -1;
    else if (a1->addr.sa_family > a2->addr.sa_family)
	return 1;

    /* Compare addresses */
    result = pj_memcmp(pj_sockaddr_get_addr(a1),
		       pj_sockaddr_get_addr(a2),
		       pj_sockaddr_get_addr_len(a1));
    if (result != 0)
	return result;

    /* Compare port number */
    port1 = pj_sockaddr_get_port(a1);
    port2 = pj_sockaddr_get_port(a2);

    if (port1 < port2)
	return -1;
    else if (port1 > port2)
	return 1;

    /* TODO:
     *	Do we need to compare flow label and scope id in IPv6? 
     */
    
    /* Looks equal */
    return 0;
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
 * Get port number of a pj_sockaddr_in
 */
PJ_DEF(pj_uint16_t) pj_sockaddr_in_get_port(const pj_sockaddr_in *addr)
{
    return pj_ntohs(addr->sin_port);
}

/*
 * Get the address part
 */
PJ_DEF(void*) pj_sockaddr_get_addr(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*)addr;

    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, NULL);

    if (a->addr.sa_family == PJ_AF_INET6)
	return (void*) &a->ipv6.sin6_addr;
    else
	return (void*) &a->ipv4.sin_addr;
}

/*
 * Check if sockaddr contains a non-zero address
 */
PJ_DEF(pj_bool_t) pj_sockaddr_has_addr(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*)addr;

    /* It's probably not wise to raise assertion here if
     * the address doesn't contain a valid address family, and
     * just return PJ_FALSE instead.
     * 
     * The reason is because application may need to distinguish 
     * these three conditions with sockaddr:
     *	a) sockaddr is not initialized. This is by convention
     *	   indicated by sa_family==0.
     *	b) sockaddr is initialized with zero address. This is
     *	   indicated with the address field having zero address.
     *	c) sockaddr is initialized with valid address/port.
     *
     * If we enable this assertion, then application will loose
     * the capability to specify condition a), since it will be
     * forced to always initialize sockaddr (even with zero address).
     * This may break some parts of upper layer libraries.
     */
    //PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
    //		     a->addr.sa_family == PJ_AF_INET6, PJ_FALSE);

    if (a->addr.sa_family!=PJ_AF_INET && a->addr.sa_family!=PJ_AF_INET6) {
	return PJ_FALSE;
    } else if (a->addr.sa_family == PJ_AF_INET6) {
	pj_uint8_t zero[24];
	pj_bzero(zero, sizeof(zero));
	return pj_memcmp(a->ipv6.sin6_addr.s6_addr, zero, 
			 sizeof(pj_in6_addr)) != 0;
    } else
	return a->ipv4.sin_addr.s_addr != PJ_INADDR_ANY;
}

/*
 * Get port number
 */
PJ_DEF(pj_uint16_t) pj_sockaddr_get_port(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;

    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, (pj_uint16_t)0xFFFF);

    return pj_ntohs((pj_uint16_t)(a->addr.sa_family == PJ_AF_INET6 ?
				    a->ipv6.sin6_port : a->ipv4.sin_port));
}

/*
 * Get the length of the address part.
 */
PJ_DEF(unsigned) pj_sockaddr_get_addr_len(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;
    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, 0);
    return a->addr.sa_family == PJ_AF_INET6 ?
	    sizeof(pj_in6_addr) : sizeof(pj_in_addr);
}

/*
 * Get socket address length.
 */
PJ_DEF(unsigned) pj_sockaddr_get_len(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;
    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, 0);
    return a->addr.sa_family == PJ_AF_INET6 ?
	    sizeof(pj_sockaddr_in6) : sizeof(pj_sockaddr_in);
}

/*
 * Copy only the address part (sin_addr/sin6_addr) of a socket address.
 */
PJ_DEF(void) pj_sockaddr_copy_addr( pj_sockaddr *dst,
				    const pj_sockaddr *src)
{
    /* Destination sockaddr might not be initialized */
    const char *srcbuf = (char*)pj_sockaddr_get_addr(src);
    char *dstbuf = ((char*)dst) + (srcbuf - (char*)src);
    pj_memcpy(dstbuf, srcbuf, pj_sockaddr_get_addr_len(src));
}

/*
 * Copy socket address.
 */
PJ_DEF(void) pj_sockaddr_cp(pj_sockaddr_t *dst, const pj_sockaddr_t *src)
{
    pj_memcpy(dst, src, pj_sockaddr_get_len(src));
}

/*
 * Set port number of pj_sockaddr_in
 */
PJ_DEF(void) pj_sockaddr_in_set_port(pj_sockaddr_in *addr, 
				     pj_uint16_t hostport)
{
    addr->sin_port = pj_htons(hostport);
}

/*
 * Set port number of pj_sockaddr
 */
PJ_DEF(pj_status_t) pj_sockaddr_set_port(pj_sockaddr *addr, 
					 pj_uint16_t hostport)
{
    int af = addr->addr.sa_family;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    if (af == PJ_AF_INET6)
	addr->ipv6.sin6_port = pj_htons(hostport);
    else
	addr->ipv4.sin_port = pj_htons(hostport);

    return PJ_SUCCESS;
}

/*
 * Get IPv4 address
 */
PJ_DEF(pj_in_addr) pj_sockaddr_in_get_addr(const pj_sockaddr_in *addr)
{
    pj_in_addr in_addr;
    in_addr.s_addr = pj_ntohl(addr->sin_addr.s_addr);
    return in_addr;
}

/*
 * Set IPv4 address
 */
PJ_DEF(void) pj_sockaddr_in_set_addr(pj_sockaddr_in *addr,
				     pj_uint32_t hostaddr)
{
    addr->sin_addr.s_addr = pj_htonl(hostaddr);
}

/*
 * Parse address
 */
PJ_DEF(pj_status_t) pj_sockaddr_parse2(int af, unsigned options,
				       const pj_str_t *str,
				       pj_str_t *p_hostpart,
				       pj_uint16_t *p_port,
				       int *raf)
{
    const char *end = str->ptr + str->slen;
    const char *last_colon_pos = NULL;
    unsigned colon_cnt = 0;
    const char *p;

    PJ_ASSERT_RETURN((af==PJ_AF_INET || af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) &&
		     options==0 &&
		     str!=NULL, PJ_EINVAL);

    /* Special handling for empty input */
    if (str->slen==0 || str->ptr==NULL) {
	if (p_hostpart)
	    p_hostpart->slen = 0;
	if (p_port)
	    *p_port = 0;
	if (raf)
	    *raf = PJ_AF_INET;
	return PJ_SUCCESS;
    }

    /* Count the colon and get the last colon */
    for (p=str->ptr; p!=end; ++p) {
	if (*p == ':') {
	    ++colon_cnt;
	    last_colon_pos = p;
	}
    }

    /* Deduce address family if it's not given */
    if (af == PJ_AF_UNSPEC) {
	if (colon_cnt > 1)
	    af = PJ_AF_INET6;
	else
	    af = PJ_AF_INET;
    } else if (af == PJ_AF_INET && colon_cnt > 1)
	return PJ_EINVAL;

    if (raf)
	*raf = af;

    if (af == PJ_AF_INET) {
	/* Parse as IPv4. Supported formats:
	 *  - "10.0.0.1:80"
	 *  - "10.0.0.1"
	 *  - "10.0.0.1:"
	 *  - ":80"
	 *  - ":"
	 */
	pj_str_t hostpart;
	unsigned long port;

	hostpart.ptr = (char*)str->ptr;

	if (last_colon_pos) {
	    pj_str_t port_part;
	    int i;

	    hostpart.slen = last_colon_pos - str->ptr;

	    port_part.ptr = (char*)last_colon_pos + 1;
	    port_part.slen = end - port_part.ptr;

	    /* Make sure port number is valid */
	    for (i=0; i<port_part.slen; ++i) {
		if (!pj_isdigit(port_part.ptr[i]))
		    return PJ_EINVAL;
	    }
	    port = pj_strtoul(&port_part);
	    if (port > 65535)
		return PJ_EINVAL;
	} else {
	    hostpart.slen = str->slen;
	    port = 0;
	}

	if (p_hostpart)
	    *p_hostpart = hostpart;
	if (p_port)
	    *p_port = (pj_uint16_t)port;

	return PJ_SUCCESS;

    } else if (af == PJ_AF_INET6) {

	/* Parse as IPv6. Supported formats:
	 *  - "fe::01:80"  ==> note: port number is zero in this case, not 80!
	 *  - "[fe::01]:80"
	 *  - "fe::01"
	 *  - "fe::01:"
	 *  - "[fe::01]"
	 *  - "[fe::01]:"
	 *  - "[::]:80"
	 *  - ":::80"
	 *  - "[::]"
	 *  - "[::]:"
	 *  - ":::"
	 *  - "::"
	 */
	pj_str_t hostpart, port_part;

	if (*str->ptr == '[') {
	    char *end_bracket;
	    int i;
	    unsigned long port;

	    if (last_colon_pos == NULL)
		return PJ_EINVAL;

	    end_bracket = pj_strchr(str, ']');
	    if (end_bracket == NULL)
		return PJ_EINVAL;

	    hostpart.ptr = (char*)str->ptr + 1;
	    hostpart.slen = end_bracket - hostpart.ptr;

	    if (last_colon_pos < end_bracket) {
		port_part.ptr = NULL;
		port_part.slen = 0;
	    } else {
		port_part.ptr = (char*)last_colon_pos + 1;
		port_part.slen = end - port_part.ptr;
	    }

	    /* Make sure port number is valid */
	    for (i=0; i<port_part.slen; ++i) {
		if (!pj_isdigit(port_part.ptr[i]))
		    return PJ_EINVAL;
	    }
	    port = pj_strtoul(&port_part);
	    if (port > 65535)
		return PJ_EINVAL;

	    if (p_hostpart)
		*p_hostpart = hostpart;
	    if (p_port)
		*p_port = (pj_uint16_t)port;

	    return PJ_SUCCESS;

	} else {
	    /* Treat everything as part of the IPv6 IP address */
	    if (p_hostpart)
		*p_hostpart = *str;
	    if (p_port)
		*p_port = 0;

	    return PJ_SUCCESS;
	}

    } else {
	return PJ_EAFNOTSUP;
    }

}

/*
 * Parse address
 */
PJ_DEF(pj_status_t) pj_sockaddr_parse( int af, unsigned options,
				       const pj_str_t *str,
				       pj_sockaddr *addr)
{
    pj_str_t hostpart;
    pj_uint16_t port;
    pj_status_t status;

    PJ_ASSERT_RETURN(addr, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_UNSPEC ||
		     af==PJ_AF_INET ||
		     af==PJ_AF_INET6, PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    status = pj_sockaddr_parse2(af, options, str, &hostpart, &port, &af);
    if (status != PJ_SUCCESS)
	return status;
    
#if !defined(PJ_HAS_IPV6) || !PJ_HAS_IPV6
    if (af==PJ_AF_INET6)
	return PJ_EIPV6NOTSUP;
#endif

    status = pj_sockaddr_init(af, addr, &hostpart, port);
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
    if (status != PJ_SUCCESS && af == PJ_AF_INET6) {
	/* Parsing does not yield valid address. Try to treat the last 
	 * portion after the colon as port number.
	 */
	const char *last_colon_pos=NULL, *p;
	const char *end = str->ptr + str->slen;
	unsigned long long_port;
	pj_str_t port_part;
	int i;

	/* Parse as IPv6:port */
	for (p=str->ptr; p!=end; ++p) {
	    if (*p == ':')
		last_colon_pos = p;
	}

	if (last_colon_pos == NULL)
	    return status;

	hostpart.ptr = (char*)str->ptr;
	hostpart.slen = last_colon_pos - str->ptr;

	port_part.ptr = (char*)last_colon_pos + 1;
	port_part.slen = end - port_part.ptr;

	/* Make sure port number is valid */
	for (i=0; i<port_part.slen; ++i) {
	    if (!pj_isdigit(port_part.ptr[i]))
		return status;
	}
	long_port = pj_strtoul(&port_part);
	if (long_port > 65535)
	    return status;

	port = (pj_uint16_t)long_port;

	status = pj_sockaddr_init(PJ_AF_INET6, addr, &hostpart, port);
    }
#endif
    
    return status;
}

/* Resolve the IP address of local machine */
PJ_DEF(pj_status_t) pj_gethostip(int af, pj_sockaddr *addr)
{
    unsigned i, count, cand_cnt;
    enum {
	CAND_CNT = 8,

	/* Weighting to be applied to found addresses */
	WEIGHT_HOSTNAME	= 1,	/* hostname IP is not always valid! */
	WEIGHT_DEF_ROUTE = 2,
	WEIGHT_INTERFACE = 1,
	WEIGHT_LOOPBACK = -5,
	WEIGHT_LINK_LOCAL = -4,
	WEIGHT_DISABLED = -50,

	MIN_WEIGHT = WEIGHT_DISABLED+1	/* minimum weight to use */
    };
    /* candidates: */
    pj_sockaddr cand_addr[CAND_CNT];
    int		cand_weight[CAND_CNT];
    int	        selected_cand;
    char	strip[PJ_INET6_ADDRSTRLEN+10];
    /* Special IPv4 addresses. */
    struct spec_ipv4_t
    {
	pj_uint32_t addr;
	pj_uint32_t mask;
	int	    weight;
    } spec_ipv4[] =
    {
	/* 127.0.0.0/8, loopback addr will be used if there is no other
	 * addresses.
	 */
	{ 0x7f000000, 0xFF000000, WEIGHT_LOOPBACK },

	/* 0.0.0.0/8, special IP that doesn't seem to be practically useful */
	{ 0x00000000, 0xFF000000, WEIGHT_DISABLED },

	/* 169.254.0.0/16, a zeroconf/link-local address, which has higher
	 * priority than loopback and will be used if there is no other
	 * valid addresses.
	 */
	{ 0xa9fe0000, 0xFFFF0000, WEIGHT_LINK_LOCAL }
    };
    /* Special IPv6 addresses */
    struct spec_ipv6_t
    {
	pj_uint8_t addr[16];
	pj_uint8_t mask[16];
	int	   weight;
    } spec_ipv6[] =
    {
	/* Loopback address, ::1/128 */
	{ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
	  WEIGHT_LOOPBACK
	},

	/* Link local, fe80::/10 */
	{ {0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	  {0xff,0xc0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	  WEIGHT_LINK_LOCAL
	},

	/* Disabled, ::/128 */
	{ {0x0,0x0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{ 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
	  WEIGHT_DISABLED
	}
    };
    pj_addrinfo ai;
    pj_status_t status;

    /* May not be used if TRACE_ is disabled */
    PJ_UNUSED_ARG(strip);

#ifdef _MSC_VER
    /* Get rid of "uninitialized he variable" with MS compilers */
    pj_bzero(&ai, sizeof(ai));
#endif

    cand_cnt = 0;
    pj_bzero(cand_addr, sizeof(cand_addr));
    pj_bzero(cand_weight, sizeof(cand_weight));
    for (i=0; i<PJ_ARRAY_SIZE(cand_addr); ++i) {
	cand_addr[i].addr.sa_family = (pj_uint16_t)af;
	PJ_SOCKADDR_RESET_LEN(&cand_addr[i]);
    }

    addr->addr.sa_family = (pj_uint16_t)af;
    PJ_SOCKADDR_RESET_LEN(addr);

#if !defined(PJ_GETHOSTIP_DISABLE_LOCAL_RESOLUTION) || \
	PJ_GETHOSTIP_DISABLE_LOCAL_RESOLUTION == 0
	TRACE_((THIS_FILE, "pj_gethostip() pj_getaddrinfo1"));
    /* Get hostname's IP address */
    count = 1;
    status = pj_getaddrinfo(af, pj_gethostname(), &count, &ai);
    if (status == PJ_SUCCESS) {
    	pj_assert(ai.ai_addr.addr.sa_family == (pj_uint16_t)af);
    	pj_sockaddr_copy_addr(&cand_addr[cand_cnt], &ai.ai_addr);
	pj_sockaddr_set_port(&cand_addr[cand_cnt], 0);
	cand_weight[cand_cnt] += WEIGHT_HOSTNAME;
	++cand_cnt;

	TRACE_((THIS_FILE, "hostname IP is %s",
		pj_sockaddr_print(&ai.ai_addr, strip, sizeof(strip), 0)));
	}
	TRACE_((THIS_FILE, "pj_gethostip() pj_getaddrinfo2"));
#else
    PJ_UNUSED_ARG(ai);
    PJ_UNUSED_ARG(count);
#endif

    /* Get default interface (interface for default route) */
    if (cand_cnt < PJ_ARRAY_SIZE(cand_addr)) {
	status = pj_getdefaultipinterface(af, addr);
	if (status == PJ_SUCCESS) {
	    TRACE_((THIS_FILE, "default IP is %s",
		    pj_sockaddr_print(addr, strip, sizeof(strip), 0)));

	    pj_sockaddr_set_port(addr, 0);
	    for (i=0; i<cand_cnt; ++i) {
		if (pj_sockaddr_cmp(&cand_addr[i], addr)==0)
		    break;
	    }

	    cand_weight[i] += WEIGHT_DEF_ROUTE;
	    if (i >= cand_cnt) {
		pj_sockaddr_copy_addr(&cand_addr[i], addr);
		++cand_cnt;
	    }
	}
    }


    /* Enumerate IP interfaces */
    if (cand_cnt < PJ_ARRAY_SIZE(cand_addr)) {
	unsigned start_if = cand_cnt;
	unsigned count = PJ_ARRAY_SIZE(cand_addr) - start_if;

	status = pj_enum_ip_interface(af, &count, &cand_addr[start_if]);
	if (status == PJ_SUCCESS && count) {
	    /* Clear the port number */
	    for (i=0; i<count; ++i)            
		pj_sockaddr_set_port(&cand_addr[start_if+i], 0);

	    /* For each candidate that we found so far (that is the hostname
	     * address and default interface address, check if they're found
	     * in the interface list. If found, add the weight, and if not,
	     * decrease the weight.
	     */
	    for (i=0; i<cand_cnt; ++i) {
		unsigned j;
		for (j=0; j<count; ++j) {
		    if (pj_sockaddr_cmp(&cand_addr[i], 
					&cand_addr[start_if+j])==0)
			break;
		}

		if (j == count) {
		    /* Not found */
		    cand_weight[i] -= WEIGHT_INTERFACE;
		} else {
		    cand_weight[i] += WEIGHT_INTERFACE;
		}
	    }

	    /* Add remaining interface to candidate list. */
	    for (i=0; i<count; ++i) {
		unsigned j;
		for (j=0; j<cand_cnt; ++j) {
		    if (pj_sockaddr_cmp(&cand_addr[start_if+i], 
					&cand_addr[j])==0)
			break;
		}

		if (j == cand_cnt) {
		    pj_sockaddr_copy_addr(&cand_addr[cand_cnt], 
					  &cand_addr[start_if+i]);
		    cand_weight[cand_cnt] += WEIGHT_INTERFACE;
		    ++cand_cnt;
		}
	    }
	}
    }

    /* Apply weight adjustment for special IPv4/IPv6 addresses
     * See http://trac.pjsip.org/repos/ticket/1046
     */
    if (af == PJ_AF_INET) {
	for (i=0; i<cand_cnt; ++i) {
	    unsigned j;
	    for (j=0; j<PJ_ARRAY_SIZE(spec_ipv4); ++j) {
		    pj_uint32_t a = pj_ntohl(cand_addr[i].ipv4.sin_addr.s_addr);
		    pj_uint32_t pa = spec_ipv4[j].addr;
		    pj_uint32_t pm = spec_ipv4[j].mask;

		    if ((a & pm) == pa) {
			cand_weight[i] += spec_ipv4[j].weight;
			break;
		    }
	    }
	}
    } else if (af == PJ_AF_INET6) {
	for (i=0; i<PJ_ARRAY_SIZE(spec_ipv6); ++i) {
		unsigned j;
		for (j=0; j<cand_cnt; ++j) {
		    pj_uint8_t *a = cand_addr[j].ipv6.sin6_addr.s6_addr;
		    pj_uint8_t am[16];
		    pj_uint8_t *pa = spec_ipv6[i].addr;
		    pj_uint8_t *pm = spec_ipv6[i].mask;
		    unsigned k;

		    for (k=0; k<16; ++k) {
			am[k] = (pj_uint8_t)((a[k] & pm[k]) & 0xFF);
		    }

		    if (pj_memcmp(am, pa, 16)==0) {
			cand_weight[j] += spec_ipv6[i].weight;
		    }
		}
	}
    } else {
	return PJ_EAFNOTSUP;
    }

    /* Enumerate candidates to get the best IP address to choose */
    selected_cand = -1;
    for (i=0; i<cand_cnt; ++i) {
	TRACE_((THIS_FILE, "Checking candidate IP %s, weight=%d",
		pj_sockaddr_print(&cand_addr[i], strip, sizeof(strip), 0),
		cand_weight[i]));

	if (cand_weight[i] < MIN_WEIGHT) {
	    continue;
	}

	if (selected_cand == -1)
	    selected_cand = i;
	else if (cand_weight[i] > cand_weight[selected_cand])
	    selected_cand = i;
    }

    /* If else fails, returns loopback interface as the last resort */
    if (selected_cand == -1) {
	if (af==PJ_AF_INET) {
	    addr->ipv4.sin_addr.s_addr = pj_htonl (0x7f000001);
	} else {
	    pj_in6_addr *s6_addr;

	    s6_addr = (pj_in6_addr*) pj_sockaddr_get_addr(addr);
	    pj_bzero(s6_addr, sizeof(pj_in6_addr));
	    s6_addr->s6_addr[15] = 1;
	}
	TRACE_((THIS_FILE, "Loopback IP %s returned",
		pj_sockaddr_print(addr, strip, sizeof(strip), 0)));
    } else {
	pj_sockaddr_copy_addr(addr, &cand_addr[selected_cand]);
	TRACE_((THIS_FILE, "Candidate %s selected",
		pj_sockaddr_print(addr, strip, sizeof(strip), 0)));
    }

    return PJ_SUCCESS;
}

/* Get the default IP interface */
PJ_DEF(pj_status_t) pj_getdefaultipinterface(int af, pj_sockaddr *addr)
{
    pj_sock_t fd;
    pj_str_t cp;
    pj_sockaddr a;
    int len;
    pj_uint8_t zero[64];
    pj_status_t status;

    addr->addr.sa_family = (pj_uint16_t)af;

    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &fd);
    if (status != PJ_SUCCESS) {
	return status;
	}
	PJ_LOG(4, ("sock_common.c", "pj_getdefaultipinterface() pj_sock_socket."));

    if (af == PJ_AF_INET) {
	cp = pj_str("1.1.1.1");
    } else {
	cp = pj_str("1::1");
    }
    status = pj_sockaddr_init(af, &a, &cp, 53);
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
	}
	PJ_LOG(4, ("sock_common.c", "pj_getdefaultipinterface() pj_sockaddr_init."));

    status = pj_sock_connect(fd, &a, pj_sockaddr_get_len(&a));
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
	}
	PJ_LOG(4, ("sock_common.c", "pj_getdefaultipinterface() pj_sock_connect."));

    len = sizeof(a);
    status = pj_sock_getsockname(fd, &a, &len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
	}
	PJ_LOG(4, ("sock_common.c", "pj_getdefaultipinterface() pj_sock_getsockname."));

    pj_sock_close(fd);

    /* Check that the address returned is not zero */
    pj_bzero(zero, sizeof(zero));
    if (pj_memcmp(pj_sockaddr_get_addr(&a), zero,
		  pj_sockaddr_get_addr_len(&a))==0)
	{
		PJ_LOG(4, ("sock_common.c", "pj_getdefaultipinterface() interface not found."));
		return PJ_ENOTFOUND;
    }

    pj_sockaddr_copy_addr(addr, &a);

    /* Success */
    return PJ_SUCCESS;
}


/* Only need to implement these in DLL build */
#if defined(PJ_DLL)

PJ_DEF(pj_uint16_t) pj_AF_UNSPEC(void)
{
    return PJ_AF_UNSPEC;
}

PJ_DEF(pj_uint16_t) pj_AF_UNIX(void)
{
    return PJ_AF_UNIX;
}

PJ_DEF(pj_uint16_t) pj_AF_INET(void)
{
    return PJ_AF_INET;
}

PJ_DEF(pj_uint16_t) pj_AF_INET6(void)
{
    return PJ_AF_INET6;
}

PJ_DEF(pj_uint16_t) pj_AF_PACKET(void)
{
    return PJ_AF_PACKET;
}

PJ_DEF(pj_uint16_t) pj_AF_IRDA(void)
{
    return PJ_AF_IRDA;
}

PJ_DEF(int) pj_SOCK_STREAM(void)
{
    return PJ_SOCK_STREAM;
}

PJ_DEF(int) pj_SOCK_DGRAM(void)
{
    return PJ_SOCK_DGRAM;
}

PJ_DEF(int) pj_SOCK_RAW(void)
{
    return PJ_SOCK_RAW;
}

PJ_DEF(int) pj_SOCK_RDM(void)
{
    return PJ_SOCK_RDM;
}

PJ_DEF(pj_uint16_t) pj_SOL_SOCKET(void)
{
    return PJ_SOL_SOCKET;
}

PJ_DEF(pj_uint16_t) pj_SOL_IP(void)
{
    return PJ_SOL_IP;
}

PJ_DEF(pj_uint16_t) pj_SOL_TCP(void)
{
    return PJ_SOL_TCP;
}

PJ_DEF(pj_uint16_t) pj_SOL_UDP(void)
{
    return PJ_SOL_UDP;
}

PJ_DEF(pj_uint16_t) pj_SOL_IPV6(void)
{
    return PJ_SOL_IPV6;
}

PJ_DEF(int) pj_IP_TOS(void)
{
    return PJ_IP_TOS;
}

PJ_DEF(int) pj_IPTOS_LOWDELAY(void)
{
    return PJ_IPTOS_LOWDELAY;
}

PJ_DEF(int) pj_IPTOS_THROUGHPUT(void)
{
    return PJ_IPTOS_THROUGHPUT;
}

PJ_DEF(int) pj_IPTOS_RELIABILITY(void)
{
    return PJ_IPTOS_RELIABILITY;
}

PJ_DEF(int) pj_IPTOS_MINCOST(void)
{
    return PJ_IPTOS_MINCOST;
}

PJ_DEF(pj_uint16_t) pj_SO_TYPE(void)
{
    return PJ_SO_TYPE;
}

PJ_DEF(pj_uint16_t) pj_SO_RCVBUF(void)
{
    return PJ_SO_RCVBUF;
}

PJ_DEF(pj_uint16_t) pj_SO_SNDBUF(void)
{
    return PJ_SO_SNDBUF;
}

PJ_DEF(pj_uint16_t) pj_TCP_NODELAY(void)
{
    return PJ_TCP_NODELAY;
}

PJ_DEF(pj_uint16_t) pj_SO_REUSEADDR(void)
{
    return PJ_SO_REUSEADDR;
}

PJ_DEF(pj_uint16_t) pj_SO_NOSIGPIPE(void)
{
    return PJ_SO_NOSIGPIPE;
}

PJ_DEF(pj_uint16_t) pj_SO_PRIORITY(void)
{
    return PJ_SO_PRIORITY;
}

PJ_DEF(pj_uint16_t) pj_IP_MULTICAST_IF(void)
{
    return PJ_IP_MULTICAST_IF;
}

PJ_DEF(pj_uint16_t) pj_IP_MULTICAST_TTL(void)
{
    return PJ_IP_MULTICAST_TTL;
}

PJ_DEF(pj_uint16_t) pj_IP_MULTICAST_LOOP(void)
{
    return PJ_IP_MULTICAST_LOOP;
}

PJ_DEF(pj_uint16_t) pj_IP_ADD_MEMBERSHIP(void)
{
    return PJ_IP_ADD_MEMBERSHIP;
}

PJ_DEF(pj_uint16_t) pj_IP_DROP_MEMBERSHIP(void)
{
    return PJ_IP_DROP_MEMBERSHIP;
}

PJ_DEF(pj_uint16_t) pj_TCP_KEEPALIVE(void)
{
	return PJ_TCP_KEEPALIVE;
}

PJ_DEF(pj_uint16_t) pj_SO_KEEPALIVE(void)
{
	return PJ_SO_KEEPALIVE;
}

PJ_DEF(pj_uint16_t) pj_TCP_KEEPIDLE(void)
{
	return PJ_TCP_KEEPIDLE;
}

PJ_DEF(pj_uint16_t) pj_TCP_KEEPINTVL(void)
{
	return PJ_TCP_KEEPINTVL;
}

PJ_DEF(pj_uint16_t) pj_TCP_KEEPCNT(void)
{
	return PJ_TCP_KEEPCNT;
}

PJ_DEF(int) pj_MSG_OOB(void)
{
    return PJ_MSG_OOB;
}

PJ_DEF(int) pj_MSG_PEEK(void)
{
    return PJ_MSG_PEEK;
}

PJ_DEF(int) pj_MSG_DONTROUTE(void)
{
    return PJ_MSG_DONTROUTE;
}

#endif	/* PJ_DLL */


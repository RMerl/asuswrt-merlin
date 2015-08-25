/* $Id: addr_resolv.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_ADDR_RESOLV_H__
#define __PJ_ADDR_RESOLV_H__

/**
 * @file addr_resolv.h
 * @brief IP address resolution.
 */

#include <pj/sock.h>

PJ_BEGIN_DECL

/**
 * @defgroup pj_addr_resolve Network Address Resolution
 * @ingroup PJ_IO
 * @{
 *
 * This module provides function to resolve Internet address of the
 * specified host name. To resolve a particular host name, application
 * can just call #pj_gethostbyname().
 *
 * Example:
 * <pre>
 *   ...
 *   pj_hostent he;
 *   pj_status_t rc;
 *   pj_str_t host = pj_str("host.example.com");
 *   
 *   rc = pj_gethostbyname( &host, &he);
 *   if (rc != PJ_SUCCESS) {
 *      char errbuf[80];
 *      pj_strerror( rc, errbuf, sizeof(errbuf));
 *      PJ_LOG(2,("sample", "Unable to resolve host, error=%s", errbuf));
 *      return rc;
 *   }
 *
 *   // process address...
 *   addr.sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
 *   ...
 * </pre>
 *
 * It's pretty simple really...
 */

/** This structure describes an Internet host address. */
typedef struct pj_hostent
{
    char    *h_name;		/**< The official name of the host. */
    char   **h_aliases;		/**< Aliases list. */
    int	     h_addrtype;	/**< Host address type. */
    int	     h_length;		/**< Length of address. */
    char   **h_addr_list;	/**< List of addresses. */
} pj_hostent;

/** Shortcut to h_addr_list[0] */
#define h_addr h_addr_list[0]

/** 
 * This structure describes address information pj_getaddrinfo().
 */
typedef struct pj_addrinfo
{
    char	 ai_canonname[PJ_MAX_HOSTNAME]; /**< Canonical name for host*/
    pj_sockaddr  ai_addr;			/**< Binary address.	    */
} pj_addrinfo;


/**
 * This function fills the structure of type pj_hostent for a given host name.
 * For host resolution function that also works with IPv6, please see
 * #pj_getaddrinfo().
 *
 * @param name	    Host name to resolve. Specifying IPv4 address here
 *		    may fail on some platforms (e.g. Windows)
 * @param he	    The pj_hostent structure to be filled. Note that
 *		    the pointers in this structure points to temporary
 *		    variables which value will be reset upon subsequent
 *		    invocation.
 *
 * @return	    PJ_SUCCESS, or the appropriate error codes.
 */ 
PJ_DECL(pj_status_t) pj_gethostbyname(const pj_str_t *name, pj_hostent *he);


/**
 * Resolve the primary IP address of local host. 
 *
 * @param af	    The desired address family to query. Valid values
 *		    are pj_AF_INET() or pj_AF_INET6().
 * @param addr      On successful resolution, the address family and address
 *		    part of this socket address will be filled up with the host
 *		    IP address, in network byte order. Other parts of the socket
 *		    address are untouched.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_gethostip(int af, pj_sockaddr *addr);


/**
 * Get the IP address of the default interface. Default interface is the
 * interface of the default route.
 *
 * @param af	    The desired address family to query. Valid values
 *		    are pj_AF_INET() or pj_AF_INET6().
 * @param addr      On successful resolution, the address family and address
 *		    part of this socket address will be filled up with the host
 *		    IP address, in network byte order. Other parts of the socket
 *		    address are untouched.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_getdefaultipinterface(int af,
					      pj_sockaddr *addr);


/**
 * This function translates the name of a service location (for example, 
 * a host name) and returns a set of addresses and associated information
 * to be used in creating a socket with which to address the specified 
 * service.
 *
 * @param af	    The desired address family to query. Valid values
 *		    are pj_AF_INET(), pj_AF_INET6(), or pj_AF_UNSPEC().
 * @param name	    Descriptive name or an address string, such as host
 *		    name.
 * @param count	    On input, it specifies the number of elements in
 *		    \a ai array. On output, this will be set with the
 *		    number of address informations found for the
 *		    specified name.
 * @param ai	    Array of address info to be filled with the information
 *		    about the host.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *name,
				    unsigned *count, pj_addrinfo ai[]);



/** @} */

PJ_END_DECL

#endif	/* __PJ_ADDR_RESOLV_H__ */


/* $Id: dns_server.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_DNS_SERVER_H__
#define __PJLIB_UTIL_DNS_SERVER_H__

/**
 * @file dns_server.h
 * @brief Simple DNS server
 */
#include <pjlib-util/types.h>
#include <pjlib-util/dns.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_DNS_SERVER Simple DNS Server
 * @ingroup PJ_DNS
 * @{
 * This contains a simple but fully working DNS server implementation, 
 * mostly for testing purposes. It supports serving various DNS resource 
 * records such as SRV, CNAME, A, and AAAA.
 */

/**
 * Opaque structure to hold DNS server instance.
 */
typedef struct pj_dns_server pj_dns_server;

/**
 * Create the DNS server instance. The instance will run immediately.
 *
 * @param pf	    The pool factory to create memory pools.
 * @param ioqueue   Ioqueue instance where the server socket will be
 *		    registered to.
 * @param af	    Address family of the server socket (valid values
 *		    are pj_AF_INET() for IPv4 and pj_AF_INET6() for IPv6).
 * @param port	    The UDP port to listen.
 * @param flags	    Flags, currently must be zero.
 * @param p_srv	    Pointer to receive the DNS server instance.
 *
 * @return	    PJ_SUCCESS if server has been created successfully,
 *		    otherwise the function will return the appropriate
 *		    error code.
 */
PJ_DECL(pj_status_t) pj_dns_server_create(pj_pool_factory *pf,
				          pj_ioqueue_t *ioqueue,
					  int af,
					  unsigned port,
					  unsigned flags,
				          pj_dns_server **p_srv);

/**
 * Destroy DNS server instance.
 *
 * @param srv	    The DNS server instance.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_dns_server_destroy(pj_dns_server *srv);


/**
 * Add generic resource record entries to the server.
 *
 * @param srv	    The DNS server instance.
 * @param count	    Number of records to be added.
 * @param rr	    Array of records to be added.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_dns_server_add_rec(pj_dns_server *srv,
					   unsigned count,
					   const pj_dns_parsed_rr rr[]);

/**
 * Remove the specified record from the server.
 *
 * @param srv	    The DNS server instance.
 * @param dns_class The resource's DNS class. Valid value is PJ_DNS_CLASS_IN.
 * @param type	    The resource type.
 * @param name	    The resource name to be removed.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_dns_server_del_rec(pj_dns_server *srv,
					   int dns_class,
					   pj_dns_type type,
					   const pj_str_t *name);



/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_DNS_SERVER_H__ */


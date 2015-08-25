/* $Id: sip_resolve.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_RESOLVE_H__
#define __PJSIP_SIP_RESOLVE_H__

/**
 * @file sip_resolve.h
 * @brief 
 * This module contains the mechanism to resolve server address as specified by
 * RFC 3263 - Locating SIP Servers
 */

#include <pjsip/sip_types.h>
#include <pjlib-util/resolver.h>
#include <pj/sock.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_RESOLVE SIP SRV Server Resolution (RFC 3263 - Locating SIP Servers)
 * @ingroup PJSIP_TRANSPORT
 * @brief Framework to resolve SIP servers based on RFC 3263.
 * @{
 * \section PJSIP_RESOLVE_FEATURES Features
 *
 * This is the SIP server resolution framework, which is modelled after 
 * RFC 3263 - Locating SIP Servers document. The SIP server resolution
 * framework is asynchronous; callback will be called once the server 
 * address has been resolved (successfully or with errors).
 *
 * \subsection PJSIP_RESOLVE_CONFORMANT Conformance to RFC 3263
 *
 * The SIP server resolution framework is modelled after RFC 3263 (Locating 
 * SIP Servers) document, and it provides a single function (#pjsip_resolve())
 * to resolve a domain into actual IP addresses of the servers, by querying 
 * DNS SRV record and DNS A record where necessary.
 *
 * The #pjsip_resolve() function performs the server resolution according
 * to RFC 3263 with some additional fallback mechanisms, as follows:
 *  - if the target name is an IP address, the callback will be called
 *    immediately with the IP address. If port number was specified, this
 *    port number will be used, otherwise the default port number for the
 *    transport will be used (5060 for TCP/UDP, 5061 for TLS) if the transport
 *    is specified. If the transport is not specified, UDP with port number
 *    5060 will be used.
 *  - if target name is not an IP address but it contains port number,
 *    then the target name is resolved with DNS A (or AAAA, when IPv6 is
 *    supported in the future) query, and the port is taken from the
 *    port number argument. The callback will be called once the DNS A
 *    resolution completes. If the DNS A resolution returns multiple IP
 *    addresses, these IP addresses will be returned to the caller.
 *  - if target name is not an IP address and port number is not specified,
 *    DNS SRV resolution will be performed for the specified name and
 *    transport type (or UDP when transport is not specified), 
 *    then followed by DNS A (or AAAA, when IPv6 is supported)
 *    resolution for each target in the SRV record. If DNS SRV
 *    resolution returns error, DNS A (or AAAA) resolution will be
 *    performed for the original target (it is assumed that the target domain
 *    does not support SRV records). Upon successful completion, 
 *    application callback will be called with each IP address of the
 *    target selected based on the load-balancing and fail-over criteria
 *    below.
 *
 * The above server resolution procedure differs from RFC 3263 in these
 * regards:
 *  - currently #pjsip_resolve() doesn't support DNS NAPTR record.
 *  - if transport is not specified, it is assumed to be UDP (the proper
 *    behavior is to query the NAPTR record, but we don't support this
 *    yet).
 *
 *
 * \subsection PJSIP_SIP_RESOLVE_FAILOVER_LOADBALANCE Load-Balancing and Fail-Over
 *
 * When multiple targets are returned in the DNS SRV response, server entries
 * are selected based on the following rule (which is described in RFC 2782):
 *  - targets will be sorted based on the priority first.
 *  - for targets with the same priority, #pjsip_resolve() will select
 *    only one target according to its weight. To select this one target,
 *    the function associates running-sum for all targets, and generates 
 *    a random number between zero and the total running-sum (inclusive).
 *    The target selected is the first target with running-sum greater than
 *    or equal to this random number.
 *
 * The above procedure will select one target for each priority, allowing
 * application to fail-over to the next target when the previous target fails.
 * These targets are returned in the #pjsip_server_addresses structure 
 * argument of the callback. 
 *
 * \subsection PJSIP_SIP_RESOLVE_SIP_FEATURES SIP SRV Resolver Features
 *
 * Some features of the SIP resolver:
 *  - DNS SRV entries are returned on sorted order based on priority 
 *    to allow failover to the next appropriate server.
 *  - The procedure in RFC 2782 is used to select server with the same
 *    priority to load-balance the servers load.
 *  - A single function (#pjsip_resolve()) performs all server resolution
 *    works, from resolving the SRV records to getting the actual IP addresses
 *    of the servers with DNS A (or AAAA) resolution.
 *  - When multiple DNS SRV records are returned, parallel DNS A (or AAAA)
 *    queries will be issued simultaneously.
 *  - The PJLIB-UTIL DNS resolver provides additional functionality such as
 *    response caching, query aggregation, parallel nameservers, fallback
 *    nameserver, etc., which will be described below.
 * 
 *
 * \subsection PJSIP_RESOLVE_DNS_FEATURES DNS Resolver Features
 *
 * The PJSIP server resolution framework uses PJLIB-UTIL DNS resolver engine
 * for performing the asynchronous DNS request. The PJLIB-UTIL DNS resolver
 * has some useful features, such as:
 *  - queries are asynchronous with configurable timeout,
 *  - query aggregation to combine multiple pending queries to the same
 *    DNS target into a single DNS request (to save message round-trip and
 *    processing),
 *  - response caching with TTL negotiated between the minimum TTL found in
 *    the response and the maximum TTL allowed in the configuration,
 *  - multiple nameservers, with active nameserver is selected from nameserver
 *    which provides the best response time,
 *  - fallback nameserver, with periodic detection of which name servers are
 *    active or down.
 *  - etc.
 *
 * Please consult PJLIB-UTIL DNS resolver documentation for more details.
 *
 *
 * \section PJSIP_RESOLVE_USING Using the Resolver
 *
 * To maintain backward compatibility, the resolver MUST be enabled manually.
 * With the default settings, the resolver WILL NOT perform DNS SRV resolution,
 * as it will just resolve the name with standard pj_gethostbyname() function.
 *
 * Application can enable the SRV resolver by creating the PJLIB-UTIL DNS 
 * resolver with #pjsip_endpt_create_resolver(), configure the
 * nameservers of the PJLIB-UTIL DNS resolver object by calling
 * pj_dns_resolver_set_ns() function, and pass the DNS resolver object to 
 * #pjsip_resolver_set_resolver() function.
 *
 * Once the resolver is set, it will be used automatically by PJSIP everytime
 * PJSIP needs to send SIP request/response messages.
 *
 *
 * \section PJSIP_RESOLVE_REFERENCE Reference
 *
 * Reference:
 *  - RFC 2782: A DNS RR for specifying the location of services (DNS SRV)
 *  - RFC 3263: Locating SIP Servers
 */

/**
 * The server addresses returned by the resolver.
 */
typedef struct pjsip_server_addresses
{
    /** Number of address records. */
    unsigned	count;

    /** Address records. */
    struct
    {
	/** Preferable transport to be used to contact this address. */
	pjsip_transport_type_e	type;

	/** Server priority (the lower the higher the priority). */
	unsigned		priority;

	/** Server weight (the higher the more load it can handle). */
	unsigned		weight;

	/** The server's address. */
	pj_sockaddr		addr;

	/** Address length. */
	int			addr_len;

    } entry[PJSIP_MAX_RESOLVED_ADDRESSES];

} pjsip_server_addresses;


/**
 * The type of callback function to be called when resolver finishes the job.
 *
 * @param status    The status of the operation, which is zero on success.
 * @param token	    The token that was associated with the job when application
 *		    call the resolve function.
 * @param addr	    The addresses resolved by the operation.
 * @param target_prt 2013-05-15 DEAN. To determine transport type (TCP or TLS)
 */
typedef void pjsip_resolver_callback(pj_status_t status,
				     void *token,
				     const struct pjsip_server_addresses *addr,
					 int target_port);

/**
 * Create SIP resolver engine. Note that this function is normally called
 * internally by pjsip_endpoint instance.
 *
 * @param pool	    Pool to allocate memory from.
 * @param p_res	    Pointer to receive SIP resolver instance.
 *
 * @return	    PJ_SUCCESS when resolver can be successfully created.
 */
PJ_DECL(pj_status_t) pjsip_resolver_create(pj_pool_t *pool,
					   pjsip_resolver_t **p_res);

/**
 * Set the DNS resolver instance of the SIP resolver engine. Before the
 * DNS resolver is set, the SIP resolver will use standard pj_gethostbyname()
 * to resolve addresses.
 *
 * Note that application normally will use #pjsip_endpt_set_resolver() instead
 * since it does not normally have access to the SIP resolver instance.
 *
 * @param res	    The SIP resolver engine.
 * @param dns_res   The DNS resolver instance to be used by the SIP resolver.
 *		    This argument can be NULL to reset the internal DNS
 *		    instance.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_resolver_set_resolver(pjsip_resolver_t *res,
						 pj_dns_resolver *dns_res);


/**
 * Get the DNS resolver instance of the SIP resolver engine. 
 *
 * Note that application normally will use #pjsip_endpt_get_resolver() instead
 * since it does not normally have access to the SIP resolver instance.
 *
 * @param res	    The SIP resolver engine.
 *
 * @return	    The DNS resolver instance (may be NULL)
 */
PJ_DECL(pj_dns_resolver*) pjsip_resolver_get_resolver(pjsip_resolver_t *res);

/**
 * Destroy resolver engine. Note that this will also destroy the internal
 * DNS resolver inside the engine. If application doesn't want the internal
 * DNS resolver to be destroyed, it should set the internal DNS resolver
 * to NULL before calling this function.
 *
 * Note that this function will normally called by the SIP endpoint instance
 * when the SIP endpoint instance is destroyed.
 *
 * @param resolver The resolver.
 */
PJ_DECL(void) pjsip_resolver_destroy(pjsip_resolver_t *resolver);

/**
 * Asynchronously resolve a SIP target host or domain according to rule 
 * specified in RFC 3263 (Locating SIP Servers). When the resolving operation
 * has completed, the callback will be called.
 *
 * Note that application normally will use #pjsip_endpt_resolve() instead
 * since it does not normally have access to the SIP resolver instance.
 *
 * @param resolver	The resolver engine.
 * @param pool		The pool to allocate resolver job.
 * @param target	The target specification to be resolved.
 * @param token		A user defined token to be passed back to callback function.
 * @param cb		The callback function.
 */
PJ_DECL(void) pjsip_resolve( pjsip_resolver_t *resolver,
			     pj_pool_t *pool,
			     const pjsip_host_info *target,
			     void *token,
			     pjsip_resolver_callback *cb);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_RESOLVE_H__ */

/* $Id: sip_resolve.c 4103 2012-04-26 23:42:27Z bennylp $ */
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
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/srv_resolver.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "sip_resolve.c"

#define ADDR_MAX_COUNT	    8

struct naptr_target
{
    pj_str_t		    res_type;	    /**< e.g. "_sip._udp"   */
    pj_str_t		    name;	    /**< Domain name.	    */
    pjsip_transport_type_e  type;	    /**< Transport type.    */
    unsigned		    order;	    /**< Order		    */
    unsigned		    pref;	    /**< Preference.	    */
};

struct query
{
    char		    *objname;

    pj_dns_type		     query_type;
    void		    *token;
    pjsip_resolver_callback *cb;
    pj_dns_async_query	    *object;
    pj_status_t		     last_error;

    /* Original request: */
    struct {
	pjsip_host_info	     target;
	unsigned	     def_port;
    } req;

    /* NAPTR records: */
    unsigned		     naptr_cnt;
    struct naptr_target	     naptr[8];
};


struct pjsip_resolver_t
{
    pj_dns_resolver *res;
};


static void srv_resolver_cb(void *user_data,
			    pj_status_t status,
			    const pj_dns_srv_record *rec);
static void dns_a_callback(void *user_data,
			   pj_status_t status,
			   pj_dns_parsed_packet *response);


/*
 * Public API to create the resolver.
 */
PJ_DEF(pj_status_t) pjsip_resolver_create( pj_pool_t *pool,
					   pjsip_resolver_t **p_res)
{
    pjsip_resolver_t *resolver;

    PJ_ASSERT_RETURN(pool && p_res, PJ_EINVAL);
    resolver = PJ_POOL_ZALLOC_T(pool, pjsip_resolver_t);
    *p_res = resolver;

    return PJ_SUCCESS;
}


/*
 * Public API to set the DNS resolver instance for the SIP resolver.
 */
PJ_DEF(pj_status_t) pjsip_resolver_set_resolver(pjsip_resolver_t *res,
						pj_dns_resolver *dns_res)
{
#if PJSIP_HAS_RESOLVER
    res->res = dns_res;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(res);
    PJ_UNUSED_ARG(dns_res);
    pj_assert(!"Resolver is disabled (PJSIP_HAS_RESOLVER==0)");
    return PJ_EINVALIDOP;
#endif
}


/*
 * Public API to get the internal DNS resolver.
 */
PJ_DEF(pj_dns_resolver*) pjsip_resolver_get_resolver(pjsip_resolver_t *res)
{
    return res->res;
}


/*
 * Public API to create destroy the resolver
 */
PJ_DEF(void) pjsip_resolver_destroy(pjsip_resolver_t *resolver)
{
    if (resolver->res) {
#if PJSIP_HAS_RESOLVER
	pj_dns_resolver_destroy(resolver->res, PJ_FALSE);
#endif
	resolver->res = NULL;
    }
}

/*
 * Internal:
 *  determine if an address is a valid IP address, and if it is,
 *  return the IP version (4 or 6).
 */
static int get_ip_addr_ver(const pj_str_t *host)
{
    pj_in_addr dummy;
    pj_in6_addr dummy6;

    /* First check with inet_aton() */
    if (pj_inet_aton(host, &dummy) > 0)
	return 4;

    /* Then check if this is an IPv6 address */
    if (pj_inet_pton(pj_AF_INET6(), host, &dummy6) == PJ_SUCCESS)
	return 6;

    /* Not an IP address */
    return 0;
}


/*
 * This is the main function for performing server resolution.
 */
PJ_DEF(void) pjsip_resolve( pjsip_resolver_t *resolver,
			    pj_pool_t *pool,
			    const pjsip_host_info *target,
			    void *token,
			    pjsip_resolver_callback *cb)
{
    pjsip_server_addresses svr_addr;
    pj_status_t status = PJ_SUCCESS;
    int ip_addr_ver;
    struct query *query;
    pjsip_transport_type_e type = target->type;

    /* Is it IP address or hostname? And if it's an IP, which version? */
    ip_addr_ver = get_ip_addr_ver(&target->addr.host);

    /* Set the transport type if not explicitly specified. 
     * RFC 3263 section 4.1 specify rules to set up this.
     */
    if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
	if (ip_addr_ver || (target->addr.port != 0)) {
#if PJ_HAS_TCP
	    if (target->flag & PJSIP_TRANSPORT_SECURE) 
	    {
		type = PJSIP_TRANSPORT_TLS;
	    } else if (target->flag & PJSIP_TRANSPORT_RELIABLE) 
	    {
		type = PJSIP_TRANSPORT_TCP;
	    } else 
#endif
	    {
		type = PJSIP_TRANSPORT_UDP;
	    }
	} else {
	    /* No type or explicit port is specified, and the address is
	     * not IP address.
	     * In this case, full NAPTR resolution must be performed.
	     * But we don't support it (yet).
	     */
#if PJ_HAS_TCP
	    if (target->flag & PJSIP_TRANSPORT_SECURE) 
	    {
		type = PJSIP_TRANSPORT_TLS;
	    } else if (target->flag & PJSIP_TRANSPORT_RELIABLE) 
	    {
		type = PJSIP_TRANSPORT_TCP;
	    } else 
#endif
	    {
		type = PJSIP_TRANSPORT_UDP;
	    }
	}

	/* Add IPv6 flag for IPv6 address */
	if (ip_addr_ver == 6)
	    type = (pjsip_transport_type_e)((int)type + PJSIP_TRANSPORT_IPV6);
    }


    /* If target is an IP address, or if resolver is not configured, 
     * we can just finish the resolution now using pj_gethostbyname()
     */
    if (ip_addr_ver || resolver->res == NULL) {
	char addr_str[PJ_INET6_ADDRSTRLEN+10];
	pj_uint16_t srv_port;

	if (ip_addr_ver != 0) {
	    /* Target is an IP address, no need to resolve */
	    if (ip_addr_ver == 4) {
		pj_sockaddr_init(pj_AF_INET(), &svr_addr.entry[0].addr, 
				 NULL, 0);
		pj_inet_aton(&target->addr.host,
			     &svr_addr.entry[0].addr.ipv4.sin_addr);
	    } else {
		pj_sockaddr_init(pj_AF_INET6(), &svr_addr.entry[0].addr, 
				 NULL, 0);
		pj_inet_pton(pj_AF_INET6(), &target->addr.host,
			     &svr_addr.entry[0].addr.ipv6.sin6_addr);
	    }
	} else {
	    pj_addrinfo ai;
	    unsigned count;
	    int af;

	    PJ_LOG(5,(THIS_FILE,
		      "DNS resolver not available, target '%.*s:%d' type=%s "
		      "will be resolved with getaddrinfo()",
		      target->addr.host.slen,
		      target->addr.host.ptr,
		      target->addr.port,
		      pjsip_transport_get_type_name(target->type)));

	    if (type & PJSIP_TRANSPORT_IPV6) {
		af = pj_AF_INET6();
	    } else {
		af = pj_AF_INET();
	    }

	    /* Resolve */
	    count = 1;
	    status = pj_getaddrinfo(af, &target->addr.host, &count, &ai);
	    if (status != PJ_SUCCESS) {
		/* "Normalize" error to PJ_ERESOLVE. This is a special error
		 * because it will be translated to SIP status 502 by
		 * sip_transaction.c
		 */
		status = PJ_ERESOLVE;
		goto on_error;
	    }

	    svr_addr.entry[0].addr.addr.sa_family = (pj_uint16_t)af;
	    pj_memcpy(&svr_addr.entry[0].addr, &ai.ai_addr,
		      sizeof(pj_sockaddr));
	}

	/* Set the port number */
	if (target->addr.port == 0) {
	   srv_port = (pj_uint16_t)
		      pjsip_transport_get_default_port_for_type(type);
	} else {
	   srv_port = (pj_uint16_t)target->addr.port;
	}
	pj_sockaddr_set_port(&svr_addr.entry[0].addr, srv_port);

	/* Call the callback. */
	PJ_LOG(5,(THIS_FILE, 
		  "Target '%.*s:%d' type=%s resolved to "
		  "'%s' type=%s (%s)",
		  (int)target->addr.host.slen,
		  target->addr.host.ptr,
		  target->addr.port,
		  pjsip_transport_get_type_name(target->type),
		  pj_sockaddr_print(&svr_addr.entry[0].addr, addr_str,
				    sizeof(addr_str), 3),
		  pjsip_transport_get_type_name(type),
		  pjsip_transport_get_type_desc(type)));
	svr_addr.count = 1;
	svr_addr.entry[0].priority = 0;
	svr_addr.entry[0].weight = 0;
	svr_addr.entry[0].type = type;
    	svr_addr.entry[0].addr_len = pj_sockaddr_get_len(&svr_addr.entry[0].addr);
	(*cb)(status, token, &svr_addr, target->addr.port);

	/* Done. */
	return;
    }

    /* Target is not an IP address so we need to resolve it. */
#if PJSIP_HAS_RESOLVER

    /* Build the query state */
    query = PJ_POOL_ZALLOC_T(pool, struct query);
    query->objname = THIS_FILE;
    query->token = token;
    query->cb = cb;
    query->req.target = *target;
    pj_strdup(pool, &query->req.target.addr.host, &target->addr.host);

    /* If port is not specified, start with SRV resolution
     * (should be with NAPTR, but we'll do that later)
     */
    PJ_TODO(SUPPORT_DNS_NAPTR);

    /* Build dummy NAPTR entry */
    query->naptr_cnt = 1;
    pj_bzero(&query->naptr[0], sizeof(query->naptr[0]));
    query->naptr[0].order = 0;
    query->naptr[0].pref = 0;
    query->naptr[0].type = type;
    pj_strdup(pool, &query->naptr[0].name, &target->addr.host);


    /* Start DNS SRV or A resolution, depending on whether port is specified */
    if (target->addr.port == 0) {
	query->query_type = PJ_DNS_TYPE_SRV;

	query->req.def_port = 5060;

	if (type == PJSIP_TRANSPORT_TLS) {
	    query->naptr[0].res_type = pj_str("_sips._tcp.");
	    query->req.def_port = 5061;
	} else if (type == PJSIP_TRANSPORT_TCP)
	    query->naptr[0].res_type = pj_str("_sip._tcp.");
	else if (type == PJSIP_TRANSPORT_UDP)
	    query->naptr[0].res_type = pj_str("_sip._udp.");
	else {
	    pj_assert(!"Unknown transport type");
	    query->naptr[0].res_type = pj_str("_sip._udp.");
	    
	}

    } else {
	/* Otherwise if port is specified, start with A (or AAAA) host 
	 * resolution 
	 */
	query->query_type = PJ_DNS_TYPE_A;
	query->naptr[0].res_type.slen = 0;
	query->req.def_port = target->addr.port;
    }

    /* Start the asynchronous query */
    PJ_LOG(5, (query->objname, 
	       "Starting async DNS %s query: target=%.*s%.*s, transport=%s, "
	       "port=%d",
	       pj_dns_get_type_name(query->query_type),
	       (int)query->naptr[0].res_type.slen,
	       query->naptr[0].res_type.ptr,
	       (int)query->naptr[0].name.slen, query->naptr[0].name.ptr,
	       pjsip_transport_get_type_name(target->type),
	       target->addr.port));

    if (query->query_type == PJ_DNS_TYPE_SRV) {

	status = pj_dns_srv_resolve(&query->naptr[0].name,
				    &query->naptr[0].res_type,
				    query->req.def_port, pool, resolver->res,
				    PJ_TRUE, query, &srv_resolver_cb, NULL);

    } else if (query->query_type == PJ_DNS_TYPE_A) {

	status = pj_dns_resolver_start_query(resolver->res, 
					     &query->naptr[0].name,
					     PJ_DNS_TYPE_A, 0, 
					     &dns_a_callback,
    					     query, &query->object);

    } else {
	pj_assert(!"Unexpected");
	status = PJ_EBUG;
    }

    if (status != PJ_SUCCESS)
	goto on_error;

    return;

#else /* PJSIP_HAS_RESOLVER */
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(query);
    PJ_UNUSED_ARG(srv_name);
#endif /* PJSIP_HAS_RESOLVER */

on_error:
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_LOG(4,(THIS_FILE, "Failed to resolve '%.*s'. Err=%d (%s)",
			     (int)target->addr.host.slen,
			     target->addr.host.ptr,
			     status,
			     pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*cb)(status, token, NULL,  target->addr.port);
	return;
    }
}

#if PJSIP_HAS_RESOLVER

/* 
 * This callback is called when target is resolved with DNS A query.
 */
static void dns_a_callback(void *user_data,
			   pj_status_t status,
			   pj_dns_parsed_packet *pkt)
{
    struct query *query = (struct query*) user_data;
    pjsip_server_addresses srv;
    pj_dns_a_record rec;
    unsigned i;

    rec.addr_count = 0;

    /* Parse the response */
    if (status == PJ_SUCCESS) {
	status = pj_dns_parse_a_response(pkt, &rec);
    }

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];

	/* Log error */
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(4,(query->objname, "DNS A record resolution failed: %s", 
		  errmsg));

	/* Call the callback */
	(*query->cb)(status, query->token, NULL, query->req.def_port);
	return;
    }

    /* Build server addresses and call callback */
    srv.count = 0;
    for (i=0; i<rec.addr_count; ++i) {
	srv.entry[srv.count].type = query->naptr[0].type;
	srv.entry[srv.count].priority = 0;
	srv.entry[srv.count].weight = 0;
	srv.entry[srv.count].addr_len = sizeof(pj_sockaddr_in);
	pj_sockaddr_in_init(&srv.entry[srv.count].addr.ipv4,
			    0, (pj_uint16_t)query->req.def_port);
	srv.entry[srv.count].addr.ipv4.sin_addr.s_addr =
	    rec.addr[i].s_addr;

	++srv.count;
    }

    /* Call the callback */
    (*query->cb)(PJ_SUCCESS, query->token, &srv, query->req.def_port);
}


/* Callback to be called by DNS SRV resolution */
static void srv_resolver_cb(void *user_data,
			    pj_status_t status,
			    const pj_dns_srv_record *rec)
{
    struct query *query = (struct query*) user_data;
    pjsip_server_addresses srv;
    unsigned i;

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];

	/* Log error */
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(4,(query->objname, "DNS A record resolution failed: %s", 
		  errmsg));

	/* Call the callback */
	(*query->cb)(status, query->token, NULL, query->req.def_port);
	return;
    }

    /* Build server addresses and call callback */
    srv.count = 0;
    for (i=0; i<rec->count; ++i) {
	unsigned j;

	for (j=0; j<rec->entry[i].server.addr_count; ++j) {
	    srv.entry[srv.count].type = query->naptr[0].type;
	    srv.entry[srv.count].priority = rec->entry[i].priority;
	    srv.entry[srv.count].weight = rec->entry[i].weight;
	    srv.entry[srv.count].addr_len = sizeof(pj_sockaddr_in);
	    pj_sockaddr_in_init(&srv.entry[srv.count].addr.ipv4,
				0, (pj_uint16_t)rec->entry[i].port);
	    srv.entry[srv.count].addr.ipv4.sin_addr.s_addr =
		rec->entry[i].server.addr[j].s_addr;

	    ++srv.count;
	}
    }

    /* Call the callback */
    (*query->cb)(PJ_SUCCESS, query->token, &srv, query->req.def_port);
}

#endif	/* PJSIP_HAS_RESOLVER */


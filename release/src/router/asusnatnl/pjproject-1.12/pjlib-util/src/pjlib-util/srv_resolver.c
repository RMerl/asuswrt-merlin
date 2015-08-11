/* $Id: srv_resolver.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/srv_resolver.h>
#include <pjlib-util/errno.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "srv_resolver.c"

#define ADDR_MAX_COUNT	    PJ_DNS_MAX_IP_IN_A_REC

struct common
{
    pj_dns_type		     type;	    /**< Type of this structure.*/
};

struct srv_target
{
    struct common	    common;
    pj_dns_srv_async_query *parent;
    pj_str_t		    target_name;
    pj_dns_async_query	   *q_a;
    char		    target_buf[PJ_MAX_HOSTNAME];
    pj_str_t		    cname;
    char		    cname_buf[PJ_MAX_HOSTNAME];
    unsigned		    port;
    unsigned		    priority;
    unsigned		    weight;
    unsigned		    sum;
    unsigned		    addr_cnt;
    pj_in_addr		    addr[ADDR_MAX_COUNT];
};

struct pj_dns_srv_async_query
{
    struct common	     common;
    char		    *objname;

    pj_dns_type		     dns_state;	    /**< DNS type being resolved.   */
    pj_dns_resolver	    *resolver;	    /**< Resolver SIP instance.	    */
    void		    *token;
    pj_dns_async_query	    *q_srv;
    pj_dns_srv_resolver_cb  *cb;
    pj_status_t		     last_error;

    /* Original request: */
    unsigned		     option;
    pj_str_t		     full_name;
    pj_str_t		     domain_part;
    pj_uint16_t		     def_port;

    /* SRV records and their resolved IP addresses: */
    unsigned		     srv_cnt;
    struct srv_target	     srv[PJ_DNS_SRV_MAX_ADDR];

    /* Number of hosts in SRV records that the IP address has been resolved */
    unsigned		     host_resolved;

};


/* Async resolver callback, forward decl. */
static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *pkt);



/*
 * The public API to invoke DNS SRV resolution.
 */
PJ_DEF(pj_status_t) pj_dns_srv_resolve( const pj_str_t *domain_name,
				        const pj_str_t *res_name,
					unsigned def_port,
					pj_pool_t *pool,
					pj_dns_resolver *resolver,
					unsigned option,
					void *token,
					pj_dns_srv_resolver_cb *cb,
					pj_dns_srv_async_query **p_query)
{
    int len;
    pj_str_t target_name;
    pj_dns_srv_async_query *query_job;
    pj_status_t status;

    PJ_ASSERT_RETURN(domain_name && domain_name->slen &&
		     res_name && res_name->slen &&
		     pool && resolver && cb, PJ_EINVAL);

    /* Build full name */
    len = domain_name->slen + res_name->slen + 2;
    target_name.ptr = (char*) pj_pool_alloc(pool, len);
    pj_strcpy(&target_name, res_name);
    if (res_name->ptr[res_name->slen-1] != '.')
	pj_strcat2(&target_name, ".");
    len = target_name.slen;
    pj_strcat(&target_name, domain_name);
    target_name.ptr[target_name.slen] = '\0';


    /* Build the query_job state */
    query_job = PJ_POOL_ZALLOC_T(pool, pj_dns_srv_async_query);
    query_job->common.type = PJ_DNS_TYPE_SRV;
    query_job->objname = target_name.ptr;
    query_job->resolver = resolver;
    query_job->token = token;
    query_job->cb = cb;
    query_job->option = option;
    query_job->full_name = target_name;
    query_job->domain_part.ptr = target_name.ptr + len;
    query_job->domain_part.slen = target_name.slen - len;
    query_job->def_port = (pj_uint16_t)def_port;

    /* Start the asynchronous query_job */

    query_job->dns_state = PJ_DNS_TYPE_SRV;

    PJ_LOG(5, (query_job->objname, 
	       "Starting async DNS %s query_job: target=%.*s:%d",
	       pj_dns_get_type_name(query_job->dns_state),
	       (int)target_name.slen, target_name.ptr,
	       def_port));

    status = pj_dns_resolver_start_query(resolver, &target_name, 
				         query_job->dns_state, 0, 
					 &dns_callback,
    					 query_job, &query_job->q_srv);
    if (status==PJ_SUCCESS && p_query)
	*p_query = query_job;

    return status;
}


/*
 * Cancel pending query.
 */
PJ_DEF(pj_status_t) pj_dns_srv_cancel_query(pj_dns_srv_async_query *query,
					    pj_bool_t notify)
{
    pj_bool_t has_pending = PJ_FALSE;
    unsigned i;

    if (query->q_srv) {
	pj_dns_resolver_cancel_query(query->q_srv, PJ_FALSE);
	query->q_srv = NULL;
	has_pending = PJ_TRUE;
    }

    for (i=0; i<query->srv_cnt; ++i) {
	struct srv_target *srv = &query->srv[i];
	if (srv->q_a) {
	    pj_dns_resolver_cancel_query(srv->q_a, PJ_FALSE);
	    srv->q_a = NULL;
	    has_pending = PJ_TRUE;
	}
    }

    if (has_pending && notify && query->cb) {
	(*query->cb)(query->token, PJ_ECANCELLED, NULL);
    }

    return has_pending? PJ_SUCCESS : PJ_EINVALIDOP;
}


#define SWAP(type,ptr1,ptr2) if (ptr1 != ptr2) { \
				type tmp; \
				pj_memcpy(&tmp, ptr1, sizeof(type)); \
				pj_memcpy(ptr1, ptr2, sizeof(type)); \
				(ptr1)->target_name.ptr = (ptr1)->target_buf;\
				pj_memcpy(ptr2, &tmp, sizeof(type)); \
				(ptr2)->target_name.ptr = (ptr2)->target_buf;\
			     } else {}


/* Build server entries in the query_job based on received SRV response */
static void build_server_entries(pj_dns_srv_async_query *query_job, 
				 pj_dns_parsed_packet *response)
{
    unsigned i;

    /* Save the Resource Records in DNS answer into SRV targets. */
    query_job->srv_cnt = 0;
    for (i=0; i<response->hdr.anscount && 
	      query_job->srv_cnt < PJ_DNS_SRV_MAX_ADDR; ++i) 
    {
	pj_dns_parsed_rr *rr = &response->ans[i];
	struct srv_target *srv = &query_job->srv[query_job->srv_cnt];

	if (rr->type != PJ_DNS_TYPE_SRV) {
	    PJ_LOG(4,(query_job->objname, 
		      "Received non SRV answer for SRV query_job!"));
	    continue;
	}

	if (rr->rdata.srv.target.slen > PJ_MAX_HOSTNAME) {
	    PJ_LOG(4,(query_job->objname, "Hostname is too long!"));
	    continue;
	}

	/* Build the SRV entry for RR */
	pj_bzero(srv, sizeof(*srv));
	srv->target_name.ptr = srv->target_buf;
	pj_strncpy(&srv->target_name, &rr->rdata.srv.target,
		   sizeof(srv->target_buf));
	srv->port = rr->rdata.srv.port;
	srv->priority = rr->rdata.srv.prio;
	srv->weight = rr->rdata.srv.weight;
	
	++query_job->srv_cnt;
    }

    if (query_job->srv_cnt == 0) {
	PJ_LOG(4,(query_job->objname, 
		  "Could not find SRV record in DNS answer!"));
	return;
    }

    /* First pass: 
     *	order the entries based on priority.
     */
    for (i=0; i<query_job->srv_cnt-1; ++i) {
	unsigned min = i, j;
	for (j=i+1; j<query_job->srv_cnt; ++j) {
	    if (query_job->srv[j].priority < query_job->srv[min].priority)
		min = j;
	}
	SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[min]);
    }

    /* Second pass:
     *	pick one host among hosts with the same priority, according
     *	to its weight. The idea is when one server fails, client should
     *	contact the next server with higher priority rather than contacting
     *	server with the same priority as the failed one.
     *
     *  The algorithm for selecting server among servers with the same
     *  priority is described in RFC 2782.
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	unsigned j, count=1, sum;

	/* Calculate running sum for servers with the same priority */
	sum = query_job->srv[i].sum = query_job->srv[i].weight;
	for (j=i+1; j<query_job->srv_cnt && 
		    query_job->srv[j].priority == query_job->srv[i].priority; ++j)
	{
	    sum += query_job->srv[j].weight;
	    query_job->srv[j].sum = sum;
	    ++count;
	}

	if (count > 1) {
	    unsigned r;

	    /* Elect one random number between zero and the total sum of
	     * weight (inclusive).
	     */
	    r = pj_rand() % (sum + 1);

	    /* Select the first server which running sum is greater than or
	     * equal to the random number.
	     */
	    for (j=i; j<i+count; ++j) {
		if (query_job->srv[j].sum >= r)
		    break;
	    }

	    /* Must have selected one! */
	    pj_assert(j != i+count);

	    /* Put this entry in front (of entries with same priority) */
	    SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[j]);

	    /* Remove all other entries (of the same priority) */
	    while (count > 1) {
		pj_array_erase(query_job->srv, sizeof(struct srv_target), 
			       query_job->srv_cnt, i+1);
		--count;
		--query_job->srv_cnt;
	    }
	}
    }

    /* Since we've been moving around SRV entries, update the pointers
     * in target_name.
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	query_job->srv[i].target_name.ptr = query_job->srv[i].target_buf;
    }

    /* Check for Additional Info section if A records are available, and
     * fill in the IP address (so that we won't need to resolve the A 
     * record with another DNS query_job). 
     */
    for (i=0; i<response->hdr.arcount; ++i) {
	pj_dns_parsed_rr *rr = &response->arr[i];
	unsigned j;

	if (rr->type != PJ_DNS_TYPE_A)
	    continue;

	/* Yippeaiyee!! There is an "A" record! 
	 * Update the IP address of the corresponding SRV record.
	 */
	for (j=0; j<query_job->srv_cnt; ++j) {
	    if (pj_stricmp(&rr->name, &query_job->srv[j].target_name)==0) {
		unsigned cnt = query_job->srv[j].addr_cnt;
		query_job->srv[j].addr[cnt].s_addr = rr->rdata.a.ip_addr.s_addr;
		/* Only increment host_resolved once per SRV record */
		if (query_job->srv[j].addr_cnt == 0)
		    ++query_job->host_resolved;
		++query_job->srv[j].addr_cnt;
		break;
	    }
	}

	/* Not valid message; SRV entry might have been deleted in
	 * server selection process.
	 */
	/*
	if (j == query_job->srv_cnt) {
	    PJ_LOG(4,(query_job->objname, 
		      "Received DNS SRV answer with A record, but "
		      "couldn't find matching name (name=%.*s)",
		      (int)rr->name.slen,
		      rr->name.ptr));
	}
	*/
    }

    /* Rescan again the name specified in the SRV record to see if IP
     * address is specified as the target name (unlikely, but well, who 
     * knows..).
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	pj_in_addr addr;

	if (query_job->srv[i].addr_cnt != 0) {
	    /* IP address already resolved */
	    continue;
	}

	if (pj_inet_aton(&query_job->srv[i].target_name, &addr) != 0) {
	    query_job->srv[i].addr[query_job->srv[i].addr_cnt++] = addr;
	    ++query_job->host_resolved;
	}
    }

    /* Print resolved entries to the log */
    PJ_LOG(5,(query_job->objname, 
	      "SRV query_job for %.*s completed, "
	      "%d of %d total entries selected%c",
	      (int)query_job->full_name.slen,
	      query_job->full_name.ptr,
	      query_job->srv_cnt,
	      response->hdr.anscount,
	      (query_job->srv_cnt ? ':' : ' ')));

    for (i=0; i<query_job->srv_cnt; ++i) {
	const char *addr;

	if (query_job->srv[i].addr_cnt != 0)
	    addr = pj_inet_ntoa(query_job->srv[i].addr[0]);
	else
	    addr = "-";

	PJ_LOG(5,(query_job->objname, 
		  " %d: SRV %d %d %d %.*s (%s)",
		  i, query_job->srv[i].priority, 
		  query_job->srv[i].weight, 
		  query_job->srv[i].port, 
		  (int)query_job->srv[i].target_name.slen, 
		  query_job->srv[i].target_name.ptr,
		  addr));
    }
}


/* Start DNS A record queries for all SRV records in the query_job structure */
static pj_status_t resolve_hostnames(pj_dns_srv_async_query *query_job)
{
    unsigned i;
    pj_status_t err=PJ_SUCCESS, status;

    query_job->dns_state = PJ_DNS_TYPE_A;
    for (i=0; i<query_job->srv_cnt; ++i) {
	struct srv_target *srv = &query_job->srv[i];

	PJ_LOG(5, (query_job->objname, 
		   "Starting async DNS A query_job for %.*s",
		   (int)srv->target_name.slen, 
		   srv->target_name.ptr));

	srv->common.type = PJ_DNS_TYPE_A;
	srv->parent = query_job;

	status = pj_dns_resolver_start_query(query_job->resolver,
					     &srv->target_name,
					     PJ_DNS_TYPE_A, 0,
					     &dns_callback,
					     srv, &srv->q_a);
	if (status != PJ_SUCCESS) {
	    query_job->host_resolved++;
	    err = status;
	}
    }
    
    return (query_job->host_resolved == query_job->srv_cnt) ? err : PJ_SUCCESS;
}

/* 
 * This callback is called by PJLIB-UTIL DNS resolver when asynchronous
 * query_job has completed (successfully or with error).
 */
static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *pkt)
{
    struct common *common = (struct common*) user_data;
    pj_dns_srv_async_query *query_job;
    struct srv_target *srv = NULL;
    unsigned i;

    if (common->type == PJ_DNS_TYPE_SRV) {
	query_job = (pj_dns_srv_async_query*) common;
	srv = NULL;
    } else if (common->type == PJ_DNS_TYPE_A) {
	srv = (struct srv_target*) common;
	query_job = srv->parent;
    } else {
	pj_assert(!"Unexpected user data!");
	return;
    }

    /* Proceed to next stage */
    if (query_job->dns_state == PJ_DNS_TYPE_SRV) {

	/* We are getting SRV response */

	query_job->q_srv = NULL;

	if (status == PJ_SUCCESS && pkt->hdr.anscount != 0) {
	    /* Got SRV response, build server entry. If A records are available
	     * in additional records section of the DNS response, save them too.
	     */
	    build_server_entries(query_job, pkt);

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    /* Update query_job last error */
	    query_job->last_error = status;

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query_job->objname, 
		      "DNS SRV resolution failed for %.*s: %s", 
		      (int)query_job->full_name.slen, 
		      query_job->full_name.ptr,
		      errmsg));

	    /* Trigger error when fallback is disabled */
	    if ((query_job->option &
		 (PJ_DNS_SRV_FALLBACK_A | PJ_DNS_SRV_FALLBACK_AAAA)) == 0) 
	    {
		goto on_error;
	    }
	}

	/* If we can't build SRV record, assume the original target is
	 * an A record and resolve with DNS A resolution.
	 */
	if (query_job->srv_cnt == 0) {
	    /* Looks like we aren't getting any SRV responses.
	     * Resolve the original target as A record by creating a 
	     * single "dummy" srv record and start the hostname resolution.
	     */
	    PJ_LOG(4, (query_job->objname, 
		       "DNS SRV resolution failed for %.*s, trying "
		       "resolving A record for %.*s",
		       (int)query_job->full_name.slen, 
		       query_job->full_name.ptr,
		       (int)query_job->domain_part.slen,
		       query_job->domain_part.ptr));

	    /* Create a "dummy" srv record using the original target */
	    i = query_job->srv_cnt++;
	    pj_bzero(&query_job->srv[i], sizeof(query_job->srv[i]));
	    query_job->srv[i].target_name = query_job->domain_part;
	    query_job->srv[i].priority = 0;
	    query_job->srv[i].weight = 0;
	    query_job->srv[i].port = query_job->def_port;
	} 
	

	/* Resolve server hostnames (DNS A record) for hosts which don't have
	 * A record yet.
	 */
	if (query_job->host_resolved != query_job->srv_cnt) {
	    status = resolve_hostnames(query_job);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    /* Must return now. Callback may have been called and query_job
	     * may have been destroyed.
	     */
	    return;
	}

    } else if (query_job->dns_state == PJ_DNS_TYPE_A) {

	/* Clear the outstanding job */
	srv->q_a = NULL;

	/* Check that we really have answer */
	if (status==PJ_SUCCESS && pkt->hdr.anscount != 0) {
	    pj_dns_a_record rec;

	    /* Parse response */
	    status = pj_dns_parse_a_response(pkt, &rec);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    pj_assert(rec.addr_count != 0);

	    /* Update CNAME alias, if present. */
	    if (rec.alias.slen) {
		pj_assert(rec.alias.slen <= (int)sizeof(srv->cname_buf));
		srv->cname.ptr = srv->cname_buf;
		pj_strcpy(&srv->cname, &rec.alias);
	    } else {
		srv->cname.slen = 0;
	    }

	    /* Update IP address of the corresponding hostname or CNAME */
	    if (srv->addr_cnt < ADDR_MAX_COUNT) {
		srv->addr[srv->addr_cnt++].s_addr = rec.addr[0].s_addr;

		PJ_LOG(5,(query_job->objname, 
			  "DNS A for %.*s: %s",
			  (int)srv->target_name.slen, 
			  srv->target_name.ptr,
			  pj_inet_ntoa(rec.addr[0])));
	    }

	    /* Check for multiple IP addresses */
	    for (i=1; i<rec.addr_count && srv->addr_cnt < ADDR_MAX_COUNT; ++i)
	    {
		srv->addr[srv->addr_cnt++].s_addr = rec.addr[i].s_addr;

		PJ_LOG(5,(query_job->objname, 
			  "Additional DNS A for %.*s: %s",
			  (int)srv->target_name.slen, 
			  srv->target_name.ptr,
			  pj_inet_ntoa(rec.addr[i])));
	    }

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    /* Update last error */
	    query_job->last_error = status;

	    /* Log error */
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query_job->objname, "DNS A record resolution failed: %s", 
		      errmsg));
	}

	++query_job->host_resolved;

    } else {
	pj_assert(!"Unexpected state!");
	query_job->last_error = status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Check if all hosts have been resolved */
    if (query_job->host_resolved == query_job->srv_cnt) {
	/* Got all answers, build server addresses */
	pj_dns_srv_record srv_rec;

	srv_rec.count = 0;
	for (i=0; i<query_job->srv_cnt; ++i) {
	    unsigned j;
	    struct srv_target *srv = &query_job->srv[i];

	    srv_rec.entry[srv_rec.count].priority = srv->priority;
	    srv_rec.entry[srv_rec.count].weight = srv->weight;
	    srv_rec.entry[srv_rec.count].port = (pj_uint16_t)srv->port ;

	    srv_rec.entry[srv_rec.count].server.name = srv->target_name;
	    srv_rec.entry[srv_rec.count].server.alias = srv->cname;
	    srv_rec.entry[srv_rec.count].server.addr_count = 0;

	    pj_assert(srv->addr_cnt <= PJ_DNS_MAX_IP_IN_A_REC);

	    for (j=0; j<srv->addr_cnt; ++j) {
		srv_rec.entry[srv_rec.count].server.addr[j].s_addr = 
		    srv->addr[j].s_addr;
		++srv_rec.entry[srv_rec.count].server.addr_count;
	    }

	    if (srv->addr_cnt > 0) {
		++srv_rec.count;
		if (srv_rec.count == PJ_DNS_SRV_MAX_ADDR)
		    break;
	    }
	}

	PJ_LOG(5,(query_job->objname, 
		  "Server resolution complete, %d server entry(s) found",
		  srv_rec.count));


	if (srv_rec.count > 0)
	    status = PJ_SUCCESS;
	else {
	    status = query_job->last_error;
	    if (status == PJ_SUCCESS)
		status = PJLIB_UTIL_EDNSNOANSWERREC;
	}

	/* Call the callback */
	(*query_job->cb)(query_job->token, status, &srv_rec);
    }


    return;

on_error:
    /* Check for failure */
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_UNUSED_ARG(errmsg);
	PJ_LOG(4,(query_job->objname, 
		  "DNS %s record resolution error for '%.*s'."
		  " Err=%d (%s)",
		  pj_dns_get_type_name(query_job->dns_state),
		  (int)query_job->domain_part.slen,
		  query_job->domain_part.ptr,
		  status,
		  pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*query_job->cb)(query_job->token, status, NULL);
	return;
    }
}



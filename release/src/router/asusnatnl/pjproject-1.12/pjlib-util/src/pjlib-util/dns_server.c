/* $Id: dns_server.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/dns_server.h>
#include <pjlib-util/errno.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE   "dns_server.c"
#define MAX_ANS	    16
#define MAX_PKT	    1500
#define MAX_LABEL   32

struct label_tab
{
    unsigned count;

    struct {
	unsigned pos;
	pj_str_t label;
    } a[MAX_LABEL];
};

struct rr
{
    PJ_DECL_LIST_MEMBER(struct rr);
    pj_dns_parsed_rr	rec;
};


struct pj_dns_server
{
    pj_pool_t		*pool;
    pj_pool_factory	*pf;
    pj_activesock_t	*asock;
    pj_ioqueue_op_key_t	 send_key;
    struct rr		 rr_list;
};


static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);


PJ_DEF(pj_status_t) pj_dns_server_create( pj_pool_factory *pf,
				          pj_ioqueue_t *ioqueue,
					  int af,
					  unsigned port,
					  unsigned flags,
				          pj_dns_server **p_srv)
{
    pj_pool_t *pool;
    pj_dns_server *srv;
    pj_sockaddr sock_addr;
    pj_activesock_cb sock_cb;
    pj_status_t status;

    PJ_ASSERT_RETURN(pf && ioqueue && p_srv && flags==0, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==pj_AF_INET() || af==pj_AF_INET6(), PJ_EINVAL);
    
    pool = pj_pool_create(pf, "dnsserver", 256, 256, NULL);
    srv = (pj_dns_server*) PJ_POOL_ZALLOC_T(pool, pj_dns_server);
    srv->pool = pool;
    srv->pf = pf;
    pj_list_init(&srv->rr_list);

    pj_bzero(&sock_addr, sizeof(sock_addr));
    sock_addr.addr.sa_family = (pj_uint16_t)af;
    pj_sockaddr_set_port(&sock_addr, (pj_uint16_t)port);
    
    pj_bzero(&sock_cb, sizeof(sock_cb));
    sock_cb.on_data_recvfrom = &on_data_recvfrom;

    status = pj_activesock_create_udp(pool, &sock_addr, NULL, ioqueue,
				      &sock_cb, srv, &srv->asock, NULL);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&srv->send_key, sizeof(srv->send_key));

    status = pj_activesock_start_recvfrom(srv->asock, pool, MAX_PKT, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    *p_srv = srv;
    return PJ_SUCCESS;

on_error:
    pj_dns_server_destroy(srv);
    return status;
}


PJ_DEF(pj_status_t) pj_dns_server_destroy(pj_dns_server *srv)
{
    PJ_ASSERT_RETURN(srv, PJ_EINVAL);

    if (srv->asock) {
	pj_activesock_close(srv->asock);
	srv->asock = NULL;
    }

    if (srv->pool) {
	pj_pool_t *pool = srv->pool;
	srv->pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


static struct rr* find_rr( pj_dns_server *srv,
			   unsigned dns_class,
			   unsigned type	/* pj_dns_type */,
			   const pj_str_t *name)
{
    struct rr *r;

    r = srv->rr_list.next;
    while (r != &srv->rr_list) {
	if (r->rec.dnsclass == dns_class && r->rec.type == type && 
	    pj_stricmp(&r->rec.name, name)==0)
	{
	    return r;
	}
	r = r->next;
    }

    return NULL;
}


PJ_DEF(pj_status_t) pj_dns_server_add_rec( pj_dns_server *srv,
					   unsigned count,
					   const pj_dns_parsed_rr rr_param[])
{
    unsigned i;

    PJ_ASSERT_RETURN(srv && count && rr_param, PJ_EINVAL);

    for (i=0; i<count; ++i) {
	struct rr *rr;

	PJ_ASSERT_RETURN(find_rr(srv, rr_param[i].dnsclass, rr_param[i].type,
				 &rr_param[i].name) == NULL,
			 PJ_EEXISTS);

	rr = (struct rr*) PJ_POOL_ZALLOC_T(srv->pool, struct rr);
	pj_memcpy(&rr->rec, &rr_param[i], sizeof(pj_dns_parsed_rr));

	pj_list_push_back(&srv->rr_list, rr);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_dns_server_del_rec( pj_dns_server *srv,
					   int dns_class,
					   pj_dns_type type,
					   const pj_str_t *name)
{
    struct rr *rr;

    PJ_ASSERT_RETURN(srv && type && name, PJ_EINVAL);

    rr = find_rr(srv, dns_class, type, name);
	if (!rr) {
		PJ_LOG(4, ("dns_server.c", "pj_dns_server_del_rec() dns server not found."));
		return PJ_ENOTFOUND;
	}

    pj_list_erase(rr);

    return PJ_SUCCESS;
}


static void write16(pj_uint8_t *p, pj_uint16_t val)
{
    p[0] = (pj_uint8_t)(val >> 8);
    p[1] = (pj_uint8_t)(val & 0xFF);
}

static void write32(pj_uint8_t *p, pj_uint32_t val)
{
    val = pj_htonl(val);
    pj_memcpy(p, &val, 4);
}

static int print_name(pj_uint8_t *pkt, int size,
		      pj_uint8_t *pos, const pj_str_t *name,
		      struct label_tab *tab)
{
    pj_uint8_t *p = pos;
    const char *endlabel, *endname;
    unsigned i;
    pj_str_t label;

    /* Check if name is in the table */
    for (i=0; i<tab->count; ++i) {
	if (pj_strcmp(&tab->a[i].label, name)==0)
	    break;
    }

    if (i != tab->count) {
	write16(p, (pj_uint16_t)(tab->a[i].pos | (0xc0 << 8)));
	return 2;
    } else {
	if (tab->count < MAX_LABEL) {
	    tab->a[tab->count].pos = (p-pkt);
	    tab->a[tab->count].label.ptr = (char*)(p+1);
	    tab->a[tab->count].label.slen = name->slen;
	    ++tab->count;
	}
    }

    endlabel = name->ptr;
    endname = name->ptr + name->slen;

    label.ptr = (char*)name->ptr;

    while (endlabel != endname) {

	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;

	label.slen = (endlabel - label.ptr);

	if (size < label.slen+1)
	    return -1;

	*p = (pj_uint8_t)label.slen;
	pj_memcpy(p+1, label.ptr, label.slen);

	size -= (label.slen+1);
	p += (label.slen+1);

	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	label.ptr = (char*)endlabel;
    }

    if (size == 0)
	return -1;

    *p++ = '\0';

    return p-pos;
}

static int print_rr(pj_uint8_t *pkt, int size, pj_uint8_t *pos,
		    const pj_dns_parsed_rr *rr, struct label_tab *tab)
{
    pj_uint8_t *p = pos;
    int len;

    len = print_name(pkt, size, pos, &rr->name, tab);
    if (len < 0)
	return -1;

    p += len;
    size -= len;

    if (size < 8)
	return -1;

    pj_assert(rr->dnsclass == 1);

    write16(p+0, (pj_uint16_t)rr->type);	/* type	    */
    write16(p+2, (pj_uint16_t)rr->dnsclass);	/* class    */
    write32(p+4, rr->ttl);			/* TTL	    */

    p += 8;
    size -= 8;

    if (rr->type == PJ_DNS_TYPE_A) {

	if (size < 6)
	    return -1;

	/* RDLEN is 4 */
	write16(p, 4);

	/* Address */
	pj_memcpy(p+2, &rr->rdata.a.ip_addr, 4);

	p += 6;
	size -= 6;

    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
	       rr->type == PJ_DNS_TYPE_NS ||
	       rr->type == PJ_DNS_TYPE_PTR) {

	if (size < 4)
	    return -1;

	len = print_name(pkt, size-2, p+2, &rr->rdata.cname.name, tab);
	if (len < 0)
	    return -1;

	write16(p, (pj_uint16_t)len);

	p += (len + 2);
	size -= (len + 2);

    } else if (rr->type == PJ_DNS_TYPE_SRV) {

	if (size < 10)
	    return -1;

	write16(p+2, rr->rdata.srv.prio);   /* Priority */
	write16(p+4, rr->rdata.srv.weight); /* Weight */
	write16(p+6, rr->rdata.srv.port);   /* Port */

	/* Target */
	len = print_name(pkt, size-8, p+8, &rr->rdata.srv.target, tab);
	if (len < 0)
	    return -1;

	/* RDLEN */
	write16(p, (pj_uint16_t)(len + 6));

	p += (len + 8);
	size -= (len + 8);

    } else {
	pj_assert(!"Not supported");
	return -1;
    }

    return p-pos;
}

static int print_packet(const pj_dns_parsed_packet *rec, pj_uint8_t *pkt,
			int size)
{
    pj_uint8_t *p = pkt;
    struct label_tab tab;
    int i, len;

    tab.count = 0;

    pj_assert(sizeof(pj_dns_hdr)==12);
    if (size < (int)sizeof(pj_dns_hdr))
	return -1;

    /* Initialize header */
    write16(p+0,  rec->hdr.id);
    write16(p+2,  rec->hdr.flags);
    write16(p+4,  rec->hdr.qdcount);
    write16(p+6,  rec->hdr.anscount);
    write16(p+8,  rec->hdr.nscount);
    write16(p+10, rec->hdr.arcount);

    p = pkt + sizeof(pj_dns_hdr);
    size -= sizeof(pj_dns_hdr);

    /* Print queries */
    for (i=0; i<rec->hdr.qdcount; ++i) {

	len = print_name(pkt, size, p, &rec->q[i].name, &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;

	if (size < 4)
	    return -1;

	/* Set type */
	write16(p+0, (pj_uint16_t)rec->q[i].type);

	/* Set class (IN=1) */
	pj_assert(rec->q[i].dnsclass == 1);
	write16(p+2, rec->q[i].dnsclass);

	p += 4;
    }

    /* Print answers */
    for (i=0; i<rec->hdr.anscount; ++i) {
	len = print_rr(pkt, size, p, &rec->ans[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    /* Print NS records */
    for (i=0; i<rec->hdr.nscount; ++i) {
	len = print_rr(pkt, size, p, &rec->ns[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    /* Print additional records */
    for (i=0; i<rec->hdr.arcount; ++i) {
	len = print_rr(pkt, size, p, &rec->arr[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    return p - pkt;
}


static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status)
{
    pj_dns_server *srv;
    pj_pool_t *pool;
    pj_dns_parsed_packet *req;
    pj_dns_parsed_packet ans;
    struct rr *rr;
    pj_ssize_t pkt_len;
    unsigned i;

    if (status != PJ_SUCCESS)
	return PJ_TRUE;

    srv = (pj_dns_server*) pj_activesock_get_user_data(asock);
    pool = pj_pool_create(srv->pf, "dnssrvrx", 512, 256, NULL);

    status = pj_dns_parse_packet(pool, data, size, &req);
    if (status != PJ_SUCCESS) {
	char addrinfo[PJ_INET6_ADDRSTRLEN+10];
	pj_sockaddr_print(src_addr, addrinfo, sizeof(addrinfo), 3);
	PJ_LOG(4,(THIS_FILE, "Error parsing query from %s", addrinfo));
	goto on_return;
    }

    /* Init answer */
    pj_bzero(&ans, sizeof(ans));
    ans.hdr.id = req->hdr.id;
    ans.hdr.qdcount = 1;
    ans.q = (pj_dns_parsed_query*) PJ_POOL_ALLOC_T(pool, pj_dns_parsed_query);
    pj_memcpy(ans.q, req->q, sizeof(pj_dns_parsed_query));

    if (req->hdr.qdcount != 1) {
	ans.hdr.flags = PJ_DNS_SET_RCODE(PJ_DNS_RCODE_FORMERR);
	goto send_pkt;
    }

    if (req->q[0].dnsclass != PJ_DNS_CLASS_IN) {
	ans.hdr.flags = PJ_DNS_SET_RCODE(PJ_DNS_RCODE_NOTIMPL);
	goto send_pkt;
    }

    /* Find the record */
    rr = find_rr(srv, req->q->dnsclass, req->q->type, &req->q->name);
    if (rr == NULL) {
	ans.hdr.flags = PJ_DNS_SET_RCODE(PJ_DNS_RCODE_NXDOMAIN);
	goto send_pkt;
    }

    /* Init answer record */
    ans.hdr.anscount = 0;
    ans.ans = (pj_dns_parsed_rr*)
	      pj_pool_calloc(pool, MAX_ANS, sizeof(pj_dns_parsed_rr));

    /* DNS SRV query needs special treatment since it returns multiple
     * records
     */
    if (req->q->type == PJ_DNS_TYPE_SRV) {
	struct rr *r;

	r = srv->rr_list.next;
	while (r != &srv->rr_list) {
	    if (r->rec.dnsclass == req->q->dnsclass && 
		r->rec.type == PJ_DNS_TYPE_SRV && 
		pj_stricmp(&r->rec.name, &req->q->name)==0 &&
		ans.hdr.anscount < MAX_ANS)
	    {
		pj_memcpy(&ans.ans[ans.hdr.anscount], &r->rec,
			  sizeof(pj_dns_parsed_rr));
		++ans.hdr.anscount;
	    }
	    r = r->next;
	}
    } else {
	/* Otherwise just copy directly from the server record */
	pj_memcpy(&ans.ans[ans.hdr.anscount], &rr->rec,
			  sizeof(pj_dns_parsed_rr));
	++ans.hdr.anscount;
    }

    /* For each CNAME entry, add A entry */
    for (i=0; i<ans.hdr.anscount && ans.hdr.anscount < MAX_ANS; ++i) {
	if (ans.ans[i].type == PJ_DNS_TYPE_CNAME) {
	    struct rr *r;

	    r = find_rr(srv, ans.ans[i].dnsclass, PJ_DNS_TYPE_A,
		        &ans.ans[i].name);
	    pj_memcpy(&ans.ans[ans.hdr.anscount], &r->rec,
			      sizeof(pj_dns_parsed_rr));
	    ++ans.hdr.anscount;
	}
    }

send_pkt:
    pkt_len = print_packet(&ans, (pj_uint8_t*)data, MAX_PKT);
    if (pkt_len < 1) {
	PJ_LOG(4,(THIS_FILE, "Error: answer too large"));
	goto on_return;
    }

    status = pj_activesock_sendto(srv->asock, &srv->send_key, data, &pkt_len,
				  0, src_addr, addr_len);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	PJ_LOG(4,(THIS_FILE, "Error sending answer, status=%d", status));
	goto on_return;
    }

on_return:
    pj_pool_release(pool);
    return PJ_TRUE;
}


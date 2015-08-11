/* $Id: resolver_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util.h>
#include <pjlib.h>
#include "test.h"


#define THIS_FILE   "srv_resolver_test.c"

////////////////////////////////////////////////////////////////////////////
/*
 * TODO: create various invalid DNS packets.
 */


////////////////////////////////////////////////////////////////////////////


#define ACTION_REPLY	0
#define ACTION_IGNORE	-1
#define ACTION_CB	-2

static struct server_t
{
    pj_sock_t	     sock;
    pj_uint16_t	     port;
    pj_thread_t	    *thread;

    /* Action:
     *	0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int		    action;

    pj_dns_parsed_packet    resp;
    void		  (*action_cb)(const pj_dns_parsed_packet *pkt,
				       pj_dns_parsed_packet **p_res);

    unsigned	    pkt_count;

} g_server[2];

static pj_pool_t *pool;
static pj_dns_resolver *resolver;
static pj_bool_t thread_quit;
static pj_timer_heap_t *timer_heap;
static pj_ioqueue_t *ioqueue;
static pj_thread_t *poll_thread;
static pj_sem_t *sem;
static pj_dns_settings set;

#define MAX_LABEL   32

struct label_tab
{
    unsigned count;

    struct {
	unsigned pos;
	pj_str_t label;
    } a[MAX_LABEL];
};

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

#if 0
    pj_enter_critical_section();
    PJ_LOG(3,(THIS_FILE, "Sending response:"));
    pj_dns_dump_packet(rec);
    pj_leave_critical_section();
#endif

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


static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;

    while (!thread_quit) {
	pj_fd_set_t rset;
	pj_time_val timeout = {0, 500};
	pj_sockaddr_in src_addr;
	pj_dns_parsed_packet *req;
	char pkt[1024];
	pj_ssize_t pkt_len;
	int rc, src_len;

	PJ_FD_ZERO(&rset);
	PJ_FD_SET(srv->sock, &rset);

	rc = pj_sock_select(srv->sock+1, &rset, NULL, NULL, &timeout);
	if (rc != 1)
	    continue;

	src_len = sizeof(src_addr);
	pkt_len = sizeof(pkt);
	rc = pj_sock_recvfrom(srv->sock, pkt, &pkt_len, 0, 
			      &src_addr, &src_len);
	if (rc != 0) {
	    app_perror("Server error receiving packet", rc);
	    continue;
	}

	PJ_LOG(5,(THIS_FILE, "Server %d processing packet", srv - &g_server[0]));
	srv->pkt_count++;

	rc = pj_dns_parse_packet(pool, pkt, pkt_len, &req);
	if (rc != PJ_SUCCESS) {
	    app_perror("server error parsing packet", rc);
	    continue;
	}

	/* Verify packet */
	pj_assert(req->hdr.qdcount == 1);
	pj_assert(req->q[0].dnsclass == 1);

	/* Simulate network RTT */
	pj_thread_sleep(50);

	if (srv->action == ACTION_IGNORE) {
	    continue;
	} else if (srv->action == ACTION_REPLY) {
	    srv->resp.hdr.id = req->hdr.id;
	    pkt_len = print_packet(&srv->resp, (pj_uint8_t*)pkt, sizeof(pkt));
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action == ACTION_CB) {
	    pj_dns_parsed_packet *resp;
	    (*srv->action_cb)(req, &resp);
	    resp->hdr.id = req->hdr.id;
	    pkt_len = print_packet(resp, (pj_uint8_t*)pkt, sizeof(pkt));
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action > 0) {
	    req->hdr.flags |= PJ_DNS_SET_RCODE(srv->action);
	    pkt_len = print_packet(req, (pj_uint8_t*)pkt, sizeof(pkt));
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	}
    }

    return 0;
}

static int poll_worker_thread(void *p)
{
    PJ_UNUSED_ARG(p);

    while (!thread_quit) {
	pj_time_val delay = {0, 100};
	pj_timer_heap_poll(timer_heap, NULL);
	pj_ioqueue_poll(ioqueue, &delay);
    }

    return 0;
}

static void destroy(void);

static int init(void)
{
    pj_status_t status;
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    int i;

    nameservers[0] = pj_str("127.0.0.1");
    ports[0] = 5553;
    nameservers[1] = pj_str("127.0.0.1");
    ports[1] = 5554;

    g_server[0].port = ports[0];
    g_server[1].port = ports[1];

    pool = pj_pool_create(mem, NULL, 2000, 2000, NULL);

    status = pj_sem_create(pool, NULL, 0, 2, &sem);
    pj_assert(status == PJ_SUCCESS);

    for (i=0; i<2; ++i) {
	pj_sockaddr_in addr;

	status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &g_server[i].sock);
	if (status != PJ_SUCCESS)
	    return -10;

	pj_sockaddr_in_init(&addr, NULL, (pj_uint16_t)g_server[i].port);

	status = pj_sock_bind(g_server[i].sock, &addr, sizeof(addr));
	if (status != PJ_SUCCESS)
	    return -20;

	status = pj_thread_create(pool, NULL, &server_thread, &g_server[i],
				  0, 0, &g_server[i].thread);
	if (status != PJ_SUCCESS)
	    return -30;
    }

    status = pj_timer_heap_create(pool, 16, &timer_heap);
    pj_assert(status == PJ_SUCCESS);

    status = pj_ioqueue_create(pool, 16, &ioqueue);
    pj_assert(status == PJ_SUCCESS);

    status = pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue, &resolver);
    if (status != PJ_SUCCESS)
	return -40;

    pj_dns_resolver_get_settings(resolver, &set);
    set.good_ns_ttl = 20;
    set.bad_ns_ttl = 20;
    pj_dns_resolver_set_settings(resolver, &set);

    status = pj_dns_resolver_set_ns(resolver, 2, nameservers, ports);
    pj_assert(status == PJ_SUCCESS);

    status = pj_thread_create(pool, NULL, &poll_worker_thread, NULL, 0, 0, &poll_thread);
    pj_assert(status == PJ_SUCCESS);

    return 0;
}


static void destroy(void)
{
    int i;

    thread_quit = PJ_TRUE;

    for (i=0; i<2; ++i) {
	pj_thread_join(g_server[i].thread);
	pj_sock_close(g_server[i].sock);
    }

    pj_thread_join(poll_thread);

    pj_dns_resolver_destroy(resolver, PJ_FALSE);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);

    pj_sem_destroy(sem);
    pj_pool_release(pool);
}


////////////////////////////////////////////////////////////////////////////
/* DNS A parser tests */
static int a_parser_test(void)
{
    pj_dns_parsed_packet pkt;
    pj_dns_a_record rec;
    pj_status_t rc;

    PJ_LOG(3,(THIS_FILE, "  DNS A record parser tests"));

    pkt.q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    pkt.ans = (pj_dns_parsed_rr*)
	      pj_pool_calloc(pool, 32, sizeof(pj_dns_parsed_rr));

    /* Simple answer with direct A record, but with addition of
     * a CNAME and another A to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    A RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 3;

    /* This is the RR corresponding to the query */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x01020304;

    /* CNAME to confuse the parser */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("bhost");

    /* DNS A RR to confuse the parser */
    pkt.ans[2].name = pj_str("bhost");
    pkt.ans[2].type = PJ_DNS_TYPE_A;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.a.ip_addr.s_addr = 0x0203;


    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJ_SUCCESS);
    pj_assert(pj_strcmp2(&rec.name, "ahost")==0);
    pj_assert(rec.alias.slen == 0);
    pj_assert(rec.addr_count == 1);
    pj_assert(rec.addr[0].s_addr == 0x01020304);

    /* Answer with the target corresponds to a CNAME entry, but not
     * as the first record, and with additions of some CNAME and A
     * entries to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 4;

    /* This is the DNS A record for the alias */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    /* CNAME entry corresponding to the query */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("ahostalias");

    /* Another CNAME to confuse the parser */
    pkt.ans[2].name = pj_str("ahost");
    pkt.ans[2].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.cname.name = pj_str("ahostalias2");

    /* Another DNS A to confuse the parser */
    pkt.ans[3].name = pj_str("ahostalias2");
    pkt.ans[3].type = PJ_DNS_TYPE_A;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.a.ip_addr.s_addr = 0x03030303;

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJ_SUCCESS);
    pj_assert(pj_strcmp2(&rec.name, "ahost")==0);
    pj_assert(pj_strcmp2(&rec.alias, "ahostalias")==0);
    pj_assert(rec.addr_count == 1);
    pj_assert(rec.addr[0].s_addr == 0x02020202);

    /*
     * No query section.
     */
    PJ_LOG(3,(THIS_FILE, "    No query section"));
    pkt.hdr.qdcount = 0;
    pkt.hdr.anscount = 0;

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJLIB_UTIL_EDNSINANSWER);

    /*
     * No answer section.
     */
    PJ_LOG(3,(THIS_FILE, "    No answer section"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 0;

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJLIB_UTIL_EDNSNOANSWERREC);

    /*
     * Answer doesn't match query.
     */
    PJ_LOG(3,(THIS_FILE, "    Answer doesn't match query"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* An answer that doesn't match the query */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJLIB_UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (1)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJLIB_UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (2)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 2;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    /* DNS A record, but the name doesn't match */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_A;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.a.ip_addr.s_addr = 0x01020304;

    rc = pj_dns_parse_a_response(&pkt, &rec);
    pj_assert(rc == PJLIB_UTIL_EDNSNOANSWERREC);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Simple DNS test */
#define IP_ADDR0    0x00010203

static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(resp, return);
    PJ_ASSERT_ON_FAIL(resp->hdr.anscount == 1, return);
    PJ_ASSERT_ON_FAIL(resp->ans[0].type == PJ_DNS_TYPE_A, return);
    PJ_ASSERT_ON_FAIL(resp->ans[0].rdata.a.ip_addr.s_addr == IP_ADDR0, return);

}


static int simple_test(void)
{
    pj_str_t name = pj_str("helloworld");
    pj_dns_parsed_packet *r;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  simple successful test"));

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = ACTION_REPLY;
    r = &g_server[0].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    g_server[1].action = ACTION_REPLY;
    r = &g_server[1].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);


    /* Both servers must get packet */
    pj_assert(g_server[0].pkt_count == 1);
    pj_assert(g_server[1].pkt_count == 1);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* DNS nameserver fail-over test */

static void dns_callback_1b(void *user_data,
			    pj_status_t status,
			    pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(resp);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status==PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN),
		      return);
}




/* DNS test */
static int dns_test(void)
{
    pj_str_t name = pj_str("name00");
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  simple error response test"));

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Now only server 0 should get packet, since both servers are
     * in STATE_ACTIVE state
     */
    pj_assert((g_server[0].pkt_count == 1 && g_server[1].pkt_count == 0) ||
	      (g_server[1].pkt_count == 1 && g_server[0].pkt_count == 0));

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "  waiting for active NS to expire (%d sec)",
			 set.good_ns_ttl));
    pj_thread_sleep(set.good_ns_ttl * 1000);

    /* 
     * Fail-over test 
     */
    PJ_LOG(3,(THIS_FILE, "  failing server0"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name01");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);

    /*
     * Check that both servers still receive requests, since they are
     * in probing state.
     */
    PJ_LOG(3,(THIS_FILE, "  checking both NS during probing period"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name02");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(set.qretr_delay *  set.qretr_count);

    /* Both servers must get requests */
    pj_assert(g_server[0].pkt_count >= 1);
    pj_assert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "  waiting for probing state to end (%d sec)",
			 set.qretr_delay * 
			 (set.qretr_count+2) / 1000));
    pj_thread_sleep(set.qretr_delay * (set.qretr_count + 2));


    /*
     * Now only server 1 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "  verifying only good NS is used"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name03");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Both servers must get requests */
    pj_assert(g_server[0].pkt_count == 0);
    pj_assert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "  waiting for active NS to expire (%d sec)",
			 set.good_ns_ttl));
    pj_thread_sleep(set.good_ns_ttl * 1000);

    /*
     * Now fail server 1 to switch to server 0
     */
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name04");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "  waiting for probing state (%d sec)",
			 set.qretr_delay * (set.qretr_count+2) / 1000));
    pj_thread_sleep(set.qretr_delay * (set.qretr_count + 2));

    /*
     * Now only server 0 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "  verifying good NS"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name05");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Only good NS should get request */
    pj_assert(g_server[0].pkt_count == 1);
    pj_assert(g_server[1].pkt_count == 0);


    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Resolver test, normal, with CNAME */
#define IP_ADDR1    0x02030405
#define PORT1	    50061

static void action1_1(const pj_dns_parsed_packet *pkt,
		      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    char *target = "sip.somedomain.com";

    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    if (res->q == NULL) {
	res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }
    if (res->ans == NULL) {
	res->ans = (pj_dns_parsed_rr*) 
		  pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));
    }

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

	pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp.somedomain.com")==0);

	res->hdr.anscount = 1;
	res->ans[0].type = PJ_DNS_TYPE_SRV;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.srv.prio = 1;
	res->ans[0].rdata.srv.weight = 2;
	res->ans[0].rdata.srv.port = PORT1;
	res->ans[0].rdata.srv.target = pj_str(target);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
	char *alias = "sipalias.somedomain.com";

	pj_assert(pj_strcmp2(&res->q[0].name, target)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = PJ_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].ttl = 1000;	/* resolver should select minimum TTL */
	res->ans[0].name = res->q[0].name;
	res->ans[0].rdata.cname.name = pj_str(alias);

	res->ans[1].type = PJ_DNS_TYPE_A;
	res->ans[1].dnsclass = 1;
	res->ans[1].ttl = 1;
	res->ans[1].name = pj_str(alias);
	res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR1;
    }

    *p_res = res;
}

static void srv_cb_1(void *user_data,
		     pj_status_t status,
		     const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(rec->count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
		      return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
		      return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].s_addr == IP_ADDR1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    
}

static void srv_cb_1b(void *user_data,
		      pj_status_t status,
		      const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status==PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN),
		      return);
    PJ_ASSERT_ON_FAIL(rec->count == 0, return);
}

static int srv_resolver_test(void)
{
    pj_status_t status;
    pj_str_t domain = pj_str("somedomain.com");
    pj_str_t res_name = pj_str("_sip._udp.");

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): success scenario"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1, NULL);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);

    /* Because of previous tests, only NS 1 should get the request */
    pj_assert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    pj_assert(g_server[1].pkt_count == 0);


    /* Wait until cache expires and nameserver state moves out from STATE_PROBING */
    PJ_LOG(3,(THIS_FILE, "  waiting for cache to expire (~15 secs).."));
    pj_thread_sleep(1000 + 
		    ((set.qretr_count + 2) * set.qretr_delay));

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): parallel queries"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1, NULL);
    pj_assert(status == PJ_SUCCESS);


    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1, NULL);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);
    pj_sem_wait(sem);

    /* Only server one should get a query */
    pj_assert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    pj_assert(g_server[1].pkt_count == 0);

    /* Since TTL is one, subsequent queries should fail */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): cache expires scenario"));


    pj_thread_sleep(1000);

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1b, NULL);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Fallback because there's no SRV in answer */
#define TARGET	    "domain2.com"
#define IP_ADDR2    0x02030405
#define PORT2	    50062

static void action2_1(const pj_dns_parsed_packet *pkt,
		      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;

    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    res->ans = (pj_dns_parsed_rr*) 
	       pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

	pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp." TARGET)==0);

	res->hdr.anscount = 1;
	res->ans[0].type = PJ_DNS_TYPE_A;    // <-- this will cause the fallback
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.srv.prio = 1;
	res->ans[0].rdata.srv.weight = 2;
	res->ans[0].rdata.srv.port = PORT2;
	res->ans[0].rdata.srv.target = pj_str("sip01." TARGET);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
	char *alias = "sipalias01." TARGET;

	pj_assert(pj_strcmp2(&res->q[0].name, TARGET)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = PJ_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.cname.name = pj_str(alias);

	res->ans[1].type = PJ_DNS_TYPE_A;
	res->ans[1].dnsclass = 1;
	res->ans[1].name = pj_str(alias);
	res->ans[1].ttl = 1;
	res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR2;
    }

    *p_res = res;
}

static void srv_cb_2(void *user_data,
		     pj_status_t status,
		     const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(rec->count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].priority == 0, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].weight == 0, return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.name, TARGET)==0,
		      return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.alias, "sipalias01." TARGET)==0,
		      return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].s_addr == IP_ADDR2, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].port == PORT2, return);
}

static int srv_resolver_fallback_test(void)
{
    pj_status_t status;
    pj_str_t domain = pj_str(TARGET);
    pj_str_t res_name = pj_str("_sip._udp.");

    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): fallback test"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    status = pj_dns_srv_resolve(&domain, &res_name, PORT2, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_2, NULL);
    if (status != PJ_SUCCESS) {
	app_perror("   srv_resolve error", status);
	pj_assert(status == PJ_SUCCESS);
    }

    pj_sem_wait(sem);

    /* Subsequent query should just get the response from the cache */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): cache test"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, PORT2, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_2, NULL);
    if (status != PJ_SUCCESS) {
	app_perror("   srv_resolve error", status);
	pj_assert(status == PJ_SUCCESS);
    }

    pj_sem_wait(sem);

    pj_assert(g_server[0].pkt_count == 0);
    pj_assert(g_server[1].pkt_count == 0);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Too many SRV or A entries */
#define DOMAIN3	    "d3"
#define SRV_COUNT3  (PJ_DNS_SRV_MAX_ADDR+1)
#define A_COUNT3    (PJ_DNS_MAX_IP_IN_A_REC+1)
#define PORT3	    50063
#define IP_ADDR3    0x03030303

static void action3_1(const pj_dns_parsed_packet *pkt,
		      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    unsigned i;

    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    if (res->q == NULL) {
	res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

	pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp." DOMAIN3)==0);

	res->hdr.anscount = SRV_COUNT3;
	res->ans = (pj_dns_parsed_rr*) 
		   pj_pool_calloc(pool, SRV_COUNT3, sizeof(pj_dns_parsed_rr));

	for (i=0; i<SRV_COUNT3; ++i) {
	    char *target;

	    res->ans[i].type = PJ_DNS_TYPE_SRV;
	    res->ans[i].dnsclass = 1;
	    res->ans[i].name = res->q[0].name;
	    res->ans[i].ttl = 1;
	    res->ans[i].rdata.srv.prio = (pj_uint16_t)i;
	    res->ans[i].rdata.srv.weight = 2;
	    res->ans[i].rdata.srv.port = (pj_uint16_t)(PORT3+i);

	    target = (char*)pj_pool_alloc(pool, 16);
	    sprintf(target, "sip%02d." DOMAIN3, i);
	    res->ans[i].rdata.srv.target = pj_str(target);
	}

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {

	//pj_assert(pj_strcmp2(&res->q[0].name, "sip." DOMAIN3)==0);

	res->hdr.anscount = A_COUNT3;
	res->ans = (pj_dns_parsed_rr*) 
		   pj_pool_calloc(pool, A_COUNT3, sizeof(pj_dns_parsed_rr));

	for (i=0; i<A_COUNT3; ++i) {
	    res->ans[i].type = PJ_DNS_TYPE_A;
	    res->ans[i].dnsclass = 1;
	    res->ans[i].ttl = 1;
	    res->ans[i].name = res->q[0].name;
	    res->ans[i].rdata.a.ip_addr.s_addr = IP_ADDR3+i;
	}
    }

    *p_res = res;
}

static void srv_cb_3(void *user_data,
		     pj_status_t status,
		     const pj_dns_srv_record *rec)
{
    unsigned i;

    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(rec);

    pj_assert(status == PJ_SUCCESS);
    pj_assert(rec->count == PJ_DNS_SRV_MAX_ADDR);

    for (i=0; i<PJ_DNS_SRV_MAX_ADDR; ++i) {
	unsigned j;

	pj_assert(rec->entry[i].priority == i);
	pj_assert(rec->entry[i].weight == 2);
	//pj_assert(pj_strcmp2(&rec->entry[i].server.name, "sip." DOMAIN3)==0);
	pj_assert(rec->entry[i].server.alias.slen == 0);
	pj_assert(rec->entry[i].port == PORT3+i);

	pj_assert(rec->entry[i].server.addr_count == PJ_DNS_MAX_IP_IN_A_REC);

	for (j=0; j<PJ_DNS_MAX_IP_IN_A_REC; ++j) {
	    pj_assert(rec->entry[i].server.addr[j].s_addr == IP_ADDR3+j);
	}
    }

    pj_sem_post(sem);
}

static int srv_resolver_many_test(void)
{
    pj_status_t status;
    pj_str_t domain = pj_str(DOMAIN3);
    pj_str_t res_name = pj_str("_sip._udp.");

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): too many entries test"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action3_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action3_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 1, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_3, NULL);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);

    return 0;
}


////////////////////////////////////////////////////////////////////////////


int resolver_test(void)
{
    int rc;
    
    rc = init();
    if (rc != 0)
	goto on_error;

    rc = a_parser_test();
    if (rc != 0)
	goto on_error;

    rc = simple_test();
    if (rc != 0)
	goto on_error;

    rc = dns_test();
    if (rc != 0)
	goto on_error;

    srv_resolver_test();
    srv_resolver_fallback_test();
    srv_resolver_many_test();

    destroy();
    return 0;

on_error:
    destroy();
    return rc;
}



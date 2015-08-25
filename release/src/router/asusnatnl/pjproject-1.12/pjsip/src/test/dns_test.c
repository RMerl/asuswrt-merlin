/* $Id: dns_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "test.h"
#include <pjsip.h>
#include <pjlib.h>
#include <pjlib-util.h>

/* For logging purpose. */
#define THIS_FILE   "dns_test.c"

struct result
{
    pj_status_t		    status;
    pjsip_server_addresses  servers;
};


static void cb(pj_status_t status,
	       void *token,
	       const struct pjsip_server_addresses *addr,
		   int target_port)
{
    struct result *result = (struct result*) token;

	PJ_UNUSED_ARG(target_port);

    result->status = status;
    if (status == PJ_SUCCESS)
	pj_memcpy(&result->servers, addr, sizeof(*addr));
}


static void add_dns_entries(pj_dns_resolver *resv)
{
    /* Inject DNS SRV entry */
    pj_dns_parsed_packet pkt;
    pj_dns_parsed_query q;
    pj_dns_parsed_rr ans[4];
    pj_dns_parsed_rr ar[5];
    pj_str_t tmp;
    unsigned i;

    /*
     * This is answer to SRV query to "example.com" domain, and
     * the answer contains full reference to the A records of
     * the server. The full DNS records is :

     _sip._udp.example.com 3600 IN SRV 0 0  5060 sip01.example.com.
     _sip._udp.example.com 3600 IN SRV 0 20 5060 sip02.example.com.
     _sip._udp.example.com 3600 IN SRV 0 10 5060 sip03.example.com.
     _sip._udp.example.com 3600 IN SRV 1 0  5060 sip04.example.com.
     
     sip01.example.com. 3600 IN A       1.1.1.1
     sip02.example.com. 3600 IN A       2.2.2.2
     sip03.example.com. 3600 IN A       3.3.3.3
     sip04.example.com. 3600 IN A       4.4.4.4
     
     ; Additionally, add A record for "example.com"
     example.com.	3600 IN A       5.5.5.5

     */
    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(ans, sizeof(ans));
    pj_bzero(ar, sizeof(ar));

    pkt.hdr.flags = PJ_DNS_SET_QR(1);
    pkt.hdr.anscount = PJ_ARRAY_SIZE(ans);
    pkt.hdr.arcount = 0;
    pkt.ans = ans;
    pkt.arr = ar;

    ans[0].name = pj_str("_sip._udp.example.com");
    ans[0].type = PJ_DNS_TYPE_SRV;
    ans[0].dnsclass = PJ_DNS_CLASS_IN;
    ans[0].ttl = 3600;
    ans[0].rdata.srv.prio = 0;
    ans[0].rdata.srv.weight = 0;
    ans[0].rdata.srv.port = 5060;
    ans[0].rdata.srv.target = pj_str("sip01.example.com");

    ans[1].name = pj_str("_sip._udp.example.com");
    ans[1].type = PJ_DNS_TYPE_SRV;
    ans[1].dnsclass = PJ_DNS_CLASS_IN;
    ans[1].ttl = 3600;
    ans[1].rdata.srv.prio = 0;
    ans[1].rdata.srv.weight = 20;
    ans[1].rdata.srv.port = 5060;
    ans[1].rdata.srv.target = pj_str("sip02.example.com");

    ans[2].name = pj_str("_sip._udp.example.com");
    ans[2].type = PJ_DNS_TYPE_SRV;
    ans[2].dnsclass = PJ_DNS_CLASS_IN;
    ans[2].ttl = 3600;
    ans[2].rdata.srv.prio = 0;
    ans[2].rdata.srv.weight = 10;
    ans[2].rdata.srv.port = 5060;
    ans[2].rdata.srv.target = pj_str("sip03.example.com");

    ans[3].name = pj_str("_sip._udp.example.com");
    ans[3].type = PJ_DNS_TYPE_SRV;
    ans[3].dnsclass = PJ_DNS_CLASS_IN;
    ans[3].ttl = 3600;
    ans[3].rdata.srv.prio = 1;
    ans[3].rdata.srv.weight = 0;
    ans[3].rdata.srv.port = 5060;
    ans[3].rdata.srv.target = pj_str("sip04.example.com");

    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);

    ar[0].name = pj_str("sip01.example.com");
    ar[0].type = PJ_DNS_TYPE_A;
    ar[0].dnsclass = PJ_DNS_CLASS_IN;
    ar[0].ttl = 3600;
    ar[0].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "1.1.1.1"));

    ar[1].name = pj_str("sip02.example.com");
    ar[1].type = PJ_DNS_TYPE_A;
    ar[1].dnsclass = PJ_DNS_CLASS_IN;
    ar[1].ttl = 3600;
    ar[1].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "2.2.2.2"));

    ar[2].name = pj_str("sip03.example.com");
    ar[2].type = PJ_DNS_TYPE_A;
    ar[2].dnsclass = PJ_DNS_CLASS_IN;
    ar[2].ttl = 3600;
    ar[2].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "3.3.3.3"));

    ar[3].name = pj_str("sip04.example.com");
    ar[3].type = PJ_DNS_TYPE_A;
    ar[3].dnsclass = PJ_DNS_CLASS_IN;
    ar[3].ttl = 3600;
    ar[3].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "4.4.4.4"));

    ar[4].name = pj_str("example.com");
    ar[4].type = PJ_DNS_TYPE_A;
    ar[4].dnsclass = PJ_DNS_CLASS_IN;
    ar[4].ttl = 3600;
    ar[4].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "5.5.5.5"));

    /* 
     * Create individual A records for all hosts in "example.com" domain.
     */
    for (i=0; i<PJ_ARRAY_SIZE(ar); ++i) {
	pj_bzero(&pkt, sizeof(pkt));
	pkt.hdr.flags = PJ_DNS_SET_QR(1);
	pkt.hdr.qdcount = 1;
	pkt.q = &q;
	q.name = ar[i].name;
	q.type = ar[i].type;
	q.dnsclass = PJ_DNS_CLASS_IN;
	pkt.hdr.anscount = 1;
	pkt.ans = &ar[i];

	pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);
    }

    /* 
     * Simulate DNS error response by creating these answers.
     * Sample of invalid SRV records: _sip._udp.sip01.example.com.
     */
    for (i=0; i<PJ_ARRAY_SIZE(ans); ++i) {
	pj_dns_parsed_query q;
	char buf[128];
	char *services[] = { "_sip._udp.", "_sip._tcp.", "_sips._tcp."};
	unsigned j;

	for (j=0; j<PJ_ARRAY_SIZE(services); ++j) {
	    q.dnsclass = PJ_DNS_CLASS_IN;
	    q.type = PJ_DNS_TYPE_SRV;

	    q.name.ptr = buf;
	    pj_bzero(buf, sizeof(buf));
	    pj_strcpy2(&q.name, services[j]);
	    pj_strcat(&q.name, &ans[i].rdata.srv.target);

	    pj_bzero(&pkt, sizeof(pkt));
	    pkt.hdr.qdcount = 1;
	    pkt.hdr.flags = PJ_DNS_SET_QR(1) |
			    PJ_DNS_SET_RCODE(PJ_DNS_RCODE_NXDOMAIN);
	    pkt.q = &q;
	    
	    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);
	}
    }


    /*
     * ANOTHER DOMAIN.
     *
     * This time we let SRV and A get answered in different DNS
     * query.
     */

    /* The "domain.com" DNS records (note the different the port):

	_sip._tcp.domain.com 3600 IN SRV 1 0 50060 sip06.domain.com.
	_sip._tcp.domain.com 3600 IN SRV 2 0 50060 sip07.domain.com.

	sip06.domain.com. 3600 IN A       6.6.6.6
	sip07.domain.com. 3600 IN A       7.7.7.7
     */

    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(&ans, sizeof(ans));
    pkt.hdr.flags = PJ_DNS_SET_QR(1);
    pkt.hdr.anscount = 2;
    pkt.ans = ans;

    /* Add the SRV records, with reverse priority (to test that sorting
     * works.
     */
    ans[0].name = pj_str("_sip._tcp.domain.com");
    ans[0].type = PJ_DNS_TYPE_SRV;
    ans[0].dnsclass = PJ_DNS_CLASS_IN;
    ans[0].ttl = 3600;
    ans[0].rdata.srv.prio = 2;
    ans[0].rdata.srv.weight = 0;
    ans[0].rdata.srv.port = 50060;
    ans[0].rdata.srv.target = pj_str("SIP07.DOMAIN.COM");

    ans[1].name = pj_str("_sip._tcp.domain.com");
    ans[1].type = PJ_DNS_TYPE_SRV;
    ans[1].dnsclass = PJ_DNS_CLASS_IN;
    ans[1].ttl = 3600;
    ans[1].rdata.srv.prio = 1;
    ans[1].rdata.srv.weight = 0;
    ans[1].rdata.srv.port = 50060;
    ans[1].rdata.srv.target = pj_str("SIP06.DOMAIN.COM");

    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);

    /* From herein there is only one answer */
    pkt.hdr.anscount = 1;

    /* Add a single SRV for UDP */
    ans[0].name = pj_str("_sip._udp.domain.com");
    ans[0].type = PJ_DNS_TYPE_SRV;
    ans[0].dnsclass = PJ_DNS_CLASS_IN;
    ans[0].ttl = 3600;
    ans[0].rdata.srv.prio = 0;
    ans[0].rdata.srv.weight = 0;
    ans[0].rdata.srv.port = 50060;
    ans[0].rdata.srv.target = pj_str("SIP06.DOMAIN.COM");

    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);


    /* Add the A record for sip06.domain.com */
    ans[0].name = pj_str("sip06.domain.com");
    ans[0].type = PJ_DNS_TYPE_A;
    ans[0].dnsclass = PJ_DNS_CLASS_IN;
    ans[0].ttl = 3600;
    ans[0].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "6.6.6.6"));

    pkt.hdr.qdcount = 1;
    pkt.q = &q;
    q.name = ans[0].name;
    q.type = ans[0].type;
    q.dnsclass = ans[0].dnsclass;

    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);

    /* Add the A record for sip07.domain.com */
    ans[0].name = pj_str("sip07.domain.com");
    ans[0].type = PJ_DNS_TYPE_A;
    ans[0].dnsclass = PJ_DNS_CLASS_IN;
    ans[0].ttl = 3600;
    ans[0].rdata.a.ip_addr = pj_inet_addr(pj_cstr(&tmp, "7.7.7.7"));

    pkt.hdr.qdcount = 1;
    pkt.q = &q;
    q.name = ans[0].name;
    q.type = ans[0].type;
    q.dnsclass = ans[0].dnsclass;

    pj_dns_resolver_add_entry( resv, &pkt, PJ_FALSE);

    pkt.hdr.qdcount = 0;
}


/*
 * Perform server resolution where the results are expected to
 * come in strict order.
 */
static int test_resolve(const char *title,
			pj_pool_t *pool,
			pjsip_transport_type_e type,
			char *name,
			int port,
			pjsip_server_addresses *ref)
{
    pjsip_host_info dest;
    struct result result;

    PJ_LOG(3,(THIS_FILE, " test_resolve(): %s", title));

    dest.type = type;
    dest.flag = pjsip_transport_get_flag_from_type(type);
    dest.addr.host = pj_str(name);
    dest.addr.port = port;

    result.status = 0x12345678;

    pjsip_endpt_resolve(endpt, pool, &dest, &result, &cb);

    while (result.status == 0x12345678) {
	int i = 0;
	pj_time_val timeout = { 1, 0 };
	pjsip_endpt_handle_events(endpt, &timeout);
	if (i == 1)
	    pj_dns_resolver_dump(pjsip_endpt_get_resolver(endpt), PJ_TRUE);
    }

    if (result.status != PJ_SUCCESS) {
	app_perror("  pjsip_endpt_resolve() error", result.status);
	return result.status;
    }

    if (ref) {
	unsigned i;

	if (ref->count != result.servers.count) {
	    PJ_LOG(3,(THIS_FILE, "  test_resolve() error 10: result count mismatch"));
	    return 10;
	}
	
	for (i=0; i<ref->count; ++i) {
	    pj_sockaddr_in *ra = (pj_sockaddr_in *)&ref->entry[i].addr;
	    pj_sockaddr_in *rb = (pj_sockaddr_in *)&result.servers.entry[i].addr;

	    if (ra->sin_addr.s_addr != rb->sin_addr.s_addr) {
		PJ_LOG(3,(THIS_FILE, "  test_resolve() error 20: IP address mismatch"));
		return 20;
	    }
	    if (ra->sin_port != rb->sin_port) {
		PJ_LOG(3,(THIS_FILE, "  test_resolve() error 30: port mismatch"));
		return 30;
	    }
	    if (ref->entry[i].addr_len != result.servers.entry[i].addr_len) {
		PJ_LOG(3,(THIS_FILE, "  test_resolve() error 40: addr_len mismatch"));
		return 40;
	    }
	    if (ref->entry[i].type != result.servers.entry[i].type) {
		PJ_LOG(3,(THIS_FILE, "  test_resolve() error 50: transport type mismatch"));
		return 50;
	    }
	}
    }

    return PJ_SUCCESS;
}

/*
 * Perform round-robin/load balance test.
 */
static int round_robin_test(pj_pool_t *pool)
{
    enum { COUNT = 400, PCT_ALLOWANCE = 5 };
    unsigned i;
    struct server_hit
    {
	char *ip_addr;
	unsigned percent;
	unsigned hits;
    } server_hit[] =
    {
	{ "1.1.1.1", 3,  0 },
	{ "2.2.2.2", 65, 0 },
	{ "3.3.3.3", 32, 0 },
	{ "4.4.4.4", 0,  0 }
    };

    PJ_LOG(3,(THIS_FILE, " Performing round-robin/load-balance test.."));

    /* Do multiple resolve request to "example.com".
     * The resolver should select the server based on the weight proportion
     * the the servers in the SRV entry.
     */
    for (i=0; i<COUNT; ++i) {
	pjsip_host_info dest;
	struct result result;
	unsigned j;

	dest.type = PJSIP_TRANSPORT_UDP;
	dest.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_UDP);
	dest.addr.host = pj_str("example.com");
	dest.addr.port = 0;

	result.status = 0x12345678;

	pjsip_endpt_resolve(endpt, pool, &dest, &result, &cb);

	while (result.status == 0x12345678) {
	    int i = 0;
	    pj_time_val timeout = { 1, 0 };
	    pjsip_endpt_handle_events(endpt, &timeout);
	    if (i == 1)
		pj_dns_resolver_dump(pjsip_endpt_get_resolver(endpt), PJ_TRUE);
	}

	/* Find which server was "hit" */
	for (j=0; j<PJ_ARRAY_SIZE(server_hit); ++j) {
	    pj_str_t tmp;
	    pj_in_addr a1;
	    pj_sockaddr_in *a2;

	    tmp = pj_str(server_hit[j].ip_addr);
	    a1 = pj_inet_addr(&tmp);
	    a2 = (pj_sockaddr_in*) &result.servers.entry[0].addr;

	    if (a1.s_addr == a2->sin_addr.s_addr) {
		server_hit[j].hits++;
		break;
	    }
	}

	if (j == PJ_ARRAY_SIZE(server_hit)) {
	    PJ_LOG(1,(THIS_FILE, "..round_robin_test() error 10: returned address mismatch"));
	    return 10;
	}
    }

    /* Print the actual hit rate */
    for (i=0; i<PJ_ARRAY_SIZE(server_hit); ++i) {
	PJ_LOG(3,(THIS_FILE, " ..Server %s: weight=%d%%, hit %d%% times",
		  server_hit[i].ip_addr, server_hit[i].percent,
		  (server_hit[i].hits * 100) / COUNT));
    }

    /* Compare the actual hit with the weight proportion */
    for (i=0; i<PJ_ARRAY_SIZE(server_hit); ++i) {
	int actual_pct = (server_hit[i].hits * 100) / COUNT;

	if (actual_pct + PCT_ALLOWANCE < (int)server_hit[i].percent ||
	    actual_pct - PCT_ALLOWANCE > (int)server_hit[i].percent)
	{
	    PJ_LOG(1,(THIS_FILE, 
		      "..round_robin_test() error 20: "
		      "hit rate difference for server %s (%d%%) is more than "
		      "tolerable allowance (%d%%)",
		      server_hit[i].ip_addr, 
		      actual_pct - server_hit[i].percent, 
		      PCT_ALLOWANCE));
	    return 20;
	}
    }

    PJ_LOG(3,(THIS_FILE, 
	      " Load balance test success, hit-rate is "
	      "within %d%% allowance", PCT_ALLOWANCE));
    return PJ_SUCCESS;
}


#define C(expr)	    status = expr; \
		    if (status != PJ_SUCCESS) app_perror(THIS_FILE, "Error", status);

static void add_ref(pjsip_server_addresses *r,
		    pjsip_transport_type_e type,
		    char *addr,
		    int port)
{
    pj_sockaddr_in *a;
    pj_str_t tmp;

    r->entry[r->count].type = type;
    r->entry[r->count].priority = 0;
    r->entry[r->count].weight = 0;
    r->entry[r->count].addr_len = sizeof(pj_sockaddr_in);

    a = (pj_sockaddr_in *)&r->entry[r->count].addr;
    a->sin_family = pj_AF_INET();
    tmp = pj_str(addr);
    a->sin_addr = pj_inet_addr(&tmp);
    a->sin_port = pj_htons((pj_uint16_t)port);

    r->count++;
}

static void create_ref(pjsip_server_addresses *r,
		       pjsip_transport_type_e type,
		       char *addr,
		       int port)
{
    r->count = 0;
    add_ref(r, type, addr, port);
}


/*
 * Main test entry.
 */
int resolve_test(void)
{
    pj_pool_t *pool;
    pj_dns_resolver *resv;
    pj_str_t nameserver;
    pj_uint16_t port = 5353;
    pj_status_t status;

    pool = pjsip_endpt_create_pool(endpt, NULL, 4000, 4000);

    status = pjsip_endpt_create_resolver(endpt, &resv);

    nameserver = pj_str("192.168.0.106");
    pj_dns_resolver_set_ns(resv, 1, &nameserver, &port);
    pjsip_endpt_set_resolver(endpt, resv);

    add_dns_entries(resv);

    /* These all should be resolved as IP addresses (DNS A query) */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_UDP, "1.1.1.1", 5060);
	status = test_resolve("IP address without transport and port", pool, PJSIP_TRANSPORT_UNSPECIFIED, "1.1.1.1", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -100;
    }
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_UDP, "1.1.1.1", 5060);
	status = test_resolve("IP address with explicit port", pool, PJSIP_TRANSPORT_UNSPECIFIED, "1.1.1.1", 5060, &ref);
	if (status != PJ_SUCCESS)
	    return -110;
    }
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_TCP, "1.1.1.1", 5060);
	status = test_resolve("IP address without port (TCP)", pool, PJSIP_TRANSPORT_TCP,"1.1.1.1", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -120;
    }
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_TLS, "1.1.1.1", 5061);
	status = test_resolve("IP address without port (TLS)", pool, PJSIP_TRANSPORT_TLS, "1.1.1.1", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -130;
    }

    /* This should be resolved as DNS A record (because port is present) */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_UDP, "5.5.5.5", 5060);
	status = test_resolve("domain name with port should resolve to A record", pool, PJSIP_TRANSPORT_UNSPECIFIED, "example.com", 5060, &ref);
	if (status != PJ_SUCCESS)
	    return -140;
    }

    /* This will fail to be resolved as SRV, resolver should fallback to 
     * resolving to A record.
     */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_UDP, "2.2.2.2", 5060);
	status = test_resolve("failure with SRV fallback to A record", pool, PJSIP_TRANSPORT_UNSPECIFIED, "sip02.example.com", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -150;
    }

    /* Same as above, but explicitly for TLS. */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_TLS, "2.2.2.2", 5061);
	status = test_resolve("failure with SRV fallback to A record (for TLS)", pool, PJSIP_TRANSPORT_TLS, "sip02.example.com", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -150;
    }

    /* Standard DNS SRV followed by A recolution */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_UDP, "6.6.6.6", 50060);
	status = test_resolve("standard SRV resolution", pool, PJSIP_TRANSPORT_UNSPECIFIED, "domain.com", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -155;
    }

    /* Standard DNS SRV followed by A recolution (explicit transport) */
    {
	pjsip_server_addresses ref;
	create_ref(&ref, PJSIP_TRANSPORT_TCP, "6.6.6.6", 50060);
	add_ref(&ref, PJSIP_TRANSPORT_TCP, "7.7.7.7", 50060);
	status = test_resolve("standard SRV resolution with explicit transport (TCP)", pool, PJSIP_TRANSPORT_TCP, "domain.com", 0, &ref);
	if (status != PJ_SUCCESS)
	    return -160;
    }


    /* Round robin/load balance test */
    if (round_robin_test(pool) != 0)
	return -170;

    /* Timeout test */
    {
	status = test_resolve("timeout test", pool, PJSIP_TRANSPORT_UNSPECIFIED, "an.invalid.address", 0, NULL);
	if (status == PJ_SUCCESS)
	    return -150;
    }

    return 0;
}


/* $Id: dns_dump.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/dns.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/string.h>

#define THIS_FILE   "dns_dump.c"
#define LEVEL	    3

static const char *spell_ttl(char *buf, int size, unsigned ttl)
{
#define DAY	(3600*24)
#define HOUR	(3600)
#define MINUTE	(60)

    char *p = buf;
    int len;

    if (ttl > DAY) {
	len = pj_ansi_snprintf(p, size, "%dd ", ttl/DAY);
	if (len < 1)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= DAY;
    }

    if (ttl > HOUR) {
	len = pj_ansi_snprintf(p, size, "%dh ", ttl/HOUR);
	if (len < 1)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= HOUR;
    }

    if (ttl > MINUTE) {
	len = pj_ansi_snprintf(p, size, "%dm ", ttl/MINUTE);
	if (len < 1)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= MINUTE;
    }

    if (ttl > 0) {
	len = pj_ansi_snprintf(p, size, "%ds ", ttl);
	if (len < 1)
	    return "-err-";
	size -= len;
	p += len;
	ttl = 0;
    }

    *p = '\0';
    return buf;
}


static void dump_query(unsigned index, const pj_dns_parsed_query *q)
{
    PJ_LOG(3,(THIS_FILE, " %d. Name: %.*s", 
			 index, (int)q->name.slen, q->name.ptr));
    PJ_LOG(3,(THIS_FILE, "    Type: %s (%d)", 
			 pj_dns_get_type_name(q->type), q->type));
    PJ_LOG(3,(THIS_FILE, "    Class: %s (%d)", 
		         (q->dnsclass==1 ? "IN" : "<Unknown>"), q->dnsclass));
}

static void dump_answer(unsigned index, const pj_dns_parsed_rr *rr)
{
    const pj_str_t root_name = { "<Root>", 6 };
    const pj_str_t *name = &rr->name;
    char ttl_words[32];

    if (name->slen == 0)
	name = &root_name;

    PJ_LOG(3,(THIS_FILE, " %d. %s record (type=%d)", 
			 index, pj_dns_get_type_name(rr->type),
			rr->type));
    PJ_LOG(3,(THIS_FILE, "    Name: %.*s", (int)name->slen, name->ptr));
    PJ_LOG(3,(THIS_FILE, "    TTL: %u (%s)", rr->ttl, 
			  spell_ttl(ttl_words, sizeof(ttl_words), rr->ttl)));
    PJ_LOG(3,(THIS_FILE, "    Data length: %u", rr->rdlength));

    if (rr->type == PJ_DNS_TYPE_SRV) {
	PJ_LOG(3,(THIS_FILE, "    SRV: prio=%d, weight=%d %.*s:%d", 
			     rr->rdata.srv.prio, rr->rdata.srv.weight,
			     (int)rr->rdata.srv.target.slen, 
			     rr->rdata.srv.target.ptr,
			     rr->rdata.srv.port));
    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
	       rr->type == PJ_DNS_TYPE_NS ||
	       rr->type == PJ_DNS_TYPE_PTR) 
    {
	PJ_LOG(3,(THIS_FILE, "    Name: %.*s",
		  (int)rr->rdata.cname.name.slen,
		  rr->rdata.cname.name.ptr));
    } else if (rr->type == PJ_DNS_TYPE_A) {
	PJ_LOG(3,(THIS_FILE, "    IP address: %s",
		  pj_inet_ntoa(rr->rdata.a.ip_addr)));
    } else if (rr->type == PJ_DNS_TYPE_AAAA) {
	char addr[PJ_INET6_ADDRSTRLEN];
	PJ_LOG(3,(THIS_FILE, "    IPv6 address: %s",
		  pj_inet_ntop2(pj_AF_INET6(), &rr->rdata.aaaa.ip_addr,
			        addr, sizeof(addr))));
    }
}


PJ_DEF(void) pj_dns_dump_packet(const pj_dns_parsed_packet *res)
{
    unsigned i;

    PJ_ASSERT_ON_FAIL(res != NULL, return);

    /* Header part */
    PJ_LOG(3,(THIS_FILE, "Domain Name System packet (%s):",
		(PJ_DNS_GET_QR(res->hdr.flags) ? "response" : "query")));
    PJ_LOG(3,(THIS_FILE, " Transaction ID: %d", res->hdr.id));
    PJ_LOG(3,(THIS_FILE, 
	      " Flags: opcode=%d, authoritative=%d, truncated=%d, rcode=%d",
		 PJ_DNS_GET_OPCODE(res->hdr.flags),
		 PJ_DNS_GET_AA(res->hdr.flags),
		 PJ_DNS_GET_TC(res->hdr.flags),
		 PJ_DNS_GET_RCODE(res->hdr.flags)));
    PJ_LOG(3,(THIS_FILE, " Nb of queries: %d", res->hdr.qdcount));
    PJ_LOG(3,(THIS_FILE, " Nb of answer RR: %d", res->hdr.anscount));
    PJ_LOG(3,(THIS_FILE, " Nb of authority RR: %d", res->hdr.nscount));
    PJ_LOG(3,(THIS_FILE, " Nb of additional RR: %d", res->hdr.arcount));
    PJ_LOG(3,(THIS_FILE, ""));

    /* Dump queries */
    if (res->hdr.qdcount) {
	PJ_LOG(3,(THIS_FILE, " Queries:"));

	for (i=0; i<res->hdr.qdcount; ++i) {
	    dump_query(i, &res->q[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

    /* Dump answers */
    if (res->hdr.anscount) {
	PJ_LOG(3,(THIS_FILE, " Answers RR:"));

	for (i=0; i<res->hdr.anscount; ++i) {
	    dump_answer(i, &res->ans[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

    /* Dump NS sections */
    if (res->hdr.anscount) {
	PJ_LOG(3,(THIS_FILE, " NS Authority RR:"));

	for (i=0; i<res->hdr.nscount; ++i) {
	    dump_answer(i, &res->ns[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

    /* Dump Additional info sections */
    if (res->hdr.arcount) {
	PJ_LOG(3,(THIS_FILE, " Additional Info RR:"));

	for (i=0; i<res->hdr.arcount; ++i) {
	    dump_answer(i, &res->arr[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

}


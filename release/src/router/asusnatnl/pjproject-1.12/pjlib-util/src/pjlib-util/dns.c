/* $Id: dns.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>


PJ_DEF(const char *) pj_dns_get_type_name(int type)
{
    switch (type) {
    case PJ_DNS_TYPE_A:	    return "A";
    case PJ_DNS_TYPE_AAAA:  return "AAAA";
    case PJ_DNS_TYPE_SRV:   return "SRV";
    case PJ_DNS_TYPE_NS:    return "NS";
    case PJ_DNS_TYPE_CNAME: return "CNAME";
    case PJ_DNS_TYPE_PTR:   return "PTR";
    case PJ_DNS_TYPE_MX:    return "MX";
    case PJ_DNS_TYPE_TXT:   return "TXT";
    case PJ_DNS_TYPE_NAPTR: return "NAPTR";
    }
    return "(Unknown)";
}


static void write16(pj_uint8_t *p, pj_uint16_t val)
{
    p[0] = (pj_uint8_t)(val >> 8);
    p[1] = (pj_uint8_t)(val & 0xFF);
}


/**
 * Initialize a DNS query transaction.
 */
PJ_DEF(pj_status_t) pj_dns_make_query( void *packet,
				       unsigned *size,
				       pj_uint16_t id,
				       int qtype,
				       const pj_str_t *name)
{
    pj_uint8_t *query, *p = (pj_uint8_t*)packet;
    const char *startlabel, *endlabel, *endname;
    unsigned d;

    /* Sanity check */
    PJ_ASSERT_RETURN(packet && size && qtype && name, PJ_EINVAL);

    /* Calculate total number of bytes required. */
    d = sizeof(pj_dns_hdr) + name->slen + 4;

    /* Check that size is sufficient. */
    PJ_ASSERT_RETURN(*size >= d, PJLIB_UTIL_EDNSQRYTOOSMALL);

    /* Initialize header */
    pj_assert(sizeof(pj_dns_hdr)==12);
    pj_bzero(p, sizeof(struct pj_dns_hdr));
    write16(p+0, id);
    write16(p+2, (pj_uint16_t)PJ_DNS_SET_RD(1));
    write16(p+4, (pj_uint16_t)1);

    /* Initialize query */
    query = p = ((pj_uint8_t*)packet)+sizeof(pj_dns_hdr);

    /* Tokenize name */
    startlabel = endlabel = name->ptr;
    endname = name->ptr + name->slen;
    while (endlabel != endname) {
	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;
	*p++ = (pj_uint8_t)(endlabel - startlabel);
	pj_memcpy(p, startlabel, endlabel-startlabel);
	p += (endlabel-startlabel);
	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	startlabel = endlabel;
    }
    *p++ = '\0';

    /* Set type */
    write16(p, (pj_uint16_t)qtype);
    p += 2;

    /* Set class (IN=1) */
    write16(p, 1);
    p += 2;

    /* Done, calculate length */
    *size = p - (pj_uint8_t*)packet;

    return 0;
}


/* Get a name length (note: name consists of multiple labels and
 * it may contain pointers when name compression is applied) 
 */
static pj_status_t get_name_len(int rec_counter, const pj_uint8_t *pkt, 
				const pj_uint8_t *start, const pj_uint8_t *max, 
				int *parsed_len, int *name_len)
{
    const pj_uint8_t *p;
    pj_status_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return PJLIB_UTIL_EDNSINNAMEPTR;
    }

    *name_len = *parsed_len = 0;
    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    int ptr_len = 0;
	    int dummy;
	    pj_uint16_t offset;

	    /* Get the 14bit offset */
	    pj_memcpy(&offset, p, 2);
	    offset ^= pj_htons((pj_uint16_t)(0xc0 << 8));
	    offset = pj_ntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    /* Get the name length from that offset. */
	    status = get_name_len(rec_counter+1, pkt, pkt + offset, max, 
				  &dummy, &ptr_len);
	    if (status != PJ_SUCCESS)
		return status;

	    *parsed_len += 2;
	    *name_len += ptr_len;

	    return PJ_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    p += (label_len + 1);
	    *parsed_len += (label_len + 1);

	    if (*p != 0)
		++label_len;
	    
	    *name_len += label_len;

	    if (p >= max)
		return PJLIB_UTIL_EDNSINSIZE;
	}
    }
    ++p;
    (*parsed_len)++;

    return PJ_SUCCESS;
}


/* Parse and copy name (note: name consists of multiple labels and
 * it may contain pointers when compression is applied).
 */
static pj_status_t get_name(int rec_counter, const pj_uint8_t *pkt, 
			    const pj_uint8_t *start, const pj_uint8_t *max,
			    pj_str_t *name)
{
    const pj_uint8_t *p;
    pj_status_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return PJLIB_UTIL_EDNSINNAMEPTR;
    }

    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    pj_uint16_t offset;

	    /* Get the 14bit offset */
	    pj_memcpy(&offset, p, 2);
	    offset ^= pj_htons((pj_uint16_t)(0xc0 << 8));
	    offset = pj_ntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    /* Retrieve the name from that offset. */
	    status = get_name(rec_counter+1, pkt, pkt + offset, max, name);
	    if (status != PJ_SUCCESS)
		return status;

	    return PJ_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    pj_memcpy(name->ptr + name->slen, p+1, label_len);
	    name->slen += label_len;

	    p += label_len + 1;
	    if (*p != 0) {
		*(name->ptr + name->slen) = '.';
		++name->slen;
	    }

	    if (p >= max)
		return PJLIB_UTIL_EDNSINSIZE;
	}
    }

    return PJ_SUCCESS;
}


/* Parse query records. */
static pj_status_t parse_query(pj_dns_parsed_query *q, pj_pool_t *pool,
			       const pj_uint8_t *pkt, const pj_uint8_t *start,
			       const pj_uint8_t *max, int *parsed_len)
{
    const pj_uint8_t *p = start;
    int name_len, name_part_len;
    pj_status_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != PJ_SUCCESS)
	return status;

    /* Allocate memory for the name */
    q->name.ptr = (char*) pj_pool_alloc(pool, name_len+4);
    q->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &q->name);
    if (status != PJ_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Get the type */
    pj_memcpy(&q->type, p, 2);
    q->type = pj_ntohs(q->type);
    p += 2;

    /* Get the class */
    pj_memcpy(&q->dnsclass, p, 2);
    q->dnsclass = pj_ntohs(q->dnsclass);
    p += 2;

    *parsed_len = (int)(p - start);

    return PJ_SUCCESS;
}


/* Parse RR records */
static pj_status_t parse_rr(pj_dns_parsed_rr *rr, pj_pool_t *pool,
			    const pj_uint8_t *pkt,
			    const pj_uint8_t *start, const pj_uint8_t *max,
			    int *parsed_len)
{
    const pj_uint8_t *p = start;
    int name_len, name_part_len;
    pj_status_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != PJ_SUCCESS)
	return status;

    /* Allocate memory for the name */
    rr->name.ptr = (char*) pj_pool_alloc(pool, name_len+4);
    rr->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &rr->name);
    if (status != PJ_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Check the size can accomodate next few fields. */
    if (p+10 > max)
	return PJLIB_UTIL_EDNSINSIZE;

    /* Get the type */
    pj_memcpy(&rr->type, p, 2);
    rr->type = pj_ntohs(rr->type);
    p += 2;
    
    /* Get the class */
    pj_memcpy(&rr->dnsclass, p, 2);
    rr->dnsclass = pj_ntohs(rr->dnsclass);
    p += 2;

    /* Class MUST be IN */
    if (rr->dnsclass != 1)
	return PJLIB_UTIL_EDNSINCLASS;

    /* Get TTL */
    pj_memcpy(&rr->ttl, p, 4);
    rr->ttl = pj_ntohl(rr->ttl);
    p += 4;

    /* Get rdlength */
    pj_memcpy(&rr->rdlength, p, 2);
    rr->rdlength = pj_ntohs(rr->rdlength);
    p += 2;

    /* Check that length is valid */
    if (p + rr->rdlength > max)
	return PJLIB_UTIL_EDNSINSIZE;

    /* Parse some well known records */
    if (rr->type == PJ_DNS_TYPE_A) {
	pj_memcpy(&rr->rdata.a.ip_addr, p, 4);
	p += 4;

    } else if (rr->type == PJ_DNS_TYPE_AAAA) {
	pj_memcpy(&rr->rdata.aaaa.ip_addr, p, 16);
	p += 16;

    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
	       rr->type == PJ_DNS_TYPE_NS ||
	       rr->type == PJ_DNS_TYPE_PTR) 
    {

	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != PJ_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.cname.name.ptr = (char*) pj_pool_alloc(pool, name_len);
	rr->rdata.cname.name.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.cname.name);
	if (status != PJ_SUCCESS)
	    return status;

	p += name_part_len;

    } else if (rr->type == PJ_DNS_TYPE_SRV) {

	/* Priority */
	pj_memcpy(&rr->rdata.srv.prio, p, 2);
	rr->rdata.srv.prio = pj_ntohs(rr->rdata.srv.prio);
	p += 2;

	/* Weight */
	pj_memcpy(&rr->rdata.srv.weight, p, 2);
	rr->rdata.srv.weight = pj_ntohs(rr->rdata.srv.weight);
	p += 2;

	/* Port */
	pj_memcpy(&rr->rdata.srv.port, p, 2);
	rr->rdata.srv.port = pj_ntohs(rr->rdata.srv.port);
	p += 2;
	
	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != PJ_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.srv.target.ptr = (char*) pj_pool_alloc(pool, name_len);
	rr->rdata.srv.target.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.srv.target);
	if (status != PJ_SUCCESS)
	    return status;
	p += name_part_len;

    } else {
	/* Copy the raw data */
	rr->data = pj_pool_alloc(pool, rr->rdlength);
	pj_memcpy(rr->data, p, rr->rdlength);

	p += rr->rdlength;
    }

    *parsed_len = (int)(p - start);
    return PJ_SUCCESS;
}


/*
 * Parse raw DNS packet into DNS packet structure.
 */
PJ_DEF(pj_status_t) pj_dns_parse_packet( pj_pool_t *pool,
				  	 const void *packet,
					 unsigned size,
					 pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    const pj_uint8_t *start, *end;
    pj_status_t status;
    unsigned i;

    /* Sanity checks */
    PJ_ASSERT_RETURN(pool && packet && size && p_res, PJ_EINVAL);

    /* Packet size must be at least as big as the header */
    if (size < sizeof(pj_dns_hdr))
	return PJLIB_UTIL_EDNSINSIZE;

    /* Create the structure */
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    /* Copy the DNS header, and convert endianness to host byte order */
    pj_memcpy(&res->hdr, packet, sizeof(pj_dns_hdr));
    res->hdr.id	      = pj_ntohs(res->hdr.id);
    res->hdr.flags    = pj_ntohs(res->hdr.flags);
    res->hdr.qdcount  = pj_ntohs(res->hdr.qdcount);
    res->hdr.anscount = pj_ntohs(res->hdr.anscount);
    res->hdr.nscount  = pj_ntohs(res->hdr.nscount);
    res->hdr.arcount  = pj_ntohs(res->hdr.arcount);

    /* Mark start and end of payload */
    start = ((const pj_uint8_t*)packet) + sizeof(pj_dns_hdr);
    end = ((const pj_uint8_t*)packet) + size;

    /* Parse query records (if any).
     */
    if (res->hdr.qdcount) {
	res->q = (pj_dns_parsed_query*)
		 pj_pool_zalloc(pool, res->hdr.qdcount *
				      sizeof(pj_dns_parsed_query));
	for (i=0; i<res->hdr.qdcount; ++i) {
	    int parsed_len = 0;
	    
	    status = parse_query(&res->q[i], pool, (const pj_uint8_t*)packet, 
	    			 start, end, &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse answer, if any */
    if (res->hdr.anscount) {
	res->ans = (pj_dns_parsed_rr*)
		   pj_pool_zalloc(pool, res->hdr.anscount * 
					sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.anscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ans[i], pool, (const pj_uint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse authoritative NS records, if any */
    if (res->hdr.nscount) {
	res->ns = (pj_dns_parsed_rr*)
		  pj_pool_zalloc(pool, res->hdr.nscount *
				       sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.nscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ns[i], pool, (const pj_uint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse additional RR answer, if any */
    if (res->hdr.arcount) {
	res->arr = (pj_dns_parsed_rr*)
		   pj_pool_zalloc(pool, res->hdr.arcount *
					sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.arcount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->arr[i], pool, (const pj_uint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Looks like everything is okay */
    *p_res = res;

    return PJ_SUCCESS;
}


/* Perform name compression scheme.
 * If a name is already in the nametable, when no need to duplicate
 * the string with the pool, but rather just use the pointer there.
 */
static void apply_name_table( unsigned *count,
			      pj_str_t nametable[],
		    	      const pj_str_t *src,
			      pj_pool_t *pool,
			      pj_str_t *dst)
{
    unsigned i;

    /* Scan strings in nametable */
    for (i=0; i<*count; ++i) {
	if (pj_stricmp(&nametable[i], src) == 0)
	    break;
    }

    /* If name is found in nametable, use the pointer in the nametable */
    if (i != *count) {
	dst->ptr = nametable[i].ptr;
	dst->slen = nametable[i].slen;
	return;
    }

    /* Otherwise duplicate the string, and insert new name in nametable */
    pj_strdup(pool, dst, src);

    if (*count < PJ_DNS_MAX_NAMES_IN_NAMETABLE) {
	nametable[*count].ptr = dst->ptr;
	nametable[*count].slen = dst->slen;

	++(*count);
    }
}

static void copy_query(pj_pool_t *pool, pj_dns_parsed_query *dst,
		       const pj_dns_parsed_query *src,
		       unsigned *nametable_count,
		       pj_str_t nametable[])
{
    pj_memcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);
}


static void copy_rr(pj_pool_t *pool, pj_dns_parsed_rr *dst,
		    const pj_dns_parsed_rr *src,
		    unsigned *nametable_count,
		    pj_str_t nametable[])
{
    pj_memcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);

    if (src->data) {
	dst->data = pj_pool_alloc(pool, src->rdlength);
	pj_memcpy(dst->data, src->data, src->rdlength);
    }

    if (src->type == PJ_DNS_TYPE_SRV) {
	apply_name_table(nametable_count, nametable, &src->rdata.srv.target, 
			 pool, &dst->rdata.srv.target);
    } else if (src->type == PJ_DNS_TYPE_A) {
	dst->rdata.a.ip_addr.s_addr =  src->rdata.a.ip_addr.s_addr;
    } else if (src->type == PJ_DNS_TYPE_AAAA) {
	pj_memcpy(&dst->rdata.aaaa.ip_addr, &src->rdata.aaaa.ip_addr,
		  sizeof(pj_in6_addr));
    } else if (src->type == PJ_DNS_TYPE_CNAME) {
	pj_strdup(pool, &dst->rdata.cname.name, &src->rdata.cname.name);
    } else if (src->type == PJ_DNS_TYPE_NS) {
	pj_strdup(pool, &dst->rdata.ns.name, &src->rdata.ns.name);
    } else if (src->type == PJ_DNS_TYPE_PTR) {
	pj_strdup(pool, &dst->rdata.ptr.name, &src->rdata.ptr.name);
    }
}

/*
 * Duplicate DNS packet.
 */
PJ_DEF(void) pj_dns_packet_dup(pj_pool_t *pool,
			       const pj_dns_parsed_packet*p,
			       unsigned options,
			       pj_dns_parsed_packet **p_dst)
{
    pj_dns_parsed_packet *dst;
    unsigned nametable_count = 0;
#if PJ_DNS_MAX_NAMES_IN_NAMETABLE
    pj_str_t nametable[PJ_DNS_MAX_NAMES_IN_NAMETABLE];
#else
    pj_str_t *nametable = NULL;
#endif
    unsigned i;

    PJ_ASSERT_ON_FAIL(pool && p && p_dst, return);

    /* Create packet and copy header */
    *p_dst = dst = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);
    pj_memcpy(&dst->hdr, &p->hdr, sizeof(p->hdr));

    /* Initialize section counts in the target packet to zero.
     * If memory allocation fails during copying process, the target packet
     * should have a correct section counts.
     */
    dst->hdr.qdcount = 0;
    dst->hdr.anscount = 0;
    dst->hdr.nscount = 0;
    dst->hdr.arcount = 0;
	

    /* Copy query section */
    if (p->hdr.qdcount && (options & PJ_DNS_NO_QD)==0) {
	dst->q = (pj_dns_parsed_query*)
		 pj_pool_alloc(pool, p->hdr.qdcount * 
				     sizeof(pj_dns_parsed_query));
	for (i=0; i<p->hdr.qdcount; ++i) {
	    copy_query(pool, &dst->q[i], &p->q[i], 
		       &nametable_count, nametable);
	    ++dst->hdr.qdcount;
	}
    }

    /* Copy answer section */
    if (p->hdr.anscount && (options & PJ_DNS_NO_ANS)==0) {
	dst->ans = (pj_dns_parsed_rr*)
		   pj_pool_alloc(pool, p->hdr.anscount * 
				       sizeof(pj_dns_parsed_rr));
	for (i=0; i<p->hdr.anscount; ++i) {
	    copy_rr(pool, &dst->ans[i], &p->ans[i],
		    &nametable_count, nametable);
	    ++dst->hdr.anscount;
	}
    }

    /* Copy NS section */
    if (p->hdr.nscount && (options & PJ_DNS_NO_NS)==0) {
	dst->ns = (pj_dns_parsed_rr*)
		  pj_pool_alloc(pool, p->hdr.nscount * 
				      sizeof(pj_dns_parsed_rr));
	for (i=0; i<p->hdr.nscount; ++i) {
	    copy_rr(pool, &dst->ns[i], &p->ns[i],
		    &nametable_count, nametable);
	    ++dst->hdr.nscount;
	}
    }

    /* Copy additional info section */
    if (p->hdr.arcount && (options & PJ_DNS_NO_AR)==0) {
	dst->arr = (pj_dns_parsed_rr*)
		   pj_pool_alloc(pool, p->hdr.arcount * 
				       sizeof(pj_dns_parsed_rr));
	for (i=0; i<p->hdr.arcount; ++i) {
	    copy_rr(pool, &dst->arr[i], &p->arr[i],
		    &nametable_count, nametable);
	    ++dst->hdr.arcount;
	}
    }
}


PJ_DEF(void) pj_dns_init_srv_rr( pj_dns_parsed_rr *rec,
				 const pj_str_t *res_name,
				 unsigned dnsclass,
				 unsigned ttl,
				 unsigned prio,
				 unsigned weight,
				 unsigned port,
				 const pj_str_t *target)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_SRV;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.srv.prio = (pj_uint16_t) prio;
    rec->rdata.srv.weight = (pj_uint16_t) weight;
    rec->rdata.srv.port = (pj_uint16_t) port;
    rec->rdata.srv.target = *target;
}


PJ_DEF(void) pj_dns_init_cname_rr( pj_dns_parsed_rr *rec,
				   const pj_str_t *res_name,
				   unsigned dnsclass,
				   unsigned ttl,
				   const pj_str_t *name)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_CNAME;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.cname.name = *name;
}


PJ_DEF(void) pj_dns_init_a_rr( pj_dns_parsed_rr *rec,
			       const pj_str_t *res_name,
			       unsigned dnsclass,
			       unsigned ttl,
			       const pj_in_addr *ip_addr)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_A;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.a.ip_addr = *ip_addr;
}


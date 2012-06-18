/* $Id: iptcrdr.c,v 1.32 2010/03/03 11:23:52 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2008 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <libiptc/libiptc.h>
#include <iptables.h>

#include <linux/version.h>

#if IPTABLES_143
/* IPTABLES API version >= 1.4.3 */
#include <net/netfilter/nf_nat.h>
#define ip_nat_multi_range	nf_nat_multi_range
#define ip_nat_range		nf_nat_range
#define IPTC_HANDLE		struct iptc_handle *
#else
/* IPTABLES API version < 1.4.3 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#include <linux/netfilter_ipv4/ip_nat.h>
#else
#include <linux/netfilter/nf_nat.h>
#endif
#define IPTC_HANDLE		iptc_handle_t
#endif

#include "iptcrdr.h"
#include "../upnpglobalvars.h"

/* dummy init and shutdown functions */
int init_redirect(void)
{
	return 0;
}

void shutdown_redirect(void)
{
	return;
}

/* convert an ip address to string */
static int snprintip(char * dst, size_t size, uint32_t ip)
{
	return snprintf(dst, size,
	       "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xff,
	       (ip >> 8) & 0xff, ip & 0xff);
}

/* netfilter cannot store redirection descriptions, so we use our
 * own structure to store them */
struct rdr_desc {
	struct rdr_desc * next;
	unsigned short eport;
	short proto;
	char str[];
};

/* pointer to the chained list where descriptions are stored */
static struct rdr_desc * rdr_desc_list = 0;

static void
add_redirect_desc(unsigned short eport, int proto, const char * desc)
{
	struct rdr_desc * p;
	size_t l;
	if(desc)
	{
		l = strlen(desc) + 1;
		p = malloc(sizeof(struct rdr_desc) + l);
		if(p)
		{
			p->next = rdr_desc_list;
			p->eport = eport;
			p->proto = (short)proto;
			memcpy(p->str, desc, l);
			rdr_desc_list = p;
		}
	}
}

static void
del_redirect_desc(unsigned short eport, int proto)
{
	struct rdr_desc * p, * last;
	p = rdr_desc_list;
	last = 0;
	while(p)
	{
		if(p->eport == eport && p->proto == proto)
		{
			if(!last)
				rdr_desc_list = p->next;
			else
				last->next = p->next;
			free(p);
			return;
		}
		last = p;
		p = p->next;
	}
}

static void
get_redirect_desc(unsigned short eport, int proto,
                  char * desc, int desclen)
{
	struct rdr_desc * p;
	if(!desc || (desclen == 0))
		return;
	for(p = rdr_desc_list; p; p = p->next)
	{
		if(p->eport == eport && p->proto == (short)proto)
		{
			strncpy(desc, p->str, desclen);
			return;
		}
	}
	/* if no description was found, return miniupnpd as default */
	strncpy(desc, "miniupnpd", desclen);
}

/* add_redirect_rule2() */
int
add_redirect_rule2(const char * ifname, unsigned short eport,
                   const char * iaddr, unsigned short iport, int proto,
				   const char * desc)
{
	int r = addnatrule(proto, eport, iaddr, iport);
	if(r >= 0)
		add_redirect_desc(eport, proto, desc);
	return r;
}

int
add_filter_rule2(const char * ifname, const char * iaddr,
                 unsigned short eport, unsigned short iport,
                 int proto, const char * desc)
{
	return add_filter_rule(proto, iaddr, iport);
}

/* get_redirect_rule() 
 * returns -1 if the rule is not found */
int
get_redirect_rule(const char * ifname, unsigned short eport, int proto,
                  char * iaddr, int iaddrlen, unsigned short * iport,
                  char * desc, int desclen,
                  u_int64_t * packets, u_int64_t * bytes)
{
	int r = -1;
	IPTC_HANDLE h;
	const struct ipt_entry * e;
	const struct ipt_entry_target * target;
	const struct ip_nat_multi_range * mr;
	const struct ipt_entry_match *match;

	h = iptc_init("nat");
	if(!h)
	{
		syslog(LOG_ERR, "get_redirect_rule() : "
		                "iptc_init() failed : %s",
		       iptc_strerror(errno));
		return -1;
	}
	if(!iptc_is_chain(miniupnpd_nat_chain, h))
	{
		syslog(LOG_ERR, "chain %s not found", miniupnpd_nat_chain);
	}
	else
	{
#ifdef IPTABLES_143
		for(e = iptc_first_rule(miniupnpd_nat_chain, h);
		    e;
			e = iptc_next_rule(e, h))
#else
		for(e = iptc_first_rule(miniupnpd_nat_chain, &h);
		    e;
			e = iptc_next_rule(e, &h))
#endif
		{
			if(proto==e->ip.proto)
			{
				match = (const struct ipt_entry_match *)&e->elems;
				if(0 == strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN))
				{
					const struct ipt_tcp * info;
					info = (const struct ipt_tcp *)match->data;
					if(eport != info->dpts[0])
						continue;
				}
				else
				{
					const struct ipt_udp * info;
					info = (const struct ipt_udp *)match->data;
					if(eport != info->dpts[0])
						continue;
				}
				target = (void *)e + e->target_offset;
				//target = ipt_get_target(e);
				mr = (const struct ip_nat_multi_range *)&target->data[0];
				snprintip(iaddr, iaddrlen, ntohl(mr->range[0].min_ip));
				*iport = ntohs(mr->range[0].min.all);
				/*if(desc)
					strncpy(desc, "miniupnpd", desclen);*/
				get_redirect_desc(eport, proto, desc, desclen);
				if(packets)
					*packets = e->counters.pcnt;
				if(bytes)
					*bytes = e->counters.bcnt;
				r = 0;
				break;
			}
		}
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	return r;
}

/* get_redirect_rule_by_index() 
 * return -1 when the rule was not found */
int
get_redirect_rule_by_index(int index,
                           char * ifname, unsigned short * eport,
                           char * iaddr, int iaddrlen, unsigned short * iport,
                           int * proto, char * desc, int desclen,
                           u_int64_t * packets, u_int64_t * bytes)
{
	int r = -1;
	int i = 0;
	IPTC_HANDLE h;
	const struct ipt_entry * e;
	const struct ipt_entry_target * target;
	const struct ip_nat_multi_range * mr;
	const struct ipt_entry_match *match;

	h = iptc_init("nat");
	if(!h)
	{
		syslog(LOG_ERR, "get_redirect_rule_by_index() : "
		                "iptc_init() failed : %s",
		       iptc_strerror(errno));
		return -1;
	}
	if(!iptc_is_chain(miniupnpd_nat_chain, h))
	{
		syslog(LOG_ERR, "chain %s not found", miniupnpd_nat_chain);
	}
	else
	{
#ifdef IPTABLES_143
		for(e = iptc_first_rule(miniupnpd_nat_chain, h);
		    e;
			e = iptc_next_rule(e, h))
#else
		for(e = iptc_first_rule(miniupnpd_nat_chain, &h);
		    e;
			e = iptc_next_rule(e, &h))
#endif
		{
			if(i==index)
			{
				*proto = e->ip.proto;
				match = (const struct ipt_entry_match *)&e->elems;
				if(0 == strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN))
				{
					const struct ipt_tcp * info;
					info = (const struct ipt_tcp *)match->data;
					*eport = info->dpts[0];
				}
				else
				{
					const struct ipt_udp * info;
					info = (const struct ipt_udp *)match->data;
					*eport = info->dpts[0];
				}
				target = (void *)e + e->target_offset;
				mr = (const struct ip_nat_multi_range *)&target->data[0];
				snprintip(iaddr, iaddrlen, ntohl(mr->range[0].min_ip));
				*iport = ntohs(mr->range[0].min.all);
                /*if(desc)
				    strncpy(desc, "miniupnpd", desclen);*/
				get_redirect_desc(*eport, *proto, desc, desclen);
				if(packets)
					*packets = e->counters.pcnt;
				if(bytes)
					*bytes = e->counters.bcnt;
				r = 0;
				break;
			}
			i++;
		}
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	return r;
}

/* delete_rule_and_commit() :
 * subfunction used in delete_redirect_and_filter_rules() */
static int
delete_rule_and_commit(unsigned int index, IPTC_HANDLE h,
                       const char * miniupnpd_chain,
                       const char * logcaller)
{
	int r = 0;
#ifdef IPTABLES_143
	if(!iptc_delete_num_entry(miniupnpd_chain, index, h))
#else
	if(!iptc_delete_num_entry(miniupnpd_chain, index, &h))
#endif
	{
		syslog(LOG_ERR, "%s() : iptc_delete_num_entry(): %s\n",
	    	   logcaller, iptc_strerror(errno));
		r = -1;
	}
#ifdef IPTABLES_143
	else if(!iptc_commit(h))
#else
	else if(!iptc_commit(&h))
#endif
	{
		syslog(LOG_ERR, "%s() : iptc_commit(): %s\n",
	    	   logcaller, iptc_strerror(errno));
		r = -1;
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	return r;
}

/* delete_redirect_and_filter_rules()
 */
int
delete_redirect_and_filter_rules(unsigned short eport, int proto)
{
	int r = -1;
	unsigned index = 0;
	unsigned i = 0;
	IPTC_HANDLE h;
	const struct ipt_entry * e;
	const struct ipt_entry_match *match;

	h = iptc_init("nat");
	if(!h)
	{
		syslog(LOG_ERR, "delete_redirect_and_filter_rules() : "
		                "iptc_init() failed : %s",
		       iptc_strerror(errno));
		return -1;
	}
	if(!iptc_is_chain(miniupnpd_nat_chain, h))
	{
		syslog(LOG_ERR, "chain %s not found", miniupnpd_nat_chain);
	}
	else
	{
#ifdef IPTABLES_143
		for(e = iptc_first_rule(miniupnpd_nat_chain, h);
		    e;
			e = iptc_next_rule(e, h), i++)
#else
		for(e = iptc_first_rule(miniupnpd_nat_chain, &h);
		    e;
			e = iptc_next_rule(e, &h), i++)
#endif
		{
			if(proto==e->ip.proto)
			{
				match = (const struct ipt_entry_match *)&e->elems;
				if(0 == strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN))
				{
					const struct ipt_tcp * info;
					info = (const struct ipt_tcp *)match->data;
					if(eport != info->dpts[0])
						continue;
				}
				else
				{
					const struct ipt_udp * info;
					info = (const struct ipt_udp *)match->data;
					if(eport != info->dpts[0])
						continue;
				}
				index = i;
				r = 0;
				break;
			}
		}
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	if(r == 0)
	{
		syslog(LOG_INFO, "Trying to delete rules at index %u", index);
		/* Now delete both rules */
		h = iptc_init("nat");
		if(h)
		{
			r = delete_rule_and_commit(index, h, miniupnpd_nat_chain, "delete_redirect_rule");
		}
		if((r == 0) && (h = iptc_init("filter")))
		{
			r = delete_rule_and_commit(index, h, miniupnpd_forward_chain, "delete_filter_rule");
		}
	}
	del_redirect_desc(eport, proto);
	return r;
}

/* ==================================== */
/* TODO : add the -m state --state NEW,ESTABLISHED,RELATED 
 * only for the filter rule */
static struct ipt_entry_match *
get_tcp_match(unsigned short dport)
{
	struct ipt_entry_match *match;
	struct ipt_tcp * tcpinfo;
	size_t size;
	size =   IPT_ALIGN(sizeof(struct ipt_entry_match))
	       + IPT_ALIGN(sizeof(struct ipt_tcp));
	match = calloc(1, size);
	match->u.match_size = size;
	strncpy(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN);
	tcpinfo = (struct ipt_tcp *)match->data;
	tcpinfo->spts[0] = 0;		/* all source ports */
	tcpinfo->spts[1] = 0xFFFF;
	tcpinfo->dpts[0] = dport;	/* specified destination port */
	tcpinfo->dpts[1] = dport;
	return match;
}

static struct ipt_entry_match *
get_udp_match(unsigned short dport)
{
	struct ipt_entry_match *match;
	struct ipt_udp * udpinfo;
	size_t size;
	size =   IPT_ALIGN(sizeof(struct ipt_entry_match))
	       + IPT_ALIGN(sizeof(struct ipt_udp));
	match = calloc(1, size);
	match->u.match_size = size;
	strncpy(match->u.user.name, "udp", IPT_FUNCTION_MAXNAMELEN);
	udpinfo = (struct ipt_udp *)match->data;
	udpinfo->spts[0] = 0;		/* all source ports */
	udpinfo->spts[1] = 0xFFFF;
	udpinfo->dpts[0] = dport;	/* specified destination port */
	udpinfo->dpts[1] = dport;
	return match;
}

static struct ipt_entry_target *
get_dnat_target(const char * daddr, unsigned short dport)
{
	struct ipt_entry_target * target;
	struct ip_nat_multi_range * mr;
	struct ip_nat_range * range;
	size_t size;

	size =   IPT_ALIGN(sizeof(struct ipt_entry_target))
	       + IPT_ALIGN(sizeof(struct ip_nat_multi_range));
	target = calloc(1, size);
	target->u.target_size = size;
	strncpy(target->u.user.name, "DNAT", IPT_FUNCTION_MAXNAMELEN);
	/* one ip_nat_range already included in ip_nat_multi_range */
	mr = (struct ip_nat_multi_range *)&target->data[0];
	mr->rangesize = 1;
	range = &mr->range[0];
	range->min_ip = range->max_ip = inet_addr(daddr);
	range->flags |= IP_NAT_RANGE_MAP_IPS;
	range->min.all = range->max.all = htons(dport);
	range->flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
	return target;
}

/* iptc_init_verify_and_append()
 * return 0 on success, -1 on failure */
static int
iptc_init_verify_and_append(const char * table, const char * miniupnpd_chain, struct ipt_entry * e,
                            const char * logcaller)
{
	IPTC_HANDLE h;
	h = iptc_init(table);
	if(!h)
	{
		syslog(LOG_ERR, "%s : iptc_init() error : %s\n",
		       logcaller, iptc_strerror(errno));
		return -1;
	}
	if(!iptc_is_chain(miniupnpd_chain, h))
	{
		syslog(LOG_ERR, "%s : iptc_is_chain() error : %s\n",
		       logcaller, iptc_strerror(errno));
		if(h)
#ifdef IPTABLES_143
			iptc_free(h);
#else
			iptc_free(&h);
#endif
		return -1;
	}
	/* iptc_insert_entry(miniupnpd_chain, e, n, h/&h) could also be used */
#ifdef IPTABLES_143
	if(!iptc_append_entry(miniupnpd_chain, e, h))
#else
	if(!iptc_append_entry(miniupnpd_chain, e, &h))
#endif
	{
		syslog(LOG_ERR, "%s : iptc_append_entry() error : %s\n",
		       logcaller, iptc_strerror(errno));
		if(h)
#ifdef IPTABLES_143
			iptc_free(h);
#else
			iptc_free(&h);
#endif
		return -1;
	}
#ifdef IPTABLES_143
	if(!iptc_commit(h))
#else
	if(!iptc_commit(&h))
#endif
	{
		syslog(LOG_ERR, "%s : iptc_commit() error : %s\n",
		       logcaller, iptc_strerror(errno));
		if(h)
#ifdef IPTABLES_143
			iptc_free(h);
#else
			iptc_free(&h);
#endif
		return -1;
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	return 0;
}

/* add nat rule 
 * iptables -t nat -A MINIUPNPD -p proto --dport eport -j DNAT --to iaddr:iport
 * */
int
addnatrule(int proto, unsigned short eport,
           const char * iaddr, unsigned short iport)
{
	int r = 0;
	struct ipt_entry * e;
	struct ipt_entry_match *match = NULL;
	struct ipt_entry_target *target = NULL;

	e = calloc(1, sizeof(struct ipt_entry));
	e->ip.proto = proto;
	if(proto == IPPROTO_TCP)
	{
		match = get_tcp_match(eport);
	}
	else
	{
		match = get_udp_match(eport);
	}
	e->nfcache = NFC_IP_DST_PT;
	target = get_dnat_target(iaddr, iport);
	e->nfcache |= NFC_UNKNOWN;
	e = realloc(e, sizeof(struct ipt_entry)
	               + match->u.match_size
				   + target->u.target_size);
	memcpy(e->elems, match, match->u.match_size);
	memcpy(e->elems + match->u.match_size, target, target->u.target_size);
	e->target_offset = sizeof(struct ipt_entry)
	                   + match->u.match_size;
	e->next_offset = sizeof(struct ipt_entry)
	                 + match->u.match_size
					 + target->u.target_size;
	
	r = iptc_init_verify_and_append("nat", miniupnpd_nat_chain, e, "addnatrule()");
	free(target);
	free(match);
	free(e);
	return r;
}
/* ================================= */
static struct ipt_entry_target *
get_accept_target(void)
{
	struct ipt_entry_target * target = NULL;
	size_t size;
	size =   IPT_ALIGN(sizeof(struct ipt_entry_target))
	       + IPT_ALIGN(sizeof(int));
	target = calloc(1, size);
	target->u.user.target_size = size;
	strncpy(target->u.user.name, "ACCEPT", IPT_FUNCTION_MAXNAMELEN);
	return target;
}

/* add_filter_rule()
 * */
int
add_filter_rule(int proto, const char * iaddr, unsigned short iport)
{
	int r = 0;
	struct ipt_entry * e;
	struct ipt_entry_match *match = NULL;
	struct ipt_entry_target *target = NULL;

	e = calloc(1, sizeof(struct ipt_entry));
	e->ip.proto = proto;
	if(proto == IPPROTO_TCP)
	{
		match = get_tcp_match(iport);
	}
	else
	{
		match = get_udp_match(iport);
	}
	e->nfcache = NFC_IP_DST_PT;
	e->ip.dst.s_addr = inet_addr(iaddr);
	e->ip.dmsk.s_addr = INADDR_NONE;
	target = get_accept_target();
	e->nfcache |= NFC_UNKNOWN;
	e = realloc(e, sizeof(struct ipt_entry)
	               + match->u.match_size
				   + target->u.target_size);
	memcpy(e->elems, match, match->u.match_size);
	memcpy(e->elems + match->u.match_size, target, target->u.target_size);
	e->target_offset = sizeof(struct ipt_entry)
	                   + match->u.match_size;
	e->next_offset = sizeof(struct ipt_entry)
	                 + match->u.match_size
					 + target->u.target_size;
	
	r = iptc_init_verify_and_append("filter", miniupnpd_forward_chain, e, "add_filter_rule()");
	free(target);
	free(match);
	free(e);
	return r;
}

/* ================================ */
static int
print_match(const struct ipt_entry_match *match)
{
	printf("match %s\n", match->u.user.name);
	if(0 == strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN))
	{
		struct ipt_tcp * tcpinfo;
		tcpinfo = (struct ipt_tcp *)match->data;
		printf("srcport = %hu:%hu dstport = %hu:%hu\n",
		       tcpinfo->spts[0], tcpinfo->spts[1],
			   tcpinfo->dpts[0], tcpinfo->dpts[1]);
	}
	else if(0 == strncmp(match->u.user.name, "udp", IPT_FUNCTION_MAXNAMELEN))
	{
		struct ipt_udp * udpinfo;
		udpinfo = (struct ipt_udp *)match->data;
		printf("srcport = %hu:%hu dstport = %hu:%hu\n",
		       udpinfo->spts[0], udpinfo->spts[1],
			   udpinfo->dpts[0], udpinfo->dpts[1]);
	}
	return 0;
}

static void
print_iface(const char * iface, const unsigned char * mask, int invert)
{
	unsigned i;
	if(mask[0] == 0)
		return;
	if(invert)
		printf("! ");
	for(i=0; i<IFNAMSIZ; i++)
	{
		if(mask[i])
		{
			if(iface[i])
				putchar(iface[i]);
		}
		else
		{
			if(iface[i-1])
				putchar('+');
			break;
		}
	}
}

static void
printip(uint32_t ip)
{
	printf("%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xff,
	       (ip >> 8) & 0xff, ip & 0xff);
}

/* for debug */
/* read the "filter" and "nat" tables */
int
list_redirect_rule(const char * ifname)
{
	IPTC_HANDLE h;
	const struct ipt_entry * e;
	const struct ipt_entry_target * target;
	const struct ip_nat_multi_range * mr;
	const char * target_str;

	h = iptc_init("nat");
	if(!h)
	{
		printf("iptc_init() error : %s\n", iptc_strerror(errno));
		return -1;
	}
	if(!iptc_is_chain(miniupnpd_nat_chain, h))
	{
		printf("chain %s not found\n", miniupnpd_nat_chain);
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
		return -1;
	}
#ifdef IPTABLES_143
	for(e = iptc_first_rule(miniupnpd_nat_chain, h);
		e;
		e = iptc_next_rule(e, h))
	{
		target_str = iptc_get_target(e, h);
#else
	for(e = iptc_first_rule(miniupnpd_nat_chain, &h);
		e;
		e = iptc_next_rule(e, &h))
	{
		target_str = iptc_get_target(e, &h);
#endif
		printf("===\n");
		printf("src = %s%s/%s\n", (e->ip.invflags & IPT_INV_SRCIP)?"! ":"",
		       inet_ntoa(e->ip.src), inet_ntoa(e->ip.smsk));
		printf("dst = %s%s/%s\n", (e->ip.invflags & IPT_INV_DSTIP)?"! ":"",
		       inet_ntoa(e->ip.dst), inet_ntoa(e->ip.dmsk));
		/*printf("in_if = %s  out_if = %s\n", e->ip.iniface, e->ip.outiface);*/
		printf("in_if = ");
		print_iface(e->ip.iniface, e->ip.iniface_mask,
		            e->ip.invflags & IPT_INV_VIA_IN);
		printf(" out_if = ");
		print_iface(e->ip.outiface, e->ip.outiface_mask,
		            e->ip.invflags & IPT_INV_VIA_OUT);
		printf("\n");
		printf("ip.proto = %s%d\n", (e->ip.invflags & IPT_INV_PROTO)?"! ":"",
		       e->ip.proto);
		/* display matches stuff */
		if(e->target_offset)
		{
			IPT_MATCH_ITERATE(e, print_match);
			/*printf("\n");*/
		}
		printf("target = %s\n", target_str);
		target = (void *)e + e->target_offset;
		mr = (const struct ip_nat_multi_range *)&target->data[0];
		printf("ips ");
		printip(ntohl(mr->range[0].min_ip));
		printf(" ");
		printip(ntohl(mr->range[0].max_ip));
		printf("\nports %hu %hu\n", ntohs(mr->range[0].min.all),
		          ntohs(mr->range[0].max.all));
		printf("flags = %x\n", mr->range[0].flags);
	}
	if(h)
#ifdef IPTABLES_143
		iptc_free(h);
#else
		iptc_free(&h);
#endif
	return 0;
}


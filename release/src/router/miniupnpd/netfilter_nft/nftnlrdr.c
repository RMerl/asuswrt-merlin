/*
 * MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2015 Tomofumi Hayashi
 * 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include <linux/version.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>

#include <libmnl/libmnl.h>
#include <libnftnl/rule.h>
#include <libnftnl/expr.h>

#include "tiny_nf_nat.h"

#include "../macros.h"
#include "../config.h"
#include "nftnlrdr.h"
#include "../upnpglobalvars.h"

#include "nftnlrdr_misc.h"

#ifdef DEBUG
#define d_printf(x) do { printf x; } while (0)
#else
#define d_printf(x)
#endif

/* dummy init and shutdown functions */
int init_redirect(void)
{
	return 0;
}

void shutdown_redirect(void)
{
	return;
}


int
add_redirect_rule2(const char * ifname,
		   const char * rhost, unsigned short eport,
		   const char * iaddr, unsigned short iport, int proto,
		   const char * desc, unsigned int timestamp)
{
	struct nft_rule *r;
	UNUSED(rhost);
	UNUSED(timestamp);
        d_printf(("add redirect rule2(%s, %s, %u, %s, %u, %d, %s)!\n",
	          ifname, rhost, eport, iaddr, iport, proto, desc));
	r = rule_set_dnat(NFPROTO_IPV4, ifname, proto,
			  0, eport, 
			  inet_addr(iaddr), iport,  desc, NULL);
	return nft_send_request(r, NFT_MSG_NEWRULE);
}

/*
 * This function submit the rule as following: 
 * nft add rule nat miniupnpd-pcp-peer ip 
 *    saddr <iaddr> ip daddr <rhost> tcp sport <iport> 
 *    tcp dport <rport> snat <eaddr>:<eport>
 */
int
add_peer_redirect_rule2(const char * ifname,
			const char * rhost, unsigned short rport,
			const char * eaddr, unsigned short eport,
			const char * iaddr, unsigned short iport, int proto,
			const char * desc, unsigned int timestamp)
{
	struct nft_rule *r;
	UNUSED(ifname); UNUSED(timestamp);

        d_printf(("add peer redirect rule2()!\n"));
	r = rule_set_snat(NFPROTO_IPV4, proto, 
			  inet_addr(rhost), rport, 
			  inet_addr(eaddr), eport, 
			  inet_addr(iaddr), iport, desc, NULL);

	return nft_send_request(r, NFT_MSG_NEWRULE);
}

/*
 * This function submit the rule as following: 
 * nft add rule filter miniupnpd 
 *    ip daddr <iaddr> tcp dport <iport> accept
 * 
 */
int
add_filter_rule2(const char * ifname,
		 const char * rhost, const char * iaddr,
		 unsigned short eport, unsigned short iport,
		 int proto, const char * desc)
{
	struct nft_rule *r = NULL;
	in_addr_t rhost_addr = 0;

	d_printf(("add_filter_rule2(%s, %s, %s, %d, %d, %d, %s)\n",
	          ifname, rhost, iaddr, eport, iport, proto, desc));
	if (rhost != NULL && strcmp(rhost, "") != 0) {
            rhost_addr = inet_addr(rhost);
        }
	r = rule_set_filter(NFPROTO_IPV4, ifname, proto,
			    rhost_addr, inet_addr(iaddr), eport, iport,
			    desc, 0);
	return nft_send_request(r, NFT_MSG_NEWRULE);
}

/*
 * add_peer_dscp_rule2() is not supported due to nft does not support
 * dscp set.
 */
int
add_peer_dscp_rule2(const char * ifname,
		    const char * rhost, unsigned short rport,
		    unsigned char dscp,
		    const char * iaddr, unsigned short iport, int proto,
		    const char * desc, unsigned int timestamp)
{
	UNUSED(ifname); UNUSED(rhost); UNUSED(rport); 
	UNUSED(dscp); UNUSED(iaddr); UNUSED(iport); UNUSED(proto);
	UNUSED(desc); UNUSED(timestamp);
	syslog(LOG_ERR, "add_peer_dscp_rule2: not supported");
	return 0;
}

/*
 * Clear all rules corresponding eport/proto
 */
int
delete_redirect_and_filter_rules(unsigned short eport, int proto)
{
	rule_t *p;
	struct nft_rule *r = NULL;
        in_addr_t iaddr = 0;
        uint16_t iport = 0;
        extern void print_rule(rule_t *r) ;

	d_printf(("delete_redirect_and_filter_rules(%d %d)\n", eport, proto));
	reflesh_nft_cache(NFPROTO_IPV4);
	LIST_FOREACH(p, &head, entry) {
		if (p->eport == eport && p->proto == proto && 
		    (p->type == RULE_NAT || p->type == RULE_SNAT)) {
			iaddr = p->iaddr;
			iport = p->iport;

			r = rule_del_handle(p);
			/* Todo: send bulk request */
			nft_send_request(r, NFT_MSG_DELRULE);
			break;
		}
	}

	if (iaddr == 0 && iport == 0) {
		return -1;
	}
	reflesh_nft_cache(NFPROTO_IPV4);
	LIST_FOREACH(p, &head, entry) {
		if (p->eport == iport && 
		    p->iaddr == iaddr && p->type == RULE_FILTER) {
			r = rule_del_handle(p);
			/* Todo: send bulk request */
			nft_send_request(r, NFT_MSG_DELRULE);
			break;
		}
	}

	return 0;
}

/* 
 * get peer by index as array. 
 * return -1 when not found.
 */
int
get_peer_rule_by_index(int index,
		       char * ifname, unsigned short * eport,
		       char * iaddr, int iaddrlen, unsigned short * iport,
		       int * proto, char * desc, int desclen,
		       char * rhost, int rhostlen, unsigned short * rport,
		       unsigned int * timestamp,
		       u_int64_t * packets, u_int64_t * bytes)
{
	int i;
	struct in_addr addr;
	char *addr_str;
	rule_t *r;
	UNUSED(timestamp); UNUSED(packets); UNUSED(bytes);

        d_printf(("get_peer_rule_by_index()\n"));
	reflesh_nft_cache(NFPROTO_IPV4);
	if (peer_cache == NULL) {
		return -1;
	}

	for (i = 0; peer_cache[i] != NULL; i++) {
		if (index == i) {
			r = peer_cache[i];
			if (ifname != NULL) {
				if_indextoname(r->ingress_ifidx, ifname);
			}
			if (eport != NULL) {
				*eport = r->eport;
			}
			if (iaddr != NULL) {
				addr.s_addr = r->iaddr;
				addr_str = inet_ntoa(addr);
				strncpy(iaddr , addr_str, iaddrlen);
			}
			if (iport != NULL) {
				*iport = r->iport;
			}
			if (proto != NULL) {
				*proto = r->proto;
			}
			if (rhost != NULL) {
				addr.s_addr = r->rhost;
				addr_str = inet_ntoa(addr);
				strncpy(iaddr , addr_str, rhostlen);
			}
			if (rport != NULL) {
				*rport = r->rport;
			}
			if (desc != NULL) {
				strncpy(desc, r->desc, desclen);
			}

			/* 
			 * TODO: Implement counter in case of add {nat,filter} 
			 */
			return 0;
		}
	}
	return -1;
}

/* 
 * get_redirect_rule()
 * returns -1 if the rule is not found 
 */
int
get_redirect_rule(const char * ifname, unsigned short eport, int proto,
		  char * iaddr, int iaddrlen, unsigned short * iport,
		  char * desc, int desclen,
		  char * rhost, int rhostlen,
		  unsigned int * timestamp,
		  u_int64_t * packets, u_int64_t * bytes)
{
	return get_nat_redirect_rule(NFT_TABLE_NAT,
	                             ifname, eport, proto,
	                             iaddr, iaddrlen, iport,
	                             desc, desclen,
	                             rhost, rhostlen,
	                             timestamp, packets, bytes);
}

/*
 * get_redirect_rule_by_index()
 * return -1 when the rule was not found 
 */
int
get_redirect_rule_by_index(int index,
			   char * ifname, unsigned short * eport,
			   char * iaddr, int iaddrlen, unsigned short * iport,
			   int * proto, char * desc, int desclen,
			   char * rhost, int rhostlen,
			   unsigned int * timestamp,
			   u_int64_t * packets, u_int64_t * bytes)
{
	int i;
	struct in_addr addr;
	char *addr_str;
	rule_t *r;
	UNUSED(timestamp); UNUSED(packets); UNUSED(bytes);

        d_printf(("get_redirect_rule_by_index()\n"));
	reflesh_nft_cache(NFPROTO_IPV4);
	if (redirect_cache == NULL) {
		return -1;
	}

	for (i = 0; redirect_cache[i] != NULL; i++) {
		if (index == i) {
			r = redirect_cache[i];
			if (ifname != NULL) {
				if_indextoname(r->ingress_ifidx, ifname);
			}
			if (eport != NULL) {
				*eport = r->eport;
			}
			if (iaddr != NULL) {
				addr.s_addr = r->iaddr;
				addr_str = inet_ntoa(addr);
				strncpy(iaddr , addr_str, iaddrlen);
			}
			if (iport != NULL) {
				*iport = r->iport;
			}
			if (proto != NULL) {
				*proto = r->proto;
			}
			if (rhost != NULL) {
				addr.s_addr = r->rhost;
				addr_str = inet_ntoa(addr);
				strncpy(iaddr , addr_str, rhostlen);
			}
			if (desc != NULL && r->desc) {
				strncpy(desc, r->desc, desclen);
			}

			/* 
			 * TODO: Implement counter in case of add {nat,filter}
			 */
			return 0;
		}
	}
	return -1;
}

/*
 * return -1 not found.
 * return 0 found
 */
int
get_nat_redirect_rule(const char * nat_chain_name, const char * ifname,
		      unsigned short eport, int proto,
		      char * iaddr, int iaddrlen, unsigned short * iport,
		      char * desc, int desclen,
		      char * rhost, int rhostlen,
		      unsigned int * timestamp,
		      u_int64_t * packets, u_int64_t * bytes)
{
	rule_t *p;
	struct in_addr addr;
	char *addr_str;
	UNUSED(nat_chain_name);
	UNUSED(ifname);
	UNUSED(iaddrlen);
	UNUSED(timestamp);
	UNUSED(packets);
	UNUSED(bytes);

        d_printf(("get_nat_redirect_rule()\n"));
	reflesh_nft_cache(NFPROTO_IPV4);

	LIST_FOREACH(p, &head, entry) {
		if (p->proto == proto &&
		    p->eport == eport) {
			if (p->rhost && rhost) {
				addr.s_addr = p->rhost;
				addr_str = inet_ntoa(addr);
				strncpy(iaddr , addr_str, rhostlen);

			}
			if (desc != NULL && p->desc) {
				strncpy(desc, p->desc, desclen);
			}
			*iport = p->iport;
			return 0;
		}
	}

	return -1;
}

/* 
 * return an (malloc'ed) array of "external" port for which there is
 * a port mapping. number is the size of the array 
 */
unsigned short *
get_portmappings_in_range(unsigned short startport, unsigned short endport,
			  int proto, unsigned int * number)
{
	uint32_t capacity;
	rule_t *p;
	unsigned short *array;
	unsigned short *tmp;

        d_printf(("get_portmappings_in_range()\n"));
	*number = 0;
	capacity = 128;
	array = calloc(capacity, sizeof(unsigned short));

	if (array == NULL) {
		syslog(LOG_ERR, "get_portmappings_in_range(): calloc error");
		return NULL;
	}

	LIST_FOREACH(p, &head, entry) {
		if (p->proto == proto &&
		    startport <= p->eport && 
		    p->eport <= endport) {

			if (*number >= capacity) {
				tmp = realloc(array, 
					      sizeof(unsigned short)*capacity);
				if (tmp == NULL) {
					syslog(LOG_ERR,
					       "get_portmappings_in_range(): "
					       "realloc(%u) error",
					       (unsigned)sizeof(unsigned short)*capacity);
					*number = 0;
					free(array);
					return NULL;   
				}
				array = tmp;
			}
			array[*number] = p->eport;
			(*number)++;
		}
	}
	return array;
}

/* for debug */
/* read the "filter" and "nat" tables */
int
list_redirect_rule(const char * ifname)
{
	rule_t *p;
	UNUSED(ifname);

	reflesh_nft_cache(NFPROTO_IPV4);

	LIST_FOREACH(p, &head, entry) {
		print_rule(p);
	}

	return -1;
	return 0;
}


#if 0
/* delete_rule_and_commit() :
 * subfunction used in delete_redirect_and_filter_rules() */
static int
delete_rule_and_commit(unsigned int index, IPTC_HANDLE h,
		       const char * miniupnpd_chain,
		       const char * logcaller)
{
/* TODO: Implement it */
}

/* TODO: Implement it */
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
	return ;
}

#ifdef DEBUG
static void
printip(uint32_t ip)
{
	printf("%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xff,
	       (ip >> 8) & 0xff, ip & 0xff);
}
#endif

#endif /* if 0 */

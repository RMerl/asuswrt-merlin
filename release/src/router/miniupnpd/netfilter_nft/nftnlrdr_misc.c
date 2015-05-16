/*
 * MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2015 Tomofumi Hayashi
 * 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <stddef.h>
#include <syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <errno.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>

#include <libmnl/libmnl.h>
#include <libnftnl/rule.h>
#include <libnftnl/expr.h>

#include "nftnlrdr_misc.h"
#include "../macros.h"
#include "../upnpglobalvars.h"

#ifdef DEBUG
#define d_printf(x) do { printf x; } while (0)
#else
#define d_printf(x)
#endif

#define RULE_CACHE_INVALID  0
#define RULE_CACHE_VALID    1

const char * miniupnpd_nft_nat_chain = "miniupnpd";
const char * miniupnpd_nft_peer_chain = "miniupnpd-pcp-peer";
const char * miniupnpd_nft_forward_chain = "miniupnpd";

static struct mnl_socket *nl = NULL;
struct rule_list head = LIST_HEAD_INITIALIZER(head);
static uint32_t rule_list_validate = RULE_CACHE_INVALID;

uint32_t rule_list_length = 0;
uint32_t rule_list_peer_length = 0;

rule_t **redirect_cache;
rule_t **peer_cache;


static const char *
get_family_string(uint32_t family)
{
	switch (family) {
	case NFPROTO_IPV4:
		return "ipv4";
	}
	return "unknown family";
}

static const char *
get_proto_string(uint32_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_UDP:
		return "udp";
	}
	return "unknown proto";
}

static const char *
get_verdict_string(uint32_t val)
{
	switch (val) {
	case NF_ACCEPT:
		return "accept";
	case NF_DROP:
		return "drop";
	default:
		return "unknown verdict";
	}
}

void 
print_rule(rule_t *r) 
{
	struct in_addr addr;
	char *iaddr_str = NULL, *rhost_str = NULL, *eaddr_str = NULL;
	char ifname_buf[IF_NAMESIZE];

	switch (r->type) {
	case RULE_NAT:
		if (r->iaddr != 0) {
			addr.s_addr = r->iaddr;
			iaddr_str = strdupa(inet_ntoa(addr));
		}
		if (r->rhost != 0) {
			addr.s_addr = r->rhost;
			rhost_str = strdupa(inet_ntoa(addr));
		}
		if (r->eaddr != 0) {
			addr.s_addr = r->eaddr;
			eaddr_str = strdupa(inet_ntoa(addr));
		}
		if (r->nat_type == NFT_NAT_DNAT) {
			printf("%"PRIu64":[%s/%s] iif %s, %s/%s, %d -> "
			       "%s:%d (%s)\n",
			       r->handle,
			       r->table, r->chain, 
			       if_indextoname(r->ingress_ifidx, ifname_buf),
			       get_family_string(r->family),
			       get_proto_string(r->proto), r->eport, 
			       iaddr_str, r->iport,
			       r->desc);
		} else if (r->nat_type == NFT_NAT_SNAT) {
			printf("%"PRIu64":[%s/%s] "
			       "nat type:%d, family:%d, ifidx: %d, "
			       "eaddr: %s, eport:%d, "
			       "proto:%d, iaddr: %s, "
			       "iport:%d, rhost:%s rport:%d (%s)\n",
			       r->handle, r->table, r->chain,
			       r->nat_type, r->family, r->ingress_ifidx,
			       eaddr_str, r->eport,
			       r->proto, iaddr_str, r->iport, 
			       rhost_str, r->rport,
			       r->desc);
		} else {
			printf("%"PRIu64":[%s/%s] "
			       "nat type:%d, family:%d, ifidx: %d, "
			       "eaddr: %s, eport:%d, "
			       "proto:%d, iaddr: %s, iport:%d, rhost:%s (%s)\n",
			       r->handle, r->table, r->chain,
			       r->nat_type, r->family, r->ingress_ifidx,
			       eaddr_str, r->eport,
			       r->proto, iaddr_str, r->iport, rhost_str,
			       r->desc);
		}
		break;
	case RULE_FILTER:
		if (r->iaddr != 0) {
			addr.s_addr = r->iaddr;
			iaddr_str = strdupa(inet_ntoa(addr));
		}
		if (r->rhost != 0) {
			addr.s_addr = r->rhost;
			rhost_str = strdupa(inet_ntoa(addr));
		}
		printf("%"PRIu64":[%s/%s] %s/%s, %s %s:%d: %s (%s)\n",
		       r->handle, r->table, r->chain, 
		       get_family_string(r->family), get_proto_string(r->proto),
                       rhost_str, 
		       iaddr_str, r->eport, 
		       get_verdict_string(r->filter_action),
		       r->desc);
		break;
	case RULE_COUNTER:
		if (r->iaddr != 0) {
			addr.s_addr = r->iaddr;
			iaddr_str = strdupa(inet_ntoa(addr));
		}
		if (r->rhost != 0) {
			addr.s_addr = r->iaddr;
			rhost_str = strdupa(inet_ntoa(addr));
		}
		printf("%"PRIu64":[%s/%s] %s/%s, %s:%d: "
		       "packets:%"PRIu64", bytes:%"PRIu64"\n",
		       r->handle, r->table, r->chain, 
		       get_family_string(r->family), get_proto_string(r->proto),
		       iaddr_str, r->eport, r->packets, r->bytes);
		break;
	default:
		printf("nftables: unknown type: %d\n", r->type);
	}
}

static enum rule_reg_type *
get_reg_type_ptr(rule_t *r, uint32_t dreg) 
{
	switch (dreg) {
	case NFT_REG_1:
		return &r->reg1_type;
	case NFT_REG_2:
		return &r->reg2_type;
	default:
		return NULL;
	}
}

static uint32_t *
get_reg_val_ptr(rule_t *r, uint32_t dreg) 
{
	switch (dreg) {
	case NFT_REG_1:
		return &r->reg1_val;
	case NFT_REG_2:
		return &r->reg2_val;
	default:
		return NULL;
	}
}

static void
set_reg (rule_t *r, uint32_t dreg, enum rule_reg_type type, uint32_t val)
{
	if (dreg == NFT_REG_1) {
		r->reg1_type = type;
		if (type == RULE_REG_IMM_VAL) {
			r->reg1_val = val;
		}
	} else if (dreg == NFT_REG_2) {
		r->reg2_type = type;
		if (type == RULE_REG_IMM_VAL) {
			r->reg2_val = val;
		}
	} else if (dreg == NFT_REG_VERDICT) {
		if (r->type == RULE_FILTER) {
			r->filter_action = val;
		}
	} else {
		syslog(LOG_ERR, "%s: unknown reg:%d", "set_reg", dreg);
	}
	return ;
}

static inline void
parse_rule_immediate(struct nft_rule_expr *e, rule_t *r)
{
	uint32_t dreg, reg_val, reg_len;

	dreg = nft_rule_expr_get_u32(e, NFT_EXPR_IMM_DREG);

	if (dreg == NFT_REG_VERDICT) {
		reg_val = nft_rule_expr_get_u32(e, NFT_EXPR_IMM_VERDICT);
	} else {
		reg_val = *(uint32_t *)nft_rule_expr_get(e,
							 NFT_EXPR_IMM_DATA,
							 &reg_len);
	}

	set_reg(r, dreg, RULE_REG_IMM_VAL, reg_val);
	return;
}

static inline void
parse_rule_counter(struct nft_rule_expr *e, rule_t *r)
{
	r->type = RULE_COUNTER;
	r->bytes = nft_rule_expr_get_u64(e, NFT_EXPR_CTR_BYTES);
	r->packets = nft_rule_expr_get_u64(e, NFT_EXPR_CTR_PACKETS);

	return;
}

static inline void
parse_rule_meta(struct nft_rule_expr *e, rule_t *r)
{
	uint32_t key = nft_rule_expr_get_u32(e, NFT_EXPR_META_KEY);
	uint32_t dreg = nft_rule_expr_get_u32(e, NFT_EXPR_META_DREG);
	enum rule_reg_type reg_type;

	switch (key) {
	case NFT_META_IIF:
		reg_type = RULE_REG_IIF;
		set_reg(r, dreg, reg_type, 0);
		return ;
		
	case NFT_META_OIF:
		reg_type = RULE_REG_IIF;
		set_reg(r, dreg, reg_type, 0);
		return ;
		
	}
	syslog(LOG_DEBUG, "parse_rule_meta :Not support key %d\n", key);

	return;
}

static inline void
parse_rule_nat(struct nft_rule_expr *e, rule_t *r)
{
	uint32_t addr_min_reg, addr_max_reg, proto_min_reg, proto_max_reg;
	uint16_t proto_min_val;
	r->type = RULE_NAT;

	r->nat_type = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_TYPE);
	r->family = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_FAMILY);
	addr_min_reg = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_REG_ADDR_MIN);
	addr_max_reg = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_REG_ADDR_MAX);
	proto_min_reg = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_REG_PROTO_MIN);
	proto_max_reg = nft_rule_expr_get_u32(e, NFT_EXPR_NAT_REG_PROTO_MAX);

	if (addr_min_reg != addr_max_reg ||
	    proto_min_reg != proto_max_reg) {
		syslog(LOG_ERR, "Unsupport proto/addr range for NAT");
	}

	proto_min_val = htons((uint16_t)*get_reg_val_ptr(r, proto_min_reg));
	if (r->nat_type == NFT_NAT_DNAT) {
		r->iaddr = (in_addr_t)*get_reg_val_ptr(r, addr_min_reg);
		r->iport = proto_min_val;
	} else if (r->nat_type == NFT_NAT_SNAT) {
		r->eaddr = (in_addr_t)*get_reg_val_ptr(r, addr_min_reg);
		if (proto_min_reg == NFT_REG_1) { 
			r->eport = proto_min_val;
		}
	}

	set_reg(r, NFT_REG_1, RULE_REG_NONE, 0);
	set_reg(r, NFT_REG_2, RULE_REG_NONE, 0);
	return;
}

static inline void
parse_rule_payload(struct nft_rule_expr *e, rule_t *r)
{
	uint32_t  base, dreg, offset, len;
	uint32_t  *regptr; 

	dreg = nft_rule_expr_get_u32(e, NFT_EXPR_PAYLOAD_DREG);
	base = nft_rule_expr_get_u32(e, NFT_EXPR_PAYLOAD_BASE);
	offset = nft_rule_expr_get_u32(e, NFT_EXPR_PAYLOAD_OFFSET);
	len = nft_rule_expr_get_u32(e, NFT_EXPR_PAYLOAD_LEN);
	regptr = get_reg_type_ptr(r, dreg);

	switch (base) {
	case NFT_PAYLOAD_NETWORK_HEADER:
		if (offset == offsetof(struct iphdr, daddr) &&
		    len == sizeof(in_addr_t)) {
			*regptr = RULE_REG_IP_DEST_ADDR;
			return;
		} else if (offset == offsetof(struct iphdr, saddr) &&
			   len == sizeof(in_addr_t)) {
			*regptr = RULE_REG_IP_SRC_ADDR;
			return;
		} else if (offset == offsetof(struct iphdr, saddr) &&
			   len == sizeof(in_addr_t) * 2) {
			*regptr = RULE_REG_IP_SD_ADDR;
			return;
		} else if (offset == offsetof(struct iphdr, protocol) &&
			   len == sizeof(uint8_t)) {
			*regptr = RULE_REG_IP_PROTO;
			return;
		}
	case NFT_PAYLOAD_TRANSPORT_HEADER:
		if (offset == offsetof(struct tcphdr, dest) &&
		    len == sizeof(uint16_t)) {
			*regptr = RULE_REG_TCP_DPORT;
			return;
		} else if (offset == offsetof(struct tcphdr, source) &&
			   len == sizeof(uint16_t) * 2) {
			*regptr = RULE_REG_TCP_SD_PORT;
			return;
		}
	}
	syslog(LOG_DEBUG, 
	       "Unsupport payload: (dreg:%d, base:%d, offset:%d, len:%d)",
	       dreg, base, offset, len);
	return;
}

/*
 *
 * Note: Currently support only NFT_REG_1
 */
static inline void
parse_rule_cmp(struct nft_rule_expr *e, rule_t *r) {
	uint32_t data_len;
	void *data_val;
	uint32_t op, sreg;
	uint16_t *ports;
	in_addr_t *addrp;

	data_val = (void *)nft_rule_expr_get(e, NFT_EXPR_CMP_DATA, &data_len);
	sreg = nft_rule_expr_get_u32(e, NFT_EXPR_CMP_SREG);
	op = nft_rule_expr_get_u32(e, NFT_EXPR_CMP_OP);

	if (sreg != NFT_REG_1) {
		syslog(LOG_ERR, "parse_rule_cmp: Unsupport reg:%d", sreg);
		return;
	}

	switch (r->reg1_type) {
	case RULE_REG_IIF:
		if (data_len == sizeof(uint32_t) && op == NFT_CMP_EQ) {
			r->ingress_ifidx = *(uint32_t *)data_val;
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_IP_SRC_ADDR:
		if (data_len == sizeof(in_addr_t) && op == NFT_CMP_EQ) {
			r->rhost = *(in_addr_t *)data_val;
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_IP_DEST_ADDR:
		if (data_len == sizeof(in_addr_t) && op == NFT_CMP_EQ) {
			if (r->type == RULE_FILTER) {
				r->iaddr = *(in_addr_t *)data_val;
			} else {
				r->rhost = *(in_addr_t *)data_val;
			}
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_IP_SD_ADDR:
		if (data_len == sizeof(in_addr_t)  * 2 && op == NFT_CMP_EQ) {
			addrp = (in_addr_t *)data_val;
			r->iaddr = addrp[0];
			r->rhost = addrp[1];
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_IP_PROTO:
		if (data_len == sizeof(uint8_t) && op == NFT_CMP_EQ) {
			r->proto = *(uint8_t *)data_val;
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_TCP_DPORT:
		if (data_len == sizeof(uint16_t) && op == NFT_CMP_EQ) {
			r->eport = ntohs(*(uint16_t *)data_val);
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	case RULE_REG_TCP_SD_PORT:
		if (data_len == sizeof(uint16_t) * 2 && op == NFT_CMP_EQ) {
			ports = (uint16_t *)data_val;
			r->iport = ntohs(ports[0]);
			r->rport = ntohs(ports[1]);
			r->reg1_type = RULE_REG_NONE;
			return;
		}
	default:
		break;
	}
	syslog(LOG_DEBUG, "Unknown cmp (r1type:%d, data_len:%d, op:%d)",
	       r->reg1_type, data_len, op);
	return;
}

static int
rule_expr_cb(struct nft_rule_expr *e, void *data) 
{
	rule_t *r = data;
	const char *attr_name = nft_rule_expr_get_str(e, 
						      NFT_RULE_EXPR_ATTR_NAME);

	if (strncmp("cmp", attr_name, sizeof("cmp")) == 0) {
		parse_rule_cmp(e, r);
	} else if (strncmp("nat", attr_name, sizeof("nat")) == 0) {
		parse_rule_nat(e, r);
	} else if (strncmp("meta", attr_name, sizeof("meta")) == 0) {
		parse_rule_meta(e, r);
	} else if (strncmp("counter", attr_name, sizeof("counter")) == 0) {
		parse_rule_counter(e, r);
	} else if (strncmp("payload", attr_name, sizeof("payload")) == 0) {
		parse_rule_payload(e, r);
	} else if (strncmp("immediate", attr_name, sizeof("immediate")) == 0) {
		parse_rule_immediate(e, r);
	} else {
		syslog(LOG_ERR, "unknown attr: %s\n", attr_name);
	} 
	return MNL_CB_OK;
}


static int
table_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nft_rule *t;
	uint32_t len;
	struct nft_rule_expr *expr;
	struct nft_rule_expr_iter *itr;
	rule_t *r;
	char *chain;
	UNUSED(data);

	r = malloc(sizeof(rule_t)); 

	memset(r, 0, sizeof(rule_t));
	t = nft_rule_alloc();
	if (t == NULL) {
		perror("OOM");
		goto err;
	}

	if (nft_rule_nlmsg_parse(nlh, t) < 0) {
		perror("nft_rule_nlmsg_parse");
		goto err_free;
	}

	chain = (char *)nft_rule_attr_get_data(t, NFT_RULE_ATTR_CHAIN, &len);
	if (strcmp(chain, miniupnpd_nft_nat_chain) != 0 &&
	    strcmp(chain, miniupnpd_nft_peer_chain) != 0 &&
	    strcmp(chain, miniupnpd_nft_forward_chain) != 0) {
		goto rule_skip;
	}

	r->table = strdup(
		(char *)nft_rule_attr_get_data(t, NFT_RULE_ATTR_TABLE, &len));
	r->chain = strdup(chain);
	r->family = *(uint32_t*)nft_rule_attr_get_data(t, NFT_RULE_ATTR_FAMILY,
						       &len);
	r->desc = (char *)nft_rule_attr_get_data(t, NFT_RULE_ATTR_USERDATA,
						 &len);
	r->handle = *(uint32_t*)nft_rule_attr_get_data(t,
						       NFT_RULE_ATTR_HANDLE,
						       &len);
	if (strcmp(r->table, NFT_TABLE_NAT) == 0) {
		r->type = RULE_NAT;
	} else if (strcmp(r->table, NFT_TABLE_FILTER) == 0) {
		r->type = RULE_FILTER;
	} 
	if (strcmp(r->chain, miniupnpd_nft_peer_chain) == 0) {
		rule_list_peer_length++;
	}

	itr = nft_rule_expr_iter_create(t);

	while ((expr = nft_rule_expr_iter_next(itr)) != NULL) {
		rule_expr_cb(expr, r);
	}

	if (r->type == RULE_NONE) {
		free(r);
	} else {
		LIST_INSERT_HEAD(&head, r, entry);
		rule_list_length++;
	}

rule_skip:
err_free:
	nft_rule_free(t);
err:
	return MNL_CB_OK;
}

void
reflesh_nft_redirect_cache(void)
{
	rule_t *p;
	int i;
	uint32_t len;

	if (redirect_cache != NULL) {
		free(redirect_cache);
	}
	len = rule_list_length - rule_list_peer_length;
	if (len == 0) {
		redirect_cache = NULL;
		return;
	} 

	redirect_cache = (rule_t **)malloc(sizeof(rule_t *) * len);
	bzero(redirect_cache, sizeof(rule_t *) * len);

	i = 0;
	LIST_FOREACH(p, &head, entry) {
		if (strcmp(p->chain, miniupnpd_nft_nat_chain) == 0 && 
		    (p->type == RULE_NAT || p->type == RULE_SNAT)) {
			redirect_cache[i] = p;
			i++;
		}
	}

	return;
}

void
reflesh_nft_peer_cache(void)
{
	rule_t *p;
	int i;

	if (peer_cache != NULL) {
		free(peer_cache);
	}
	if (rule_list_peer_length == 0) {
		peer_cache = NULL;
		return;
	}
	peer_cache = (rule_t **)malloc(
		sizeof(rule_t *) * rule_list_peer_length);
	bzero(peer_cache, sizeof(rule_t *) * rule_list_peer_length);

	i = 0;
	LIST_FOREACH(p, &head, entry) {
		if (strcmp(p->chain, miniupnpd_nft_peer_chain) == 0) {
			peer_cache[i] = p;
			i++;
		}
	}

	return;
}

void
reflesh_nft_cache(uint32_t family)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	uint32_t portid, seq, type = NFT_OUTPUT_DEFAULT;
	struct nft_rule *t;
	rule_t *p1, *p2;
	int ret;

	if (rule_list_validate == RULE_CACHE_VALID) {
		return;
	}

	t = NULL;
	p1 = LIST_FIRST(&head);
	if (p1 != NULL) {
		while(p1 != NULL) {
			p2 = (rule_t *)LIST_NEXT(p1, entry);
			if (p1->desc != NULL) {
				free(p1->desc);
			}
			if (p1->table != NULL) {
				free(p1->table);
			}
			if (p1->chain != NULL) {
				free(p1->chain);
			}
			free(p1);
			p1 = p2;
		}
	}
	LIST_INIT(&head);

	t = nft_rule_alloc();
	if (t == NULL) {
		perror("OOM");
		exit(EXIT_FAILURE);
	}

	seq = time(NULL);
	nlh = nft_rule_nlmsg_build_hdr(buf, NFT_MSG_GETRULE, family,
				       NLM_F_DUMP, seq);
	nft_rule_nlmsg_build_payload(nlh, t);
	nft_rule_free(t);

	if (nl == NULL) {
		nl = mnl_socket_open(NETLINK_NETFILTER);
		if (nl == NULL) {
			perror("mnl_socket_open");
			exit(EXIT_FAILURE);
		}

		if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
			perror("mnl_socket_bind");
			exit(EXIT_FAILURE);
		}
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	rule_list_peer_length = 0;
	rule_list_length = 0;
	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, table_cb, &type);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		exit(EXIT_FAILURE);
	}
	/* mnl_socket_close(nl); */

	reflesh_nft_peer_cache();
	reflesh_nft_redirect_cache();
	rule_list_validate = RULE_CACHE_VALID;
	return;
}

static void
expr_add_payload(struct nft_rule *r, uint32_t base, uint32_t dreg,
		 uint32_t offset, uint32_t len)
{
	struct nft_rule_expr *e;

	e = nft_rule_expr_alloc("payload");
	if (e == NULL) {
		perror("expr payload oom");
		exit(EXIT_FAILURE);
	}

	nft_rule_expr_set_u32(e, NFT_EXPR_PAYLOAD_BASE, base);
	nft_rule_expr_set_u32(e, NFT_EXPR_PAYLOAD_DREG, dreg);
	nft_rule_expr_set_u32(e, NFT_EXPR_PAYLOAD_OFFSET, offset);
	nft_rule_expr_set_u32(e, NFT_EXPR_PAYLOAD_LEN, len);

	nft_rule_add_expr(r, e);
}

#if 0
static void
expr_add_bitwise(struct nft_rule *r, uint32_t sreg, uint32_t dreg,
		 uint32_t len, uint32_t mask, uint32_t xor)
{
	struct nft_rule_expr *e;

	e = nft_rule_expr_alloc("bitwise");
	if (e == NULL) {
		perror("expr cmp bitwise");
		exit(EXIT_FAILURE);
	}

	nft_rule_expr_set_u32(e, NFT_EXPR_BITWISE_SREG, sreg);
	nft_rule_expr_set_u32(e, NFT_EXPR_BITWISE_DREG, dreg);
	nft_rule_expr_set_u32(e, NFT_EXPR_BITWISE_LEN, len);
	nft_rule_expr_set(e, NFT_EXPR_BITWISE_MASK, &mask, sizeof(mask));
	nft_rule_expr_set(e, NFT_EXPR_BITWISE_XOR, &xor, sizeof(xor));

	nft_rule_add_expr(r, e);
}
#endif

static void
expr_add_cmp(struct nft_rule *r, uint32_t sreg, uint32_t op,
	     const void *data, uint32_t data_len)
{
	struct nft_rule_expr *e;

	e = nft_rule_expr_alloc("cmp");
	if (e == NULL) {
		perror("expr cmp oom");
		exit(EXIT_FAILURE);
	}

	nft_rule_expr_set_u32(e, NFT_EXPR_CMP_SREG, sreg);
	nft_rule_expr_set_u32(e, NFT_EXPR_CMP_OP, op);
	nft_rule_expr_set(e, NFT_EXPR_CMP_DATA, data, data_len);

	nft_rule_add_expr(r, e);
}

static void
expr_add_meta(struct nft_rule *r, uint32_t meta_key, uint32_t dreg)
{
	struct nft_rule_expr *e;

	e = nft_rule_expr_alloc("meta");
	if (e == NULL) {
		perror("expr meta oom");
		exit(EXIT_FAILURE);
	}

	nft_rule_expr_set_u32(e, NFT_EXPR_META_KEY, meta_key);
	nft_rule_expr_set_u32(e, NFT_EXPR_META_DREG, dreg);

	nft_rule_add_expr(r, e);
}

static void
expr_set_reg_val_u32(struct nft_rule *r, enum nft_registers dreg, uint32_t val)
{
	struct nft_rule_expr *e;
	e = nft_rule_expr_alloc("immediate");
	if (e == NULL) {
		perror("expr dreg oom");
		exit(EXIT_FAILURE);
	}
	nft_rule_expr_set_u32(e, NFT_EXPR_IMM_DREG, dreg);
	nft_rule_expr_set_u32(e, NFT_EXPR_IMM_DATA, val);
	nft_rule_add_expr(r, e);
}

static void
expr_set_reg_val_u16(struct nft_rule *r, enum nft_registers dreg, uint32_t val)
{
	struct nft_rule_expr *e;
	e = nft_rule_expr_alloc("immediate");
	if (e == NULL) {
		perror("expr dreg oom");
		exit(EXIT_FAILURE);
	}
	nft_rule_expr_set_u32(e, NFT_EXPR_IMM_DREG, dreg);
	nft_rule_expr_set_u16(e, NFT_EXPR_IMM_DATA, val);
	nft_rule_add_expr(r, e);
}

static void
expr_set_reg_verdict(struct nft_rule *r, uint32_t val) 
{
	struct nft_rule_expr *e;
	e = nft_rule_expr_alloc("immediate");
	if (e == NULL) {
		perror("expr dreg oom");
		exit(EXIT_FAILURE);
	}
	nft_rule_expr_set_u32(e, NFT_EXPR_IMM_DREG, NFT_REG_VERDICT);
	nft_rule_expr_set_u32(e, NFT_EXPR_IMM_VERDICT, val);
	nft_rule_add_expr(r, e);
}

static void
expr_add_nat(struct nft_rule *r, uint32_t t, uint32_t family,
	     in_addr_t addr_min, uint32_t proto_min, uint32_t flags)
{
	struct nft_rule_expr *e;
	UNUSED(flags);

	e = nft_rule_expr_alloc("nat");
	if (e == NULL) {
		perror("expr nat oom");
		exit(EXIT_FAILURE);
	}
	
	nft_rule_expr_set_u32(e, NFT_EXPR_NAT_TYPE, t);
	nft_rule_expr_set_u32(e, NFT_EXPR_NAT_FAMILY, family);

	expr_set_reg_val_u32(r, NFT_REG_1, (uint32_t)addr_min);
	nft_rule_expr_set_u32(e, NFT_EXPR_NAT_REG_ADDR_MIN, NFT_REG_1);
	nft_rule_expr_set_u32(e, NFT_EXPR_NAT_REG_ADDR_MAX, NFT_REG_1);
	expr_set_reg_val_u16(r, NFT_REG_2, proto_min);
	nft_rule_expr_set_u16(e, NFT_EXPR_NAT_REG_PROTO_MIN, NFT_REG_2);
	nft_rule_expr_set_u16(e, NFT_EXPR_NAT_REG_PROTO_MAX, NFT_REG_2);

	nft_rule_add_expr(r, e);
}


/*
 * Todo: add expr for rhost
 */
struct nft_rule *
rule_set_snat(uint8_t family, uint8_t proto,
	      in_addr_t rhost, unsigned short rport,
	      in_addr_t ehost, unsigned short eport,
	      in_addr_t ihost, unsigned short iport,
	      const char *descr,
	      const char *handle)
{
	struct nft_rule *r = NULL;
	uint32_t destport;
	in_addr_t addr[2];
	uint16_t port[2];
	uint32_t descr_len;
	UNUSED(handle);

	r = nft_rule_alloc();
	if (r == NULL) {
		perror("OOM");
		exit(EXIT_FAILURE);
	}

	nft_rule_attr_set(r, NFT_RULE_ATTR_TABLE, NFT_TABLE_NAT);
	nft_rule_attr_set(r, NFT_RULE_ATTR_CHAIN, miniupnpd_nft_peer_chain);
	if (descr != NULL) {
		descr_len = strlen(descr);
		nft_rule_attr_set_data(r, NFT_RULE_ATTR_USERDATA, 
				       descr, descr_len);
	}
	nft_rule_attr_set_u32(r, NFT_RULE_ATTR_FAMILY, family);

	addr[0] = ihost;
	addr[1] = rhost;
	expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
			 offsetof(struct iphdr, saddr), sizeof(uint32_t)*2);
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, addr, sizeof(uint32_t)*2);

	expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
			 offsetof(struct iphdr, protocol), sizeof(uint8_t));
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &proto, sizeof(uint8_t));

	port[0] = htons(iport);
	port[1] = htons(rport);
	if (proto == IPPROTO_TCP) {
		expr_add_payload(r, NFT_PAYLOAD_TRANSPORT_HEADER, NFT_REG_1,
				 offsetof(struct tcphdr, source), 
				 sizeof(uint32_t));
	} else if (proto == IPPROTO_UDP) {
		expr_add_payload(r, NFT_PAYLOAD_TRANSPORT_HEADER, NFT_REG_1,
				 offsetof(struct udphdr, source), 
				 sizeof(uint32_t));
	}
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, port, sizeof(uint32_t));

	destport = htons(eport);
	expr_add_nat(r, NFT_NAT_SNAT, AF_INET, ehost, destport, 0);

	return r;
}

/*
 * Todo: add expr for rhost
 */
struct nft_rule *
rule_set_dnat(uint8_t family, const char * ifname, uint8_t proto,
	      in_addr_t rhost, unsigned short eport,
	      in_addr_t ihost, uint32_t iport,
	      const char *descr,
	      const char *handle)
{
	struct nft_rule *r = NULL;
	uint16_t dport;
	uint64_t handle_num;
	uint32_t if_idx;
	uint32_t descr_len;

	UNUSED(handle);
	UNUSED(rhost);

	r = nft_rule_alloc();
	if (r == NULL) {
		perror("OOM");
		exit(EXIT_FAILURE);
	}

	nft_rule_attr_set(r, NFT_RULE_ATTR_TABLE, NFT_TABLE_NAT);
	nft_rule_attr_set(r, NFT_RULE_ATTR_CHAIN, miniupnpd_nft_nat_chain);
	nft_rule_attr_set_u32(r, NFT_RULE_ATTR_FAMILY, family);
	if (descr != NULL) {
		descr_len = strlen(descr);
		nft_rule_attr_set_data(r, NFT_RULE_ATTR_USERDATA, 
				       descr, descr_len);
	}

	if (handle != NULL) {
		handle_num = atoll(handle);
		nft_rule_attr_set_u64(r, NFT_RULE_ATTR_POSITION, handle_num);
	}

	if (ifname != NULL) {
		if_idx = (uint32_t)if_nametoindex(ifname);
		expr_add_meta(r, NFT_META_IIF, NFT_REG_1);
		expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &if_idx, 
			     sizeof(uint32_t));
	}

	expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
			 offsetof(struct iphdr, protocol), sizeof(uint8_t));
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &proto, sizeof(uint8_t));

	if (proto == IPPROTO_TCP) {
		dport = htons(eport);
		expr_add_payload(r, NFT_PAYLOAD_TRANSPORT_HEADER, NFT_REG_1,
				 offsetof(struct tcphdr, dest), 
				 sizeof(uint16_t));
		expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &dport, 
			     sizeof(uint16_t));
	} else if (proto == IPPROTO_UDP) {
		dport = htons(eport);
		expr_add_payload(r, NFT_PAYLOAD_TRANSPORT_HEADER, NFT_REG_1,
				 offsetof(struct udphdr, dest), 
				 sizeof(uint16_t));
		expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &dport,
			     sizeof(uint16_t));
	}

	expr_add_nat(r, NFT_NAT_DNAT, AF_INET, ihost, htons(iport), 0);

	return r;
}

struct nft_rule *
rule_set_filter(uint8_t family, const char * ifname, uint8_t proto,
		in_addr_t rhost, in_addr_t iaddr, unsigned short eport,
		unsigned short iport, const char *descr, const char *handle)
{
	struct nft_rule *r = NULL;
	uint16_t dport;
	uint64_t handle_num;
	uint32_t if_idx;
	uint32_t descr_len;
	UNUSED(eport);

	r = nft_rule_alloc();
	if (r == NULL) {
		perror("OOM");
		exit(EXIT_FAILURE);
	}

	nft_rule_attr_set(r, NFT_RULE_ATTR_TABLE, NFT_TABLE_FILTER);
	nft_rule_attr_set(r, NFT_RULE_ATTR_CHAIN, miniupnpd_nft_forward_chain);
	nft_rule_attr_set_u32(r, NFT_RULE_ATTR_FAMILY, family);
	if (descr != NULL) {
		descr_len = strlen(descr);
		nft_rule_attr_set_data(r, NFT_RULE_ATTR_USERDATA, 
				       descr, descr_len);
	}

	if (handle != NULL) {
		handle_num = atoll(handle);
		nft_rule_attr_set_u64(r, NFT_RULE_ATTR_POSITION, handle_num);
	}

	if (ifname != NULL) {
		if_idx = (uint32_t)if_nametoindex(ifname);
		expr_add_meta(r, NFT_META_IIF, NFT_REG_1);
		expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &if_idx,
			     sizeof(uint32_t));
	}

	expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
			 offsetof(struct iphdr, daddr), sizeof(uint32_t));
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &iaddr, sizeof(uint32_t));

	expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
			 offsetof(struct iphdr, protocol), sizeof(uint8_t));
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &proto, sizeof(uint8_t));

	dport = htons(iport);
	expr_add_payload(r, NFT_PAYLOAD_TRANSPORT_HEADER, NFT_REG_1,
		         offsetof(struct tcphdr, dest), sizeof(uint16_t));
	expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &dport, sizeof(uint16_t));

	if (rhost != 0) {
		expr_add_payload(r, NFT_PAYLOAD_NETWORK_HEADER, NFT_REG_1,
				 offsetof(struct iphdr, saddr),
				 sizeof(in_addr_t));
		expr_add_cmp(r, NFT_REG_1, NFT_CMP_EQ, &rhost,
			     sizeof(in_addr_t));
	}

	expr_set_reg_verdict(r, NF_ACCEPT);

	return r;
}

struct nft_rule *
rule_del_handle(rule_t *rule)
{
	struct nft_rule *r = NULL;

	r = nft_rule_alloc();
	if (r == NULL) {
		perror("OOM");
		exit(EXIT_FAILURE);
	}

	nft_rule_attr_set(r, NFT_RULE_ATTR_TABLE, rule->table);
	nft_rule_attr_set(r, NFT_RULE_ATTR_CHAIN, rule->chain);
	nft_rule_attr_set_u32(r, NFT_RULE_ATTR_FAMILY, rule->family);
	nft_rule_attr_set_u64(r, NFT_RULE_ATTR_HANDLE, rule->handle);

	return r;
}

static void
nft_mnl_batch_put(char *buf, uint16_t type, uint32_t seq)
{
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfg;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = type;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = seq;

	nfg = mnl_nlmsg_put_extra_header(nlh, sizeof(*nfg));
	nfg->nfgen_family = AF_INET;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = NFNL_SUBSYS_NFTABLES;
}

int
nft_send_request(struct nft_rule * rule, uint16_t cmd)
{
	struct nlmsghdr *nlh;
	struct mnl_nlmsg_batch *batch;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	uint32_t seq = time(NULL);
	int ret;

	rule_list_validate = RULE_CACHE_INVALID;
	if (nl == NULL) {
		nl = mnl_socket_open(NETLINK_NETFILTER);
		if (nl == NULL) {
			perror("mnl_socket_open");
			return -1;
		}

		if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
			perror("mnl_socket_bind");
			return -1;
		}
	}

	batch = mnl_nlmsg_batch_start(buf, sizeof(buf));

	nft_mnl_batch_put(mnl_nlmsg_batch_current(batch),
			  NFNL_MSG_BATCH_BEGIN, seq++);
	mnl_nlmsg_batch_next(batch);

	nlh = nft_rule_nlmsg_build_hdr(mnl_nlmsg_batch_current(batch),
				       cmd,
				       nft_rule_attr_get_u32(rule, NFT_RULE_ATTR_FAMILY),
				       NLM_F_APPEND|NLM_F_CREATE|NLM_F_ACK,
				       seq++);

	nft_rule_nlmsg_build_payload(nlh, rule);
	nft_rule_free(rule);
	mnl_nlmsg_batch_next(batch);

	nft_mnl_batch_put(mnl_nlmsg_batch_current(batch), NFNL_MSG_BATCH_END,
			  seq++);
	mnl_nlmsg_batch_next(batch);

	ret = mnl_socket_sendto(nl, mnl_nlmsg_batch_head(batch),
				mnl_nlmsg_batch_size(batch));
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	mnl_nlmsg_batch_stop(batch);

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;	
	}

	ret = mnl_cb_run(buf, ret, 0, mnl_socket_get_portid(nl), NULL, NULL);
	if (ret < 0) {
		perror("mnl_cb_run");
		return -1;	
	}

	/* mnl_socket_close(nl); */
	return 0;
}

/*
 * MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2015 Tomofumi Hayashi
 * 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution.
 */
#include <sys/queue.h>

#define NFT_TABLE_NAT  "nat"
#define NFT_TABLE_FILTER  "filter"

enum rule_reg_type { 
	RULE_REG_NONE,
	RULE_REG_IIF,
	RULE_REG_OIF,
	RULE_REG_IP_SRC_ADDR,
	RULE_REG_IP_DEST_ADDR,
	RULE_REG_IP_SD_ADDR, /* source & dest */
	RULE_REG_IP_PROTO,
	RULE_REG_TCP_DPORT,
	RULE_REG_TCP_SD_PORT, /* source & dest */
	RULE_REG_IMM_VAL,
	RULE_REG_MAX,
};

enum rule_type {
	RULE_NONE,
	RULE_NAT,
	RULE_SNAT,
	RULE_FILTER,
	RULE_COUNTER,
};

typedef struct rule_ {
	LIST_ENTRY(rule_t) entry;
	char * table;
	char * chain;
	uint64_t handle;
	enum rule_type type;
	uint32_t nat_type;
	uint32_t filter_action;
	uint32_t family;
	uint32_t ingress_ifidx;
	uint32_t egress_ifidx;
	in_addr_t eaddr;
	in_addr_t iaddr;
	in_addr_t rhost;
	uint16_t eport;
	uint16_t iport;
	uint16_t rport;
	uint8_t proto;
	enum rule_reg_type reg1_type;
	enum rule_reg_type reg2_type;
	uint32_t reg1_val;
	uint32_t reg2_val;
	uint64_t packets;
	uint64_t bytes;
	char *desc;
} rule_t;

LIST_HEAD(rule_list, rule_);
extern struct rule_list head;
extern rule_t **peer_cache;
extern rule_t **redirect_cache;

int
nft_send_request(struct nft_rule * rule, uint16_t cmd);
struct nft_rule *
rule_set_dnat(uint8_t family, const char * ifname, uint8_t proto,
	      in_addr_t rhost, unsigned short eport,
	      in_addr_t ihost, uint32_t iport,
	      const char *descr,
	      const char *handle);
struct nft_rule *
rule_set_snat(uint8_t family, uint8_t proto,
	      in_addr_t rhost, unsigned short rport,
	      in_addr_t ehost, unsigned short eport,
	      in_addr_t ihost, unsigned short iport,
	      const char *descr,
	      const char *handle);
struct nft_rule *
rule_set_filter(uint8_t family, const char * ifname, uint8_t proto,
		in_addr_t rhost, in_addr_t iaddr, unsigned short eport,
		unsigned short iport, const char * descr, const char *handle);
struct nft_rule *
rule_del_handle(rule_t *r);
void
reflesh_nft_cache(uint32_t family);
void print_rule(rule_t *r);

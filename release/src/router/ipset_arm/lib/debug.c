/* Copyright 2011 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <arpa/inet.h>					/* inet_ntop */
#include <libmnl/libmnl.h>				/* libmnl backend */

struct ipset_attrname {
	const char *name;
};

static const struct ipset_attrname cmdattr2name[] = {
	[IPSET_ATTR_PROTOCOL]	= { .name = "PROTOCOL" },
	[IPSET_ATTR_SETNAME]	= { .name = "SETNAME" },
	[IPSET_ATTR_TYPENAME]	= { .name = "TYPENAME" },
	[IPSET_ATTR_REVISION]	= { .name = "REVISION" },
	[IPSET_ATTR_FAMILY]	= { .name = "FAMILY" },
	[IPSET_ATTR_FLAGS]	= { .name = "FLAGS" },
	[IPSET_ATTR_DATA]	= { .name = "DATA" },
	[IPSET_ATTR_ADT]	= { .name = "ADT" },
	[IPSET_ATTR_LINENO]	= { .name = "LINENO" },
	[IPSET_ATTR_PROTOCOL_MIN] = { .name = "PROTO_MIN" },
};

static const struct ipset_attrname createattr2name[] = {
	[IPSET_ATTR_IP]		= { .name = "IP" },
	[IPSET_ATTR_IP_TO]	= { .name = "IP_TO" },
	[IPSET_ATTR_CIDR]	= { .name = "CIDR" },
	[IPSET_ATTR_PORT]	= { .name = "PORT" },
	[IPSET_ATTR_PORT_TO]	= { .name = "PORT_TO" },
	[IPSET_ATTR_TIMEOUT]	= { .name = "TIMEOUT" },
	[IPSET_ATTR_PROTO]	= { .name = "PROTO" },
	[IPSET_ATTR_CADT_FLAGS]	= { .name = "CADT_FLAGS" },
	[IPSET_ATTR_CADT_LINENO] = { .name = "CADT_LINENO" },
	[IPSET_ATTR_GC]		= { .name = "GC" },
	[IPSET_ATTR_HASHSIZE]	= { .name = "HASHSIZE" },
	[IPSET_ATTR_MAXELEM]	= { .name = "MAXELEM" },
	[IPSET_ATTR_MARKMASK]	= { .name = "MARKMASK" },
	[IPSET_ATTR_NETMASK]	= { .name = "NETMASK" },
	[IPSET_ATTR_PROBES]	= { .name = "PROBES" },
	[IPSET_ATTR_RESIZE]	= { .name = "RESIZE" },
	[IPSET_ATTR_SIZE]	= { .name = "SIZE" },
	[IPSET_ATTR_ELEMENTS]	= { .name = "ELEMENTS" },
	[IPSET_ATTR_REFERENCES]	= { .name = "REFERENCES" },
	[IPSET_ATTR_MEMSIZE]	= { .name = "MEMSIZE" },
};

static const struct ipset_attrname adtattr2name[] = {
	[IPSET_ATTR_IP]		= { .name = "IP" },
	[IPSET_ATTR_IP_TO]	= { .name = "IP_TO" },
	[IPSET_ATTR_CIDR]	= { .name = "CIDR" },
	[IPSET_ATTR_MARK]	= { .name = "MARK" },
	[IPSET_ATTR_PORT]	= { .name = "PORT" },
	[IPSET_ATTR_PORT_TO]	= { .name = "PORT_TO" },
	[IPSET_ATTR_TIMEOUT]	= { .name = "TIMEOUT" },
	[IPSET_ATTR_PROTO]	= { .name = "PROTO" },
	[IPSET_ATTR_CADT_FLAGS]	= { .name = "CADT_FLAGS" },
	[IPSET_ATTR_CADT_LINENO] = { .name = "CADT_LINENO" },
	[IPSET_ATTR_ETHER]	= { .name = "ETHER" },
	[IPSET_ATTR_NAME]	= { .name = "NAME" },
	[IPSET_ATTR_NAMEREF]	= { .name = "NAMEREF" },
	[IPSET_ATTR_IP2]	= { .name = "IP2" },
	[IPSET_ATTR_CIDR2]	= { .name = "CIDR2" },
	[IPSET_ATTR_IP2_TO]	= { .name = "IP2_TO" },
	[IPSET_ATTR_IFACE]	= { .name = "IFACE" },
	[IPSET_ATTR_COMMENT]	= { .name = "COMMENT" },
	[IPSET_ATTR_SKBMARK]	= { .name = "SKBMARK" },
	[IPSET_ATTR_SKBPRIO]	= { .name = "SKBPRIO" },
	[IPSET_ATTR_SKBQUEUE]	= { .name = "SKBQUEUE" },
};

static void
debug_cadt_attrs(int max, const struct ipset_attr_policy *policy,
		 const struct ipset_attrname attr2name[],
		 struct nlattr *nla[])
{
	uint64_t tmp;
	uint32_t v;
	int i;

	fprintf(stderr, "\t\t%s attributes:\n",
		policy == create_attrs ? "CREATE" : "ADT");
	for (i = IPSET_ATTR_UNSPEC + 1; i <= max; i++) {
		if (!nla[i])
			continue;
		switch (policy[i].type) {
		case MNL_TYPE_U8:
			v = *(uint8_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
				attr2name[i].name, v);
			break;
		case MNL_TYPE_U16:
			v = *(uint16_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
				attr2name[i].name, ntohs(v));
			break;
		case MNL_TYPE_U32:
			v = *(uint32_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t\t%s: %u\n",
				attr2name[i].name, ntohl(v));
			break;
		case MNL_TYPE_U64:
			memcpy(&tmp, mnl_attr_get_payload(nla[i]), sizeof(tmp));
			fprintf(stderr, "\t\t%s: 0x%llx\n",
				attr2name[i].name, (long long int)
				be64toh(tmp));
			break;
		case MNL_TYPE_NUL_STRING:
			fprintf(stderr, "\t\t%s: %s\n",
				attr2name[i].name,
				(const char *) mnl_attr_get_payload(nla[i]));
			break;
		case MNL_TYPE_NESTED: {
			struct nlattr *ipattr[IPSET_ATTR_IPADDR_MAX+1] = {};
			char addr[INET6_ADDRSTRLEN];
			void *d;

			if (mnl_attr_parse_nested(nla[i], ipaddr_attr_cb,
						  ipattr) < 0) {
				fprintf(stderr,
					"\t\tIPADDR: cannot validate "
					"and parse attributes\n");
				continue;
			}
			if (ipattr[IPSET_ATTR_IPADDR_IPV4]) {
				d = mnl_attr_get_payload(
					ipattr[IPSET_ATTR_IPADDR_IPV4]);

				inet_ntop(NFPROTO_IPV4, d, addr,
					  INET6_ADDRSTRLEN);
				fprintf(stderr, "\t\t%s: %s\n",
					attr2name[i].name, addr);
			} else if (ipattr[IPSET_ATTR_IPADDR_IPV6]) {
				d = mnl_attr_get_payload(
					ipattr[IPSET_ATTR_IPADDR_IPV6]);

				inet_ntop(NFPROTO_IPV6, d, addr,
					  INET6_ADDRSTRLEN);
				fprintf(stderr, "\t\t%s: %s\n",
					attr2name[i].name, addr);
			}
			break;
		}
		default:
			fprintf(stderr, "\t\t%s: unresolved!\n",
				attr2name[i].name);
		}
	}
}

static void
debug_cmd_attrs(int cmd, struct nlattr *nla[])
{
	struct nlattr *adt[IPSET_ATTR_ADT_MAX+1] = {};
	struct nlattr *cattr[IPSET_ATTR_CREATE_MAX+1] = {};
	uint32_t v;
	int i;

	fprintf(stderr, "\tCommand attributes:\n");
	for (i = IPSET_ATTR_UNSPEC + 1; i <= IPSET_ATTR_CMD_MAX; i++) {
		if (!nla[i])
			continue;
		switch (cmd_attrs[i].type) {
		case MNL_TYPE_U8:
			v = *(uint8_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
				cmdattr2name[i].name, v);
			break;
		case MNL_TYPE_U16:
			v = *(uint16_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
				cmdattr2name[i].name, ntohs(v));
			break;
		case MNL_TYPE_U32:
			v = *(uint32_t *) mnl_attr_get_payload(nla[i]);
			fprintf(stderr, "\t%s: %u\n",
				cmdattr2name[i].name, ntohl(v));
			break;
		case MNL_TYPE_NUL_STRING:
			fprintf(stderr, "\t%s: %s\n",
				cmdattr2name[i].name,
				(const char *) mnl_attr_get_payload(nla[i]));
			break;
		case MNL_TYPE_NESTED:
			if (i == IPSET_ATTR_DATA) {
				switch (cmd) {
				case IPSET_CMD_ADD:
				case IPSET_CMD_DEL:
				case IPSET_CMD_TEST:
					if (mnl_attr_parse_nested(nla[i],
							adt_attr_cb, adt) < 0) {
						fprintf(stderr,
							"\tADT: cannot validate "
							"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(IPSET_ATTR_ADT_MAX,
							 adt_attrs,
							 adtattr2name,
							 adt);
					break;
				default:
					if (mnl_attr_parse_nested(nla[i],
							create_attr_cb,
							cattr) < 0) {
						fprintf(stderr,
							"\tCREATE: cannot validate "
							"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(IPSET_ATTR_CREATE_MAX,
							 create_attrs,
							 createattr2name,
							 cattr);
				}
			} else {
				struct nlattr *tb;
				mnl_attr_for_each_nested(tb, nla[i]) {
					memset(adt, 0, sizeof(adt));
					if (mnl_attr_parse_nested(tb,
							adt_attr_cb, adt) < 0) {
						fprintf(stderr,
							"\tADT: cannot validate "
							"and parse attributes\n");
						continue;
					}
					debug_cadt_attrs(IPSET_ATTR_ADT_MAX,
							 adt_attrs,
							 adtattr2name,
							 adt);
				}
			}
			break;
		default:
			fprintf(stderr, "\t%s: unresolved!\n",
				cmdattr2name[i].name);
		}
	}
}

void
ipset_debug_msg(const char *dir, void *buffer, int len)
{
	const struct nlmsghdr *nlh = buffer;
	struct nlattr *nla[IPSET_ATTR_CMD_MAX+1] = {};
	int cmd, nfmsglen = MNL_ALIGN(sizeof(struct nfgenmsg));

	debug = 0;
	if (!mnl_nlmsg_ok(nlh, len)) {
		fprintf(stderr, "Broken message received!\n");
		if (len < (int)sizeof(struct nlmsghdr)) {
			fprintf(stderr, "len (%d) < sizeof(struct nlmsghdr) (%d)\n",
				len, (int)sizeof(struct nlmsghdr));
		} else if (nlh->nlmsg_len < sizeof(struct nlmsghdr)) {
			fprintf(stderr, "nlmsg_len (%u) < sizeof(struct nlmsghdr) (%d)\n",
				nlh->nlmsg_len, (int)sizeof(struct nlmsghdr));
		} else if ((int)nlh->nlmsg_len > len) {
			fprintf(stderr, "nlmsg_len (%u) > len (%d)\n",
				 nlh->nlmsg_len, len);
		}
	}
	while (mnl_nlmsg_ok(nlh, len)) {
		switch (nlh->nlmsg_type) {
		case NLMSG_NOOP:
		case NLMSG_DONE:
		case NLMSG_OVERRUN:
			fprintf(stderr, "Message header: %s msg %s\n"
					"\tlen %d\n"
					"\tseq  %u\n",
				dir,
				nlh->nlmsg_type == NLMSG_NOOP ? "NOOP" :
				nlh->nlmsg_type == NLMSG_DONE ? "DONE" :
				"OVERRUN",
				len, nlh->nlmsg_seq);
			goto next_msg;
		case NLMSG_ERROR: {
			const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
			fprintf(stderr, "Message header: %s msg ERROR\n"
					"\tlen %d\n"
					"\terrcode %d\n"
					"\tseq %u\n",
				dir, len, err->error, nlh->nlmsg_seq);
			goto next_msg;
			}
		default:
			;
		}
		cmd = ipset_get_nlmsg_type(nlh);
		fprintf(stderr, "Message header: %s cmd  %s (%d)\n"
				"\tlen %d\n"
				"\tflag %s\n"
				"\tseq %u\n",
			dir,
			cmd <= IPSET_CMD_NONE ? "NONE!" :
			cmd >= IPSET_CMD_MAX ? "MAX!" : cmd2name[cmd], cmd,
			len,
			!(nlh->nlmsg_flags & NLM_F_EXCL) ? "EXIST" : "none",
			nlh->nlmsg_seq);
		if (cmd <= IPSET_CMD_NONE || cmd >= IPSET_CMD_MAX)
			goto next_msg;
		memset(nla, 0, sizeof(nla));
		if (mnl_attr_parse(nlh, nfmsglen,
				cmd_attr_cb, nla) < MNL_CB_STOP) {
			fprintf(stderr, "\tcannot validate "
				"and parse attributes\n");
			goto next_msg;
		}
		debug_cmd_attrs(cmd, nla);
next_msg:
		nlh = mnl_nlmsg_next(nlh, &len);
	}
	debug = 1;
}

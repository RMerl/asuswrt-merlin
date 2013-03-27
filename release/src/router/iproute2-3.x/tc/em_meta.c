/*
 * em_meta.c		Metadata Ematch
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Thomas Graf <tgraf@suug.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "m_ematch.h"
#include <linux/tc_ematch/tc_em_meta.h>

extern struct ematch_util meta_ematch_util;

static void meta_print_usage(FILE *fd)
{
	fprintf(fd,
	    "Usage: meta(OBJECT { eq | lt | gt } OBJECT)\n" \
	    "where: OBJECT  := { META_ID | VALUE }\n" \
	    "       META_ID := id [ shift SHIFT ] [ mask MASK ]\n" \
	    "\n" \
	    "Example: meta(nfmark gt 24)\n" \
	    "         meta(indev shift 1 eq \"ppp\")\n" \
	    "         meta(tcindex mask 0xf0 eq 0xf0)\n" \
	    "\n" \
	    "For a list of meta identifiers, use meta(list).\n");
}

struct meta_entry {
	int		id;
	char *		kind;
	char *		mask;
	char *		desc;
} meta_table[] = {
#define TCF_META_ID_SECTION 0
#define __A(id, name, mask, desc) { TCF_META_ID_##id, name, mask, desc }
	__A(SECTION,		"Generic", "", ""),
	__A(RANDOM,		"random",	"i",
				"Random value (32 bit)"),
	__A(LOADAVG_0,		"loadavg_1",	"i",
				"Load average in last minute"),
	__A(LOADAVG_1,		"loadavg_5",	"i",
				"Load average in last 5 minutes"),
	__A(LOADAVG_2,		"loadavg_15",	"i",
				"Load average in last 15 minutes"),

	__A(SECTION,		"Interfaces", "", ""),
	__A(DEV,		"dev",		"iv",
				"Device the packet is on"),
	__A(SECTION,		"Packet attributes", "", ""),
	__A(PRIORITY,		"priority",	"i",
				"Priority of packet"),
	__A(PROTOCOL,		"protocol",	"i",
				"Link layer protocol"),
	__A(PKTTYPE,		"pkt_type",	"i",
				"Packet type (uni|multi|broad|...)cast"),
	__A(PKTLEN,		"pkt_len",	"i",
				"Length of packet"),
	__A(DATALEN,		"data_len",	"i",
				"Length of data in packet"),
	__A(MACLEN,		"mac_len",	"i",
				"Length of link layer header"),

	__A(SECTION,		"Netfilter", "", ""),
	__A(NFMARK,		"nf_mark",	"i",
				"Netfilter mark"),
	__A(NFMARK,		"fwmark",	"i",
				"Alias for nf_mark"),

	__A(SECTION,		"Traffic Control", "", ""),
	__A(TCINDEX,		"tc_index",	"i",	"TC Index"),
	__A(SECTION,		"Routing", "", ""),
	__A(RTCLASSID,		"rt_classid",	"i",
				"Routing ClassID (cls_route)"),
	__A(RTIIF,		"rt_iif",	"i",
				"Incoming interface index"),
	__A(VLAN_TAG,		"vlan",		"i",	"Vlan tag"),

	__A(SECTION,		"Sockets", "", ""),
	__A(SK_FAMILY,		"sk_family",	"i",	"Address family"),
	__A(SK_STATE,		"sk_state",	"i",	"State"),
	__A(SK_REUSE,		"sk_reuse",	"i",	"Reuse Flag"),
	__A(SK_BOUND_IF,	"sk_bind_if",	"iv",	"Bound interface"),
	__A(SK_REFCNT,		"sk_refcnt",	"i",	"Reference counter"),
	__A(SK_SHUTDOWN,	"sk_shutdown",	"i",	"Shutdown mask"),
	__A(SK_PROTO,		"sk_proto",	"i",	"Protocol"),
	__A(SK_TYPE,		"sk_type",	"i",	"Type"),
	__A(SK_RCVBUF,		"sk_rcvbuf",	"i",	"Receive buffer size"),
	__A(SK_RMEM_ALLOC,	"sk_rmem",	"i",	"RMEM"),
	__A(SK_WMEM_ALLOC,	"sk_wmem",	"i",	"WMEM"),
	__A(SK_OMEM_ALLOC,	"sk_omem",	"i",	"OMEM"),
	__A(SK_WMEM_QUEUED,	"sk_wmem_queue","i",	"WMEM queue"),
	__A(SK_SND_QLEN,	"sk_snd_queue",	"i",	"Send queue length"),
	__A(SK_RCV_QLEN,	"sk_rcv_queue",	"i",	"Receive queue length"),
	__A(SK_ERR_QLEN,	"sk_err_queue",	"i",	"Error queue length"),
	__A(SK_FORWARD_ALLOCS,	"sk_fwd_alloc",	"i",	"Forward allocations"),
	__A(SK_SNDBUF,		"sk_sndbuf",	"i",	"Send buffer size"),
#undef __A
};

static inline int map_type(char k)
{
	switch (k) {
		case 'i': return TCF_META_TYPE_INT;
		case 'v': return TCF_META_TYPE_VAR;
	}

	fprintf(stderr, "BUG: Unknown map character '%c'\n", k);
	return INT_MAX;
}

static struct meta_entry * lookup_meta_entry(struct bstr *kind)
{
	int i;

	for (i = 0; i < (sizeof(meta_table)/sizeof(meta_table[0])); i++)
		if (!bstrcmp(kind, meta_table[i].kind) &&
		    meta_table[i].id != 0)
			return &meta_table[i];

	return NULL;
}

static struct meta_entry * lookup_meta_entry_byid(int id)
{
	int i;

	for (i = 0; i < (sizeof(meta_table)/sizeof(meta_table[0])); i++)
		if (meta_table[i].id == id)
			return &meta_table[i];

	return NULL;
}

static inline void dump_value(struct nlmsghdr *n, int tlv, unsigned long val,
			      struct tcf_meta_val *hdr)
{
	__u32 t;

	switch (TCF_META_TYPE(hdr->kind)) {
		case TCF_META_TYPE_INT:
			t = val;
			addattr_l(n, MAX_MSG, tlv, &t, sizeof(t));
			break;

		case TCF_META_TYPE_VAR:
			if (TCF_META_ID(hdr->kind) == TCF_META_ID_VALUE) {
				struct bstr *a = (struct bstr *) val;
				addattr_l(n, MAX_MSG, tlv, a->data, a->len);
			}
			break;
	}
}

static inline int is_compatible(struct tcf_meta_val *what,
				struct tcf_meta_val *needed)
{
	char *p;
	struct meta_entry *entry;

	entry = lookup_meta_entry_byid(TCF_META_ID(what->kind));

	if (entry == NULL)
		return 0;

	for (p = entry->mask; p; p++)
		if (map_type(*p) == TCF_META_TYPE(needed->kind))
			return 1;

	return 0;
}

static void list_meta_ids(FILE *fd)
{
	int i;

	fprintf(fd,
	    "--------------------------------------------------------\n" \
	    "  ID               Type       Description\n" \
	    "--------------------------------------------------------");

	for (i = 0; i < (sizeof(meta_table)/sizeof(meta_table[0])); i++) {
		if (meta_table[i].id == TCF_META_ID_SECTION) {
			fprintf(fd, "\n%s:\n", meta_table[i].kind);
		} else {
			char *p = meta_table[i].mask;
			char buf[64] = {0};

			fprintf(fd, "  %-16s ", meta_table[i].kind);

			while (*p) {
				int type = map_type(*p);

				switch (type) {
					case TCF_META_TYPE_INT:
						strcat(buf, "INT");
						break;

					case TCF_META_TYPE_VAR:
						strcat(buf, "VAR");
						break;
				}

				if (*(++p))
					strcat(buf, ",");
			}

			fprintf(fd, "%-10s %s\n", buf, meta_table[i].desc);
		}
	}

	fprintf(fd,
	    "--------------------------------------------------------\n");
}

#undef TCF_META_ID_SECTION

#define PARSE_FAILURE ((void *) (-1))

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &meta_ematch_util, FMT ,##ARGS)

static inline int can_adopt(struct tcf_meta_val *val)
{
	return !!TCF_META_ID(val->kind);
}

static inline int overwrite_type(struct tcf_meta_val *src,
				 struct tcf_meta_val *dst)
{
	return (TCF_META_TYPE(dst->kind) << 12) | TCF_META_ID(src->kind);
}


static inline struct bstr *
parse_object(struct bstr *args, struct bstr *arg, struct tcf_meta_val *obj,
	     unsigned long *dst, struct tcf_meta_val *left)
{
	struct meta_entry *entry;
	unsigned long num;
	struct bstr *a;

	if (arg->quoted) {
		obj->kind = TCF_META_TYPE_VAR << 12;
		obj->kind |= TCF_META_ID_VALUE;
		*dst = (unsigned long) arg;
		return bstr_next(arg);
	}

	num = bstrtoul(arg);
	if (num != ULONG_MAX) {
		obj->kind = TCF_META_TYPE_INT << 12;
		obj->kind |= TCF_META_ID_VALUE;
		*dst = (unsigned long) num;
		return bstr_next(arg);
	}

	entry = lookup_meta_entry(arg);

	if (entry == NULL) {
		PARSE_ERR(arg, "meta: unknown meta id\n");
		return PARSE_FAILURE;
	}

	obj->kind = entry->id | (map_type(entry->mask[0]) << 12);

	if (left) {
		struct tcf_meta_val *right = obj;

		if (TCF_META_TYPE(right->kind) == TCF_META_TYPE(left->kind))
			goto compatible;

		if (can_adopt(left) && !can_adopt(right)) {
			if (is_compatible(left, right))
				left->kind = overwrite_type(left, right);
			else
				goto not_compatible;
		} else if (can_adopt(right) && !can_adopt(left)) {
			if (is_compatible(right, left))
				right->kind = overwrite_type(right, left);
			else
				goto not_compatible;
		} else if (can_adopt(left) && can_adopt(right)) {
			if (is_compatible(left, right))
				left->kind = overwrite_type(left, right);
			else if (is_compatible(right, left))
				right->kind = overwrite_type(right, left);
			else
				goto not_compatible;
		} else
			goto not_compatible;
	}

compatible:

	a = bstr_next(arg);

	while(a) {
		if (!bstrcmp(a, "shift")) {
			unsigned long shift;

			if (a->next == NULL) {
				PARSE_ERR(a, "meta: missing argument");
				return PARSE_FAILURE;
			}
			a = bstr_next(a);

			shift = bstrtoul(a);
			if (shift == ULONG_MAX) {
				PARSE_ERR(a, "meta: invalid shift, must " \
				    "be numeric");
				return PARSE_FAILURE;
			}

			obj->shift = (__u8) shift;
			a = bstr_next(a);
		} else if (!bstrcmp(a, "mask")) {
			unsigned long mask;

			if (a->next == NULL) {
				PARSE_ERR(a, "meta: missing argument");
				return PARSE_FAILURE;
			}
			a = bstr_next(a);

			mask = bstrtoul(a);
			if (mask == ULONG_MAX) {
				PARSE_ERR(a, "meta: invalid mask, must be " \
				    "numeric");
				return PARSE_FAILURE;
			}
			*dst = (unsigned long) mask;
			a = bstr_next(a);
		} else
			break;
	}

	return a;

not_compatible:
	PARSE_ERR(arg, "lvalue and rvalue are not compatible.");
	return PARSE_FAILURE;
}

static int meta_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			   struct bstr *args)
{
	int opnd;
	struct bstr *a;
	struct tcf_meta_hdr meta_hdr;
	unsigned long lvalue = 0, rvalue = 0;

	memset(&meta_hdr, 0, sizeof(meta_hdr));

	if (args == NULL)
		return PARSE_ERR(args, "meta: missing arguments");

	if (!bstrcmp(args, "list")) {
		list_meta_ids(stderr);
		return -1;
	}

	a = parse_object(args, args, &meta_hdr.left, &lvalue, NULL);
	if (a == PARSE_FAILURE)
		return -1;
	else if (a == NULL)
		return PARSE_ERR(args, "meta: missing operand");

	if (!bstrcmp(a, "eq"))
		opnd = TCF_EM_OPND_EQ;
	else if (!bstrcmp(a, "gt"))
		opnd = TCF_EM_OPND_GT;
	else if (!bstrcmp(a, "lt"))
		opnd = TCF_EM_OPND_LT;
	else
		return PARSE_ERR(a, "meta: invalid operand");

	meta_hdr.left.op = (__u8) opnd;

	if (a->next == NULL)
		return PARSE_ERR(args, "meta: missing rvalue");
	a = bstr_next(a);

	a = parse_object(args, a, &meta_hdr.right, &rvalue, &meta_hdr.left);
	if (a == PARSE_FAILURE)
		return -1;
	else if (a != NULL)
		return PARSE_ERR(a, "meta: unexpected trailer");


	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));

	addattr_l(n, MAX_MSG, TCA_EM_META_HDR, &meta_hdr, sizeof(meta_hdr));

	dump_value(n, TCA_EM_META_LVALUE, lvalue, &meta_hdr.left);
	dump_value(n, TCA_EM_META_RVALUE, rvalue, &meta_hdr.right);

	return 0;
}
#undef PARSE_ERR

static inline void print_binary(FILE *fd, unsigned char *str, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (!isprint(str[i]))
			goto binary;

	for (i = 0; i < len; i++)
		fprintf(fd, "%c", str[i]);
	return;

binary:
	for (i = 0; i < len; i++)
		fprintf(fd, "%02x ", str[i]);

	fprintf(fd, "\"");
	for (i = 0; i < len; i++)
		fprintf(fd, "%c", isprint(str[i]) ? str[i] : '.');
	fprintf(fd, "\"");
}

static inline int print_value(FILE *fd, int type, struct rtattr *rta)
{
	if (rta == NULL) {
		fprintf(stderr, "Missing value TLV\n");
		return -1;
	}

	switch(type) {
		case TCF_META_TYPE_INT:
			if (RTA_PAYLOAD(rta) < sizeof(__u32)) {
				fprintf(stderr, "meta int type value TLV " \
				    "size mismatch.\n");
				return -1;
			}
			fprintf(fd, "%d", *(__u32 *) RTA_DATA(rta));
			break;

		case TCF_META_TYPE_VAR:
			print_binary(fd, RTA_DATA(rta), RTA_PAYLOAD(rta));
			break;
	}

	return 0;
}

static int print_object(FILE *fd, struct tcf_meta_val *obj, struct rtattr *rta)
{
	int id = TCF_META_ID(obj->kind);
	int type = TCF_META_TYPE(obj->kind);
	struct meta_entry *entry;

	if (id == TCF_META_ID_VALUE)
		return print_value(fd, type, rta);

	entry = lookup_meta_entry_byid(id);

	if (entry == NULL)
		fprintf(fd, "[unknown meta id %d]", id);
	else
		fprintf(fd, "%s", entry->kind);

	if (obj->shift)
		fprintf(fd, " shift %d", obj->shift);

	switch (type) {
		case TCF_META_TYPE_INT:
			if (rta) {
				if (RTA_PAYLOAD(rta) < sizeof(__u32))
					goto size_mismatch;

				fprintf(fd, " mask 0x%08x",
				    *(__u32*) RTA_DATA(rta));
			}
			break;
	}

	return 0;

size_mismatch:
	fprintf(stderr, "meta int type mask TLV size mismatch\n");
	return -1;
}


static int meta_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			   int data_len)
{
	struct rtattr *tb[TCA_EM_META_MAX+1];
	struct tcf_meta_hdr *meta_hdr;

	if (parse_rtattr(tb, TCA_EM_META_MAX, data, data_len) < 0)
		return -1;

	if (tb[TCA_EM_META_HDR] == NULL) {
		fprintf(stderr, "Missing meta header\n");
		return -1;
	}

	if (RTA_PAYLOAD(tb[TCA_EM_META_HDR]) < sizeof(*meta_hdr)) {
		fprintf(stderr, "Meta header size mismatch\n");
		return -1;
	}

	meta_hdr = RTA_DATA(tb[TCA_EM_META_HDR]);

	if (print_object(fd, &meta_hdr->left, tb[TCA_EM_META_LVALUE]) < 0)
		return -1;

	switch (meta_hdr->left.op) {
		case TCF_EM_OPND_EQ:
			fprintf(fd, " eq ");
			break;
		case TCF_EM_OPND_LT:
			fprintf(fd, " lt ");
			break;
		case TCF_EM_OPND_GT:
			fprintf(fd, " gt ");
			break;
	}

	return print_object(fd, &meta_hdr->right, tb[TCA_EM_META_RVALUE]);
}

struct ematch_util meta_ematch_util = {
	.kind = "meta",
	.kind_num = TCF_EM_META,
	.parse_eopt = meta_parse_eopt,
	.print_eopt = meta_print_eopt,
	.print_usage = meta_print_usage
};

/*
 * q_u32.c		U32 filter.
 *
 *		This program is free software; you can u32istribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *		Match mark added by Catalin(ux aka Dino) BOIE <catab at umbrella.ro> [5 nov 2004]
 *
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
#include <linux/if.h>
#include <linux/if_ether.h>

#include "utils.h"
#include "tc_util.h"

extern int show_pretty;

static void explain(void)
{
	fprintf(stderr, "Usage: ... u32 [ match SELECTOR ... ] [ link HTID ]"
		" [ classid CLASSID ]\n");
	fprintf(stderr, "               [ police POLICE_SPEC ]"
		" [ offset OFFSET_SPEC ]\n");
	fprintf(stderr, "               [ ht HTID ] [ hashkey HASHKEY_SPEC ]\n");
	fprintf(stderr, "               [ sample SAMPLE ]\n");
	fprintf(stderr, "or         u32 divisor DIVISOR\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where: SELECTOR := SAMPLE SAMPLE ...\n");
	fprintf(stderr, "       SAMPLE := { ip | ip6 | udp | tcp | icmp |"
		" u{32|16|8} | mark } SAMPLE_ARGS [divisor DIVISOR]\n");
	fprintf(stderr, "       FILTERID := X:Y:Z\n");
	fprintf(stderr, "\nNOTE: CLASSID is parsed at hexadecimal input.\n");
}

int get_u32_handle(__u32 *handle, const char *str)
{
	__u32 htid=0, hash=0, nodeid=0;
	char *tmp = strchr(str, ':');

	if (tmp == NULL) {
		if (memcmp("0x", str, 2) == 0)
			return get_u32(handle, str, 16);
		return -1;
	}
	htid = strtoul(str, &tmp, 16);
	if (tmp == str && *str != ':' && *str != 0)
		return -1;
	if (htid>=0x1000)
		return -1;
	if (*tmp) {
		str = tmp+1;
		hash = strtoul(str, &tmp, 16);
		if (tmp == str && *str != ':' && *str != 0)
			return -1;
		if (hash>=0x100)
			return -1;
		if (*tmp) {
			str = tmp+1;
			nodeid = strtoul(str, &tmp, 16);
			if (tmp == str && *str != 0)
				return -1;
			if (nodeid>=0x1000)
				return -1;
		}
	}
	*handle = (htid<<20)|(hash<<12)|nodeid;
	return 0;
}

char * sprint_u32_handle(__u32 handle, char *buf)
{
	int bsize = SPRINT_BSIZE-1;
	__u32 htid = TC_U32_HTID(handle);
	__u32 hash = TC_U32_HASH(handle);
	__u32 nodeid = TC_U32_NODE(handle);
	char *b = buf;

	if (handle == 0) {
		snprintf(b, bsize, "none");
		return b;
	}
	if (htid) {
		int l = snprintf(b, bsize, "%x:", htid>>20);
		bsize -= l;
		b += l;
	}
	if (nodeid|hash) {
		if (hash) {
			int l = snprintf(b, bsize, "%x", hash);
			bsize -= l;
			b += l;
		}
		if (nodeid) {
			int l = snprintf(b, bsize, ":%x", nodeid);
			bsize -= l;
			b += l;
		}
	}
	if (show_raw)
		snprintf(b, bsize, "[%08x] ", handle);
	return buf;
}

static int pack_key(struct tc_u32_sel *sel, __u32 key, __u32 mask,
		    int off, int offmask)
{
	int i;
	int hwm = sel->nkeys;

	key &= mask;

	for (i=0; i<hwm; i++) {
		if (sel->keys[i].off == off && sel->keys[i].offmask == offmask) {
			__u32 intersect = mask&sel->keys[i].mask;

			if ((key^sel->keys[i].val) & intersect)
				return -1;
			sel->keys[i].val |= key;
			sel->keys[i].mask |= mask;
			return 0;
		}
	}

	if (hwm >= 128)
		return -1;
	if (off % 4)
		return -1;
	sel->keys[hwm].val = key;
	sel->keys[hwm].mask = mask;
	sel->keys[hwm].off = off;
	sel->keys[hwm].offmask = offmask;
	sel->nkeys++;
	return 0;
}

static int pack_key32(struct tc_u32_sel *sel, __u32 key, __u32 mask,
		      int off, int offmask)
{
	key = htonl(key);
	mask = htonl(mask);
	return pack_key(sel, key, mask, off, offmask);
}

static int pack_key16(struct tc_u32_sel *sel, __u32 key, __u32 mask,
		      int off, int offmask)
{
	if (key > 0xFFFF || mask > 0xFFFF)
		return -1;

	if ((off & 3) == 0) {
		key <<= 16;
		mask <<= 16;
	}
	off &= ~3;
	key = htonl(key);
	mask = htonl(mask);

	return pack_key(sel, key, mask, off, offmask);
}

static int pack_key8(struct tc_u32_sel *sel, __u32 key, __u32 mask, int off, int offmask)
{
	if (key > 0xFF || mask > 0xFF)
		return -1;

	if ((off & 3) == 0) {
		key <<= 24;
		mask <<= 24;
	} else if ((off & 3) == 1) {
		key <<= 16;
		mask <<= 16;
	} else if ((off & 3) == 2) {
		key <<= 8;
		mask <<= 8;
	}
	off &= ~3;
	key = htonl(key);
	mask = htonl(mask);

	return pack_key(sel, key, mask, off, offmask);
}


int parse_at(int *argc_p, char ***argv_p, int *off, int *offmask)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	char *p = *argv;

	if (argc <= 0)
		return -1;

	if (strlen(p) > strlen("nexthdr+") &&
	    memcmp(p, "nexthdr+", strlen("nexthdr+")) == 0) {
		*offmask = -1;
		p += strlen("nexthdr+");
	} else if (matches(*argv, "nexthdr+") == 0) {
		NEXT_ARG();
		*offmask = -1;
		p = *argv;
	}

	if (get_integer(off, p, 0))
		return -1;
	argc--; argv++;

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}


static int parse_u32(int *argc_p, char ***argv_p, struct tc_u32_sel *sel,
		     int off, int offmask)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	__u32 key;
	__u32 mask;

	if (argc < 2)
		return -1;

	if (get_u32(&key, *argv, 0))
		return -1;
	argc--; argv++;

	if (get_u32(&mask, *argv, 16))
		return -1;
	argc--; argv++;

	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}

	res = pack_key32(sel, key, mask, off, offmask);
	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_u16(int *argc_p, char ***argv_p, struct tc_u32_sel *sel,
		     int off, int offmask)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	__u32 key;
	__u32 mask;

	if (argc < 2)
		return -1;

	if (get_u32(&key, *argv, 0))
		return -1;
	argc--; argv++;

	if (get_u32(&mask, *argv, 16))
		return -1;
	argc--; argv++;

	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}
	res = pack_key16(sel, key, mask, off, offmask);
	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_u8(int *argc_p, char ***argv_p, struct tc_u32_sel *sel,
		    int off, int offmask)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	__u32 key;
	__u32 mask;

	if (argc < 2)
		return -1;

	if (get_u32(&key, *argv, 0))
		return -1;
	argc--; argv++;

	if (get_u32(&mask, *argv, 16))
		return -1;
	argc--; argv++;

	if (key > 0xFF || mask > 0xFF)
		return -1;

	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}

	res = pack_key8(sel, key, mask, off, offmask);
	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_ip_addr(int *argc_p, char ***argv_p, struct tc_u32_sel *sel,
			 int off)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	inet_prefix addr;
	__u32 mask;
	int offmask = 0;

	if (argc < 1)
		return -1;

	if (get_prefix_1(&addr, *argv, AF_INET))
		return -1;
	argc--; argv++;

	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}

	mask = 0;
	if (addr.bitlen)
		mask = htonl(0xFFFFFFFF<<(32-addr.bitlen));
	if (pack_key(sel, addr.data[0], mask, off, offmask) < 0)
		return -1;
	res = 0;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_ip6_addr(int *argc_p, char ***argv_p,
			  struct tc_u32_sel *sel, int off)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	int plen = 128;
	int i;
	inet_prefix addr;
	int offmask = 0;

	if (argc < 1)
		return -1;

	if (get_prefix_1(&addr, *argv, AF_INET6))
		return -1;
	argc--; argv++;

	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}

	plen = addr.bitlen;
	for (i=0; i<plen; i+=32) {
//		if (((i+31)&~0x1F)<=plen) {
		if (i + 31 <= plen) {
			res = pack_key(sel, addr.data[i/32],
				       0xFFFFFFFF, off+4*(i/32), offmask);
			if (res < 0)
				return -1;
		} else if (i < plen) {
			__u32 mask = htonl(0xFFFFFFFF << (32 - (plen -i )));
			res = pack_key(sel, addr.data[i/32],
				       mask, off+4*(i/32), offmask);
			if (res < 0)
				return -1;
		}
	}
	res = 0;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_ip6_class(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	__u32 key;
	__u32 mask;
	int off = 0;
	int offmask = 0;

	if (argc < 2)
		return -1;

	if (get_u32(&key, *argv, 0))
		return -1;
	argc--; argv++;

	if (get_u32(&mask, *argv, 16))
		return -1;
	argc--; argv++;

	if (key > 0xFF || mask > 0xFF)
		return -1;

	key <<= 20;
	mask <<= 20;
	key = htonl(key);
	mask = htonl(mask);

	res = pack_key(sel, key, mask, off, offmask);
	if (res < 0)
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}

static int parse_ether_addr(int *argc_p, char ***argv_p,
			    struct tc_u32_sel *sel, int off)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	__u8 addr[6];
	int offmask = 0;
	int i;

	if (argc < 1)
		return -1;

	if (sscanf(*argv, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		   addr + 0, addr + 1, addr + 2,
		   addr + 3, addr + 4, addr + 5) != 6) {
		fprintf(stderr, "parse_ether_addr: improperly formed address '%s'\n",
			*argv);
		return -1;
	}

	argc--; argv++;
	if (argc > 0 && strcmp(argv[0], "at") == 0) {
		NEXT_ARG();
		if (parse_at(&argc, &argv, &off, &offmask))
			return -1;
	}

	for (i = 0; i < 6; i++) {
		res = pack_key8(sel, addr[i], 0xFF, off + i, offmask);
		if (res < 0)
			return -1;
	}

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_ip(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "src") == 0) {
		NEXT_ARG();
		res = parse_ip_addr(&argc, &argv, sel, 12);
	} else if (strcmp(*argv, "dst") == 0) {
		NEXT_ARG();
		res = parse_ip_addr(&argc, &argv, sel, 16);
	} else if (strcmp(*argv, "tos") == 0 ||
	    matches(*argv, "dsfield") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 1, 0);
	} else if (strcmp(*argv, "ihl") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 0, 0);
	} else if (strcmp(*argv, "protocol") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 9, 0);
	} else if (matches(*argv, "precedence") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 1, 0);
	} else if (strcmp(*argv, "nofrag") == 0) {
		argc--; argv++;
		res = pack_key16(sel, 0, 0x3FFF, 6, 0);
	} else if (strcmp(*argv, "firstfrag") == 0) {
		argc--; argv++;
		res = pack_key16(sel, 0, 0x1FFF, 6, 0);
	} else if (strcmp(*argv, "df") == 0) {
		argc--; argv++;
		res = pack_key16(sel, 0x4000, 0x4000, 6, 0);
	} else if (strcmp(*argv, "mf") == 0) {
		argc--; argv++;
		res = pack_key16(sel, 0x2000, 0x2000, 6, 0);
	} else if (strcmp(*argv, "dport") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 22, 0);
	} else if (strcmp(*argv, "sport") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 20, 0);
	} else if (strcmp(*argv, "icmp_type") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 20, 0);
	} else if (strcmp(*argv, "icmp_code") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 20, 1);
	} else
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}
				
static int parse_ip6(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "src") == 0) {
		NEXT_ARG();
		res = parse_ip6_addr(&argc, &argv, sel, 8);
	} else if (strcmp(*argv, "dst") == 0) {
		NEXT_ARG();
		res = parse_ip6_addr(&argc, &argv, sel, 24);
	} else if (strcmp(*argv, "priority") == 0) {
		NEXT_ARG();
		res = parse_ip6_class(&argc, &argv, sel);
	} else if (strcmp(*argv, "protocol") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 6, 0);
	} else if (strcmp(*argv, "flowlabel") == 0) {
		NEXT_ARG();
		res = parse_u32(&argc, &argv, sel, 0, 0);
	} else if (strcmp(*argv, "dport") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 42, 0);
	} else if (strcmp(*argv, "sport") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 40, 0);
	} else if (strcmp(*argv, "icmp_type") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 40, 0);
	} else if (strcmp(*argv, "icmp_code") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 41, 1);
	} else
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_ether(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "src") == 0) {
		NEXT_ARG();
		res = parse_ether_addr(&argc, &argv, sel, -8);
	} else if (strcmp(*argv, "dst") == 0) {
		NEXT_ARG();
		res = parse_ether_addr(&argc, &argv, sel, -14);
	} else {
		fprintf(stderr, "Unknown match: ether %s\n", *argv);
		return -1;
	}

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

#define parse_tcp parse_udp
static int parse_udp(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "src") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 0, -1);
	} else if (strcmp(*argv, "dst") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 2, -1);
	} else
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}


static int parse_icmp(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "type") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 0, -1);
	} else if (strcmp(*argv, "code") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 1, -1);
	} else
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_mark(int *argc_p, char ***argv_p, struct nlmsghdr *n)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;
	struct tc_u32_mark mark;

	if (argc <= 1)
		return -1;

	if (get_u32(&mark.val, *argv, 0)) {
		fprintf(stderr, "Illegal \"mark\" value\n");
		return -1;
	}
	NEXT_ARG();

	if (get_u32(&mark.mask, *argv, 0)) {
		fprintf(stderr, "Illegal \"mark\" mask\n");
		return -1;
	}
	NEXT_ARG();

	if ((mark.val & mark.mask) != mark.val) {
		fprintf(stderr, "Illegal \"mark\" (impossible combination)\n");
		return -1;
	}

	addattr_l(n, MAX_MSG, TCA_U32_MARK, &mark, sizeof(mark));
	res = 0;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_selector(int *argc_p, char ***argv_p,
			  struct tc_u32_sel *sel, struct nlmsghdr *n)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	int res = -1;

	if (argc <= 0)
		return -1;

	if (matches(*argv, "u32") == 0) {
		NEXT_ARG();
		res = parse_u32(&argc, &argv, sel, 0, 0);
	} else if (matches(*argv, "u16") == 0) {
		NEXT_ARG();
		res = parse_u16(&argc, &argv, sel, 0, 0);
	} else if (matches(*argv, "u8") == 0) {
		NEXT_ARG();
		res = parse_u8(&argc, &argv, sel, 0, 0);
	} else if (matches(*argv, "ip") == 0) {
		NEXT_ARG();
		res = parse_ip(&argc, &argv, sel);
	} else 	if (matches(*argv, "ip6") == 0) {
		NEXT_ARG();
		res = parse_ip6(&argc, &argv, sel);
	} else if (matches(*argv, "udp") == 0) {
		NEXT_ARG();
		res = parse_udp(&argc, &argv, sel);
	} else if (matches(*argv, "tcp") == 0) {
		NEXT_ARG();
		res = parse_tcp(&argc, &argv, sel);
	} else if (matches(*argv, "icmp") == 0) {
		NEXT_ARG();
		res = parse_icmp(&argc, &argv, sel);
	} else if (matches(*argv, "mark") == 0) {
		NEXT_ARG();
		res = parse_mark(&argc, &argv, n);
	} else if (matches(*argv, "ether") == 0) {
		NEXT_ARG();
		res = parse_ether(&argc, &argv, sel);
	} else 
		return -1;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int parse_offset(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int argc = *argc_p;
	char **argv = *argv_p;

	while (argc > 0) {
		if (matches(*argv, "plus") == 0) {
			int off;
			NEXT_ARG();
			if (get_integer(&off, *argv, 0))
				return -1;
			sel->off = off;
			sel->flags |= TC_U32_OFFSET;
		} else if (matches(*argv, "at") == 0) {
			int off;
			NEXT_ARG();
			if (get_integer(&off, *argv, 0))
				return -1;
			sel->offoff = off;
			if (off%2) {
				fprintf(stderr, "offset \"at\" must be even\n");
				return -1;
			}
			sel->flags |= TC_U32_VAROFFSET;
		} else if (matches(*argv, "mask") == 0) {
			__u16 mask;
			NEXT_ARG();
			if (get_u16(&mask, *argv, 16))
				return -1;
			sel->offmask = htons(mask);
			sel->flags |= TC_U32_VAROFFSET;
		} else if (matches(*argv, "shift") == 0) {
			int shift;
			NEXT_ARG();
			if (get_integer(&shift, *argv, 0))
				return -1;
			sel->offshift = shift;
			sel->flags |= TC_U32_VAROFFSET;
		} else if (matches(*argv, "eat") == 0) {
			sel->flags |= TC_U32_EAT;
		} else {
			break;
		}
		argc--; argv++;
	}

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}

static int parse_hashkey(int *argc_p, char ***argv_p, struct tc_u32_sel *sel)
{
	int argc = *argc_p;
	char **argv = *argv_p;

	while (argc > 0) {
		if (matches(*argv, "mask") == 0) {
			__u32 mask;
			NEXT_ARG();
			if (get_u32(&mask, *argv, 16))
				return -1;
			sel->hmask = htonl(mask);
		} else if (matches(*argv, "at") == 0) {
			int num;
			NEXT_ARG();
			if (get_integer(&num, *argv, 0))
				return -1;
			if (num%4)
				return -1;
			sel->hoff = num;
		} else {
			break;
		}
		argc--; argv++;
	}

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}

static void print_ipv4(FILE *f, const struct tc_u32_key *key)
{
	char abuf[256];

	switch (key->off) {
	case 0:
		switch (ntohl(key->mask)) {
		case 0x0f000000:
			fprintf(f, "\n  match IP ihl %u", ntohl(key->val) >> 24);
			return;
		case 0x00ff0000:
			fprintf(f, "\n  match IP dsfield %#x", ntohl(key->val) >> 16);
			return;
		}
		break;
	case 8:
		if (ntohl(key->mask) == 0x00ff0000) {
			fprintf(f, "\n  match IP protocol %d", ntohl(key->val) >> 16);
			return;
		}
		break;
	case 12:
	case 16: {
			int bits = mask2bits(key->mask);
			if (bits >= 0) {
				fprintf(f, "\n  %s %s/%d", 
					key->off == 12 ? "match IP src" : "match IP dst",
					inet_ntop(AF_INET, &key->val,
						  abuf, sizeof(abuf)),
					bits);
				return;
			}
		}
		break;

	case 20:
		switch (ntohl(key->mask)) {
		case 0x0000ffff:
			fprintf(f, "\n  match dport %u",
				ntohl(key->val) & 0xffff);
			return;
		case 0xffff0000:
			fprintf(f, "\n  match sport %u",
				ntohl(key->val) >> 16);
			return;
		case 0xffffffff:
			fprintf(f, "\n  match dport %u, match sport %u",
				ntohl(key->val) & 0xffff,
				ntohl(key->val) >> 16);

			return;
		}
		/* XXX: Default print_raw */
	}
}

static void print_ipv6(FILE *f, const struct tc_u32_key *key)
{
	char abuf[256];

	switch (key->off) {
	case 0:
		switch (ntohl(key->mask)) {
		case 0x0f000000:
			fprintf(f, "\n  match IP ihl %u", ntohl(key->val) >> 24);
			return;
		case 0x00ff0000:
			fprintf(f, "\n  match IP dsfield %#x", ntohl(key->val) >> 16);
			return;
		}
		break;
	case 8:
		if (ntohl(key->mask) == 0x00ff0000) {
			fprintf(f, "\n  match IP protocol %d", ntohl(key->val) >> 16);
			return;
		}
		break;
	case 12:
	case 16: {
			int bits = mask2bits(key->mask);
			if (bits >= 0) {
				fprintf(f, "\n  %s %s/%d", 
					key->off == 12 ? "match IP src" : "match IP dst",
					inet_ntop(AF_INET, &key->val,
						  abuf, sizeof(abuf)),
					bits);
				return;
			}
		}
		break;

	case 20:
		switch (ntohl(key->mask)) {
		case 0x0000ffff:
			fprintf(f, "\n  match sport %u",
				ntohl(key->val) & 0xffff);
			return;
		case 0xffff0000:
			fprintf(f, "\n  match dport %u",
				ntohl(key->val) >> 16);
			return;
		case 0xffffffff:
			fprintf(f, "\n  match sport %u, match dport %u",
				ntohl(key->val) & 0xffff,
				ntohl(key->val) >> 16);

			return;
		}
		/* XXX: Default print_raw */
	}
}

static void print_raw(FILE *f, const struct tc_u32_key *key)
{
	fprintf(f, "\n  match %08x/%08x at %s%d", 
		(unsigned int)ntohl(key->val),
		(unsigned int)ntohl(key->mask),
		key->offmask ? "nexthdr+" : "",
		key->off);
}

static const struct {
	__u16 proto;
	__u16 pad;
	void (*pprinter)(FILE *f, const struct tc_u32_key *key);
} u32_pprinters[] = {
	{0, 	   0, print_raw},
	{ETH_P_IP, 0, print_ipv4},
	{ETH_P_IPV6, 0, print_ipv6},
};

static void show_keys(FILE *f, const struct tc_u32_key *key)
{
	int i = 0;

	if (!show_pretty)
		goto show_k;

	for (i = 0; i < sizeof(u32_pprinters) / sizeof(u32_pprinters[0]); i++) {
		if (u32_pprinters[i].proto == ntohs(f_proto)) {
show_k:
			u32_pprinters[i].pprinter(f, key);
			return;
		}
	}

	i = 0;
	goto show_k;
}

static int u32_parse_opt(struct filter_util *qu, char *handle,
			 int argc, char **argv, struct nlmsghdr *n)
{
	struct {
		struct tc_u32_sel sel;
		struct tc_u32_key keys[128];
	} sel;
	struct tcmsg *t = NLMSG_DATA(n);
	struct rtattr *tail;
	int sel_ok = 0, terminal_ok = 0;
	int sample_ok = 0;
	__u32 htid = 0;
	__u32 order = 0;

	memset(&sel, 0, sizeof(sel));

	if (handle && get_u32_handle(&t->tcm_handle, handle)) {
		fprintf(stderr, "Illegal filter ID\n");
		return -1;
	}

	if (argc == 0)
		return 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "match") == 0) {
			NEXT_ARG();
			if (parse_selector(&argc, &argv, &sel.sel, n)) {
				fprintf(stderr, "Illegal \"match\"\n");
				return -1;
			}
			sel_ok++;
			continue;
		} else if (matches(*argv, "offset") == 0) {
			NEXT_ARG();
			if (parse_offset(&argc, &argv, &sel.sel)) {
				fprintf(stderr, "Illegal \"offset\"\n");
				return -1;
			}
			continue;
		} else if (matches(*argv, "hashkey") == 0) {
			NEXT_ARG();
			if (parse_hashkey(&argc, &argv, &sel.sel)) {
				fprintf(stderr, "Illegal \"hashkey\"\n");
				return -1;
			}
			continue;
		} else if (matches(*argv, "classid") == 0 ||
			   strcmp(*argv, "flowid") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, MAX_MSG, TCA_U32_CLASSID, &handle, 4);
			sel.sel.flags |= TC_U32_TERMINAL;
		} else if (matches(*argv, "divisor") == 0) {
			unsigned divisor;
			NEXT_ARG();
			if (get_unsigned(&divisor, *argv, 0) ||
			    divisor == 0 ||
			    divisor > 0x100 || ((divisor - 1) & divisor)) {
				fprintf(stderr, "Illegal \"divisor\"\n");
				return -1;
			}
			addattr_l(n, MAX_MSG, TCA_U32_DIVISOR, &divisor, 4);
		} else if (matches(*argv, "order") == 0) {
			NEXT_ARG();
			if (get_u32(&order, *argv, 0)) {
				fprintf(stderr, "Illegal \"order\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "link") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_u32_handle(&handle, *argv)) {
				fprintf(stderr, "Illegal \"link\"\n");
				return -1;
			}
			if (handle && TC_U32_NODE(handle)) {
				fprintf(stderr, "\"link\" must be a hash table.\n");
				return -1;
			}
			addattr_l(n, MAX_MSG, TCA_U32_LINK, &handle, 4);
		} else if (strcmp(*argv, "ht") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_u32_handle(&handle, *argv)) {
				fprintf(stderr, "Illegal \"ht\"\n");
				return -1;
			}
			if (handle && TC_U32_NODE(handle)) {
				fprintf(stderr, "\"ht\" must be a hash table.\n");
				return -1;
			}
			if (sample_ok)
				htid = (htid&0xFF000)|(handle&0xFFF00000);
			else
				htid = (handle&0xFFFFF000);
		} else if (strcmp(*argv, "sample") == 0) {
			__u32 hash;
			unsigned divisor = 0x100;

			struct {
				struct tc_u32_sel sel;
				struct tc_u32_key keys[4];
			} sel2;
			memset(&sel2, 0, sizeof(sel2));
			NEXT_ARG();
			if (parse_selector(&argc, &argv, &sel2.sel, n)) {
				fprintf(stderr, "Illegal \"sample\"\n");
				return -1;
			}
			if (sel2.sel.nkeys != 1) {
				fprintf(stderr, "\"sample\" must contain"
					" exactly ONE key.\n");
				return -1;
			}
			if (*argv != 0 && strcmp(*argv, "divisor") == 0) {
				NEXT_ARG();
				if (get_unsigned(&divisor, *argv, 0) || divisor == 0 ||
				    divisor > 0x100 || ((divisor - 1) & divisor)) {
					fprintf(stderr, "Illegal sample \"divisor\"\n");
					return -1;
				}
				NEXT_ARG();
			}
			hash = sel2.sel.keys[0].val&sel2.sel.keys[0].mask;
			hash ^= hash>>16;
			hash ^= hash>>8;
			htid = ((hash%divisor)<<12)|(htid&0xFFF00000);
			sample_ok = 1;
			continue;
		} else if (strcmp(*argv, "indev") == 0) {
			char ind[IFNAMSIZ + 1];
			memset(ind, 0, sizeof (ind));
			argc--;
			argv++;
			if (argc < 1) {
				fprintf(stderr, "Illegal indev\n");
				return -1;
			}
			strncpy(ind, *argv, sizeof (ind) - 1);
			addattr_l(n, MAX_MSG, TCA_U32_INDEV, ind, strlen(ind) + 1);

		} else if (matches(*argv, "action") == 0) {
			NEXT_ARG();
			if (parse_action(&argc, &argv, TCA_U32_ACT, n)) {
				fprintf(stderr, "Illegal \"action\"\n");
				return -1;
			}
			terminal_ok++;
			continue;

		} else if (matches(*argv, "police") == 0) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_U32_POLICE, n)) {
				fprintf(stderr, "Illegal \"police\"\n");
				return -1;
			}
			terminal_ok++;
			continue;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	/* We dont necessarily need class/flowids */
	if (terminal_ok)
		sel.sel.flags |= TC_U32_TERMINAL;
	
	if (order) {
		if (TC_U32_NODE(t->tcm_handle) && order != TC_U32_NODE(t->tcm_handle)) {
			fprintf(stderr, "\"order\" contradicts \"handle\"\n");
			return -1;
		}
		t->tcm_handle |= order;
	}

	if (htid)
		addattr_l(n, MAX_MSG, TCA_U32_HASH, &htid, 4);
	if (sel_ok)
		addattr_l(n, MAX_MSG, TCA_U32_SEL, &sel, 
			  sizeof(sel.sel)+sel.sel.nkeys*sizeof(struct tc_u32_key));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int u32_print_opt(struct filter_util *qu, FILE *f, struct rtattr *opt,
			 __u32 handle)
{
	struct rtattr *tb[TCA_U32_MAX+1];
	struct tc_u32_sel *sel = NULL;
	struct tc_u32_pcnt *pf = NULL;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_U32_MAX, opt);

	if (handle) {
		SPRINT_BUF(b1);
		fprintf(f, "fh %s ", sprint_u32_handle(handle, b1));
	}
	if (TC_U32_NODE(handle)) {
		fprintf(f, "order %d ", TC_U32_NODE(handle));
	}

	if (tb[TCA_U32_SEL]) {
		if (RTA_PAYLOAD(tb[TCA_U32_SEL])  < sizeof(*sel))
			return -1;

		sel = RTA_DATA(tb[TCA_U32_SEL]);
	}

	if (tb[TCA_U32_DIVISOR]) {
		fprintf(f, "ht divisor %d ", *(__u32*)RTA_DATA(tb[TCA_U32_DIVISOR]));
	} else if (tb[TCA_U32_HASH]) {
		__u32 htid = *(__u32*)RTA_DATA(tb[TCA_U32_HASH]);
		fprintf(f, "key ht %x bkt %x ", TC_U32_USERHTID(htid),
			TC_U32_HASH(htid));
	} else {
		fprintf(f, "??? ");
	}
	if (tb[TCA_U32_CLASSID]) {
		SPRINT_BUF(b1);
		fprintf(f, "%sflowid %s ",
			!sel || !(sel->flags&TC_U32_TERMINAL) ? "*" : "",
			sprint_tc_classid(*(__u32*)RTA_DATA(tb[TCA_U32_CLASSID]), b1));
	} else if (sel && sel->flags&TC_U32_TERMINAL) {
		fprintf(f, "terminal flowid ??? ");
	}
	if (tb[TCA_U32_LINK]) {
		SPRINT_BUF(b1);
		fprintf(f, "link %s ",
			sprint_u32_handle(*(__u32*)RTA_DATA(tb[TCA_U32_LINK]), b1));
	}

	if (tb[TCA_U32_PCNT]) {
		if (RTA_PAYLOAD(tb[TCA_U32_PCNT])  < sizeof(*pf)) {
			fprintf(f, "Broken perf counters \n");
			return -1;
		}
		pf = RTA_DATA(tb[TCA_U32_PCNT]);
	}

	if (sel && show_stats && NULL != pf)
		fprintf(f, " (rule hit %llu success %llu)",
			(unsigned long long) pf->rcnt,
			(unsigned long long) pf->rhit);

	if (tb[TCA_U32_MARK]) {
		struct tc_u32_mark *mark = RTA_DATA(tb[TCA_U32_MARK]);
		if (RTA_PAYLOAD(tb[TCA_U32_MARK]) < sizeof(*mark)) {
			fprintf(f, "\n  Invalid mark (kernel&iproute2 mismatch)\n");
		} else {
			fprintf(f, "\n  mark 0x%04x 0x%04x (success %d)",
				mark->val, mark->mask, mark->success);
		}
	}

	if (sel) {
		if (sel->nkeys) {
			int i;
			for (i=0; i<sel->nkeys; i++) {
				show_keys(f, sel->keys + i);
				if (show_stats && NULL != pf)
					fprintf(f, " (success %llu ) ",
						(unsigned long long) pf->kcnts[i]);
			}
		}

		if (sel->flags&(TC_U32_VAROFFSET|TC_U32_OFFSET)) {
			fprintf(f, "\n    offset ");
			if (sel->flags&TC_U32_VAROFFSET)
				fprintf(f, "%04x>>%d at %d ",
					ntohs(sel->offmask),
					sel->offshift,  sel->offoff);
			if (sel->off)
				fprintf(f, "plus %d ", sel->off);
		}
		if (sel->flags&TC_U32_EAT)
			fprintf(f, " eat ");

		if (sel->hmask) {
			fprintf(f, "\n    hash mask %08x at %d ",
				(unsigned int)htonl(sel->hmask), sel->hoff);
		}
	}

	if (tb[TCA_U32_POLICE]) {
		fprintf(f, "\n");
		tc_print_police(f, tb[TCA_U32_POLICE]);
	}
	if (tb[TCA_U32_INDEV]) {
		struct rtattr *idev = tb[TCA_U32_INDEV];
		fprintf(f, "\n  input dev %s\n", (char *) RTA_DATA(idev));
	}
	if (tb[TCA_U32_ACT]) {
		tc_print_action(f, tb[TCA_U32_ACT]);
	}

	return 0;
}

struct filter_util u32_filter_util = {
	.id = "u32",
	.parse_fopt = u32_parse_opt,
	.print_fopt = u32_print_opt,
};

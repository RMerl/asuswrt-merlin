/*
 * em_u32.c		U32 Ematch
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

extern struct ematch_util u32_ematch_util;

static void u32_print_usage(FILE *fd)
{
	fprintf(fd,
	    "Usage: u32(ALIGN VALUE MASK at [ nexthdr+ ] OFFSET)\n" \
	    "where: ALIGN  := { u8 | u16 | u32 }\n" \
	    "\n" \
	    "Example: u32(u16 0x1122 0xffff at nexthdr+4)\n");
}

static int u32_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			  struct bstr *args)
{
	struct bstr *a;
	int align, nh_len;
	unsigned long key, mask, offmask = 0, offset;
	struct tc_u32_key u_key;

	memset(&u_key, 0, sizeof(u_key));

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &u32_ematch_util, FMT ,##ARGS)

	if (args == NULL)
		return PARSE_ERR(args, "u32: missing arguments");

	if (!bstrcmp(args, "u8"))
		align = 1;
	else if (!bstrcmp(args, "u16"))
		align = 2;
	else if (!bstrcmp(args, "u32"))
		align = 4;
	else
		return PARSE_ERR(args, "u32: invalid alignment");

	a = bstr_next(args);
	if (a == NULL)
		return PARSE_ERR(a, "u32: missing key");

	key = bstrtoul(a);
	if (key == ULONG_MAX)
		return PARSE_ERR(a, "u32: invalid key, must be numeric");

	a = bstr_next(a);
	if (a == NULL)
		return PARSE_ERR(a, "u32: missing mask");

	mask = bstrtoul(a);
	if (mask == ULONG_MAX)
		return PARSE_ERR(a, "u32: invalid mask, must be numeric");

	a = bstr_next(a);
	if (a == NULL || bstrcmp(a, "at") != 0)
		return PARSE_ERR(a, "u32: missing \"at\"");

	a = bstr_next(a);
	if (a == NULL)
		return PARSE_ERR(a, "u32: missing offset");

	nh_len = strlen("nexthdr+");
	if (a->len > nh_len && !memcmp(a->data, "nexthdr+", nh_len)) {
		char buf[a->len - nh_len + 1];
		offmask = -1;
		memcpy(buf, a->data + nh_len, a->len - nh_len);
		offset = strtoul(buf, NULL, 0);
	} else if (!bstrcmp(a, "nexthdr+")) {
		a = bstr_next(a);
		if (a == NULL)
			return PARSE_ERR(a, "u32: missing offset");
		offset = bstrtoul(a);
	} else
		offset = bstrtoul(a);

	if (offset == ULONG_MAX)
		return PARSE_ERR(a, "u32: invalid offset");

	if (a->next)
		return PARSE_ERR(a->next, "u32: unexpected trailer");

	switch (align) {
		case 1:
			if (key > 0xFF)
				return PARSE_ERR(a, "Illegal key (>0xFF)");
			if (mask > 0xFF)
				return PARSE_ERR(a, "Illegal mask (>0xFF)");

			key <<= 24 - ((offset & 3) * 8);
			mask <<= 24 - ((offset & 3) * 8);
			offset &= ~3;
			break;

		case 2:
			if (key > 0xFFFF)
				return PARSE_ERR(a, "Illegal key (>0xFFFF)");
			if (mask > 0xFFFF)
				return PARSE_ERR(a, "Illegal mask (>0xFFFF)");

			if ((offset & 3) == 0) {
				key <<= 16;
				mask <<= 16;
			}
			offset &= ~3;
			break;
	}

	key = htonl(key);
	mask = htonl(mask);

	if (offset % 4)
		return PARSE_ERR(a, "u32: invalid offset alignment, " \
		    "must be aligned to 4.");

	key &= mask;

	u_key.mask = mask;
	u_key.val = key;
	u_key.off = offset;
	u_key.offmask = offmask;

	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));
	addraw_l(n, MAX_MSG, &u_key, sizeof(u_key));

#undef PARSE_ERR
	return 0;
}

static int u32_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			  int data_len)
{
	struct tc_u32_key *u_key = data;

	if (data_len < sizeof(*u_key)) {
		fprintf(stderr, "U32 header size mismatch\n");
		return -1;
	}

	fprintf(fd, "%08x/%08x at %s%d",
	    (unsigned int) ntohl(u_key->val),
	    (unsigned int) ntohl(u_key->mask),
	    u_key->offmask ? "nexthdr+" : "",
	    u_key->off);

	return 0;
}

struct ematch_util u32_ematch_util = {
	.kind = "u32",
	.kind_num = TCF_EM_U32,
	.parse_eopt = u32_parse_eopt,
	.print_eopt = u32_print_eopt,
	.print_usage = u32_print_usage
};

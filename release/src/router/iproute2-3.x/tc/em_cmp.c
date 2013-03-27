/*
 * em_cmp.c		Simple comparison Ematch
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
#include <linux/tc_ematch/tc_em_cmp.h>

extern struct ematch_util cmp_ematch_util;

static void cmp_print_usage(FILE *fd)
{
	fprintf(fd,
	    "Usage: cmp(ALIGN at OFFSET [ ATTRS ] { eq | lt | gt } VALUE)\n" \
	    "where: ALIGN  := { u8 | u16 | u32 }\n" \
	    "       ATTRS  := [ layer LAYER ] [ mask MASK ] [ trans ]\n" \
	    "       LAYER  := { link | network | transport | 0..%d }\n" \
	    "\n" \
	    "Example: cmp(u16 at 3 layer 2 mask 0xff00 gt 20)\n",
	    TCF_LAYER_MAX);
}

static int cmp_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			  struct bstr *args)
{
	struct bstr *a;
	int align, opnd = 0;
	unsigned long offset = 0, layer = TCF_LAYER_NETWORK, mask = 0, value = 0;
	int offset_present = 0, value_present = 0;
	struct tcf_em_cmp cmp;

	memset(&cmp, 0, sizeof(cmp));

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &cmp_ematch_util, FMT ,##ARGS)

	if (args == NULL)
		return PARSE_ERR(args, "cmp: missing arguments");

	if (!bstrcmp(args, "u8"))
		align = TCF_EM_ALIGN_U8;
	else if (!bstrcmp(args, "u16"))
		align = TCF_EM_ALIGN_U16;
	else if (!bstrcmp(args, "u32"))
		align = TCF_EM_ALIGN_U32;
	else
		return PARSE_ERR(args, "cmp: invalid alignment");

	for (a = bstr_next(args); a; a = bstr_next(a)) {
		if (!bstrcmp(a, "at")) {
			if (a->next == NULL)
				return PARSE_ERR(a, "cmp: missing argument");
			a = bstr_next(a);

			offset = bstrtoul(a);
			if (offset == ULONG_MAX)
				return PARSE_ERR(a, "cmp: invalid offset, " \
				    "must be numeric");

			offset_present = 1;
		} else if (!bstrcmp(a, "layer")) {
			if (a->next == NULL)
				return PARSE_ERR(a, "cmp: missing argument");
			a = bstr_next(a);

			layer = parse_layer(a);
			if (layer == INT_MAX) {
				layer = bstrtoul(a);
				if (layer == ULONG_MAX)
					return PARSE_ERR(a, "cmp: invalid " \
					    "layer");
			}

			if (layer > TCF_LAYER_MAX)
				return PARSE_ERR(a, "cmp: illegal layer, " \
				    "must be in 0..%d", TCF_LAYER_MAX);
		} else if (!bstrcmp(a, "mask")) {
			if (a->next == NULL)
				return PARSE_ERR(a, "cmp: missing argument");
			a = bstr_next(a);

			mask = bstrtoul(a);
			if (mask == ULONG_MAX)
				return PARSE_ERR(a, "cmp: invalid mask");
		} else if (!bstrcmp(a, "trans")) {
			cmp.flags |= TCF_EM_CMP_TRANS;
		} else if (!bstrcmp(a, "eq") || !bstrcmp(a, "gt") ||
		    !bstrcmp(a, "lt")) {

			if (!bstrcmp(a, "eq"))
				opnd = TCF_EM_OPND_EQ;
			else if (!bstrcmp(a, "gt"))
				opnd = TCF_EM_OPND_GT;
			else if (!bstrcmp(a, "lt"))
				opnd = TCF_EM_OPND_LT;

			if (a->next == NULL)
				return PARSE_ERR(a, "cmp: missing argument");
			a = bstr_next(a);

			value = bstrtoul(a);
			if (value == ULONG_MAX)
				return PARSE_ERR(a, "cmp: invalid value");

			value_present = 1;
		} else
			return PARSE_ERR(a, "nbyte: unknown parameter");
	}

	if (offset_present == 0 || value_present == 0)
		return PARSE_ERR(a, "cmp: offset and value required");

	cmp.val = (__u32) value;
	cmp.mask = (__u32) mask;
	cmp.off = (__u16) offset;
	cmp.align = (__u8) align;
	cmp.layer = (__u8) layer;
	cmp.opnd = (__u8) opnd;

	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));
	addraw_l(n, MAX_MSG, &cmp, sizeof(cmp));

#undef PARSE_ERR
	return 0;
}

static int cmp_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			  int data_len)
{
	struct tcf_em_cmp *cmp = data;

	if (data_len < sizeof(*cmp)) {
		fprintf(stderr, "CMP header size mismatch\n");
		return -1;
	}

	if (cmp->align == TCF_EM_ALIGN_U8)
		fprintf(fd, "u8 ");
	else if (cmp->align == TCF_EM_ALIGN_U16)
		fprintf(fd, "u16 ");
	else if (cmp->align == TCF_EM_ALIGN_U32)
		fprintf(fd, "u32 ");

	fprintf(fd, "at %d layer %d ", cmp->off, cmp->layer);

	if (cmp->mask)
		fprintf(fd, "mask 0x%x ", cmp->mask);

	if (cmp->flags & TCF_EM_CMP_TRANS)
		fprintf(fd, "trans ");

	if (cmp->opnd == TCF_EM_OPND_EQ)
		fprintf(fd, "eq ");
	else if (cmp->opnd == TCF_EM_OPND_LT)
		fprintf(fd, "lt ");
	else if (cmp->opnd == TCF_EM_OPND_GT)
		fprintf(fd, "gt ");

	fprintf(fd, "%d", cmp->val);

	return 0;
}

struct ematch_util cmp_ematch_util = {
	.kind = "cmp",
	.kind_num = TCF_EM_CMP,
	.parse_eopt = cmp_parse_eopt,
	.print_eopt = cmp_print_eopt,
	.print_usage = cmp_print_usage
};

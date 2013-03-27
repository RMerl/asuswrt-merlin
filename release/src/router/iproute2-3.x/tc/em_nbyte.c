/*
 * em_nbyte.c		N-Byte Ematch
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
#include <linux/tc_ematch/tc_em_nbyte.h>

extern struct ematch_util nbyte_ematch_util;

static void nbyte_print_usage(FILE *fd)
{
	fprintf(fd,
	    "Usage: nbyte(NEEDLE at OFFSET [layer LAYER])\n" \
	    "where: NEEDLE := { string | \"c-escape-sequence\" }\n" \
	    "       OFFSET := int\n" \
	    "       LAYER  := { link | network | transport | 0..%d }\n" \
	    "\n" \
	    "Example: nbyte(\"ababa\" at 12 layer 1)\n",
	    TCF_LAYER_MAX);
}

static int nbyte_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			    struct bstr *args)
{
	struct bstr *a;
	struct bstr *needle = args;
	unsigned long offset = 0, layer = TCF_LAYER_NETWORK;
	int offset_present = 0;
	struct tcf_em_nbyte nb;

	memset(&nb, 0, sizeof(nb));

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &nbyte_ematch_util, FMT ,##ARGS)

	if (args == NULL)
		return PARSE_ERR(args, "nbyte: missing arguments");

	if (needle->len <= 0)
		return PARSE_ERR(args, "nbyte: needle length is 0");

	for (a = bstr_next(args); a; a = bstr_next(a)) {
		if (!bstrcmp(a, "at")) {
			if (a->next == NULL)
				return PARSE_ERR(a, "nbyte: missing argument");
			a = bstr_next(a);

			offset = bstrtoul(a);
			if (offset == ULONG_MAX)
				return PARSE_ERR(a, "nbyte: invalid offset, " \
				    "must be numeric");

			offset_present = 1;
		} else if (!bstrcmp(a, "layer")) {
			if (a->next == NULL)
				return PARSE_ERR(a, "nbyte: missing argument");
			a = bstr_next(a);

			layer = parse_layer(a);
			if (layer == INT_MAX) {
				layer = bstrtoul(a);
				if (layer == ULONG_MAX)
					return PARSE_ERR(a, "nbyte: invalid " \
					    "layer");
			}

			if (layer > TCF_LAYER_MAX)
				return PARSE_ERR(a, "nbyte: illegal layer, " \
				    "must be in 0..%d", TCF_LAYER_MAX);
		} else
			return PARSE_ERR(a, "nbyte: unknown parameter");
	}

	if (offset_present == 0)
		return PARSE_ERR(a, "nbyte: offset required");

	nb.len = needle->len;
	nb.layer = (__u8) layer;
	nb.off = (__u16) offset;

	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));
	addraw_l(n, MAX_MSG, &nb, sizeof(nb));
	addraw_l(n, MAX_MSG, needle->data, needle->len);

#undef PARSE_ERR
	return 0;
}

static int nbyte_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			    int data_len)
{
	int i;
	struct tcf_em_nbyte *nb = data;
	__u8 *needle;

	if (data_len < sizeof(*nb)) {
		fprintf(stderr, "NByte header size mismatch\n");
		return -1;
	}

	if (data_len < sizeof(*nb) + nb->len) {
		fprintf(stderr, "NByte payload size mismatch\n");
		return -1;
	}

	needle = data + sizeof(*nb);

	for (i = 0; i < nb->len; i++)
		fprintf(fd, "%02x ", needle[i]);

	fprintf(fd, "\"");
	for (i = 0; i < nb->len; i++)
		fprintf(fd, "%c", isprint(needle[i]) ? needle[i] : '.');
	fprintf(fd, "\" at %d layer %d", nb->off, nb->layer);

	return 0;
}

struct ematch_util nbyte_ematch_util = {
	.kind = "nbyte",
	.kind_num = TCF_EM_NBYTE,
	.parse_eopt = nbyte_parse_eopt,
	.print_eopt = nbyte_print_eopt,
	.print_usage = nbyte_print_usage
};

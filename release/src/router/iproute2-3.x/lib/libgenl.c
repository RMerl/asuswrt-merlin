/*
 * libgenl.c	GENL library
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/genetlink.h>
#include "libgenl.h"

static int genl_parse_getfamily(struct nlmsghdr *nlh)
{
	struct rtattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *ghdr = NLMSG_DATA(nlh);
	int len = nlh->nlmsg_len;
	struct rtattr *attrs;

	if (nlh->nlmsg_type != GENL_ID_CTRL) {
		fprintf(stderr, "Not a controller message, nlmsg_len=%d "
			"nlmsg_type=0x%x\n", nlh->nlmsg_len, nlh->nlmsg_type);
		return -1;
	}

	len -= NLMSG_LENGTH(GENL_HDRLEN);

	if (len < 0) {
		fprintf(stderr, "wrong controller message len %d\n", len);
		return -1;
	}

	if (ghdr->cmd != CTRL_CMD_NEWFAMILY) {
		fprintf(stderr, "Unknown controller command %d\n", ghdr->cmd);
		return -1;
	}

	attrs = (struct rtattr *) ((char *) ghdr + GENL_HDRLEN);
	parse_rtattr(tb, CTRL_ATTR_MAX, attrs, len);

	if (tb[CTRL_ATTR_FAMILY_ID] == NULL) {
		fprintf(stderr, "Missing family id TLV\n");
		return -1;
	}

	return rta_getattr_u16(tb[CTRL_ATTR_FAMILY_ID]);
}

int genl_resolve_family(struct rtnl_handle *grth, const char *family)
{
	GENL_REQUEST(req, 1024, GENL_ID_CTRL, 0, 0, CTRL_CMD_GETFAMILY,
		     NLM_F_REQUEST);

	addattr_l(&req.n, sizeof(req), CTRL_ATTR_FAMILY_NAME,
		  family, strlen(family) + 1);

	if (rtnl_talk(grth, &req.n, 0, 0, &req.n) < 0) {
		fprintf(stderr, "Error talking to the kernel\n");
		return -2;
	}

	return genl_parse_getfamily(&req.n);
}


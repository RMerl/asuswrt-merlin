#ifndef __LIBGENL_H__
#define __LIBGENL_H__

#include "libnetlink.h"

#define GENL_REQUEST(_req, _bufsiz, _family, _hdrsiz, _ver, _cmd, _flags) \
struct {								\
	struct nlmsghdr		n;					\
	struct genlmsghdr	g;					\
	char			buf[NLMSG_ALIGN(_hdrsiz) + (_bufsiz)];	\
} _req = {								\
	.n = {								\
		.nlmsg_type = (_family),				\
		.nlmsg_flags = (_flags),				\
		.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN + (_hdrsiz)),	\
	},								\
	.g = {								\
		.cmd = (_cmd),						\
		.version = (_ver),					\
	},								\
}

extern int genl_resolve_family(struct rtnl_handle *grth, const char *family);

#endif /* __LIBGENL_H__ */

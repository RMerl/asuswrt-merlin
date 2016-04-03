#ifndef _RTNL_H
#define _RTNL_H

#include <linux/types.h>
#include <linux/rtnetlink.h>

struct rtnl_handler {
	struct rtnl_handler *next;

	u_int16_t	nlmsg_type;
	int		(*handlefn)(struct nlmsghdr *h, void *arg);
	void		*arg;
};

struct rtnl_handle {
	int rtnl_fd;
	int rtnl_seq;
	int rtnl_dump;
	struct sockaddr_nl rtnl_local;
	struct rtnl_handler *handlers;
};

/* api for handler plugins */
int rtnl_handler_register(struct rtnl_handle *rtnl_handle, 
			  struct rtnl_handler *hdlr);
int rtnl_handler_unregister(struct rtnl_handle *rtnl_handle,
			    struct rtnl_handler *hdlr);
int rtnl_parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len);
int rtnl_dump_type(struct rtnl_handle *rtnl_handle, unsigned int type);

/* api for core program */
struct rtnl_handle *rtnl_open(void);
void rtnl_close(struct rtnl_handle *rtnl_handle);
int rtnl_receive(struct rtnl_handle *rtnl_handle);
int rtnl_receive_multi(struct rtnl_handle *rtnl_handle);

#endif

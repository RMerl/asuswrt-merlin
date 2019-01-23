/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_TRANSPORT_H
#define LIBIPSET_TRANSPORT_H

#include <stdint.h>				/* uintxx_t */
#include <linux/netlink.h>			/* struct nlmsghdr  */

#include <libmnl/libmnl.h>			/* mnl_cb_t */

#include <libipset/linux_ip_set.h>		/* enum ipset_cmd */

struct ipset_handle;

struct ipset_transport {
	struct ipset_handle * (*init)(mnl_cb_t *cb_ctl, void *data);
	int (*fini)(struct ipset_handle *handle);
	void (*fill_hdr)(struct ipset_handle *handle, enum ipset_cmd cmd,
			 void *buffer, size_t len, uint8_t envflags);
	int (*query)(struct ipset_handle *handle, void *buffer, size_t len);
};

#endif /* LIBIPSET_TRANSPORT_H */

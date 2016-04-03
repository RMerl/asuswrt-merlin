/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_MNL_H
#define LIBIPSET_MNL_H

#include <stdint.h>				/* uintxx_t */
#include <libmnl/libmnl.h>			/* libmnl backend */

#include <libipset/transport.h>			/* struct ipset_transport */

#ifndef NFNETLINK_V0
#define NFNETLINK_V0		0

struct nfgenmsg {
	uint8_t nfgen_family;
	uint8_t version;
	uint16_t res_id;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int ipset_get_nlmsg_type(const struct nlmsghdr *nlh);
extern const struct ipset_transport ipset_mnl_transport;

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_MNL_H */

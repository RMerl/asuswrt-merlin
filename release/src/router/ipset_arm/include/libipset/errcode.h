/* Copyright 2007-2008 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_ERRCODE_H
#define LIBIPSET_ERRCODE_H

#include <libipset/linux_ip_set.h>		/* enum ipset_cmd */

struct ipset_session;

/* Kernel error code to message table */
struct ipset_errcode_table {
	int errcode;		/* error code returned by the kernel */
	enum ipset_cmd cmd;	/* issued command */
	const char *message;	/* error message the code translated to */
};

#ifdef __cplusplus
extern "C" {
#endif

extern int ipset_errcode(struct ipset_session *session, enum ipset_cmd cmd,
			 int errcode);

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_ERRCODE_H */

/*
 * Copyright (C) 2001 - 2003 Sistina Software
 * Copyright (C) 2004 - 2008 Red Hat, Inc. All rights reserved.
 *
 * kcopyd provides a simple interface for copying an area of one
 * block-device to one or more other block-devices, either synchronous
 * or with an asynchronous completion notification.
 *
 * This file is released under the GPL.
 */

#ifndef _LINUX_DM_KCOPYD_H
#define _LINUX_DM_KCOPYD_H

#ifdef __KERNEL__

#include <linux/dm-io.h>

#define DM_KCOPYD_MAX_REGIONS 8

#define DM_KCOPYD_IGNORE_ERROR 1

/*
 * To use kcopyd you must first create a dm_kcopyd_client object.
 */
struct dm_kcopyd_client;
int dm_kcopyd_client_create(unsigned num_pages,
			    struct dm_kcopyd_client **result);
void dm_kcopyd_client_destroy(struct dm_kcopyd_client *kc);

/*
 * Submit a copy job to kcopyd.  This is built on top of the
 * previous three fns.
 *
 * read_err is a boolean,
 * write_err is a bitset, with 1 bit for each destination region
 */
typedef void (*dm_kcopyd_notify_fn)(int read_err, unsigned long write_err,
				    void *context);

int dm_kcopyd_copy(struct dm_kcopyd_client *kc, struct dm_io_region *from,
		   unsigned num_dests, struct dm_io_region *dests,
		   unsigned flags, dm_kcopyd_notify_fn fn, void *context);

#endif	/* __KERNEL__ */
#endif	/* _LINUX_DM_KCOPYD_H */

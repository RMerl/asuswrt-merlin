/*
 * ioctl based topology -- gathers topology information
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "topology.h"

/*
 * ioctl topology values
 */
static struct topology_val {

	long  ioc;

	/* functions to set probing result */
	int (*set_ulong)(blkid_probe, unsigned long);
	int (*set_int)(blkid_probe, int);

} topology_vals[] = {
	{ BLKALIGNOFF, NULL, blkid_topology_set_alignment_offset },
	{ BLKIOMIN, blkid_topology_set_minimum_io_size },
	{ BLKIOOPT, blkid_topology_set_optimal_io_size },
	{ BLKPBSZGET, blkid_topology_set_physical_sector_size }
	/* we read BLKSSZGET in topology.c */
};

static int probe_ioctl_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(topology_vals); i++) {
		struct topology_val *val = &topology_vals[i];
		int rc = 1;
		unsigned int data;

		if (ioctl(pr->fd, val->ioc, &data) == -1)
			goto nothing;

		if (val->set_int)
			rc = val->set_int(pr, (int) data);
		else
			rc = val->set_ulong(pr, (unsigned long) data);
		if (rc)
			goto err;
	}

	return 0;
nothing:
	return 1;
err:
	return -1;
}

const struct blkid_idinfo ioctl_tp_idinfo =
{
	.name		= "ioctl",
	.probefunc	= probe_ioctl_tp,
	.magics		= BLKID_NONE_MAGIC
};


/*
 * sysfs based topology -- gathers topology information from Linux sysfs
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * For more information see Linux kernel Documentation/ABI/testing/sysfs-block.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "sysfs.h"
#include "topology.h"

/*
 * Sysfs topology values (since 2.6.31, May 2009).
 */
static struct topology_val {

	/* /sys/dev/block/<maj>:<min>/<ATTR> */
	const char *attr;

	/* functions to set probing resut */
	int (*set_ulong)(blkid_probe, unsigned long);
	int (*set_int)(blkid_probe, int);

} topology_vals[] = {
	{ "alignment_offset", NULL, blkid_topology_set_alignment_offset },
	{ "queue/minimum_io_size", blkid_topology_set_minimum_io_size },
	{ "queue/optimal_io_size", blkid_topology_set_optimal_io_size },
	{ "queue/physical_block_size", blkid_topology_set_physical_sector_size },
};

static int probe_sysfs_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	dev_t dev, disk = 0;
	int rc;
	struct sysfs_cxt sysfs, parent;
	size_t i, count = 0;

	dev = blkid_probe_get_devno(pr);
	if (!dev || sysfs_init(&sysfs, dev, NULL) != 0)
		return 1;

	rc = 1;		/* nothing (default) */

	for (i = 0; i < ARRAY_SIZE(topology_vals); i++) {
		struct topology_val *val = &topology_vals[i];
		int ok = sysfs_has_attribute(&sysfs, val->attr);

		rc = 1;	/* nothing */

		if (!ok) {
			if (!disk) {
				/*
				 * Read atrributes from "disk" if the current
				 * device is a partition.
				 */
				disk = blkid_probe_get_wholedisk_devno(pr);
				if (disk && disk != dev) {
					if (sysfs_init(&parent, disk, NULL) != 0)
						goto done;

					sysfs.parent = &parent;
					ok = sysfs_has_attribute(&sysfs,
								 val->attr);
				}
			}
			if (!ok)
				continue;	/* attrinute does not exist */
		}

		if (val->set_ulong) {
			uint64_t data;

			if (sysfs_read_u64(&sysfs, val->attr, &data) != 0)
				continue;
			rc = val->set_ulong(pr, (unsigned long) data);

		} else if (val->set_int) {
			int64_t data;

			if (sysfs_read_s64(&sysfs, val->attr, &data) != 0)
				continue;
			rc = val->set_int(pr, (int) data);
		}

		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;
	}

done:
	sysfs_deinit(&sysfs);
	if (disk)
		sysfs_deinit(&parent);
	if (count)
		return 0;		/* success */
	return rc;			/* error or nothing */
}

const struct blkid_idinfo sysfs_tp_idinfo =
{
	.name		= "sysfs",
	.probefunc	= probe_sysfs_tp,
	.magics		= BLKID_NONE_MAGIC
};


/*
 * Linux Software RAID (md) topology
 * -- this is fallback for old systems where the topology information is not
 *    exported by sysfs
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "topology.h"

#ifndef MD_MAJOR
#define MD_MAJOR	9
#endif

#ifndef _IOT__IOTBASE_uint32_t
#define _IOT__IOTBASE_uint32_t IOT_SIMPLE(uint32_t)
#endif
#define _IOT_md_array_info _IOT (_IOTS(uint32_t), 18, 0, 0, 0, 0)
#define GET_ARRAY_INFO          _IOR (MD_MAJOR, 0x11, struct md_array_info)

struct md_array_info {
	/*
	 * Generic constant information
	 */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t patch_version;
	uint32_t ctime;
	uint32_t level;
	uint32_t size;
	uint32_t nr_disks;
	uint32_t raid_disks;
	uint32_t md_minor;
	uint32_t not_persistent;

	/*
	 * Generic state information
	 */
	uint32_t utime;	  /*  0 Superblock update time		  */
	uint32_t state;	  /*  1 State bits (clean, ...)		  */
	uint32_t active_disks;  /*  2 Number of currently active disks  */
	uint32_t working_disks; /*  3 Number of working disks		  */
	uint32_t failed_disks;  /*  4 Number of failed disks		  */
	uint32_t spare_disks;	  /*  5 Number of spare disks		  */

	/*
	 * Personality information
	 */
	uint32_t layout;	  /*  0 the array's physical layout	  */
	uint32_t chunk_size;	  /*  1 chunk size in bytes		  */

};

static int is_md_device(dev_t devno)
{
	if (major(devno) == MD_MAJOR)
		return 1;
	return blkid_driver_has_major("md", major(devno));
}

static int probe_md_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	int fd = -1;
	dev_t disk = 0;
	dev_t devno = blkid_probe_get_devno(pr);
	struct md_array_info md;

	if (!devno)
		goto nothing;		/* probably not a block device */

	if (!is_md_device(devno))
		goto nothing;

	if (blkid_devno_to_wholedisk(devno, NULL, 0, &disk))
		goto nothing;

	if (disk == devno)
		fd = pr->fd;
	else {
		char *diskpath = blkid_devno_to_devname(disk);

		if (!diskpath)
			goto nothing;

		fd = open(diskpath, O_RDONLY);
		free(diskpath);

                if (fd == -1)
			goto nothing;
	}

	memset(&md, 0, sizeof(md));

	if (ioctl(fd, GET_ARRAY_INFO, &md))
		goto nothing;

	if (fd >= 0 && fd != pr->fd) {
		close(fd);
		fd = -1;
	}

	/*
	 * Ignore levels we don't want aligned (e.g. linear)
	 * and deduct disk(s) from stripe width on RAID4/5/6
	 */
	switch (md.level) {
	case 6:
		md.raid_disks--;
		/* fallthrough */
	case 5:
	case 4:
		md.raid_disks--;
		/* fallthrough */
	case 1:
	case 0:
	case 10:
		break;
	default:
		goto nothing;
	}

	blkid_topology_set_minimum_io_size(pr, md.chunk_size);
	blkid_topology_set_optimal_io_size(pr, md.chunk_size * md.raid_disks);

	return 0;

nothing:
	if (fd >= 0 && fd != pr->fd)
		close(fd);
	return 1;
}

const struct blkid_idinfo md_tp_idinfo =
{
	.name		= "md",
	.probefunc	= probe_md_tp,
	.magics		= BLKID_NONE_MAGIC
};


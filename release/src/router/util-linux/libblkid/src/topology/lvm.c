/*
 * lvm topology
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "topology.h"

#ifndef LVM_BLK_MAJOR
# define LVM_BLK_MAJOR     58
#endif

static int is_lvm_device(dev_t devno)
{
	if (major(devno) == LVM_BLK_MAJOR)
		return 1;
	return blkid_driver_has_major("lvm", major(devno));
}

static int probe_lvm_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	const char *paths[] = {
		"/usr/local/sbin/lvdisplay",
		"/usr/sbin/lvdisplay",
		"/sbin/lvdisplay"
	};
	int lvpipe[] = { -1, -1 }, stripes = 0, stripesize = 0;
	FILE *stream = NULL;
	char *cmd = NULL, *devname = NULL, buf[1024];
	size_t i;
	dev_t devno = blkid_probe_get_devno(pr);

	if (!devno)
		goto nothing;		/* probably not a block device */
	if (!is_lvm_device(devno))
		goto nothing;

	for (i = 0; i < ARRAY_SIZE(paths); i++) {
		struct stat sb;
		if (stat(paths[i], &sb) == 0) {
			cmd = (char *) paths[i];
			break;
		}
	}

	if (!cmd)
		goto nothing;

	devname = blkid_devno_to_devname(devno);
	if (!devname)
		goto nothing;

	if (pipe(lvpipe) < 0) {
		DBG(DEBUG_LOWPROBE,
			printf("Failed to open pipe: errno=%d", errno));
		goto nothing;
	}

	switch (fork()) {
	case 0:
	{
		char *lvargv[3];

		/* Plumbing */
		close(lvpipe[0]);

		if (lvpipe[1] != STDOUT_FILENO)
			dup2(lvpipe[1], STDOUT_FILENO);

		/* The libblkid library could linked with setuid programs */
		if (setgid(getgid()) < 0)
			 exit(1);
		if (setuid(getuid()) < 0)
			 exit(1);

		lvargv[0] = cmd;
		lvargv[1] = devname;
		lvargv[2] = NULL;

		execv(lvargv[0], lvargv);

		DBG(DEBUG_LOWPROBE,
			printf("Failed to execute %s: errno=%d", cmd, errno));
		exit(1);
	}
	case -1:
		DBG(DEBUG_LOWPROBE,
			printf("Failed to forking: errno=%d", errno));
		goto nothing;
	default:
		break;
	}

	stream = fdopen(lvpipe[0], "r");
	if (!stream)
		goto nothing;

	while (fgets(buf, sizeof(buf), stream) != NULL) {
		if (!strncmp(buf, "Stripes", 7))
			sscanf(buf, "Stripes %d", &stripes);

		if (!strncmp(buf, "Stripe size", 11))
			sscanf(buf, "Stripe size (KByte) %d", &stripesize);
	}

	if (!stripes)
		goto nothing;

	blkid_topology_set_minimum_io_size(pr, stripesize << 10);
	blkid_topology_set_optimal_io_size(pr, (stripes * stripesize) << 10);

	free(devname);
	fclose(stream);
	close(lvpipe[1]);
	return 0;

nothing:
	free(devname);
	if (stream)
		fclose(stream);
	else if (lvpipe[0] != -1)
		close(lvpipe[0]);
	if (lvpipe[1] != -1)
		close(lvpipe[1]);
	return 1;
}

const struct blkid_idinfo lvm_tp_idinfo =
{
	.name		= "lvm",
	.probefunc	= probe_lvm_tp,
	.magics		= BLKID_NONE_MAGIC
};


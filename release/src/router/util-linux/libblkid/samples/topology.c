/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <blkid.h>

#include "c.h"

int main(int argc, char *argv[])
{
	int rc;
	char *devname;
	blkid_probe pr;
	blkid_topology tp;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <device>  "
				"-- prints topology details about the device\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	devname = argv[1];
	pr = blkid_new_probe_from_filename(devname);
	if (!pr)
		err(EXIT_FAILURE, "%s: faild to create a new libblkid probe",
				devname);
	/*
	 * Binary interface
	 */
	tp = blkid_probe_get_topology(pr);
	if (tp) {
		printf("----- binary interface:\n");
		printf("\talignment offset     : %lu\n",
				blkid_topology_get_alignment_offset(tp));
		printf("\tminimum io size      : %lu\n",
				blkid_topology_get_minimum_io_size(tp));
		printf("\toptimal io size      : %lu\n",
				blkid_topology_get_optimal_io_size(tp));
		printf("\tlogical sector size  : %lu\n",
				blkid_topology_get_logical_sector_size(tp));
		printf("\tphysical sector size : %lu\n",
				blkid_topology_get_physical_sector_size(tp));
	}

	/*
	 * NAME=value interface
	 */

	/* enable topology probing */
	blkid_probe_enable_topology(pr, TRUE);

	/* disable superblocks probing (enabled by default) */
	blkid_probe_enable_superblocks(pr, FALSE);

	rc = blkid_do_fullprobe(pr);
	if (rc == -1)
		errx(EXIT_FAILURE, "%s: blkid_do_fullprobe() failed", devname);
	else if (rc == 1)
		warnx("%s: missing topology information", devname);
	else {
		int i, nvals = blkid_probe_numof_values(pr);

		printf("----- NAME=value interface (values: %d):\n", nvals);

		for (i = 0; i < nvals; i++) {
			const char *name, *data;

			blkid_probe_get_value(pr, i, &name, &data, NULL);
			printf("\t%s = %s\n", name, data);
		}
	}

	blkid_free_probe(pr);
	return EXIT_SUCCESS;
}

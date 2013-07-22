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

	if (argc < 2) {
		fprintf(stderr, "usage: %s <device>  "
				"-- prints superblocks details about the device\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	devname = argv[1];
	pr = blkid_new_probe_from_filename(devname);
	if (!pr)
		err(EXIT_FAILURE, "%s: faild to create a new libblkid probe",
				devname);

	/* enable topology probing */
	blkid_probe_enable_superblocks(pr, TRUE);

	/* set all flags */
	blkid_probe_set_superblocks_flags(pr,
			BLKID_SUBLKS_LABEL | BLKID_SUBLKS_LABELRAW |
			BLKID_SUBLKS_UUID | BLKID_SUBLKS_UUIDRAW |
			BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE |
			BLKID_SUBLKS_USAGE | BLKID_SUBLKS_VERSION |
			BLKID_SUBLKS_MAGIC);

	rc = blkid_do_safeprobe(pr);
	if (rc == -1)
		errx(EXIT_FAILURE, "%s: blkid_do_safeprobe() failed", devname);
	else if (rc == 1)
		warnx("%s: cannot gather information about superblocks", devname);
	else {
		int i, nvals = blkid_probe_numof_values(pr);

		for (i = 0; i < nvals; i++) {
			const char *name, *data;

			blkid_probe_get_value(pr, i, &name, &data, NULL);
			printf("\t%s = %s\n", name, data);
		}
	}

	blkid_free_probe(pr);
	return EXIT_SUCCESS;
}

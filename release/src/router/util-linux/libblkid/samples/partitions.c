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
	int i, nparts;
	char *devname;
	blkid_probe pr;
	blkid_partlist ls;
	blkid_parttable root_tab;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <device|file>  "
				"-- prints partitions\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	devname = argv[1];
	pr = blkid_new_probe_from_filename(devname);
	if (!pr)
		err(EXIT_FAILURE, "%s: faild to create a new libblkid probe",
				devname);
	/* Binary interface */
	ls = blkid_probe_get_partitions(pr);
	if (!ls)
		errx(EXIT_FAILURE, "%s: failed to read partitions\n", devname);

	/*
	 * Print info about the primary (root) partition table
	 */
	root_tab = blkid_partlist_get_table(ls);
	if (!root_tab)
		errx(EXIT_FAILURE, "%s: does not contains any "
				 "known partition table\n", devname);

	printf("size: %jd, sector size: %u, PT: %s, offset: %jd\n---\n",
		blkid_probe_get_size(pr),
		blkid_probe_get_sectorsize(pr),
		blkid_parttable_get_type(root_tab),
		blkid_parttable_get_offset(root_tab));

	/*
	 * List partitions
	 */
	nparts = blkid_partlist_numof_partitions(ls);
	if (!nparts)
		goto done;

	for (i = 0; i < nparts; i++) {
		const char *p;
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		blkid_parttable tab = blkid_partition_get_table(par);

		printf("#%d: %10llu %10llu  0x%x",
			blkid_partition_get_partno(par),
			(unsigned long long) blkid_partition_get_start(par),
			(unsigned long long) blkid_partition_get_size(par),
			blkid_partition_get_type(par));

		if (root_tab != tab)
			/* subpartition (BSD, Minix, ...) */
			printf(" (%s)", blkid_parttable_get_type(tab));

		p = blkid_partition_get_name(par);
		if (p)
			printf(" name='%s'", p);
		p = blkid_partition_get_uuid(par);
		if (p)
			printf(" uuid='%s'", p);
		p = blkid_partition_get_type_string(par);
		if (p)
			printf(" type='%s'", p);

		putc('\n', stdout);
	}

done:
	blkid_free_probe(pr);
	return EXIT_SUCCESS;
}

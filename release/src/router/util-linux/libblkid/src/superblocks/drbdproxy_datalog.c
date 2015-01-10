/*
 * Copyright (C) 2011 by Philipp Marek <philipp.marek@linbit.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

#include "superblocks.h"


struct log_header_t {
	uint64_t magic;
	uint64_t version;

	unsigned char uuid[16];

	uint64_t flags;
} __attribute__((packed));


static int probe_drbdproxy_datalog(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct log_header_t *lh;

	lh = (struct log_header_t *) blkid_probe_get_buffer(pr, 0, sizeof(*lh));
	if (!lh)
		return -1;

	blkid_probe_set_uuid(pr, lh->uuid);
	blkid_probe_sprintf_version(pr, "v%jd", le64_to_cpu(lh->version));

	return 0;
}

const struct blkid_idinfo drbdproxy_datalog_idinfo =
{
	.name		= "drbdproxy_datalog",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_drbdproxy_datalog,
	.minsz		= 16*1024,
	.magics		=
	{
		{ .magic = "DRBDdlh*", .len = 8, .sboff = 0, .kboff = 0 },
		{ NULL }
	}
};

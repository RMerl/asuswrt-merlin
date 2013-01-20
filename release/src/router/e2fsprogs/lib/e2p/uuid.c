/*
 * uuid.c -- utility routines for manipulating UUID's.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <ext2fs/ext2_types.h>

#include "e2p.h"

struct uuid {
	__u32	time_low;
	__u16	time_mid;
	__u16	time_hi_and_version;
	__u16	clock_seq;
	__u8	node[6];
};

/* Returns 1 if the uuid is the NULL uuid */
int e2p_is_null_uuid(void *uu)
{
	__u8 	*cp;
	int	i;

	for (i=0, cp = uu; i < 16; i++)
		if (*cp++)
			return 0;
	return 1;
}

static void e2p_unpack_uuid(void *in, struct uuid *uu)
{
	__u8	*ptr = in;
	__u32	tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_low = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_mid = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_hi_and_version = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->clock_seq = tmp;

	memcpy(uu->node, ptr, 6);
}

void e2p_uuid_to_str(void *uu, char *out)
{
	struct uuid uuid;

	e2p_unpack_uuid(uu, &uuid);
	sprintf(out,
		"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
		uuid.clock_seq >> 8, uuid.clock_seq & 0xFF,
		uuid.node[0], uuid.node[1], uuid.node[2],
		uuid.node[3], uuid.node[4], uuid.node[5]);
}

const char *e2p_uuid2str(void *uu)
{
	static char buf[80];

	if (e2p_is_null_uuid(uu))
		return "<none>";
	e2p_uuid_to_str(uu, buf);
	return buf;
}


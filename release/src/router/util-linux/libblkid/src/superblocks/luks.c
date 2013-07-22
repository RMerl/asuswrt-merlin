/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
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
#include <stdint.h>

#include "superblocks.h"

#define LUKS_CIPHERNAME_L		32
#define LUKS_CIPHERMODE_L		32
#define LUKS_HASHSPEC_L			32
#define LUKS_DIGESTSIZE			20
#define LUKS_SALTSIZE			32
#define LUKS_MAGIC_L			6
#define UUID_STRING_L			40

struct luks_phdr {
	uint8_t		magic[LUKS_MAGIC_L];
	uint16_t	version;
	uint8_t		cipherName[LUKS_CIPHERNAME_L];
	uint8_t		cipherMode[LUKS_CIPHERMODE_L];
	uint8_t		hashSpec[LUKS_HASHSPEC_L];
	uint32_t	payloadOffset;
	uint32_t	keyBytes;
	uint8_t		mkDigest[LUKS_DIGESTSIZE];
	uint8_t		mkDigestSalt[LUKS_SALTSIZE];
	uint32_t	mkDigestIterations;
	uint8_t		uuid[UUID_STRING_L];
} __attribute__((packed));

static int probe_luks(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct luks_phdr *header;

	header = blkid_probe_get_sb(pr, mag, struct luks_phdr);
	if (header == NULL)
		return -1;

	blkid_probe_strncpy_uuid(pr, (unsigned char *) header->uuid,
			sizeof(header->uuid));
	blkid_probe_sprintf_version(pr, "%u", be16_to_cpu(header->version));
	return 0;
}

const struct blkid_idinfo luks_idinfo =
{
	.name		= "crypto_LUKS",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_luks,
	.magics		=
	{
		{ .magic = "LUKS\xba\xbe", .len = 6 },
		{ NULL }
	}
};

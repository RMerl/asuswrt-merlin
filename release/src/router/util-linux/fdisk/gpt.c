/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 *
 * GPT (GUID Partition Table) signature detection. Based on libparted and
 * util-linux/partx.
 *
 * Warning: this code doesn't do all GPT checks (CRC32, Protective MBR, ..).
 *          It's really GPT signature detection only.
 *
 * Copyright (C) 2007 Karel Zak <kzak@redhat.com>
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gpt.h"
#include "blkdev.h"
#include "bitops.h"

#define GPT_HEADER_SIGNATURE 0x5452415020494645LL
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1

typedef struct {
        uint32_t time_low;
        uint16_t time_mid;
        uint16_t time_hi_and_version;
        uint8_t  clock_seq_hi_and_reserved;
        uint8_t  clock_seq_low;
        uint8_t  node[6];
} /* __attribute__ ((packed)) */ efi_guid_t;
/* commented out "__attribute__ ((packed))" to work around gcc bug (fixed
 * in gcc3.1): __attribute__ ((packed)) breaks addressing on initialized
 * data.  It turns out we don't need it in this case, so it doesn't break
 * anything :)
 */

typedef struct _GuidPartitionTableHeader_t {
	uint64_t Signature;
	uint32_t Revision;
	uint32_t HeaderSize;
	uint32_t HeaderCRC32;
	uint32_t Reserved1;
	uint64_t MyLBA;
	uint64_t AlternateLBA;
	uint64_t FirstUsableLBA;
	uint64_t LastUsableLBA;
	efi_guid_t DiskGUID;
	uint64_t PartitionEntryLBA;
	uint32_t NumberOfPartitionEntries;
	uint32_t SizeOfPartitionEntry;
	uint32_t PartitionEntryArrayCRC32;
	uint8_t Reserved2[512 - 92];
} __attribute__ ((packed)) GuidPartitionTableHeader_t;

static int
_get_sector_size (int fd)
{
	int sector_size;

	if (blkdev_get_sector_size(fd, &sector_size) == -1)
		return DEFAULT_SECTOR_SIZE;
	return sector_size;
}

static uint64_t
_get_num_sectors(int fd)
{
	unsigned long long bytes=0;

	if (blkdev_get_size(fd, &bytes) == -1)
		return 0;
	return bytes / _get_sector_size(fd);
}

static uint64_t
last_lba(int fd)
{
	int rc;
	uint64_t sectors = 0;
	struct stat s;

	memset(&s, 0, sizeof (s));
	rc = fstat(fd, &s);
	if (rc == -1)
	{
		fprintf(stderr, "last_lba() could not stat: %s\n",
			strerror(errno));
		return 0;
	}
	if (S_ISBLK(s.st_mode))
		sectors = _get_num_sectors(fd);
	else if (S_ISREG(s.st_mode))
		sectors = s.st_size >> _get_sector_size(fd);
	else
	{
		fprintf(stderr,
			"last_lba(): I don't know how to handle files with mode %o\n",
			s.st_mode);
		sectors = 1;
	}
	return sectors - 1;
}

static ssize_t
read_lba(int fd, uint64_t lba, void *buffer, size_t bytes)
{
	int sector_size = _get_sector_size(fd);
	off_t offset = lba * sector_size;

	lseek(fd, offset, SEEK_SET);
	return read(fd, buffer, bytes);
}

static GuidPartitionTableHeader_t *
alloc_read_gpt_header(int fd, uint64_t lba)
{
	GuidPartitionTableHeader_t *gpt =
		(GuidPartitionTableHeader_t *) malloc(sizeof (GuidPartitionTableHeader_t));
	if (!gpt)
		return NULL;
	memset(gpt, 0, sizeof (*gpt));
	if (!read_lba(fd, lba, gpt, sizeof (GuidPartitionTableHeader_t)))
	{
		free(gpt);
		return NULL;
	}
	return gpt;
}

static int
gpt_check_signature(int fd, uint64_t lba)
{
	GuidPartitionTableHeader_t *gpt;
	int res=0;

	if ((gpt = alloc_read_gpt_header(fd, lba)))
	{
		if (gpt->Signature == cpu_to_le64(GPT_HEADER_SIGNATURE))
			res = 1;
		free(gpt);
	}
	return res;
}

/* returns:
 *	0 not found GPT
 *	1 for valid primary GPT header
 *	2 for valid alternative GPT header
 */
int
gpt_probe_signature_fd(int fd)
{
	int res = 0;

	/* check primary GPT header */
	if (gpt_check_signature(fd, GPT_PRIMARY_PARTITION_TABLE_LBA))
		res = 1;
	else
	{
		/* check alternative GPT header */
		uint64_t lastlba = last_lba(fd);
		if (gpt_check_signature(fd, lastlba))
			res = 2;
	}
	return res;
}

int
gpt_probe_signature_devname(char *devname)
{
	int res, fd;
	if ((fd = open(devname, O_RDONLY)) < 0)
		return 0;
	res = gpt_probe_signature_fd(fd);
	close(fd);
	return res;
}

#ifdef GPT_TEST_MAIN
int
main(int argc, char **argv)
{
	if (argc!=2)
	{
		fprintf(stderr, "usage: %s <dev>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (gpt_probe_signature_devname(argv[1]))
		printf("GPT (GUID Partition Table) detected on %s\n", argv[1]);
	exit(EXIT_SUCCESS);
}
#endif

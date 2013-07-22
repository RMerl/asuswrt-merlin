/*
 * docfdisk.c: Modify INFTL partition tables
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define PROGRAM_NAME "docfdisk"

#define _XOPEN_SOURCE 500 /* for pread/pwrite */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>
#include <mtd/inftl-user.h>
#include <mtd_swab.h>

unsigned char *buf;

mtd_info_t meminfo;
erase_info_t erase;
int fd;
struct INFTLMediaHeader *mh;

#define MAXSCAN 10

void show_header(int mhoffs) {
	int i, unitsize, numunits, bmbits, numpart;
	int start, end, num, nextunit;
	unsigned int flags;
	struct INFTLPartition *ip;

	bmbits = le32_to_cpu(mh->BlockMultiplierBits);
	printf("  bootRecordID          = %s\n"
			"  NoOfBootImageBlocks   = %d\n"
			"  NoOfBinaryPartitions  = %d\n"
			"  NoOfBDTLPartitions    = %d\n"
			"  BlockMultiplierBits   = %d\n"
			"  FormatFlags           = %d\n"
			"  OsakVersion           = %d.%d.%d.%d\n"
			"  PercentUsed           = %d\n",
			mh->bootRecordID, le32_to_cpu(mh->NoOfBootImageBlocks),
			le32_to_cpu(mh->NoOfBinaryPartitions),
			le32_to_cpu(mh->NoOfBDTLPartitions),
			bmbits,
			le32_to_cpu(mh->FormatFlags),
			((unsigned char *) &mh->OsakVersion)[0] & 0xf,
			((unsigned char *) &mh->OsakVersion)[1] & 0xf,
			((unsigned char *) &mh->OsakVersion)[2] & 0xf,
			((unsigned char *) &mh->OsakVersion)[3] & 0xf,
			le32_to_cpu(mh->PercentUsed));

	numpart = le32_to_cpu(mh->NoOfBinaryPartitions) +
		le32_to_cpu(mh->NoOfBDTLPartitions);
	unitsize = meminfo.erasesize >> bmbits;
	numunits = meminfo.size / unitsize;
	nextunit = mhoffs / unitsize;
	nextunit++;
	printf("Unitsize is %d bytes.  Device has %d units.\n",
			unitsize, numunits);
	if (numunits > 32768) {
		printf("WARNING: More than 32768 units! Unexpectedly small BlockMultiplierBits.\n");
	}
	if (bmbits && (numunits <= 16384)) {
		printf("NOTICE: Unexpectedly large BlockMultiplierBits.\n");
	}
	for (i = 0; i < 4; i++) {
		ip = &(mh->Partitions[i]);
		flags = le32_to_cpu(ip->flags);
		start = le32_to_cpu(ip->firstUnit);
		end = le32_to_cpu(ip->lastUnit);
		num = le32_to_cpu(ip->virtualUnits);
		if (start < nextunit) {
			printf("ERROR: Overlapping or misordered partitions!\n");
		}
		if (start > nextunit) {
			printf("  Unpartitioned space: %d bytes\n"
					"    virtualUnits  = %d\n"
					"    firstUnit     = %d\n"
					"    lastUnit      = %d\n",
					(start - nextunit) * unitsize, start - nextunit,
					nextunit, start - 1);
		}
		if (flags & INFTL_BINARY)
			printf("  Partition %d   (BDK):", i+1);
		else
			printf("  Partition %d  (BDTL):", i+1);
		printf(" %d bytes\n"
				"    virtualUnits  = %d\n"
				"    firstUnit     = %d\n"
				"    lastUnit      = %d\n"
				"    flags         = 0x%x\n"
				"    spareUnits    = %d\n",
				num * unitsize, num, start, end,
				le32_to_cpu(ip->flags), le32_to_cpu(ip->spareUnits));
		if (num > (1 + end - start)) {
			printf("ERROR: virtualUnits not consistent with first/lastUnit!\n");
		}
		end++;
		if (end > nextunit)
			nextunit = end;
		if (flags & INFTL_LAST)
			break;
	}
	if (i >= 4) {
		printf("Odd.  Last partition was not marked with INFTL_LAST.\n");
		i--;
	}
	if ((i+1) != numpart) {
		printf("ERROR: Number of partitions != (NoOfBinaryPartitions + NoOfBDTLPartitions)\n");
	}
	if (nextunit > numunits) {
		printf("ERROR: Partitions appear to extend beyond end of device!\n");
	}
	if (nextunit < numunits) {
		printf("  Unpartitioned space: %d bytes\n"
				"    virtualUnits  = %d\n"
				"    firstUnit     = %d\n"
				"    lastUnit      = %d\n",
				(numunits - nextunit) * unitsize, numunits - nextunit,
				nextunit, numunits - 1);
	}
}


int main(int argc, char **argv)
{
	int ret, i, mhblock, unitsize, block;
	unsigned int nblocks[4], npart;
	unsigned int totblocks;
	struct INFTLPartition *ip;
	unsigned char *oobbuf;
	struct mtd_oob_buf oob;
	char line[20];
	int mhoffs;
	struct INFTLMediaHeader *mh2;

	if (argc < 2) {
		printf(
				"Usage: %s <mtddevice> [<size1> [<size2> [<size3> [<size4]]]]\n"
				"  Sizes are in device units (run with no sizes to show unitsize and current\n"
				"  partitions).  Last size = 0 means go to end of device.\n",
				PROGRAM_NAME);
		return 1;
	}

	npart = argc - 2;
	if (npart > 4) {
		printf("Max 4 partitions allowed.\n");
		return 1;
	}

	for (i = 0; i < npart; i++) {
		nblocks[i] = strtoul(argv[2+i], NULL, 0);
		if (i && !nblocks[i-1]) {
			printf("No sizes allowed after 0\n");
			return 1;
		}
	}

	// Open and size the device
	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("Open flash device");
		return 1;
	}

	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("ioctl(MEMGETINFO)");
		return 1;
	}

	printf("Device size is %d bytes.  Erasesize is %d bytes.\n",
			meminfo.size, meminfo.erasesize);

	buf = malloc(meminfo.erasesize);
	oobbuf = malloc((meminfo.erasesize / meminfo.writesize) * meminfo.oobsize);
	if (!buf || !oobbuf) {
		printf("Can't malloc block buffer\n");
		return 1;
	}
	oob.length = meminfo.oobsize;

	mh = (struct INFTLMediaHeader *) buf;

	for (mhblock = 0; mhblock < MAXSCAN; mhblock++) {
		if ((ret = pread(fd, buf, meminfo.erasesize, mhblock * meminfo.erasesize)) < 0) {
			if (errno == EBADMSG) {
				printf("ECC error at eraseblock %d\n", mhblock);
				continue;
			}
			perror("Read eraseblock");
			return 1;
		}
		if (ret != meminfo.erasesize) {
			printf("Short read!\n");
			return 1;
		}
		if (!strcmp("BNAND", mh->bootRecordID)) break;
	}
	if (mhblock >= MAXSCAN) {
		printf("Unable to find INFTL Media Header\n");
		return 1;
	}
	printf("Found INFTL Media Header at block %d:\n", mhblock);
	mhoffs = mhblock * meminfo.erasesize;

	oob.ptr = oobbuf;
	oob.start = mhoffs;
	for (i = 0; i < meminfo.erasesize; i += meminfo.writesize) {
		if (ioctl(fd, MEMREADOOB, &oob)) {
			perror("ioctl(MEMREADOOB)");
			return 1;
		}
		oob.start += meminfo.writesize;
		oob.ptr += meminfo.oobsize;
	}

	show_header(mhoffs);

	if (!npart)
		return 0;

	printf("\n-------------------------------------------------------------------------\n");

	unitsize = meminfo.erasesize >> le32_to_cpu(mh->BlockMultiplierBits);
	totblocks = meminfo.size / unitsize;
	block = mhoffs / unitsize;
	block++;

	mh->NoOfBDTLPartitions = 0;
	mh->NoOfBinaryPartitions = npart;

	for (i = 0; i < npart; i++) {
		ip = &(mh->Partitions[i]);
		ip->firstUnit = cpu_to_le32(block);
		if (!nblocks[i])
			nblocks[i] = totblocks - block;
		ip->virtualUnits = cpu_to_le32(nblocks[i]);
		block += nblocks[i];
		ip->lastUnit = cpu_to_le32(block-1);
		ip->spareUnits = 0;
		ip->flags = cpu_to_le32(INFTL_BINARY);
	}
	if (block > totblocks) {
		printf("Requested partitions extend beyond end of device.\n");
		return 1;
	}
	ip->flags = cpu_to_le32(INFTL_BINARY | INFTL_LAST);

	/* update the spare as well */
	mh2 = (struct INFTLMediaHeader *) (buf + 4096);
	memcpy((void *) mh2, (void *) mh, sizeof(struct INFTLMediaHeader));

	printf("\nProposed new Media Header:\n");
	show_header(mhoffs);

	printf("\nReady to update device.  Type 'yes' to proceed, anything else to abort: ");
	fgets(line, sizeof(line), stdin);
	if (strcmp("yes\n", line))
		return 0;
	printf("Updating MediaHeader...\n");

	erase.start = mhoffs;
	erase.length = meminfo.erasesize;
	if (ioctl(fd, MEMERASE, &erase)) {
		perror("ioctl(MEMERASE)");
		printf("Your MediaHeader may be hosed.  UHOH!\n");
		return 1;
	}

	oob.ptr = oobbuf;
	oob.start = mhoffs;
	for (i = 0; i < meminfo.erasesize; i += meminfo.writesize) {
		memset(oob.ptr, 0xff, 6); // clear ECC.
		if (ioctl(fd, MEMWRITEOOB, &oob)) {
			perror("ioctl(MEMWRITEOOB)");
			printf("Your MediaHeader may be hosed.  UHOH!\n");
			return 1;
		}
		if ((ret = pwrite(fd, buf, meminfo.writesize, oob.start)) < 0) {
			perror("Write page");
			printf("Your MediaHeader may be hosed.  UHOH!\n");
			return 1;
		}
		if (ret != meminfo.writesize) {
			printf("Short write!\n");
			printf("Your MediaHeader may be hosed.  UHOH!\n");
			return 1;
		}

		oob.start += meminfo.writesize;
		oob.ptr += meminfo.oobsize;
		buf += meminfo.writesize;
	}

	printf("Success.  REBOOT or unload the diskonchip module to update partitions!\n");
	return 0;
}

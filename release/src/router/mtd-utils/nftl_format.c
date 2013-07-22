/*
 * nftl_format.c: Creating a NFTL/INFTL partition on an MTD device
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
 *
 * ToDo:
 *	1. UnitSizeFactor != 0xFF cases
 *	2. test, test, and test !!!
 */

#define PROGRAM_NAME "nftl_format"

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
#include <mtd/nftl-user.h>
#include <mtd/inftl-user.h>
#include <mtd_swab.h>

unsigned char BadUnitTable[MAX_ERASE_ZONES];
unsigned char *readbuf;
unsigned char *writebuf[4];

mtd_info_t meminfo;
erase_info_t erase;
int fd;
struct NFTLMediaHeader *NFTLhdr;
struct INFTLMediaHeader *INFTLhdr;

static int do_oobcheck = 1;
static int do_rwecheck = 1;

static unsigned char check_block_1(unsigned long block)
{
	unsigned char oobbuf[16];
	struct mtd_oob_buf oob = { 0, 16, oobbuf };

	oob.start = block * meminfo.erasesize;
	if (ioctl(fd, MEMREADOOB, &oob))
		return ZONE_BAD_ORIGINAL;

	if(oobbuf[5] == 0)
		return ZONE_BAD_ORIGINAL;

	oob.start = block * meminfo.erasesize + 512 /* FIXME */;
	if (ioctl(fd, MEMREADOOB, &oob))
		return ZONE_BAD_ORIGINAL;

	if(oobbuf[5] == 0)
		return ZONE_BAD_ORIGINAL;

	return ZONE_GOOD;
}

static unsigned char check_block_2(unsigned long block)
{
	unsigned long ofs = block * meminfo.erasesize;
	unsigned long blockofs;

	/* Erase test */
	erase.start = ofs;

	for (blockofs = 0; blockofs < meminfo.erasesize; blockofs += 512) {
		pread(fd, readbuf, 512, ofs + blockofs);
		if (memcmp(readbuf, writebuf[0], 512)) {
			/* Block wasn't 0xff after erase */
			printf(": Block not 0xff after erase\n");
			return ZONE_BAD_ORIGINAL;
		}

		pwrite(fd, writebuf[1], 512, blockofs + ofs);
		pread(fd, readbuf, 512, blockofs + ofs);
		if (memcmp(readbuf, writebuf[1], 512)) {
			printf(": Block not zero after clearing\n");
			return ZONE_BAD_ORIGINAL;
		}
	}

	/* Write test */
	if (ioctl(fd, MEMERASE, &erase) != 0) {
		printf(": Second erase failed (%s)\n", strerror(errno));
		return ZONE_BAD_ORIGINAL;
	}
	for (blockofs = 0; blockofs < meminfo.erasesize; blockofs += 512) {
		pwrite(fd, writebuf[2], 512, blockofs + ofs);
		pread(fd, readbuf, 512, blockofs + ofs);
		if (memcmp(readbuf, writebuf[2], 512)) {
			printf(": Block not 0x5a after writing\n");
			return ZONE_BAD_ORIGINAL;
		}
	}

	if (ioctl(fd, MEMERASE, &erase) != 0) {
		printf(": Third erase failed (%s)\n", strerror(errno));
		return ZONE_BAD_ORIGINAL;
	}
	for (blockofs = 0; blockofs < meminfo.erasesize; blockofs += 512) {
		pwrite(fd, writebuf[3], 512, blockofs + ofs);
		pread(fd, readbuf, 512, blockofs + ofs);
		if (memcmp(readbuf, writebuf[3], 512)) {
			printf(": Block not 0xa5 after writing\n");
			return ZONE_BAD_ORIGINAL;
		}
	}
	if (ioctl(fd, MEMERASE, &erase) != 0) {
		printf(": Fourth erase failed (%s)\n", strerror(errno));
		return ZONE_BAD_ORIGINAL;
	}
	return ZONE_GOOD;
}

static unsigned char erase_block(unsigned long block)
{
	unsigned char status;
	int ret;

	status = (do_oobcheck) ? check_block_1(block) : ZONE_GOOD;
	erase.start = block * meminfo.erasesize;

	if (status != ZONE_GOOD) {
		printf("\rSkipping bad zone (factory marked) #%ld @ 0x%x\n", block, erase.start);
		fflush(stdout);
		return status;
	}

	printf("\r\t Erasing Zone #%ld @ 0x%x", block, erase.start);
	fflush(stdout);

	if ((ret=ioctl(fd, MEMERASE, &erase)) != 0) {
		printf(": Erase failed (%s)\n", strerror(errno));
		return ZONE_BAD_ORIGINAL;
	}

	if (do_rwecheck) {
		printf("\r\tChecking Zone #%ld @ 0x%x", block, erase.start);
		fflush(stdout);
		status = check_block_2(block);
		if (status != ZONE_GOOD) {
			printf("\rSkipping bad zone (RWE test failed) #%ld @ 0x%x\n", block, erase.start);
			fflush(stdout);
		}
	}
	return status;
}

static int checkbbt(void)
{
	unsigned char bbt[512];
	unsigned char bits;
	int i, addr;

	if (pread(fd, bbt, 512, 0x800) < 0) {
		printf("%s: failed to read BBT, errno=%d\n", PROGRAM_NAME, errno);
		return (-1);
	}


	for (i = 0; (i < 512); i++) {
		addr = i / 4;
		bits = 0x3 << ((i % 4) * 2);
		if ((bbt[addr] & bits) == 0) {
			BadUnitTable[i] = ZONE_BAD_ORIGINAL;
		}
	}

	return (0);
}

void usage(int rc)
{
	fprintf(stderr, "Usage: %s [-ib] <mtddevice> [<start offset> [<size>]]\n", PROGRAM_NAME);
	exit(rc);
}

int main(int argc, char **argv)
{
	unsigned long startofs = 0, part_size = 0;
	unsigned long ezones = 0, ezone = 0, bad_zones = 0;
	unsigned char unit_factor = 0xFF;
	long MediaUnit1 = -1, MediaUnit2 = -1;
	long MediaUnitOff1 = 0, MediaUnitOff2 = 0;
	unsigned char oobbuf[16];
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	char *mtddevice;
	const char *nftl;
	int c, do_inftl = 0, do_bbt = 0;


	printf("version 1.24 2005/11/07 11:15:13 gleixner\n");

	if (argc < 2)
		usage(1);

	nftl = "NFTL";

	while ((c = getopt(argc, argv, "?hib")) > 0) {
		switch (c) {
			case 'i':
				nftl = "INFTL";
				do_inftl = 1;
				break;
			case 'b':
				do_bbt = 1;
				break;
			case 'h':
			case '?':
				usage(0);
				break;
			default:
				usage(1);
				break;
		}
	}

	mtddevice = argv[optind++];
	if (argc > optind) {
		startofs = strtoul(argv[optind++], NULL, 0);
	}
	if (argc > optind) {
		part_size = strtoul(argv[optind++], NULL, 0);
	}

	// Open and size the device
	if ((fd = open(mtddevice, O_RDWR)) < 0) {
		perror("Open flash device");
		return 1;
	}

	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("ioctl(MEMGETINFO)");
		close(fd);
		return 1;
	}

	switch (meminfo.erasesize) {
		case 0x1000:
		case 0x2000:
		case 0x4000:
		case 0x8000:
			break;
		default:
			printf("Unrecognized Erase size, 0x%x - I'm confused\n",
					meminfo.erasesize);
			close(fd);
			return 1;
	}
	writebuf[0] = malloc(meminfo.erasesize * 5);
	if (!writebuf[0]) {
		printf("Malloc failed\n");
		close(fd);
		return 1;
	}
	writebuf[1] = writebuf[0] + meminfo.erasesize;
	writebuf[2] = writebuf[1] + meminfo.erasesize;
	writebuf[3] = writebuf[2] + meminfo.erasesize;
	readbuf = writebuf[3] + meminfo.erasesize;
	memset(writebuf[0], 0xff, meminfo.erasesize);
	memset(writebuf[1], 0x00, meminfo.erasesize);
	memset(writebuf[2], 0x5a, meminfo.erasesize);
	memset(writebuf[3], 0xa5, meminfo.erasesize);
	memset(BadUnitTable, ZONE_GOOD, MAX_ERASE_ZONES);

	if (part_size == 0 || (part_size > meminfo.size - startofs))
		/* the user doest not or incorrectly specify NFTL partition size */
		part_size = meminfo.size - startofs;

	erase.length = meminfo.erasesize;
	ezones = part_size / meminfo.erasesize;

	if (ezones > MAX_ERASE_ZONES) {
		/* Ought to change the UnitSizeFactor. But later. */
		part_size = meminfo.erasesize * MAX_ERASE_ZONES;
		ezones = MAX_ERASE_ZONES;
		unit_factor = 0xFF;
	}

	/* If using device BBT then parse that now */
	if (do_bbt) {
		checkbbt();
		do_oobcheck = 0;
		do_rwecheck = 0;
	}

	/* Phase 1. Erasing and checking each erase zones in the NFTL partition.
	   N.B. Erase Zones not used by the NFTL partition are untouched and marked ZONE_GOOD */
	printf("Phase 1. Checking and erasing Erase Zones from 0x%08lx to 0x%08lx\n",
			startofs, startofs + part_size);
	for (ezone = startofs / meminfo.erasesize;
			ezone < (ezones + startofs / meminfo.erasesize); ezone++) {
		if (BadUnitTable[ezone] != ZONE_GOOD)
			continue;
		if ((BadUnitTable[ezone] = erase_block(ezone)) == ZONE_GOOD) {
			if (MediaUnit1 == -1) {
				MediaUnit1 = ezone;
			} else if (MediaUnit2 == -1) {
				MediaUnit2 = ezone;
			}
		} else {
			bad_zones++;
		}
	}
	printf("\n");

	/* N.B. from dump of M-System original chips, NumEraseUnits counts the 2 Erase Unit used
	   by MediaHeader and the FirstPhysicalEUN starts from the MediaHeader */
	if (do_inftl) {
		unsigned long maxzones, pezstart, pezend, numvunits;

		INFTLhdr = (struct INFTLMediaHeader *) (writebuf[0]);
		strcpy(INFTLhdr->bootRecordID, "BNAND");
		INFTLhdr->NoOfBootImageBlocks = cpu_to_le32(0);
		INFTLhdr->NoOfBinaryPartitions = cpu_to_le32(0);
		INFTLhdr->NoOfBDTLPartitions = cpu_to_le32(1);
		INFTLhdr->BlockMultiplierBits = cpu_to_le32(0);
		INFTLhdr->FormatFlags = cpu_to_le32(0);
		INFTLhdr->OsakVersion = cpu_to_le32(OSAK_VERSION);
		INFTLhdr->PercentUsed = cpu_to_le32(PERCENTUSED);
		/*
		 * Calculate number of virtual units we will have to work
		 * with. I am calculating out the known bad units here, not
		 * sure if that is what M-Systems do...
		 */
		MediaUnit2 = MediaUnit1;
		MediaUnitOff2 = 4096;
		maxzones = meminfo.size / meminfo.erasesize;
		pezstart = startofs / meminfo.erasesize + 1;
		pezend = startofs / meminfo.erasesize + ezones - 1;
		numvunits = (ezones - 2) * PERCENTUSED / 100;
		for (ezone = pezstart; ezone < maxzones; ezone++) {
			if (BadUnitTable[ezone] != ZONE_GOOD) {
				if (numvunits > 1)
					numvunits--;
			}
		}

		INFTLhdr->Partitions[0].virtualUnits = cpu_to_le32(numvunits);
		INFTLhdr->Partitions[0].firstUnit = cpu_to_le32(pezstart);
		INFTLhdr->Partitions[0].lastUnit = cpu_to_le32(pezend);
		INFTLhdr->Partitions[0].flags = cpu_to_le32(INFTL_BDTL);
		INFTLhdr->Partitions[0].spareUnits = cpu_to_le32(0);
		INFTLhdr->Partitions[0].Reserved0 = INFTLhdr->Partitions[0].firstUnit;
		INFTLhdr->Partitions[0].Reserved1 = cpu_to_le32(0);

	} else {

		NFTLhdr = (struct NFTLMediaHeader *) (writebuf[0]);
		strcpy(NFTLhdr->DataOrgID, "ANAND");
		NFTLhdr->NumEraseUnits = cpu_to_le16(part_size / meminfo.erasesize);
		NFTLhdr->FirstPhysicalEUN = cpu_to_le16(MediaUnit1);
		/* N.B. we reserve 2 more Erase Units for "folding" of Virtual Unit Chain */
		NFTLhdr->FormattedSize = cpu_to_le32(part_size - ( (5+bad_zones) * meminfo.erasesize));
		NFTLhdr->UnitSizeFactor = unit_factor;
	}

	/* Phase 2. Writing NFTL Media Headers and Bad Unit Table */
	printf("Phase 2.a Writing %s Media Header and Bad Unit Table\n", nftl);
	pwrite(fd, writebuf[0], 512, MediaUnit1 * meminfo.erasesize + MediaUnitOff1);
	for (ezone = 0; ezone < (meminfo.size / meminfo.erasesize); ezone += 512) {
		pwrite(fd, BadUnitTable + ezone, 512,
				(MediaUnit1 * meminfo.erasesize) + 512 * (1 + ezone / 512));
	}

#if 0
	printf("  MediaHeader contents:\n");
	printf("    NumEraseUnits: %d\n", le16_to_cpu(NFTLhdr->NumEraseUnits));
	printf("    FirstPhysicalEUN: %d\n", le16_to_cpu(NFTLhdr->FirstPhysicalEUN));
	printf("    FormattedSize: %d (%d sectors)\n", le32_to_cpu(NFTLhdr->FormattedSize),
			le32_to_cpu(NFTLhdr->FormattedSize)/512);
#endif
	printf("Phase 2.b Writing Spare %s Media Header and Spare Bad Unit Table\n", nftl);
	pwrite(fd, writebuf[0], 512, MediaUnit2 * meminfo.erasesize + MediaUnitOff2);
	for (ezone = 0; ezone < (meminfo.size / meminfo.erasesize); ezone += 512) {
		pwrite(fd, BadUnitTable + ezone, 512,
				(MediaUnit2 * meminfo.erasesize + MediaUnitOff2) + 512 * (1 + ezone / 512));
	}

	/* UCI #1 for newly erased Erase Unit */
	memset(oobbuf, 0xff, 16);
	oobbuf[11] = oobbuf[10] = oobbuf[9] = 0;
	oobbuf[8]  = (do_inftl) ? 0x00 : 0x03;
	oobbuf[12] = oobbuf[14] = 0x69;
	oobbuf[13] = oobbuf[15] = 0x3c;

	/* N.B. The Media Header and Bad Erase Unit Table are considered as Free Erase Unit
	   by M-System i.e. their Virtual Unit Number == 0xFFFF in the Unit Control Information #0,
	   but their Block Status is BLOCK_USED (0x5555) in their Block Control Information */
	/* Phase 3. Writing Unit Control Information for each Erase Unit */
	printf("Phase 3. Writing Unit Control Information to each Erase Unit\n");
	for (ezone = MediaUnit1; ezone < (ezones + startofs / meminfo.erasesize); ezone++) {
		/* write UCI #1 to each Erase Unit */
		if (BadUnitTable[ezone] != ZONE_GOOD)
			continue;
		oob.start = (ezone * meminfo.erasesize) + 512 + (do_inftl * 512);
		if (ioctl(fd, MEMWRITEOOB, &oob))
			printf("MEMWRITEOOB at %lx: %s\n", (unsigned long)oob.start, strerror(errno));
	}

	exit(0);
}

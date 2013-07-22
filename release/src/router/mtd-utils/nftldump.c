/*
 * nftldump.c: Dumping the content of NFTL partitions on a "Physical Disk"
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

#define PROGRAM_NAME "nftldump"

#define _XOPEN_SOURCE 500 /* For pread */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <asm/types.h>
#include <mtd/mtd-user.h>
#include <mtd/nftl-user.h>
#include <mtd_swab.h>

static struct NFTLMediaHeader MedHead[2];
static mtd_info_t meminfo;

static struct nftl_oob oobbuf;
static struct mtd_oob_buf oob = {0, 16, (unsigned char *)&oobbuf};

static int fd, ofd = -1;;
static int NumMedHeads;

static unsigned char BadUnitTable[MAX_ERASE_ZONES];

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP16(x) do { ; } while(0)
#define SWAP32(x) do { ; } while(0)
#else
#define SWAP16(x) do { x = swab16(x); } while(0)
#define SWAP32(x) do { x = swab32(x); } while(0)
#endif

/* VUCtable, store the Erase Unit Number of the first Erase Unit in the chain */
static unsigned short *VUCtable;

/* FixMe: make this dynamic allocated */
#define ERASESIZE 0x2000
#define NUMVUNITS ((40*1024*1024) / ERASESIZE)
static union nftl_uci UCItable[NUMVUNITS][3];

static unsigned short nextEUN(unsigned short curEUN)
{
	return UCItable[curEUN][0].a.ReplUnitNum;
}

static unsigned int find_media_headers(void)
{
	int i;
	static unsigned long ofs = 0;

	NumMedHeads = 0;
	while (ofs < meminfo.size) {
		pread(fd, &MedHead[NumMedHeads], sizeof(struct NFTLMediaHeader), ofs);
		if (!strncmp(MedHead[NumMedHeads].DataOrgID, "ANAND", 6)) {
			SWAP16(MedHead[NumMedHeads].NumEraseUnits);
			SWAP16(MedHead[NumMedHeads].FirstPhysicalEUN);
			SWAP32(MedHead[NumMedHeads].FormattedSize);

			if (NumMedHeads == 0) {
				printf("NFTL Media Header found at offset 0x%08lx:\n", ofs);
				printf("NumEraseUnits:    %d\n",
						MedHead[NumMedHeads].NumEraseUnits);
				printf("FirstPhysicalEUN: %d\n",
						MedHead[NumMedHeads].FirstPhysicalEUN);
				printf("Formatted Size:   %d\n",
						MedHead[NumMedHeads].FormattedSize);
				printf("UnitSizeFactor:   0x%x\n",
						MedHead[NumMedHeads].UnitSizeFactor);

				/* read BadUnitTable, I don't know why pread() does not work for
				   larger (7680 bytes) chunks */
				for (i = 0; i < MAX_ERASE_ZONES; i += 512)
					pread(fd, &BadUnitTable[i], 512, ofs + 512 + i);
			} else
				printf("Second NFTL Media Header found at offset 0x%08lx\n",ofs);
			NumMedHeads++;
		}

		ofs += meminfo.erasesize;
		if (NumMedHeads == 2) {
			if (strncmp((char *)&MedHead[0], (char *)&MedHead[1], sizeof(struct NFTLMediaHeader)) != 0) {
				printf("warning: NFTL Media Header is not consistent with "
						"Spare NFTL Media Header\n");
			}
			break;
		}
	}

	/* allocate Virtual Unit Chain table for this NFTL partition */
	VUCtable = calloc(MedHead[0].NumEraseUnits, sizeof(unsigned short));
	return NumMedHeads;
}

static void dump_erase_units(void)
{
	int i, j;
	unsigned long ofs;

	for (i = MedHead[0].FirstPhysicalEUN; i < MedHead[0].FirstPhysicalEUN +
			MedHead[0].NumEraseUnits; i++) {
		/* For each Erase Unit */
		ofs = i * meminfo.erasesize;

		/* read the Unit Control Information */
		for (j = 0; j < 3; j++) {
			oob.start = ofs + (j * 512);
			if (ioctl(fd, MEMREADOOB, &oob))
				printf("MEMREADOOB at %lx: %s\n",
						(unsigned long) oob.start, strerror(errno));
			memcpy(&UCItable[i][j], &oobbuf.u, 8);
		}
		if (UCItable[i][1].b.EraseMark != cpu_to_le16(0x3c69)) {
			printf("EraseMark not present in unit %d: %x\n",
					i, UCItable[i][1].b.EraseMark);
		} else {
			/* a properly formatted unit */
			SWAP16(UCItable[i][0].a.VirtUnitNum);
			SWAP16(UCItable[i][0].a.ReplUnitNum);
			SWAP16(UCItable[i][0].a.SpareVirtUnitNum);
			SWAP16(UCItable[i][0].a.SpareReplUnitNum);
			SWAP32(UCItable[i][1].b.WearInfo);
			SWAP16(UCItable[i][1].b.EraseMark);
			SWAP16(UCItable[i][1].b.EraseMark1);
			SWAP16(UCItable[i][2].c.FoldMark);
			SWAP16(UCItable[i][2].c.FoldMark1);

			if (!(UCItable[i][0].a.VirtUnitNum & 0x8000)) {
				/* If this is the first in a chain, store the EUN in the VUC table */
				if (VUCtable[UCItable[i][0].a.VirtUnitNum & 0x7fff]) {
					printf("Duplicate start of chain for VUC %d: "
							"Unit %d replaces Unit %d\n",
							UCItable[i][0].a.VirtUnitNum & 0x7fff,
							i, VUCtable[UCItable[i][0].a.VirtUnitNum & 0x7fff]);
				}
				VUCtable[UCItable[i][0].a.VirtUnitNum & 0x7fff] = i;
			}
		}

		switch (BadUnitTable[i]) {
			case ZONE_BAD_ORIGINAL:
				printf("Unit %d is marked as ZONE_BAD_ORIGINAL\n", i);
				continue;
			case ZONE_BAD_MARKED:
				printf("Unit %d is marked as ZONE_BAD_MARKED\n", i);
				continue;
		}

		/* ZONE_GOOD */
		if (UCItable[i][0].a.VirtUnitNum == 0xffff)
			printf("Unit %d is free\n", i);
		else
			printf("Unit %d is in chain %d and %s a replacement\n", i,
					UCItable[i][0].a.VirtUnitNum & 0x7fff,
					UCItable[i][0].a.VirtUnitNum & 0x8000 ? "is" : "is not");
	}
}

static void dump_virtual_units(void)
{
	int i, j;
	char readbuf[512];

	for (i = 0; i < (MedHead[0].FormattedSize / meminfo.erasesize); i++) {
		unsigned short curEUN = VUCtable[i];

		printf("Virtual Unit #%d: ", i);
		if (!curEUN) {
			printf("Not present\n");
			continue;
		}
		printf("%d", curEUN);

		/* walk through the Virtual Unit Chain */
		while ((curEUN = nextEUN(curEUN)) != 0xffff) {
			printf(", %d", curEUN & 0x7fff);
		}
		printf("\n");

		if (ofd != -1) {
			/* Actually write out the data */
			for (j = 0; j < meminfo.erasesize / 512; j++) {
				/* For each sector in the block */
				unsigned short lastgoodEUN = 0xffff, thisEUN = VUCtable[i];
				unsigned int status;

				if (thisEUN == 0xffff) thisEUN = 0;

				while (thisEUN && (thisEUN & 0x7fff) != 0x7fff) {
					oob.start = (thisEUN * ERASESIZE) + (j * 512);
					ioctl(fd, MEMREADOOB, &oob);
					status = oobbuf.b.Status | oobbuf.b.Status1;

					switch (status) {
						case SECTOR_FREE:
							/* This is still free. Don't look any more */
							thisEUN = 0;
							break;

						case SECTOR_USED:
							/* SECTOR_USED. This is a good one. */
							lastgoodEUN = thisEUN;
							break;
					}

					/* Find the next erase unit in this chain, if any */
					if (thisEUN)
						thisEUN = nextEUN(thisEUN) & 0x7fff;
				}

				if (lastgoodEUN == 0xffff)
					memset(readbuf, 0, 512);
				else
					pread(fd, readbuf, 512,
							(lastgoodEUN * ERASESIZE) + (j * 512));

				write(ofd, readbuf, 512);
			}

		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <device> [<outfile>]\n", PROGRAM_NAME);
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open flash");
		exit (1);
	}

	if (argc > 2) {
		ofd = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
		if (ofd == -1)
			perror ("open outfile");
	}

	/* get size information of the MTD device */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("ioctl(MEMGETINFO)");
		close(fd);
		return 1;
	}

	while (find_media_headers() != 0) {
		dump_erase_units();
		dump_virtual_units();
		free(VUCtable);
	}

	exit(0);
}

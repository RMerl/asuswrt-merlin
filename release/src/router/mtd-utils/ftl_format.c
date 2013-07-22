/* Ported to MTD system.
 * Based on:
 */
/*======================================================================

  Utility to create an FTL partition in a memory region

  ftl_format.c 1.13 1999/10/25 20:01:35

  The contents of this file are subject to the Mozilla Public
  License Version 1.1 (the "License"); you may not use this file
  except in compliance with the License. You may obtain a copy of
  the License at http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS
  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  implied. See the License for the specific language governing
  rights and limitations under the License.

  The initial developer of the original code is David A. Hinds
  <dhinds@pcmcia.sourceforge.org>.  Portions created by David A. Hinds
  are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

  Alternatively, the contents of this file may be used under the
  terms of the GNU Public License version 2 (the "GPL"), in which
  case the provisions of the GPL are applicable instead of the
  above.  If you wish to allow the use of your version of this file
  only under the terms of the GPL and not to allow others to use
  your version of this file under the MPL, indicate your decision
  by deleting the provisions above and replace them with the notice
  and other provisions required by the GPL.  If you do not delete
  the provisions above, a recipient may use your version of this
  file under either the MPL or the GPL.

  ======================================================================*/

#define PROGRAM_NAME "ftl_format"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <mtd/mtd-user.h>
#include <mtd/ftl-user.h>

#include <byteswap.h>
#include <endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define TO_LE32(x) (x)
# define TO_LE16(x) (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
# define TO_LE32(x) (bswap_32(x))
# define TO_LE16(x) (bswap_16(x))
#else
# error cannot detect endianess
#endif

#define FROM_LE32(x) TO_LE32(x)
#define FROM_LE16(x) TO_LE16(x)

/*====================================================================*/

static void print_size(u_int s)
{
	if ((s > 0x100000) && ((s % 0x100000) == 0))
		printf("%d mb", s / 0x100000);
	else if ((s > 0x400) && ((s % 0x400) == 0))
		printf("%d kb", s / 0x400);
	else
		printf("%d bytes", s);
}

/*====================================================================*/

static const char LinkTarget[] = {
	0x13, 0x03, 'C', 'I', 'S'
};
static const char DataOrg[] = {
	0x46, 0x39, 0x00, 'F', 'T', 'L', '1', '0', '0', 0x00
};

static void build_header(erase_unit_header_t *hdr, u_int RegionSize,
		u_int BlockSize, u_int Spare, int Reserve,
		u_int BootSize)
{
	u_int i, BootUnits, nbam, __FormattedSize;

	/* Default everything to the erased state */
	memset(hdr, 0xff, sizeof(*hdr));
	memcpy(hdr->LinkTargetTuple, LinkTarget, 5);
	memcpy(hdr->DataOrgTuple, DataOrg, 10);
	hdr->EndTuple[0] = hdr->EndTuple[1] = 0xff;
	BootSize = (BootSize + (BlockSize-1)) & ~(BlockSize-1);
	BootUnits = BootSize / BlockSize;

	/* We only support 512-byte blocks */
	hdr->BlockSize = 9;
	hdr->EraseUnitSize = 0;
	for (i = BlockSize; i > 1; i >>= 1)
		hdr->EraseUnitSize++;
	hdr->EraseCount = TO_LE32(0);
	hdr->FirstPhysicalEUN = TO_LE16(BootUnits);
	hdr->NumEraseUnits = TO_LE16((RegionSize - BootSize) >> hdr->EraseUnitSize);
	hdr->NumTransferUnits = Spare;
	__FormattedSize = RegionSize - ((Spare + BootUnits) << hdr->EraseUnitSize);
	/* Leave a little bit of space between the CIS and BAM */
	hdr->BAMOffset = TO_LE32(0x80);
	/* Adjust size to account for BAM space */
	nbam = ((1 << (hdr->EraseUnitSize - hdr->BlockSize)) * sizeof(u_int)
			+ FROM_LE32(hdr->BAMOffset) + (1 << hdr->BlockSize) - 1) >> hdr->BlockSize;

	__FormattedSize -=
		(FROM_LE16(hdr->NumEraseUnits) - Spare) * (nbam << hdr->BlockSize);
	__FormattedSize -= ((__FormattedSize * Reserve / 100) & ~0xfff);

	hdr->FormattedSize = TO_LE32(__FormattedSize);

	/* hdr->FirstVMAddress defaults to erased state */
	hdr->NumVMPages = TO_LE16(0);
	hdr->Flags = 0;
	/* hdr->Code defaults to erased state */
	hdr->SerialNumber = TO_LE32(time(NULL));
	/* hdr->AltEUHOffset defaults to erased state */

} /* build_header */

/*====================================================================*/

static int format_partition(int fd, int quiet, int interrogate,
		u_int spare, int reserve, u_int bootsize)
{
	mtd_info_t mtd;
	erase_info_t erase;
	erase_unit_header_t hdr;
	u_int step, lun, i, nbam, *bam;

	/* Get partition size, block size */
	if (ioctl(fd, MEMGETINFO, &mtd) != 0) {
		perror("get info failed");
		return -1;
	}

#if 0
	/* Intel Series 100 Flash: skip first block */
	if ((region.JedecMfr == 0x89) && (region.JedecInfo == 0xaa) &&
			(bootsize == 0)) {
		if (!quiet)
			printf("Skipping first block to protect CIS info...\n");
		bootsize = 1;
	}
#endif

	/* Create header */
	build_header(&hdr, mtd.size, mtd.erasesize,
			spare, reserve, bootsize);

	if (!quiet) {
		printf("Partition size = ");
		print_size(mtd.size);
		printf(", erase unit size = ");
		print_size(mtd.erasesize);
		printf(", %d transfer units\n", spare);
		if (bootsize != 0) {
			print_size(FROM_LE16(hdr.FirstPhysicalEUN) << hdr.EraseUnitSize);
			printf(" allocated for boot image\n");
		}
		printf("Reserved %d%%, formatted size = ", reserve);
		print_size(FROM_LE32(hdr.FormattedSize));
		printf("\n");
		fflush(stdout);
	}

	if (interrogate) {
		char str[3];
		printf("This will destroy all data on the target device.  "
				"Confirm (y/n): ");
		if (fgets(str, 3, stdin) == NULL)
			return -1;
		if ((strcmp(str, "y\n") != 0) && (strcmp(str, "Y\n") != 0))
			return -1;
	}

	/* Create basic block allocation table for control blocks */
	nbam = ((mtd.erasesize >> hdr.BlockSize) * sizeof(u_int)
			+ FROM_LE32(hdr.BAMOffset) + (1 << hdr.BlockSize) - 1) >> hdr.BlockSize;
	bam = malloc(nbam * sizeof(u_int));
	for (i = 0; i < nbam; i++)
		bam[i] = TO_LE32(BLOCK_CONTROL);

	/* Erase partition */
	if (!quiet) {
		printf("Erasing all blocks...\n");
		fflush(stdout);
	}
	erase.length = mtd.erasesize;
	erase.start = mtd.erasesize * FROM_LE16(hdr.FirstPhysicalEUN);
	for (i = 0; i < FROM_LE16(hdr.NumEraseUnits); i++) {
		if (ioctl(fd, MEMERASE, &erase) < 0) {
			if (!quiet) {
				putchar('\n');
				fflush(stdout);
			}
			perror("block erase failed");
			return -1;
		}
		erase.start += erase.length;
		if (!quiet) {
			if (mtd.size <= 0x800000) {
				if (erase.start % 0x100000) {
					if (!(erase.start % 0x20000)) putchar('-');
				}
				else putchar('+');
			}
			else {
				if (erase.start % 0x800000) {
					if (!(erase.start % 0x100000)) putchar('+');
				}
				else putchar('*');
			}
			fflush(stdout);
		}
	}
	if (!quiet) putchar('\n');

	/* Prepare erase units */
	if (!quiet) {
		printf("Writing erase unit headers...\n");
		fflush(stdout);
	}
	lun = 0;
	/* Distribute transfer units over the entire region */
	step = (spare) ? (FROM_LE16(hdr.NumEraseUnits)/spare) : (FROM_LE16(hdr.NumEraseUnits)+1);
	for (i = 0; i < FROM_LE16(hdr.NumEraseUnits); i++) {
		u_int ofs = (i + FROM_LE16(hdr.FirstPhysicalEUN)) << hdr.EraseUnitSize;
		if (lseek(fd, ofs, SEEK_SET) == -1) {
			perror("seek failed");
			break;
		}
		/* Is this a transfer unit? */
		if (((i+1) % step) == 0)
			hdr.LogicalEUN = TO_LE16(0xffff);
		else {
			hdr.LogicalEUN = TO_LE16(lun);
			lun++;
		}
		if (write(fd, &hdr, sizeof(hdr)) == -1) {
			perror("write failed");
			break;
		}
		if (lseek(fd, ofs + FROM_LE32(hdr.BAMOffset), SEEK_SET) == -1) {
			perror("seek failed");
			break;
		}
		if (write(fd, bam, nbam * sizeof(u_int)) == -1) {
			perror("write failed");
			break;
		}
	}
	if (i < FROM_LE16(hdr.NumEraseUnits))
		return -1;
	else
		return 0;
} /* format_partition */

/*====================================================================*/

int main(int argc, char *argv[])
{
	int quiet, interrogate, reserve;
	int optch, errflg, fd, ret;
	u_int spare, bootsize;
	char *s;
	extern char *optarg;
	struct stat buf;

	quiet = 0;
	interrogate = 0;
	spare = 1;
	reserve = 5;
	errflg = 0;
	bootsize = 0;

	while ((optch = getopt(argc, argv, "qir:s:b:")) != -1) {
		switch (optch) {
			case 'q':
				quiet = 1; break;
			case 'i':
				interrogate = 1; break;
			case 's':
				spare = strtoul(optarg, NULL, 0); break;
			case 'r':
				reserve = strtoul(optarg, NULL, 0); break;
			case 'b':
				bootsize = strtoul(optarg, &s, 0);
				if ((*s == 'k') || (*s == 'K'))
					bootsize *= 1024;
				break;
			default:
				errflg = 1; break;
		}
	}
	if (errflg || (optind != argc-1)) {
		fprintf(stderr, "usage: %s [-q] [-i] [-s spare-blocks]"
				" [-r reserve-percent] [-b bootsize] device\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	if (stat(argv[optind], &buf) != 0) {
		perror("status check failed");
		exit(EXIT_FAILURE);
	}
	if (!(buf.st_mode & S_IFCHR)) {
		fprintf(stderr, "%s is not a character special device\n",
				argv[optind]);
		exit(EXIT_FAILURE);
	}
	fd = open(argv[optind], O_RDWR);
	if (fd == -1) {
		perror("open failed");
		exit(EXIT_FAILURE);
	}

	ret = format_partition(fd, quiet, interrogate, spare, reserve,
			bootsize);
	if (!quiet) {
		if (ret)
			printf("format failed.\n");
		else
			printf("format successful.\n");
	}
	close(fd);

	exit((ret) ? EXIT_FAILURE : EXIT_SUCCESS);
	return 0;
}

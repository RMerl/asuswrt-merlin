/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/
/*

	Modified for Tomato Firmware
	Portions, Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"

#include <limits.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#ifdef LINUX26
#include <linux/compiler.h>
#include <mtd/mtd-user.h>
#else
#include <linux/mtd/mtd.h>
#endif
#include <stdint.h>

#include <trxhdr.h>
#include <bcmutils.h>


//	#define DEBUG_SIMULATE


struct code_header {
	char magic[4];
	char res1[4];
	char fwdate[3];
	char fwvern[3];
	char id[4];
	char hw_ver;
	char res2;
	unsigned short flags;
	unsigned char res3[10];
} ;

// -----------------------------------------------------------------------------

static uint32 *crc_table = NULL;

static void crc_done(void)
{
	free(crc_table);
	crc_table = NULL;
}

static int crc_init(void)
{
	uint32 c;
	int i, j;

	if (crc_table == NULL) {
		if ((crc_table = malloc(sizeof(uint32) * 256)) == NULL) return 0;
		for (i = 255; i >= 0; --i) {
			c = i;
			for (j = 8; j > 0; --j) {
				if (c & 1) c = (c >> 1) ^ 0xEDB88320L;
					else c >>= 1;
			}
			crc_table[i] = c;
		}
	}
	return 1;
}

static uint32 crc_calc(uint32 crc, char *buf, int len)
{
	while (len-- > 0) {
		crc = crc_table[(crc ^ *((char *)buf)) & 0xFF] ^ (crc >> 8);
		buf++;
	}
	return crc;
}

// -----------------------------------------------------------------------------

static int mtd_open(const char *mtdname, mtd_info_t *mi)
{
	char path[256];
	int part;
	int size;
	int f;

	if (mtd_getinfo(mtdname, &part, &size)) {
		sprintf(path, MTD_DEV(%d), part);
		if ((f = open(path, O_RDWR|O_SYNC)) >= 0) {
			if ((mi) && ioctl(f, MEMGETINFO, mi) != 0) {
				close(f);
				return -1;
			}
			return f;
		}
	}
	return -1;
}

static int _unlock_erase(const char *mtdname, int erase)
{
	int mf;
	mtd_info_t mi;
	erase_info_t ei;
	int r, ret, skipbb;

	if (!wait_action_idle(5)) return 0;
	set_action(ACT_ERASE_NVRAM);

	r = 0;
	skipbb = 0;
	if ((mf = mtd_open(mtdname, &mi)) >= 0) {
			r = 1;
#if 1
			ei.length = mi.erasesize;
			for (ei.start = 0; ei.start < mi.size; ei.start += mi.erasesize) {
				printf("%sing 0x%x - 0x%x\n", erase ? "Eras" : "Unlock", ei.start, (ei.start + ei.length) - 1);
				fflush(stdout);

				if (!skipbb) {
					loff_t offset = ei.start;

					if ((ret = ioctl(mf, MEMGETBADBLOCK, &offset)) > 0) {
						printf("Skipping bad block at 0x%08x\n", ei.start);
						continue;
					} else if (ret < 0) {
						if (errno == EOPNOTSUPP) {
							skipbb = 1;	// Not supported by this device
						} else {
							perror("MEMGETBADBLOCK");
							r = 0;
							break;
						}
					}
				}
				if (ioctl(mf, MEMUNLOCK, &ei) != 0) {
//					perror("MEMUNLOCK");
//					r = 0;
//					break;
				}
				if (erase) {
					if (ioctl(mf, MEMERASE, &ei) != 0) {
						perror("MEMERASE");
						r = 0;
						break;
					}
				}
			}
#else
			ei.start = 0;
			ei.length = mi.size;

			printf("%sing 0x%x - 0x%x\n", erase ? "Eras" : "Unlock", ei.start, ei.length - 1);
			fflush(stdout);

			if (ioctl(mf, MEMUNLOCK, &ei) != 0) {
				perror("MEMUNLOCK");
				r = 0;
			}
			else if (erase) {
				if (ioctl(mf, MEMERASE, &ei) != 0) {
					perror("MEMERASE");
					r = 0;
				}
			}
#endif

			// checkme:
			char buf[2];
			read(mf, &buf, sizeof(buf));
			close(mf);
	}

	set_action(ACT_IDLE);

	if (r) printf("\"%s\" successfully %s.\n", mtdname, erase ? "erased" : "unlocked");
        else printf("\nError %sing MTD\n", erase ? "eras" : "unlock");

	sleep(1);
	return r;
}

int mtd_unlock(const char *mtdname)
{
	return _unlock_erase(mtdname, 0);
}

int mtd_erase(const char *mtdname)
{
	return _unlock_erase(mtdname, 1);
}

int mtd_unlock_erase_main(int argc, char *argv[])
{
	char c;
	char *dev = NULL;

	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
		case 'd':
			dev = optarg;
			break;
		}
	}

	if (!dev) {
		usage_exit(argv[0], "-d part");
	}

	return _unlock_erase(dev, strstr(argv[0], "erase") ? 1 : 0);
}

int mtd_write_main(int argc, char *argv[])
{
	int mf = -1;
	mtd_info_t mi;
	erase_info_t ei;
	uint32 sig;
	struct code_header cth;
	uint32 crc;
	FILE *f;
	char *buf = NULL;
	const char *error;
	uint32 total;
	long filelen;
	uint32 n;
	struct sysinfo si;
	uint32 ofs;
	char c;
	int web = 0;
	char *iname = NULL;
	char *dev = NULL;

	while ((c = getopt(argc, argv, "i:d:w")) != -1) {
		switch (c) {
		case 'i':
			iname = optarg;
			break;
		case 'd':
			dev = optarg;
			break;
		case 'w':
			web = 1;
			break;
		}
	}

	if ((iname == NULL) || (dev == NULL)) {
		usage_exit(argv[0], "-i file -d part");
	}

	if (!wait_action_idle(10)) {
		printf("System is busy\n");
		return 1;
	}

	set_action(ACT_WEB_UPGRADE);

	if ((f = fopen(iname, "r")) == NULL) {
		error = "Error opening input file";
		goto ERROR;
	}

	fseek( f, 0, SEEK_END);
	filelen = ftell(f);
	fseek( f, 0, SEEK_SET);
	_dprintf("file len=0x%x\n", filelen);

	if ((mf = mtd_open(dev, &mi)) < 0) {
		error = "Error opening MTD device";
		goto ERROR;
	}

	if (mi.erasesize < sizeof(struct trx_header)) {
		error = "Error obtaining MTD information";
		goto ERROR;
	}

	_dprintf("mtd size=%6x, erasesize=%6x\n", mi.size, mi.erasesize);

	total = ROUNDUP(filelen, mi.erasesize);
	
	if (total > mi.size) {
		error = "File is too big to fit in MTD";
		goto ERROR;
	}

	sysinfo(&si);
	//if ((si.freeram * si.mem_unit) > (total + (256 * 1024))) {
	if ((si.freeram * si.mem_unit) > (total + (4096 * 1024))) {
		ei.length = total;
	}
	else {
//		ei.length = ROUNDUP((si.freeram - (256 * 1024)), mi.erasesize);
		ei.length = mi.erasesize;
	}
	_dprintf("freeram=%ld ei.length=%d total=%u\n", si.freeram, ei.length, total);

	if ((buf = malloc(ei.length)) == NULL) {
		error = "Not enough memory";
		goto ERROR;
	}

#ifdef DEBUG_SIMULATE
	FILE *of;
	if ((of = fopen("/mnt/out.bin", "w")) == NULL) {
		error = "Error creating test file";
		goto ERROR;
	}
#endif

	ofs = 0;
	error = NULL;

	for (ei.start = 0; ei.start < total; ei.start += ei.length) {
		n = MIN(ei.length, filelen) - ofs;
		if (safe_fread(buf + ofs, 1, n, f) != n) {
			error = "Error reading file";
			break;
		}
		filelen -= (n + ofs);

		_dprintf("ofs=%ub  n=%ub 0x%x  trx.len=%ub  ei.start=0x%x  ei.length=0x%x  mi.erasesize=0x%x\n",
			   ofs, n, n, filelen, ei.start, ei.length, mi.erasesize);

		n += ofs;

		_dprintf(" erase start=%x len=%x\n", ei.start, ei.length);
		_dprintf(" write %x\n", n);

#ifdef DEBUG_SIMULATE
		if (fwrite(buf, 1, n, of) != n) {
			fclose(of);
			error = "Error writing to test file";
			break;
		}
#else
		ioctl(mf, MEMUNLOCK, &ei);
		if (ioctl(mf, MEMERASE, &ei) != 0) {
			error = "Error erasing MTD block";
			break;
		}
		if (write(mf, buf, n) != n) {
			error = "Error writing to MTD device";
			break;
		}
#endif
		ofs = 0;
	}

#ifdef DEBUG_SIMULATE
	fclose(of);
#endif

ERROR:
	if (buf) free(buf);
	if (mf >= 0) {
		// dummy read to ensure chip(s) are out of lock/suspend state
		read(mf, &n, sizeof(n));
		close(mf);
	}
	if (f) fclose(f);

	crc_done();

	set_action(ACT_IDLE);

	_dprintf("%s\n",  error ? error : "Image successfully flashed");
	return (error ? 1 : 0);
}


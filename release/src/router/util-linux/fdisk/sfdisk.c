/*
 * sfdisk version 3.0 - aeb - 950813
 *
 * Copyright (C) 1995  Andries E. Brouwer (aeb@cwi.nl)
 *
 * This program is free software. You can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation: either Version 1
 * or (at your option) any later version.
 *
 * A.V. Le Blanc (LeBlanc@mcc.ac.uk) wrote Linux fdisk 1992-1994,
 * patched by various people (faith@cs.unc.edu, martin@cs.unc.edu,
 * leisner@sdsp.mc.xerox.com, esr@snark.thyrsus.com, aeb@cwi.nl)
 * 1993-1995, with version numbers (as far as I have seen) 0.93 - 2.0e.
 * This program had (head,sector,cylinder) as basic unit, and was
 * (therefore) broken in several ways for the use on larger disks -
 * for example, my last patch (from 2.0d to 2.0e) was required
 * to allow a partition to cross cylinder 8064, and to write an
 * extended partition past the 4GB mark.
 *
 * The current program is a rewrite from scratch, and I started a
 * version numbering at 3.0.
 *	Andries Brouwer, aeb@cwi.nl, 950813
 *
 * Well, a good user interface is still lacking. On the other hand,
 * many configurations cannot be handled by any other fdisk.
 * I changed the name to sfdisk to prevent confusion. - aeb, 970501
 */

#define PROGNAME "sfdisk"

#include <stdio.h>
#include <stdlib.h>		/* atoi, free */
#include <stdarg.h>		/* varargs */
#include <unistd.h>		/* read, write */
#include <fcntl.h>		/* O_RDWR */
#include <errno.h>		/* ERANGE */
#include <string.h>		/* strchr(), strrchr() */
#include <ctype.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <limits.h>

#include "c.h"
#include "nls.h"
#include "xalloc.h"
#include "blkdev.h"
#include "linux_version.h"
#include "common.h"
#include "wholedisk.h"
#include "gpt.h"
#include "pathnames.h"
#include "canonicalize.h"

/*
 * Table of contents:
 *  A. About seeking
 *  B. About sectors
 *  C. About heads, sectors and cylinders
 *  D. About system Ids
 *  E. About partitions
 *  F. The standard input
 *  G. The command line
 *  H. Listing the current situation
 *  I. Writing the new situation
 */
int exit_status = 0;

int force = 0;			/* 1: do what I say, even if it is stupid ... */
int quiet = 0;			/* 1: suppress all warnings */
/* IA-64 gcc spec file currently does -DLinux... */
#undef Linux
int Linux = 0;			/* 1: suppress warnings irrelevant for Linux */
int DOS = 0;			/* 1: shift extended partitions by #sectors, not 1 */
int DOS_extended = 0;		/* 1: use starting cylinder boundary of extd partn */
int dump = 0;			/* 1: list in a format suitable for later input */
int verify = 0;			/* 1: check that listed partition is reasonable */
int no_write = 0;		/* 1: do not actually write to disk */
int no_reread = 0;		/* 1: skip the BLKRRPART ioctl test at startup */
int leave_last = 0;		/* 1: don't allocate the last cylinder */
int opt_list = 0;
char *save_sector_file = NULL;
char *restore_sector_file = NULL;

static void
my_warn(char *s, ...) {
    va_list p;

    va_start(p, s);
    if (!quiet) {
	fflush(stdout);
	vfprintf(stderr, s, p);
	fflush(stderr);
    }
    va_end(p);
}

static void
error(char *s, ...) {
    va_list p;

    va_start(p, s);
    fflush(stdout);
    fprintf(stderr, "\n" PROGNAME ": ");
    vfprintf(stderr, s, p);
    fflush(stderr);
    va_end(p);
}

/*
 *  A. About seeking
 */

/*
 * sseek: seek to specified sector - return 0 on failure
 *
 * Note: we use 512-byte sectors here, irrespective of the hardware ss.
 */

static int
sseek(char *dev, int fd, unsigned long s) {
    off_t in, out;
    in = ((off_t) s << 9);

    if ((out = lseek(fd, in, SEEK_SET)) != in) {
	perror("lseek");
	error(_("seek error on %s - cannot seek to %lu\n"), dev, s);
	return 0;
    }

    if (in != out) {
	error(_("seek error: wanted 0x%08x%08x, got 0x%08x%08x\n"),
	      (unsigned int)(in >> 32), (unsigned int)(in & 0xffffffff),
	      (unsigned int)(out >> 32), (unsigned int)(out & 0xffffffff));
	return 0;
    }
    return 1;
}

/*
 *  B. About sectors
 */

/*
 * We preserve all sectors read in a chain - some of these will
 * have to be modified and written back.
 */
struct sector {
    struct sector *next;
    unsigned long long sectornumber;
    int to_be_written;
    char data[512];
} *sectorhead;

static void
free_sectors(void) {
    struct sector *s;

    while (sectorhead) {
	s = sectorhead;
	sectorhead = s->next;
	free(s);
    }
}

static struct sector *
get_sector(char *dev, int fd, unsigned long long sno) {
    struct sector *s;

    for (s = sectorhead; s; s = s->next)
	if (s->sectornumber == sno)
	    return s;

    if (!sseek(dev, fd, sno))
	return 0;

    s = xmalloc(sizeof(struct sector));

    if (read(fd, s->data, sizeof(s->data)) != sizeof(s->data)) {
	if (errno)		/* 0 in case we read past end-of-disk */
	    perror("read");
	error(_("read error on %s - cannot read sector %lu\n"), dev, sno);
	free(s);
	return 0;
    }

    s->next = sectorhead;
    sectorhead = s;
    s->sectornumber = sno;
    s->to_be_written = 0;

    return s;
}

static int
msdos_signature(struct sector *s) {
    unsigned char *data = (unsigned char *)s->data;
    if (data[510] == 0x55 && data[511] == 0xaa)
	return 1;
    error(_("ERROR: sector %lu does not have an msdos signature\n"),
	  s->sectornumber);
    return 0;
}

static int
write_sectors(char *dev, int fd) {
    struct sector *s;

    for (s = sectorhead; s; s = s->next)
	if (s->to_be_written) {
	    if (!sseek(dev, fd, s->sectornumber))
		return 0;
	    if (write(fd, s->data, sizeof(s->data)) != sizeof(s->data)) {
		perror("write");
		error(_("write error on %s - cannot write sector %lu\n"),
		      dev, s->sectornumber);
		return 0;
	    }
	    s->to_be_written = 0;
	}
    return 1;
}

static void
ulong_to_chars(unsigned long u, char *uu) {
    int i;

    for (i = 0; i < 4; i++) {
	uu[i] = (u & 0xff);
	u >>= 8;
    }
}

static unsigned long
chars_to_ulong(unsigned char *uu) {
    int i;
    unsigned long u = 0;

    for (i = 3; i >= 0; i--)
	u = (u << 8) | uu[i];
    return u;
}

static int
save_sectors(char *dev, int fdin) {
    struct sector *s;
    char ss[516];
    int fdout = -1;

    fdout = open(save_sector_file, O_WRONLY | O_CREAT, 0444);
    if (fdout < 0) {
	perror(save_sector_file);
	error(_("cannot open partition sector save file (%s)\n"),
	      save_sector_file);
	goto err;
    }

    for (s = sectorhead; s; s = s->next)
	if (s->to_be_written) {
	    ulong_to_chars(s->sectornumber, ss);
	    if (!sseek(dev, fdin, s->sectornumber))
		goto err;
	    if (read(fdin, ss + 4, 512) != 512) {
		perror("read");
		error(_("read error on %s - cannot read sector %lu\n"),
		      dev, s->sectornumber);
		goto err;
	    }
	    if (write(fdout, ss, sizeof(ss)) != sizeof(ss)) {
		perror("write");
		error(_("write error on %s\n"), save_sector_file);
		goto err;
	    }
	}

    close(fdout);
    return 1;

 err:
    if (fdout >= 0)
	close(fdout);
    return 0;
}

static int reread_disk_partition(char *dev, int fd);

static int
restore_sectors(char *dev) {
    int fdin = -1, fdout = -1;
    int ct;
    struct stat statbuf;
    char *ss0 = NULL, *ss;
    unsigned long sno;

    if (stat(restore_sector_file, &statbuf) < 0) {
	perror(restore_sector_file);
	error(_("cannot stat partition restore file (%s)\n"),
	      restore_sector_file);
	goto err;
    }
    if (statbuf.st_size % 516) {
	error(_("partition restore file has wrong size - not restoring\n"));
	goto err;
    }
    if (!(ss0 = (char *)malloc(statbuf.st_size))) {
	error(_("out of memory?\n"));
	goto err;
    }
    ss = ss0;

    fdin = open(restore_sector_file, O_RDONLY);
    if (fdin < 0) {
	perror(restore_sector_file);
	error(_("cannot open partition restore file (%s)\n"),
	      restore_sector_file);
	goto err;
    }
    if (read(fdin, ss, statbuf.st_size) != statbuf.st_size) {
	perror("read");
	error(_("error reading %s\n"), restore_sector_file);
	goto err;
    }

    fdout = open(dev, O_WRONLY);
    if (fdout < 0) {
	perror(dev);
	error(_("cannot open device %s for writing\n"), dev);
	goto err;
    }

    ct = statbuf.st_size / 516;
    while (ct--) {
	sno = chars_to_ulong((unsigned char *)ss);
	if (!sseek(dev, fdout, sno))
	    goto err;
	if (write(fdout, ss + 4, 512) != 512) {
	    perror(dev);
	    error(_("error writing sector %lu on %s\n"), sno, dev);
	    goto err;
	}
	ss += 516;
    }
    free(ss0);
    ss0 = NULL;

    if (!reread_disk_partition(dev, fdout))	/* closes fdout */
	goto err;
    close(fdin);

    return 1;

 err:
    free(ss0);
    if (fdin >= 0)
	close(fdin);
    if (fdout >= 0)
	close(fdout);

    return 0;
}

/*
 *  C. About heads, sectors and cylinders
 */

/*
 * <linux/hdreg.h> defines HDIO_GETGEO and
 * struct hd_geometry {
 *      unsigned char heads;
 *      unsigned char sectors;
 *      unsigned short cylinders;
 *      unsigned long start;
 * };
 *
 * For large disks g.cylinders is truncated, so we use BLKGETSIZE.
 */

/*
 * We consider several geometries for a disk:
 * B - the BIOS geometry, gotten from the kernel via HDIO_GETGEO
 * F - the fdisk geometry
 * U - the user-specified geometry
 *
 * 0 means unspecified / unknown
 */
struct geometry {
    unsigned long long total_size;	/* in sectors */
    unsigned long cylindersize;	/* in sectors */
    unsigned long heads, sectors, cylinders;
    unsigned long start;
} B, F, U;

static struct geometry
get_geometry(char *dev, int fd, int silent) {
    struct hd_geometry g;
    unsigned long cyls;
    unsigned long long sectors;
    struct geometry R;

#ifdef HDIO_GETGEO
    if (ioctl(fd, HDIO_GETGEO, &g))
#endif
    {
	g.heads = g.sectors = g.cylinders = g.start = 0;
	if (!silent)
	    warnx(_("Disk %s: cannot get geometry\n"), dev);
    }

    R.start = g.start;
    R.heads = g.heads;
    R.sectors = g.sectors;
    R.cylindersize = R.heads * R.sectors;
    R.cylinders = 0;
    R.total_size = 0;

    if (blkdev_get_sectors(fd, &sectors) == -1) {
	/* maybe an ordinary file */
	struct stat s;

	if (fstat(fd, &s) == 0 && S_ISREG(s.st_mode))
	    R.total_size = (s.st_size >> 9);
	else if (!silent)
	    warnx(_("Disk %s: cannot get size\n"), dev);
    } else
	R.total_size = sectors;

    if (R.cylindersize && R.total_size) {
	sectors /= R.cylindersize;
	cyls = sectors;
	if (cyls != sectors)
	    cyls = ~0;
	R.cylinders = cyls;
    }

    return R;
}

static void
get_cylindersize(char *dev, int fd, int silent) {
    struct geometry R;

    R = get_geometry(dev, fd, silent);

    B.heads = (U.heads ? U.heads : R.heads ? R.heads : 255);
    B.sectors = (U.sectors ? U.sectors : R.sectors ? R.sectors : 63);
    B.cylinders = (U.cylinders ? U.cylinders : R.cylinders);

    B.cylindersize = B.heads * B.sectors;
    B.total_size = R.total_size;

    if (B.cylinders == 0 && B.cylindersize != 0)
	B.cylinders = B.total_size / B.cylindersize;

    if (R.start && !force) {
	my_warn(_("Warning: start=%lu - this looks like a partition rather than\n"
		  "the entire disk. Using fdisk on it is probably meaningless.\n"
		  "[Use the --force option if you really want this]\n"),
		R.start);
	exit(1);
    }
#if 0
    if (R.heads && B.heads != R.heads)
	my_warn(_("Warning: HDIO_GETGEO says that there are %lu heads\n"),
		R.heads);
    if (R.sectors && B.sectors != R.sectors)
	my_warn(_("Warning: HDIO_GETGEO says that there are %lu sectors\n"),
		R.sectors);
    if (R.cylinders && B.cylinders != R.cylinders
	&& B.cylinders < 65536 && R.cylinders < 65536)
	my_warn(_("Warning: BLKGETSIZE/HDIO_GETGEO says that there are %lu cylinders\n"),
		R.cylinders);
#endif

    if (B.sectors > 63)
	my_warn(_("Warning: unlikely number of sectors (%lu) - usually at most 63\n"
		  "This will give problems with all software that uses C/H/S addressing.\n"),
		B.sectors);
    if (!silent)
	printf(_("\nDisk %s: %lu cylinders, %lu heads, %lu sectors/track\n"),
	       dev, B.cylinders, B.heads, B.sectors);
}

typedef struct {
    unsigned char h, s, c;
} __attribute__ ((packed)) chs;			/* has some c bits in s */
chs zero_chs = { 0, 0, 0 };

typedef struct {
    unsigned long h, s, c;
} longchs;
longchs zero_longchs;

static chs
longchs_to_chs(longchs aa, struct geometry G) {
    chs a;

    if (aa.h < 256 && aa.s < 64 && aa.c < 1024) {
	a.h = aa.h;
	a.s = aa.s | ((aa.c >> 2) & 0xc0);
	a.c = (aa.c & 0xff);
    } else if (G.heads && G.sectors) {
	a.h = G.heads - 1;
	a.s = G.sectors | 0xc0;
	a.c = 0xff;
    } else
	a = zero_chs;
    return a;
}

static longchs
chs_to_longchs(chs a) {
    longchs aa;

    aa.h = a.h;
    aa.s = (a.s & 0x3f);
    aa.c = (a.s & 0xc0);
    aa.c = (aa.c << 2) + a.c;
    return aa;
}

static longchs
ulong_to_longchs(unsigned long sno, struct geometry G) {
    longchs aa;

    if (G.heads && G.sectors && G.cylindersize) {
	aa.s = 1 + sno % G.sectors;
	aa.h = (sno / G.sectors) % G.heads;
	aa.c = sno / G.cylindersize;
	return aa;
    } else {
	return zero_longchs;
    }
}

static chs
ulong_to_chs(unsigned long sno, struct geometry G) {
    return longchs_to_chs(ulong_to_longchs(sno, G), G);
}

#if 0
static unsigned long
longchs_to_ulong(longchs aa, struct geometry G) {
    return (aa.c * G.cylindersize + aa.h * G.sectors + aa.s - 1);
}

static unsigned long
chs_to_ulong(chs a, struct geometry G) {
    return longchs_to_ulong(chs_to_longchs(a), G);
}
#endif

static int
is_equal_chs(chs a, chs b) {
    return (a.h == b.h && a.s == b.s && a.c == b.c);
}

static int
chs_ok(chs a, char *v, char *w) {
    longchs aa = chs_to_longchs(a);
    int ret = 1;

    if (is_equal_chs(a, zero_chs))
	return 1;
    if (B.heads && aa.h >= B.heads) {
	my_warn(_("%s of partition %s has impossible value for head: "
		  "%lu (should be in 0-%lu)\n"), w, v, aa.h, B.heads - 1);
	ret = 0;
    }
    if (B.sectors && (aa.s == 0 || aa.s > B.sectors)) {
	my_warn(_("%s of partition %s has impossible value for sector: "
		  "%lu (should be in 1-%lu)\n"), w, v, aa.s, B.sectors);
	ret = 0;
    }
    if (B.cylinders && aa.c >= B.cylinders) {
	my_warn(_("%s of partition %s has impossible value for cylinders: "
		  "%lu (should be in 0-%lu)\n"), w, v, aa.c, B.cylinders - 1);
	ret = 0;
    }
    return ret;
}

/*
 *  D. About system Ids
 */

#define EMPTY_PARTITION		0
#define EXTENDED_PARTITION	5
#define WIN98_EXTENDED		0x0f
#define DM6_AUX1PARTITION	0x51
#define DM6_AUX3PARTITION	0x53
#define DM6_PARTITION		0x54
#define EZD_PARTITION		0x55
#define LINUX_SWAP              0x82
#define LINUX_NATIVE	        0x83
#define LINUX_EXTENDED		0x85
#define BSD_PARTITION		0xa5
#define NETBSD_PARTITION	0xa9

/* List of partition types now in i386_sys_types.c */

static const char *
sysname(unsigned char type) {
    struct systypes *s;

    for (s = i386_sys_types; s->name; s++)
	if (s->type == type)
	    return _(s->name);
    return _("Unknown");
}

static void
list_types(void) {
    struct systypes *s;

    printf(_("Id  Name\n\n"));
    for (s = i386_sys_types; s->name; s++)
	printf("%2x  %s\n", s->type, _(s->name));
}

static int
is_extended(unsigned char type) {
    return (type == EXTENDED_PARTITION
	    || type == LINUX_EXTENDED || type == WIN98_EXTENDED);
}

static int
is_bsd(unsigned char type) {
    return (type == BSD_PARTITION || type == NETBSD_PARTITION);
}

/*
 *  E. About partitions
 */

/* MS/DOS partition */

struct partition {
    unsigned char bootable;	/* 0 or 0x80 */
    chs begin_chs;
    unsigned char sys_type;
    chs end_chs;
    unsigned int start_sect;	/* starting sector counting from 0 */
    unsigned int nr_sects;	/* nr of sectors in partition */
} __attribute__ ((packed));

/* Unfortunately, partitions are not aligned, and non-Intel machines
   are unhappy with non-aligned integers. So, we need a copy by hand. */
static int
copy_to_int(unsigned char *cp) {
    unsigned int m;

    m = *cp++;
    m += (*cp++ << 8);
    m += (*cp++ << 16);
    m += (*cp++ << 24);
    return m;
}

static void
copy_from_int(int m, char *cp) {
    *cp++ = (m & 0xff);
    m >>= 8;
    *cp++ = (m & 0xff);
    m >>= 8;
    *cp++ = (m & 0xff);
    m >>= 8;
    *cp++ = (m & 0xff);
}

static void
copy_to_part(char *cp, struct partition *p) {
    p->bootable = *cp++;
    p->begin_chs.h = *cp++;
    p->begin_chs.s = *cp++;
    p->begin_chs.c = *cp++;
    p->sys_type = *cp++;
    p->end_chs.h = *cp++;
    p->end_chs.s = *cp++;
    p->end_chs.c = *cp++;
    p->start_sect = copy_to_int((unsigned char *)cp);
    p->nr_sects = copy_to_int((unsigned char *)cp + 4);
}

static void
copy_from_part(struct partition *p, char *cp) {
    *cp++ = p->bootable;
    *cp++ = p->begin_chs.h;
    *cp++ = p->begin_chs.s;
    *cp++ = p->begin_chs.c;
    *cp++ = p->sys_type;
    *cp++ = p->end_chs.h;
    *cp++ = p->end_chs.s;
    *cp++ = p->end_chs.c;
    copy_from_int(p->start_sect, cp);
    copy_from_int(p->nr_sects, cp + 4);
}

/* Roughly speaking, Linux doesn't use any of the above fields except
   for partition type, start sector and number of sectors. (However,
   see also linux/drivers/scsi/fdomain.c.)
   The only way partition type is used (in the kernel) is the comparison
   for equality with EXTENDED_PARTITION (and these Disk Manager types). */

struct part_desc {
    unsigned long long start;
    unsigned long long size;
    unsigned long long sector, offset;	/* disk location of this info */
    struct partition p;
    struct part_desc *ep;	/* extended partition containing this one */
    int ptype;
#define DOS_TYPE	0
#define BSD_TYPE	1
} zero_part_desc;

static struct part_desc *
outer_extended_partition(struct part_desc *p) {
    while (p->ep)
	p = p->ep;
    return p;
}

static int
is_parent(struct part_desc *pp, struct part_desc *p) {
    while (p) {
	if (pp == p)
	    return 1;
	p = p->ep;
    }
    return 0;
}

struct disk_desc {
    struct part_desc partitions[512];
    int partno;
} oldp, newp;

/* determine where on the disk this information goes */
static void
add_sector_and_offset(struct disk_desc *z) {
    int pno;
    struct part_desc *p;

    for (pno = 0; pno < z->partno; pno++) {
	p = &(z->partitions[pno]);
	p->offset = 0x1be + (pno % 4) * sizeof(struct partition);
	p->sector = (p->ep ? p->ep->start : 0);
    }
}

/* tell the kernel to reread the partition tables */
static int
reread_ioctl(int fd) {
#ifdef BLKRRPART
    if (ioctl(fd, BLKRRPART))
#else
    errno = ENOSYS;
#endif
    {
	/* perror might change errno */
	int err = errno;

	perror("BLKRRPART");

	/* 2.6.8 returns EIO for a zero table */
	if (err == EBUSY)
	    return -1;
    }
    return 0;
}

/* reread after writing */
static int
reread_disk_partition(char *dev, int fd) {
    printf(_("Re-reading the partition table ...\n"));
    fflush(stdout);
    sync();

    if (reread_ioctl(fd) && is_blkdev(fd)) {
	warnx(_("The command to re-read the partition table failed.\n"
		"Run partprobe(8), kpartx(8) or reboot your system now,\n"
		"before using mkfs\n"));
	return 0;
    }

    if (fsync(fd) || close(fd)) {
	perror(dev);
	warnx(_("Error closing %s\n"), dev);
	return 0;
    }
    printf("\n");

    return 1;
}

/* find Linux name of this partition, assuming that it will have a name */
static int
index_to_linux(int pno, struct disk_desc *z) {
    int i, ct = 1;
    struct part_desc *p = &(z->partitions[0]);
    for (i = 0; i < pno; i++, p++)
	if (i < 4 || (p->size > 0 && !is_extended(p->p.sys_type)))
	    ct++;
    return ct;
}

static int
linux_to_index(int lpno, struct disk_desc *z) {
    int i, ct = 0;
    struct part_desc *p = &(z->partitions[0]);
    for (i = 0; i < z->partno && ct < lpno; i++, p++)
	if ((i < 4 || (p->size > 0 && !is_extended(p->p.sys_type)))
	    && ++ct == lpno)
	    return i;
    return -1;
}

static int
asc_to_index(char *pnam, struct disk_desc *z) {
    int pnum, pno;

    if (*pnam == '#') {
	pno = atoi(pnam + 1);
    } else {
	pnum = atoi(pnam);
	pno = linux_to_index(pnum, z);
    }
    if (!(pno >= 0 && pno < z->partno))
	errx(EXIT_FAILURE, _("%s: no such partition\n"), pnam);
    return pno;
}

/*
 * List partitions - in terms of sectors, blocks or cylinders
 */
#define F_SECTOR   1
#define F_BLOCK    2
#define F_CYLINDER 3
#define F_MEGABYTE 4

int default_format = F_MEGABYTE;
int specified_format = 0;
int show_extended = 0;
int one_only = 0;
int one_only_pno;
int increment = 0;

static void
set_format(char c) {
    switch (c) {
    default:
	warnx(_("unrecognized format - using sectors\n"));
	/* fallthrough */
    case 'S':
	specified_format = F_SECTOR;
	break;
    case 'B':
	specified_format = F_BLOCK;
	break;
    case 'C':
	specified_format = F_CYLINDER;
	break;
    case 'M':
	specified_format = F_MEGABYTE;
	break;
    }
}

static unsigned long
unitsize(int format) {
    default_format = (B.cylindersize ? F_CYLINDER : F_MEGABYTE);
    if (!format && !(format = specified_format))
	format = default_format;

    switch (format) {
    default:
    case F_CYLINDER:
	if (B.cylindersize)
	    return B.cylindersize;
    case F_SECTOR:
	return 1;
    case F_BLOCK:
	return 2;
    case F_MEGABYTE:
	return 2048;
    }
}

static unsigned long long
get_disksize(int format) {
    if (B.total_size && leave_last)
	/* don't use last cylinder (--leave-last option) */
	return (B.total_size - B.cylindersize) / unitsize(format);

    return B.total_size / unitsize(format);
}

static void
out_partition_header(char *dev, int format, struct geometry G) {
    if (dump) {
	printf("# partition table of %s\n", dev);
	printf("unit: sectors\n\n");
	return;
    }

    default_format = (G.cylindersize ? F_CYLINDER : F_MEGABYTE);
    if (!format && !(format = specified_format))
	format = default_format;

    switch (format) {
    default:
	warnx(_("unimplemented format - using %s\n"),
		G.cylindersize ? _("cylinders") : _("sectors"));
    case F_CYLINDER:
	if (G.cylindersize) {
	    printf(_("Units = cylinders of %lu bytes, blocks of 1024 bytes"
		     ", counting from %d\n\n"), G.cylindersize << 9, increment);
	    printf(_("   Device Boot Start     End   #cyls    #blocks   Id  System\n"));
	    break;
	}
	/* fall through */
    case F_SECTOR:
	printf(_("Units = sectors of 512 bytes, counting from %d\n\n"),
	       increment);
	printf(_("   Device Boot    Start       End   #sectors  Id  System\n"));
	break;
    case F_BLOCK:
	printf(_("Units = blocks of 1024 bytes, counting from %d\n\n"),
	       increment);
	printf(_("   Device Boot   Start       End    #blocks   Id  System\n"));
	break;
    case F_MEGABYTE:
	printf(_("Units = mebibytes of 1048576 bytes, blocks of 1024 bytes"
		 ", counting from %d\n\n"), increment);
	printf(_("   Device Boot Start   End    MiB    #blocks   Id  System\n"));
	break;
    }
}

static void
out_rounddown(int width, unsigned long long n, unsigned long unit, int inc) {
    printf("%*llu", width, inc + n / unit);
    if (unit != 1)
	putchar((n % unit) ? '+' : ' ');
    putchar(' ');
}

static void
out_roundup(int width, unsigned long long n, unsigned long unit, int inc) {
    if (n == (unsigned long long)(-1))
	printf("%*s", width, "-");
    else
	printf("%*llu", width, inc + n / unit);
    if (unit != 1)
	putchar(((n + 1) % unit) ? '-' : ' ');
    putchar(' ');
}

static void
out_roundup_size(int width, unsigned long long n, unsigned long unit) {
    printf("%*llu", width, (n + unit - 1) / unit);
    if (unit != 1)
	putchar((n % unit) ? '-' : ' ');
    putchar(' ');
}

static struct geometry
get_fdisk_geometry_one(struct part_desc *p) {
    struct geometry G;

    memset(&G, 0, sizeof(struct geometry));
    chs b = p->p.end_chs;
    longchs bb = chs_to_longchs(b);
    G.heads = bb.h + 1;
    G.sectors = bb.s;
    G.cylindersize = G.heads * G.sectors;
    return G;
}

static int
get_fdisk_geometry(struct disk_desc *z) {
    struct part_desc *p;
    int pno, agree;
    struct geometry G0, G;

    memset(&G0, 0, sizeof(struct geometry));
    agree = 0;
    for (pno = 0; pno < z->partno; pno++) {
	p = &(z->partitions[pno]);
	if (p->size != 0 && p->p.sys_type != 0) {
	    G = get_fdisk_geometry_one(p);
	    if (!G0.heads) {
		G0 = G;
		agree = 1;
	    } else if (G.heads != G0.heads || G.sectors != G0.sectors) {
		agree = 0;
		break;
	    }
	}
    }
    F = (agree ? G0 : B);
    return (F.sectors != B.sectors || F.heads != B.heads);
}

static void
out_partition(char *dev, int format, struct part_desc *p,
	      struct disk_desc *z, struct geometry G) {
    unsigned long long start, end, size;
    int pno, lpno;

    if (!format && !(format = specified_format))
	format = default_format;

    pno = p - &(z->partitions[0]);	/* our index */
    lpno = index_to_linux(pno, z);	/* name of next one that has a name */
    if (pno == linux_to_index(lpno, z))	/* was that us? */
	printf("%s", partname(dev, lpno, 10));	/* yes */
    else if (show_extended)
	printf("    -     ");
    else
	return;
    putchar(dump ? ':' : ' ');

    start = p->start;
    end = p->start + p->size - 1;
    size = p->size;

    if (dump) {
	printf(" start=%9llu", start);
	printf(", size=%9llu", size);
	if (p->ptype == DOS_TYPE) {
	    printf(", Id=%2x", p->p.sys_type);
	    if (p->p.bootable == 0x80)
		printf(", bootable");
	}
	printf("\n");
	return;
    }

    if (p->ptype != DOS_TYPE || p->p.bootable == 0)
	printf("   ");
    else if (p->p.bootable == 0x80)
	printf(" * ");
    else
	printf(" ? ");		/* garbage */

    switch (format) {
    case F_CYLINDER:
	if (G.cylindersize) {
	    out_rounddown(6, start, G.cylindersize, increment);
	    out_roundup(6, end, G.cylindersize, increment);
	    out_roundup_size(6, size, G.cylindersize);
	    out_rounddown(9, size, 2, 0);
	    break;
	}
	/* fall through */
    default:
    case F_SECTOR:
	out_rounddown(9, start, 1, increment);
	out_roundup(9, end, 1, increment);
	out_rounddown(10, size, 1, 0);
	break;
    case F_BLOCK:
#if 0
	printf("%8lu,%3lu ",
	       p->sector / 2, ((p->sector & 1) ? 512 : 0) + p->offset);
#endif
	out_rounddown(8, start, 2, increment);
	out_roundup(8, end, 2, increment);
	out_rounddown(9, size, 2, 0);
	break;
    case F_MEGABYTE:
	out_rounddown(5, start, 2048, increment);
	out_roundup(5, end, 2048, increment);
	out_roundup_size(5, size, 2048);
	out_rounddown(9, size, 2, 0);
	break;
    }
    if (p->ptype == DOS_TYPE) {
	printf(" %2x  %s\n", p->p.sys_type, sysname(p->p.sys_type));
    } else {
	printf("\n");
    }

    /* Is chs as we expect? */
    if (!quiet && p->ptype == DOS_TYPE) {
	chs a, b;
	longchs aa, bb;
	a = (size ? ulong_to_chs(start, G) : zero_chs);
	b = p->p.begin_chs;
	aa = chs_to_longchs(a);
	bb = chs_to_longchs(b);
	if (a.s && !is_equal_chs(a, b))
	    warnx(_("\t\tstart: (c,h,s) expected (%ld,%ld,%ld) found (%ld,%ld,%ld)\n"),
		    aa.c, aa.h, aa.s, bb.c, bb.h, bb.s);
	a = (size ? ulong_to_chs(end, G) : zero_chs);
	b = p->p.end_chs;
	aa = chs_to_longchs(a);
	bb = chs_to_longchs(b);
	if (a.s && !is_equal_chs(a, b))
	    warnx(_("\t\tend: (c,h,s) expected (%ld,%ld,%ld) found (%ld,%ld,%ld)\n"),
		    aa.c, aa.h, aa.s, bb.c, bb.h, bb.s);
	if (G.cylinders && G.cylinders < 1024 && bb.c > G.cylinders)
	    warnx(_("partition ends on cylinder %ld, beyond the end of the disk\n"),
		    bb.c);
    }
}

static void
out_partitions(char *dev, struct disk_desc *z) {
    int pno, format = 0;

    if (z->partno == 0)
	warnx(_("No partitions found\n"));
    else {
	if (get_fdisk_geometry(z) && !dump) {
	    warnx(_("Warning: The partition table looks like it was made\n"
		      "  for C/H/S=*/%ld/%ld (instead of %ld/%ld/%ld).\n"
		      "For this listing I'll assume that geometry.\n"),
		    F.heads, F.sectors, B.cylinders, B.heads, B.sectors);
	}

	out_partition_header(dev, format, F);
	for (pno = 0; pno < z->partno; pno++) {
	    out_partition(dev, format, &(z->partitions[pno]), z, F);
	    if (show_extended && pno % 4 == 3)
		printf("\n");
	}
    }
}

static int
disj(struct part_desc *p, struct part_desc *q) {
    return ((p->start + p->size <= q->start)
	    || (is_extended(p->p.sys_type)
		&& q->start + q->size <= p->start + p->size));
}

static char *
pnumber(struct part_desc *p, struct disk_desc *z) {
    static char buf[20];
    int this, next;
    struct part_desc *p0 = &(z->partitions[0]);

    this = index_to_linux(p - p0, z);
    next = index_to_linux(p - p0 + 1, z);

    if (next > this)
	sprintf(buf, "%d", this);
    else
	sprintf(buf, "[%d]", this);
    return buf;
}

static int
partitions_ok(int fd, struct disk_desc *z) {
    struct part_desc *partitions = &(z->partitions[0]), *p, *q;
    int partno = z->partno;

#define PNO(p) pnumber(p, z)

    /* Have at least 4 partitions been defined? */
    if (partno < 4) {
	if (!partno)
	    errx(EXIT_FAILURE, _("no partition table present."));
	else
	    errx(EXIT_FAILURE, _("strange, only %d partitions defined."), partno);
	return 0;
    }

    /* Are the partitions of size 0 marked empty?
       And do they have start = 0? And bootable = 0? */
    for (p = partitions; p - partitions < partno; p++)
	if (p->size == 0) {
	    if (p->p.sys_type != EMPTY_PARTITION)
		my_warn(_("Warning: partition %s has size 0 but is not marked Empty\n"),
			PNO(p));
	    else if (p->p.bootable != 0)
		my_warn(_("Warning: partition %s has size 0 and is bootable\n"),
			PNO(p));
	    else if (p->p.start_sect != 0)
		my_warn(_("Warning: partition %s has size 0 and nonzero start\n"),
			PNO(p));
	    /* all this is probably harmless, no error return */
	}

    /* Are the logical partitions contained in their extended partitions? */
    for (p = partitions + 4; p < partitions + partno; p++)
	if (p->ptype == DOS_TYPE)
	    if (p->size && !is_extended(p->p.sys_type)) {
		q = p->ep;
		if (p->start < q->start
		    || p->start + p->size > q->start + q->size) {
		    my_warn(_("Warning: partition %s is not contained in "
			      "partition %s\n"), PNO(p), PNO(q));
		    return 0;
		}
	    }

    /* Are the data partitions mutually disjoint? */
    for (p = partitions; p < partitions + partno; p++)
	if (p->size && !is_extended(p->p.sys_type))
	    for (q = p + 1; q < partitions + partno; q++)
		if (q->size && !is_extended(q->p.sys_type))
		    if (!((p->start > q->start) ? disj(q, p) : disj(p, q))) {
			my_warn(_("Warning: partitions %s and %s overlap\n"),
				PNO(p), PNO(q));
			return 0;
		    }

    /* Are the data partitions and the extended partition
       table sectors disjoint? */
    for (p = partitions; p < partitions + partno; p++)
	if (p->size && !is_extended(p->p.sys_type))
	    for (q = partitions; q < partitions + partno; q++)
		if (is_extended(q->p.sys_type))
		    if (p->start <= q->start && p->start + p->size > q->start) {
			my_warn(_("Warning: partition %s contains part of "
				  "the partition table (sector %llu),\n"
				  "and will destroy it when filled\n"),
				PNO(p), q->start);
			return 0;
		    }

    /* Do they start past zero and end before end-of-disk? */
    {
	unsigned long long ds = get_disksize(F_SECTOR);
	for (p = partitions; p < partitions + partno; p++)
	    if (p->size) {
		if (p->start == 0) {
		    my_warn(_("Warning: partition %s starts at sector 0\n"),
			    PNO(p));
		    return 0;
		}
		if (p->size && p->start + p->size > ds) {
		    my_warn(_("Warning: partition %s extends past end of disk\n"),
			    PNO(p));
		    return 0;
		}
	    }
    }

    int sector_size;
    if (blkdev_get_sector_size(fd, &sector_size) == -1)
	sector_size = DEFAULT_SECTOR_SIZE;

    /* Is the size of partitions less than 2^32 sectors limit? */
    for (p = partitions; p < partitions + partno; p++)
	if (p->size > UINT_MAX) {
	    unsigned long long bytes = p->size * sector_size;
	    int giga = bytes / 1000000000;
	    int hectogiga = (giga + 50) / 100;
	    my_warn(_("Warning: partition %s has size %d.%d TB (%llu bytes),\n"
		      "which is larger than the %llu bytes limit imposed\n"
		      "by the DOS partition table for %d-byte sectors\n"),
		    PNO(p), hectogiga / 10, hectogiga % 10,
		    bytes,
		    (unsigned long long) UINT_MAX * sector_size,
		    sector_size);
	    return 0;
	}

    /* Do the partitions start below the 2^32 sectors limit? */
    for (p = partitions; p < partitions + partno; p++)
	if (p->start > UINT_MAX) {
	    unsigned long long bytes = p->start * sector_size;
	    int giga = bytes / 1000000000;
	    int hectogiga = (giga + 50) / 100;
	    my_warn(_("Warning: partition %s starts at sector %llu (%d.%d TB for %d-byte sectors),\n"
		      "which exceeds the DOS partition table limit of %llu sectors\n"),
		    PNO(p),
		    p->start,
		    hectogiga / 10,
		    hectogiga % 10,
		    sector_size,
		    (unsigned long long) UINT_MAX);
	    return 0;
	}

    /* At most one chain of DOS extended partitions ? */
    /* It seems that the OS/2 fdisk has the additional requirement
       that the extended partition must be the fourth one */
    {
	int ect = 0;
	for (p = partitions; p < partitions + 4; p++)
	    if (p->p.sys_type == EXTENDED_PARTITION)
		ect++;
	if (ect > 1 && !Linux) {
	    my_warn(_("Among the primary partitions, at most one can be extended\n"
		      " (although this is not a problem under Linux)\n"));
	    return 0;
	}
    }

    /*
     * Do all partitions start at a cylinder boundary ?
     * (this is not required for Linux)
     * The first partition starts after MBR.
     * Logical partitions start slightly after the containing extended partn.
     */
    if (B.cylindersize && !Linux) {
	for (p = partitions; p < partitions + partno; p++)
	    if (p->size) {
		if (p->start % B.cylindersize != 0
		    && (!p->ep
			|| p->start / B.cylindersize !=
			p->ep->start / B.cylindersize)
		    && (p->p.start_sect >= B.cylindersize)) {
		    my_warn(_("Warning: partition %s does not start "
			      "at a cylinder boundary\n"), PNO(p));
		    if (specified_format == F_CYLINDER)
			return 0;
		}
		if ((p->start + p->size) % B.cylindersize) {
		    my_warn(_("Warning: partition %s does not end "
			      "at a cylinder boundary\n"), PNO(p));
		    if (specified_format == F_CYLINDER)
			return 0;
		}
	    }
    }

    /* Usually, one can boot only from primary partitions. */
    /* In fact, from a unique one only. */
    /* do not warn about bootable extended partitions -
       often LILO is there */
    {
	int pno = -1;
	for (p = partitions; p < partitions + partno; p++)
	    if (p->p.bootable) {
		if (pno == -1)
		    pno = p - partitions;
		else if (p - partitions < 4) {
		    my_warn(_("Warning: more than one primary partition is marked "
			      "bootable (active)\n"
			      "This does not matter for LILO, but the DOS MBR will "
			      "not boot this disk.\n"));
		    break;
		}
		if (p - partitions >= 4) {
		    my_warn(_("Warning: usually one can boot from primary partitions "
			      "only\nLILO disregards the `bootable' flag.\n"));
		    break;
		}
	    }
	if (pno == -1 || pno >= 4)
	    my_warn(_("Warning: no primary partition is marked bootable (active)\n"
		      "This does not matter for LILO, but the DOS MBR will "
		      "not boot this disk.\n"));
    }

    /* Is chs as we expect? */
    for (p = partitions; p < partitions + partno; p++)
	if (p->ptype == DOS_TYPE) {
	    chs a, b;
	    longchs aa, bb;
	    a = p->size ? ulong_to_chs(p->start, B) : zero_chs;
	    b = p->p.begin_chs;
	    aa = chs_to_longchs(a);
	    bb = chs_to_longchs(b);
	    if (!Linux && !chs_ok(b, PNO(p), _("start")))
		return 0;
	    if (a.s && !is_equal_chs(a, b))
		my_warn(_("partition %s: start: (c,h,s) expected (%ld,%ld,%ld) found (%ld,%ld,%ld)\n"),
			PNO(p), aa.c, aa.h, aa.s, bb.c, bb.h, bb.s);
	    a = p->size ? ulong_to_chs(p->start + p->size - 1, B) : zero_chs;
	    b = p->p.end_chs;
	    aa = chs_to_longchs(a);
	    bb = chs_to_longchs(b);
	    if (!Linux && !chs_ok(b, PNO(p), _("end")))
		return 0;
	    if (a.s && !is_equal_chs(a, b))
		my_warn(_("partition %s: end: (c,h,s) expected (%ld,%ld,%ld) found (%ld,%ld,%ld)\n"),
			PNO(p), aa.c, aa.h, aa.s, bb.c, bb.h, bb.s);
	    if (B.cylinders && B.cylinders < 1024 && bb.c > B.cylinders)
		my_warn(_("partition %s ends on cylinder %ld, beyond the end of the disk\n"),
			PNO(p), bb.c);
	}

    return 1;

#undef PNO
}

static void
extended_partition(char *dev, int fd, struct part_desc *ep, struct disk_desc *z) {
    char *cp;
    struct sector *s;
    unsigned long long start, here, next;
    int i, moretodo = 1;
    struct partition p;
    struct part_desc *partitions = &(z->partitions[0]);
    size_t pno = z->partno;

    here = start = ep->start;

    if (B.cylindersize && start % B.cylindersize) {
	/* This is BAD */
	if (DOS_extended) {
	    here = start -= (start % B.cylindersize);
	    warnx(_("Warning: shifted start of the extd partition "
		      "from %lld to %lld\n"
		      "(For listing purposes only. "
		      "Do not change its contents.)\n"), ep->start, start);
	} else {
	    warnx(_("Warning: extended partition does not start at a "
		      "cylinder boundary.\n"
		      "DOS and Linux will interpret the contents differently.\n"));
	}
    }

    while (moretodo) {
	moretodo = 0;

	if (!(s = get_sector(dev, fd, here)))
	    break;

	if (!msdos_signature(s))
	    break;

	cp = s->data + 0x1be;

	if (pno + 4 >= ARRAY_SIZE(z->partitions)) {
	    warnx(_("too many partitions - ignoring those past nr (%ld)\n"),
		    pno - 1);
	    break;
	}

	next = 0;

	for (i = 0; i < 4; i++, cp += sizeof(struct partition)) {
	    partitions[pno].sector = here;
	    partitions[pno].offset = cp - s->data;
	    partitions[pno].ep = ep;
	    copy_to_part(cp, &p);
	    if (is_extended(p.sys_type)) {
		partitions[pno].start = start + p.start_sect;
		if (next)
		    warnx(_("tree of partitions?\n"));
		else
		    next = partitions[pno].start;	/* follow `upper' branch */
		moretodo = 1;
	    } else {
		partitions[pno].start = here + p.start_sect;
	    }
	    partitions[pno].size = p.nr_sects;
	    partitions[pno].ptype = DOS_TYPE;
	    partitions[pno].p = p;
	    pno++;
	}
	here = next;
    }

    z->partno = pno;
}

#define BSD_DISKMAGIC   (0x82564557UL)
#define BSD_MAXPARTITIONS       16
#define BSD_FS_UNUSED	   0
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
struct bsd_disklabel {
    u32 d_magic;
    char d_junk1[4];
    char d_typename[16];
    char d_packname[16];
    char d_junk2[92];
    u32 d_magic2;
    char d_junk3[2];
    u16 d_npartitions;		/* number of partitions in following */
    char d_junk4[8];
    struct bsd_partition {	/* the partition table */
	u32 p_size;		/* number of sectors in partition */
	u32 p_offset;		/* starting sector */
	u32 p_fsize;		/* filesystem basic fragment size */
	u8 p_fstype;		/* filesystem type, see below */
	u8 p_frag;		/* filesystem fragments per block */
	u16 p_cpg;		/* filesystem cylinders per group */
    } d_partitions[BSD_MAXPARTITIONS];	/* actually may be more */
};

static void
bsd_partition(char *dev, int fd, struct part_desc *ep, struct disk_desc *z) {
    struct bsd_disklabel *l;
    struct bsd_partition *bp, *bp0;
    unsigned long long start = ep->start;
    struct sector *s;
    struct part_desc *partitions = &(z->partitions[0]);
    size_t pno = z->partno;

    if (!(s = get_sector(dev, fd, start + 1)))
	return;
    l = (struct bsd_disklabel *)(s->data);
    if (l->d_magic != BSD_DISKMAGIC || l->d_magic2 != BSD_DISKMAGIC)
	return;

    bp = bp0 = &l->d_partitions[0];
    while (bp - bp0 < BSD_MAXPARTITIONS && bp - bp0 < l->d_npartitions) {
	if (pno + 1 >= ARRAY_SIZE(z->partitions)) {
	    warnx(_("too many partitions - ignoring those "
		      "past nr (%ld)\n"), pno - 1);
	    break;
	}
	if (bp->p_fstype != BSD_FS_UNUSED) {
	    partitions[pno].start = bp->p_offset;
	    partitions[pno].size = bp->p_size;
	    partitions[pno].sector = start + 1;
	    partitions[pno].offset = (char *)bp - (char *)bp0;
	    partitions[pno].ep = 0;
	    partitions[pno].ptype = BSD_TYPE;
	    pno++;
	}
	bp++;
    }
    z->partno = pno;
}

static int
msdos_partition(char *dev, int fd, unsigned long start, struct disk_desc *z) {
    int i;
    char *cp;
    struct partition pt;
    struct sector *s;
    struct part_desc *partitions = &(z->partitions[0]);
    int pno = z->partno;
    int bsd_later = 1;
#ifdef __linux__
    bsd_later = (get_linux_version() >= KERNEL_VERSION(2, 3, 40));
#endif

    if (!(s = get_sector(dev, fd, start)))
	return 0;

    if (!msdos_signature(s))
	return 0;

    cp = s->data + 0x1be;
    copy_to_part(cp, &pt);

    /* If I am not mistaken, recent kernels will hide this from us,
       so we will never actually see traces of a Disk Manager */
    if (pt.sys_type == DM6_PARTITION
	|| pt.sys_type == EZD_PARTITION
	|| pt.sys_type == DM6_AUX1PARTITION
	|| pt.sys_type == DM6_AUX3PARTITION) {
	warnx(_("detected Disk Manager - unable to handle that\n"));
	return 0;
    }

    unsigned int sig = *(unsigned short *)(s->data + 2);
    if (sig <= 0x1ae
	&& *(unsigned short *)(s->data + sig) == 0x55aa
	&& (1 & *(unsigned char *)(s->data + sig + 2))) {
	    warnx(_("DM6 signature found - giving up\n"));
	    return 0;
    }

    for (pno = 0; pno < 4; pno++, cp += sizeof(struct partition)) {
	partitions[pno].sector = start;
	partitions[pno].offset = cp - s->data;
	copy_to_part(cp, &pt);
	partitions[pno].start = start + pt.start_sect;
	partitions[pno].size = pt.nr_sects;
	partitions[pno].ep = 0;
	partitions[pno].p = pt;
    }

    z->partno = pno;

    for (i = 0; i < 4; i++) {
	if (is_extended(partitions[i].p.sys_type)) {
	    if (!partitions[i].size) {
		warnx(_("strange..., an extended partition of size 0?\n"));
		continue;
	    }
	    extended_partition(dev, fd, &partitions[i], z);
	}
	if (!bsd_later && is_bsd(partitions[i].p.sys_type)) {
	    if (!partitions[i].size) {
		warnx(_("strange..., a BSD partition of size 0?\n"));
		continue;
	    }
	    bsd_partition(dev, fd, &partitions[i], z);
	}
    }

    if (bsd_later) {
	for (i = 0; i < 4; i++) {
	    if (is_bsd(partitions[i].p.sys_type)) {
		if (!partitions[i].size) {
		    warnx(_("strange..., a BSD partition of size 0?\n"));
		    continue;
		}
		bsd_partition(dev, fd, &partitions[i], z);
	    }
	}
    }

    return 1;
}

static int
osf_partition(char *dev __attribute__ ((__unused__)),
	      int fd __attribute__ ((__unused__)),
	      unsigned long start __attribute__ ((__unused__)),
	      struct disk_desc *z __attribute__ ((__unused__))) {
    return 0;
}

static int
sun_partition(char *dev __attribute__ ((__unused__)),
	      int fd __attribute__ ((__unused__)),
	      unsigned long start __attribute__ ((__unused__)),
	      struct disk_desc *z __attribute__ ((__unused__))) {
    return 0;
}

static int
amiga_partition(char *dev __attribute__ ((__unused__)),
		int fd __attribute__ ((__unused__)),
		unsigned long start __attribute__ ((__unused__)),
		struct disk_desc *z __attribute__ ((__unused__))) {
    return 0;
}

static void
get_partitions(char *dev, int fd, struct disk_desc *z) {
    z->partno = 0;

    if (!msdos_partition(dev, fd, 0, z)
	&& !osf_partition(dev, fd, 0, z)
	&& !sun_partition(dev, fd, 0, z)
	&& !amiga_partition(dev, fd, 0, z)) {
	warnx(_(" %s: unrecognized partition table type\n"), dev);
	return;
    }
}

static int
write_partitions(char *dev, int fd, struct disk_desc *z) {
    struct sector *s;
    struct part_desc *partitions = &(z->partitions[0]), *p;
    int pno = z->partno;

    if (no_write) {
	warnx(_("-n flag was given: Nothing changed\n"));
	exit(0);
    }

    for (p = partitions; p < partitions + pno; p++) {
	s = get_sector(dev, fd, p->sector);
	if (!s)
	    return 0;
	s->to_be_written = 1;
	if (p->ptype == DOS_TYPE) {
	    copy_from_part(&(p->p), s->data + p->offset);
	    s->data[510] = 0x55;
	    s->data[511] = (unsigned char)0xaa;
	}
    }
    if (save_sector_file) {
	if (!save_sectors(dev, fd)) {
	    errx(EXIT_FAILURE, _("Failed saving the old sectors - aborting\n"));
	    return 0;
	}
    }
    if (!write_sectors(dev, fd)) {
	error(_("Failed writing the partition on %s\n"), dev);
	return 0;
    }
    if (fsync(fd)) {
	perror(dev);
	error(_("Failed writing the partition on %s\n"), dev);
	return 0;
    }
    return 1;
}

/*
 *  F. The standard input
 */

/*
 * Input format:
 * <start> <size> <type> <bootable> <c,h,s> <c,h,s>
 * Fields are separated by whitespace or comma or semicolon possibly
 * followed by whitespace; initial and trailing whitespace is ignored.
 * Numbers can be octal, decimal or hexadecimal, decimal is default
 * The <c,h,s> parts can (and probably should) be omitted.
 * Bootable is specified as [*|-], with as default not-bootable.
 * Type is given in hex, without the 0x prefix, or is [E|S|L|X], where
 * L (LINUX_NATIVE (83)) is the default, S is LINUX_SWAP (82), and E
 * is EXTENDED_PARTITION (5), X is LINUX_EXTENDED (85).
 * The default value of start is the first nonassigned sector/cylinder/...
 * The default value of size is as much as possible (until next
 * partition or end-of-disk).
 * .: end of chain of extended partitions.
 *
 * On interactive input an empty line means: all defaults.
 * Otherwise empty lines are ignored.
 */

int eof, eob;

struct dumpfld {
    int fldno;
    char *fldname;
    int is_bool;
} dumpflds[] = {
    {
    0, "start", 0}, {
    1, "size", 0}, {
    2, "Id", 0}, {
    3, "bootable", 1}, {
    4, "bh", 0}, {
    5, "bs", 0}, {
    6, "bc", 0}, {
    7, "eh", 0}, {
    8, "es", 0}, {
    9, "ec", 0}
};

/*
 * Read a line, split it into fields
 *
 * (some primitive handwork, but a more elaborate parser seems
 *  unnecessary)
 */
#define RD_EOF (-1)
#define RD_CMD (-2)

static int
read_stdin(char **fields, char *line, int fieldssize, int linesize) {
    char *lp, *ip;
    int c, fno;

    /* boolean true and empty string at start */
    line[0] = '*';
    line[1] = 0;
    for (fno = 0; fno < fieldssize; fno++)
	fields[fno] = line + 1;
    fno = 0;

    /* read a line from stdin */
    lp = fgets(line + 2, linesize - 2, stdin);
    if (lp == NULL) {
	eof = 1;
	return RD_EOF;
    }
    if (!(lp = strchr(lp, '\n')))
	errx(EXIT_FAILURE, _("long or incomplete input line - quitting"));
    *lp = 0;

    /* remove comments, if any */
    if ((lp = strchr(line + 2, '#')) != 0)
	*lp = 0;

    /* recognize a few commands - to be expanded */
    if (!strcmp(line + 2, "unit: sectors")) {
	specified_format = F_SECTOR;
	return RD_CMD;
    }

    /* dump style? - then bad input is fatal */
    if ((ip = strchr(line + 2, ':')) != 0) {
	struct dumpfld *d;

 nxtfld:
	ip++;
	while (isspace(*ip))
	    ip++;
	if (*ip == 0)
	    return fno;
	for (d = dumpflds; (size_t) (d - dumpflds) < ARRAY_SIZE(dumpflds); d++) {
	    if (!strncmp(ip, d->fldname, strlen(d->fldname))) {
		ip += strlen(d->fldname);
		while (isspace(*ip))
		    ip++;
		if (d->is_bool)
		    fields[d->fldno] = line;
		else if (*ip == '=') {
		    while (isspace(*++ip)) ;
		    fields[d->fldno] = ip;
		    while (isalnum(*ip))	/* 0x07FF */
			ip++;
		} else
		    errx(EXIT_FAILURE, _("input error: `=' expected after %s field"),
			  d->fldname);
		if (fno <= d->fldno)
		    fno = d->fldno + 1;
		if (*ip == 0)
		    return fno;
		if (*ip != ',' && *ip != ';')
		    errx(EXIT_FAILURE, _("input error: unexpected character %c after %s field"),
			  *ip, d->fldname);
		*ip = 0;
		goto nxtfld;
	    }
	}
	errx(EXIT_FAILURE, _("unrecognized input: %s"), ip);
    }

    /* split line into fields */
    lp = ip = line + 2;
    fields[fno++] = lp;
    while ((c = *ip++) != 0) {
	if (!lp[-1] && (c == '\t' || c == ' ')) ;
	else if (c == '\t' || c == ' ' || c == ',' || c == ';') {
	    *lp++ = 0;
	    if (fno < fieldssize)
		fields[fno++] = lp;
	    continue;
	} else
	    *lp++ = c;
    }

    if (lp == fields[fno - 1])
	fno--;
    return fno;
}

/* read a number, use default if absent */
/* a sign gives an offset from the default */
static int
get_ul(char *u, unsigned long *up, unsigned long def, int base) {
    char *nu;
    int sign = 0;
    unsigned long val;

    if (*u == '+') {
	sign = 1;
	u++;
    } else if (*u == '-') {
	sign = -1;
	u++;
    }
    if (*u) {
	errno = 0;
	val = strtoul(u, &nu, base);
	if (errno == ERANGE) {
	    warnx(_("number too big\n"));
	    return -1;
	}
	if (*nu) {
	    warnx(_("trailing junk after number\n"));
	    return -1;
	}
	if (sign == 1)
	    val = def + val;
	else if (sign == -1)
	    val = def - val;
	*up = val;
    } else
	*up = def;
    return 0;
}


/* read a number, use default if absent */
/* a sign gives an offset from the default */
static int
get_ull(char *u, unsigned long long *up, unsigned long long def, int base) {
    char *nu;
    int sign = 0;
    unsigned long long val;

    if (*u == '+') {
	sign = 1;
	u++;
    } else if (*u == '-') {
	sign = -1;
	u++;
    }
    if (*u) {
	errno = 0;
	val = strtoull(u, &nu, base);
	if (errno == ERANGE) {
	    warnx(_("number too big\n"));
	    return -1;
	}
	if (*nu) {
	    warnx(_("trailing junk after number\n"));
	    return -1;
	}
	if (sign == 1)
	    val = def + val;
	else if (sign == -1)
	    val = def - val;
	*up = val;
    } else
	*up = def;
    return 0;
}


/* There are two common ways to structure extended partitions:
   as nested boxes, and as a chain. Sometimes the partitions
   must be given in order. Sometimes all logical partitions
   must lie inside the outermost extended partition.
NESTED: every partition is contained in the surrounding partitions
   and is disjoint from all others.
CHAINED: every data partition is contained in the surrounding partitions
   and disjoint from all others, but extended partitions may lie outside
   (insofar as allowed by all_logicals_inside_outermost_extended).
ONESECTOR: all data partitions are mutually disjoint; extended partitions
   each use one sector only (except perhaps for the outermost one).
*/
int partitions_in_order = 0;
int all_logicals_inside_outermost_extended = 1;
enum { NESTED, CHAINED, ONESECTOR } boxes = NESTED;

/* find the default value for <start> - assuming entire units */
static unsigned long long
first_free(int pno, int is_extended, struct part_desc *ep, int format,
	   unsigned long long mid, struct disk_desc *z) {
    unsigned long long ff, fff;
    unsigned long unit = unitsize(format);
    struct part_desc *partitions = &(z->partitions[0]), *pp = 0;

    /* if containing ep undefined, look at its container */
    if (ep && ep->p.sys_type == EMPTY_PARTITION)
	ep = ep->ep;

    if (ep) {
	if (boxes == NESTED || (boxes == CHAINED && !is_extended))
	    pp = ep;
	else if (all_logicals_inside_outermost_extended)
	    pp = outer_extended_partition(ep);
    }
#if 0
    ff = pp ? (pp->start + unit - 1) / unit : 0;
#else
    /* rounding up wastes almost an entire cylinder - round down
       and leave it to compute_start_sect() to fix the difference */
    ff = pp ? pp->start / unit : 0;
#endif
    /* MBR and 1st sector of an extended partition are never free */
    if (unit == 1)
	ff++;

 again:
    for (pp = partitions; pp < partitions + pno; pp++) {
	if (!is_parent(pp, ep) && pp->size > 0) {
	    if ((partitions_in_order || pp->start / unit <= ff
		 || (mid && pp->start / unit <= mid))
		&& (fff = (pp->start + pp->size + unit - 1) / unit) > ff) {
		ff = fff;
		goto again;
	    }
	}
    }

    return ff;
}

/* find the default value for <size> - assuming entire units */
static unsigned long long
max_length(int pno, int is_extended, struct part_desc *ep, int format,
	   unsigned long long start, struct disk_desc *z) {
    unsigned long long fu;
    unsigned long unit = unitsize(format);
    struct part_desc *partitions = &(z->partitions[0]), *pp = 0;

    /* if containing ep undefined, look at its container */
    if (ep && ep->p.sys_type == EMPTY_PARTITION)
	ep = ep->ep;

    if (ep) {
	if (boxes == NESTED || (boxes == CHAINED && !is_extended))
	    pp = ep;
	else if (all_logicals_inside_outermost_extended)
	    pp = outer_extended_partition(ep);
    }
    fu = pp ? (pp->start + pp->size) / unit : get_disksize(format);

    for (pp = partitions; pp < partitions + pno; pp++)
	if (!is_parent(pp, ep) && pp->size > 0
	    && pp->start / unit >= start && pp->start / unit < fu)
	    fu = pp->start / unit;

    return (fu > start) ? fu - start : 0;
}

/* compute starting sector of a partition inside an extended one */
/* return 0 on failure */
/* ep is 0 or points to surrounding extended partition */
static int
compute_start_sect(struct part_desc *p, struct part_desc *ep) {
    unsigned long long base;
    int inc = (DOS && B.sectors) ? B.sectors : 1;
    int delta;

    if (ep && p->start + p->size >= ep->start + 1)
	delta = p->start - ep->start - inc;
    else if (p->start == 0 && p->size > 0)
	delta = -inc;
    else
	delta = 0;

    if (delta < 0) {
	unsigned long long old_size = p->size;
	p->start -= delta;
	p->size += delta;
	if (is_extended(p->p.sys_type) && boxes == ONESECTOR)
	    p->size = inc;
	else if ((ssize_t) old_size <= (ssize_t) - delta) {
	    my_warn(_("no room for partition descriptor\n"));
	    return 0;
	}
    }
    base = (!ep ? 0
	    : (is_extended(p->p.sys_type) ?
	       outer_extended_partition(ep) : ep)->start);
    p->ep = ep;
    if (p->p.sys_type == EMPTY_PARTITION && p->size == 0) {
	p->p.start_sect = 0;
	p->p.begin_chs = zero_chs;
	p->p.end_chs = zero_chs;
    } else {
	p->p.start_sect = p->start - base;
	p->p.begin_chs = ulong_to_chs(p->start, B);
	p->p.end_chs = ulong_to_chs(p->start + p->size - 1, B);
    }
    p->p.nr_sects = p->size;
    return 1;
}

/* build the extended partition surrounding a given logical partition */
static int
build_surrounding_extended(struct part_desc *p, struct part_desc *ep,
			   struct disk_desc *z) {
    int inc = (DOS && B.sectors) ? B.sectors : 1;
    int format = F_SECTOR;
    struct part_desc *p0 = &(z->partitions[0]), *eep = ep->ep;

    if (boxes == NESTED) {
	ep->start = first_free(ep - p0, 1, eep, format, p->start, z);
	ep->size = max_length(ep - p0, 1, eep, format, ep->start, z);
	if (ep->start > p->start || ep->start + ep->size < p->start + p->size) {
	    my_warn(_("cannot build surrounding extended partition\n"));
	    return 0;
	}
    } else {
	ep->start = p->start;
	if (boxes == CHAINED)
	    ep->size = p->size;
	else
	    ep->size = inc;
    }

    ep->p.nr_sects = ep->size;
    ep->p.bootable = 0;
    ep->p.sys_type = EXTENDED_PARTITION;
    if (!compute_start_sect(ep, eep) || !compute_start_sect(p, ep)) {
	ep->p.sys_type = EMPTY_PARTITION;
	ep->size = 0;
	return 0;
    }

    return 1;
}

static int
read_line(int pno, struct part_desc *ep, char *dev, int interactive,
	  struct disk_desc *z) {
    char line[1000];
    char *fields[11];
    int fno, pct = pno % 4;
    struct part_desc p, *orig;
    unsigned long long ff, ff1, ul, ml, ml1, def;
    int format, lpno, is_extd;

    if (eof || eob)
	return -1;

    lpno = index_to_linux(pno, z);

    if (interactive) {
	if (pct == 0 && (show_extended || pno == 0))
	    my_warn("\n");
	my_warn("%s:", partname(dev, lpno, 10));
    }

    /* read input line - skip blank lines when reading from a file */
    do {
	fno = read_stdin(fields, line, ARRAY_SIZE(fields), ARRAY_SIZE(line));
    } while (fno == RD_CMD || (fno == 0 && !interactive));
    if (fno == RD_EOF) {
	return -1;
    } else if (fno > 10 && *(fields[10]) != 0) {
	warnx(_("too many input fields\n"));
	return 0;
    }

    if (fno == 1 && !strcmp(fields[0], ".")) {
	eob = 1;
	return -1;
    }

    /* use specified format, but round to cylinders if F_MEGABYTE specified */
    format = 0;
    if (B.cylindersize && specified_format == F_MEGABYTE && !Linux)
	format = F_CYLINDER;

    orig = (one_only ? &(oldp.partitions[pno]) : 0);

    p = zero_part_desc;
    p.ep = ep;

    /* first read the type - we need to know whether it is extended */
    /* stop reading when input blank (defaults) and all is full */
    is_extd = 0;
    if (fno == 0) {		/* empty line */
	if (orig && is_extended(orig->p.sys_type))
	    is_extd = 1;
	ff = first_free(pno, is_extd, ep, format, 0, z);
	ml = max_length(pno, is_extd, ep, format, ff, z);
	if (ml == 0 && is_extd == 0) {
	    is_extd = 1;
	    ff = first_free(pno, is_extd, ep, format, 0, z);
	    ml = max_length(pno, is_extd, ep, format, ff, z);
	}
	if (ml == 0 && pno >= 4) {
	    /* no free blocks left - don't read any further */
	    my_warn(_("No room for more\n"));
	    return -1;
	}
    }
    if (fno < 3 || !*(fields[2]))
	ul = orig ? orig->p.sys_type :
	    (is_extd || (pno > 3 && pct == 1 && show_extended))
	    ? EXTENDED_PARTITION : LINUX_NATIVE;
    else if (!strcmp(fields[2], "L"))
	ul = LINUX_NATIVE;
    else if (!strcmp(fields[2], "S"))
	ul = LINUX_SWAP;
    else if (!strcmp(fields[2], "E"))
	ul = EXTENDED_PARTITION;
    else if (!strcmp(fields[2], "X"))
	ul = LINUX_EXTENDED;
    else if (get_ull(fields[2], &ul, LINUX_NATIVE, 16))
	return 0;
    if (ul > 255) {
	my_warn(_("Illegal type\n"));
	return 0;
    }
    p.p.sys_type = ul;
    is_extd = is_extended(ul);

    /* find start */
    ff = first_free(pno, is_extd, ep, format, 0, z);
    ff1 = ff * unitsize(format);
    def = orig ? orig->start : (pno > 4 && pct > 1) ? 0 : ff1;
    if (fno < 1 || !*(fields[0]))
	p.start = def;
    else {
	if (get_ull(fields[0], &ul, def / unitsize(0), 0))
	    return 0;
	p.start = ul * unitsize(0);
	p.start -= (p.start % unitsize(format));
    }

    /* find length */
    ml = max_length(pno, is_extd, ep, format, p.start / unitsize(format), z);
    ml1 = ml * unitsize(format);
    def = orig ? orig->size : (pno > 4 && pct > 1) ? 0 : ml1;
    if (fno < 2 || !*(fields[1]))
	p.size = def;
    else if (!strcmp(fields[1], "+"))
	p.size = ml1;
    else {
	if (get_ull(fields[1], &ul, def / unitsize(0), 0))
	    return 0;
	p.size = ul * unitsize(0) + unitsize(format) - 1;
	p.size -= (p.size % unitsize(format));
    }
    if (p.size > ml1) {
	my_warn(_("Warning: given size (%lu) exceeds max allowable size (%lu)\n"),
		(p.size + unitsize(0) - 1) / unitsize(0), ml1 / unitsize(0));
	if (!force)
	    return 0;
    }
    if (p.size == 0 && pno >= 4 && (fno < 2 || !*(fields[1]))) {
	my_warn(_("Warning: empty partition\n"));
	if (!force)
	    return 0;
    }
    p.p.nr_sects = p.size;

    if (p.size == 0 && !orig) {
	if (fno < 1 || !*(fields[0]))
	    p.start = 0;
	if (fno < 3 || !*(fields[2]))
	    p.p.sys_type = EMPTY_PARTITION;
    }

    if (p.start < ff1 && p.size > 0) {
	my_warn(_("Warning: bad partition start (earliest %lu)\n"),
		(ff1 + unitsize(0) - 1) / unitsize(0));
	if (!force)
	    return 0;
    }

    if (fno < 4 || !*(fields[3]))
	ul = (orig ? orig->p.bootable : 0);
    else if (!strcmp(fields[3], "-"))
	ul = 0;
    else if (!strcmp(fields[3], "*") || !strcmp(fields[3], "+"))
	ul = 0x80;
    else {
	my_warn(_("unrecognized bootable flag - choose - or *\n"));
	return 0;
    }
    p.p.bootable = ul;

    if (ep && ep->p.sys_type == EMPTY_PARTITION) {
	if (!build_surrounding_extended(&p, ep, z))
	    return 0;
    } else if (!compute_start_sect(&p, ep))
	return 0;

    {
	longchs aa = chs_to_longchs(p.p.begin_chs), bb;

	if (fno < 5) {
	    bb = aa;
	} else if (fno < 7) {
	    my_warn(_("partial c,h,s specification?\n"));
	    return 0;
	} else if (get_ul(fields[4], &bb.c, aa.c, 0) ||
		   get_ul(fields[5], &bb.h, aa.h, 0) ||
		   get_ul(fields[6], &bb.s, aa.s, 0))
	    return 0;
	p.p.begin_chs = longchs_to_chs(bb, B);
    }
    {
	longchs aa = chs_to_longchs(p.p.end_chs), bb;

	if (fno < 8) {
	    bb = aa;
	} else if (fno < 10) {
	    my_warn(_("partial c,h,s specification?\n"));
	    return 0;
	} else if (get_ul(fields[7], &bb.c, aa.c, 0) ||
		   get_ul(fields[8], &bb.h, aa.h, 0) ||
		   get_ul(fields[9], &bb.s, aa.s, 0))
	    return 0;
	p.p.end_chs = longchs_to_chs(bb, B);
    }

    if (pno > 3 && p.size && show_extended && p.p.sys_type != EMPTY_PARTITION
	&& (is_extended(p.p.sys_type) != (pct == 1))) {
	my_warn(_("Extended partition not where expected\n"));
	if (!force)
	    return 0;
    }

    z->partitions[pno] = p;
    if (pno >= z->partno)
	z->partno += 4;		/* reqd for out_partition() */

    if (interactive)
	out_partition(dev, 0, &(z->partitions[pno]), z, B);

    return 1;
}

/* ep either points to the extended partition to contain this one,
   or to the empty partition that may become extended or is 0 */
static int
read_partition(char *dev, int interactive, int pno, struct part_desc *ep,
	       struct disk_desc *z) {
    struct part_desc *p = &(z->partitions[pno]);
    int i;

    if (one_only) {
	*p = oldp.partitions[pno];
	if (one_only_pno != pno)
	    goto ret;
    } else if (!show_extended && pno > 4 && pno % 4)
	goto ret;

    while (!(i = read_line(pno, ep, dev, interactive, z)))
	if (!interactive)
	    errx(EXIT_FAILURE, _("bad input"));
    if (i < 0) {
	p->ep = ep;
	return 0;
    }

 ret:
    p->ep = ep;
    if (pno >= z->partno)
	z->partno += 4;
    return 1;
}

static void
read_partition_chain(char *dev, int interactive, struct part_desc *ep,
		     struct disk_desc *z) {
    int i;
    size_t base;

    eob = 0;
    while (1) {
	base = z->partno;
	if (base + 4 > ARRAY_SIZE(z->partitions)) {
	    warnx(_("too many partitions\n"));
	    break;
	}
	for (i = 0; i < 4; i++)
	    if (!read_partition(dev, interactive, base + i, ep, z))
		return;
	for (i = 0; i < 4; i++) {
	    ep = &(z->partitions[base + i]);
	    if (is_extended(ep->p.sys_type) && ep->size)
		break;
	}
	if (i == 4) {
	    /* nothing found - maybe an empty partition is going
	       to be extended */
	    if (one_only || show_extended)
		break;
	    ep = &(z->partitions[base + 1]);
	    if (ep->size || ep->p.sys_type != EMPTY_PARTITION)
		break;
	}
    }
}

static void
read_input(char *dev, int interactive, struct disk_desc *z) {
    size_t i;
    struct part_desc *partitions = &(z->partitions[0]), *ep;

    for (i = 0; i < ARRAY_SIZE(z->partitions); i++)
	partitions[i] = zero_part_desc;
    z->partno = 0;

    if (interactive)
	my_warn(_("Input in the following format; absent fields get a default value.\n"
		  "<start> <size> <type [E,S,L,X,hex]> <bootable [-,*]> <c,h,s> <c,h,s>\n"
		  "Usually you only need to specify <start> and <size> (and perhaps <type>).\n"));
    eof = 0;

    for (i = 0; i < 4; i++)
	read_partition(dev, interactive, i, 0, z);
    for (i = 0; i < 4; i++) {
	ep = partitions + i;
	if (is_extended(ep->p.sys_type) && ep->size)
	    read_partition_chain(dev, interactive, ep, z);
    }
    add_sector_and_offset(z);
}

/*
 *  G. The command line
 */
static void usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
		_(" %s [options] <device> [...]\n"),  program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -s, --show-size           list size of a partition\n"
		" -c, --id                  change or print partition Id\n"
		"     --change-id           change Id\n"
		"     --print-id            print Id\n"), out);
	fputs(_(" -l, --list                list partitions of each device\n"
		" -d, --dump                idem, but in a format suitable for later input\n"
		" -i, --increment           number cylinders etc. from 1 instead of from 0\n"
		" -u, --unit <letter>       units to be used; <letter> can be one of\n"
		"                             S (sectors), C (cylinders), B (blocks), or M (MB)\n"), out);
	fputs(_(" -1, --one-only            reserved option that does nothing currently\n"
		" -T, --list-types          list the known partition types\n"
		" -D, --DOS                 for DOS-compatibility: waste a little space\n"
		" -E, --DOS-extended        DOS extended partition compatibility\n"
		" -R, --re-read             make the kernel reread the partition table\n"), out);
	fputs(_(" -N <number>               change only the partition with this <number>\n"
		" -n                        do not actually write to disk\n"
		" -O <file>                 save the sectors that will be overwritten to <file>\n"
		" -I <file>                 restore sectors from <file>\n"), out);
	fputs(_(" -V, --verify              check that the listed partitions are reasonable\n"
		" -v, --version             display version information and exit\n"
		" -h, --help                display this help text and exit\n"), out);

	fputs(_("\nDangerous options:\n"), out);
	fputs(_(" -f, --force               disable all consistency checking\n"
		"     --no-reread           do not check whether the partition is in use\n"
		" -q, --quiet               suppress warning messages\n"
		" -L, --Linux               do not complain about things irrelevant for Linux\n"), out);
	fputs(_(" -g, --show-geometry       print the kernel's idea of the geometry\n"
		" -G, --show-pt-geometry    print geometry guessed from the partition table\n"), out);
	fputs(_(" -A, --activate[=<device>] activate bootable flag\n"
		" -U, --unhide[=<dev>]      set partition unhidden\n"
		" -x, --show-extended       also list extended partitions in the output,\n"
		"                             or expect descriptors for them in the input\n"), out);
	fputs(_("     --leave-last          do not allocate the last cylinder\n"
		"     --IBM                 same as --leave-last\n"), out);
	fputs(_("     --in-order            partitions are in order\n"
		"     --not-in-order        partitions are not in order\n"
		"     --inside-outer        all logicals inside outermost extended\n"
		"     --not-inside-outer    not all logicals inside outermost extended\n"), out);
	fputs(_("     --nested              every partition is disjoint from all others\n"
		"     --chained             like nested, but extended partitions may lie outside\n"
		"     --onesector           partitions are mutually disjoint\n"), out);

	fputs(_("\nOverride the detected geometry using:\n"
		" -C, --cylinders <number>  set the number of cylinders to use\n"
		" -H, --heads <number>      set the number of heads to use\n"
		" -S, --sectors <number>    set the number of sectors to use\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
activate_usage(char *progn) {
    puts(_("Usage:"));
    printf(_("%s device		 list active partitions on device\n"), progn);
    printf(_("%s device n1 n2 ... activate partitions n1 ..., inactivate the rest\n"),
	   progn);
    printf(_("%s -An device	 activate partition n, inactivate the other ones\n"),
	   PROGNAME);
    exit(1);
}

static void
unhide_usage(char *progn __attribute__ ((__unused__))) {
    exit(1);
}

static const char short_opts[] = "cdfghilnqsu:vx1A::C:DGH:I:LN:O:RS:TU::V";

#define PRINT_ID 0400
#define CHANGE_ID 01000

enum {
    OPT_NO_REREAD = CHAR_MAX + 1,
    OPT_LEAVE_LAST,
    OPT_IN_ORDER,
    OPT_NOT_IN_ORDER,
    OPT_INSIDE_OUTER,
    OPT_NOT_INSIDE_OUTER,
    OPT_NESTED,
    OPT_CHAINED,
    OPT_ONESECTOR
};

static const struct option long_opts[] = {
    { "change-id",        no_argument, NULL, 'c' + CHANGE_ID },
    { "print-id",         no_argument, NULL, 'c' + PRINT_ID },
    { "id",               no_argument, NULL, 'c' },
    { "dump",             no_argument, NULL, 'd' },
    { "force",            no_argument, NULL, 'f' },
    { "show-geometry",    no_argument, NULL, 'g' },
    { "help",             no_argument, NULL, 'h' },
    { "increment",        no_argument, NULL, 'i' },
    { "list",             no_argument, NULL, 'l' },
    { "quiet",            no_argument, NULL, 'q' },
    { "show-size",        no_argument, NULL, 's' },
    { "unit",             required_argument, NULL, 'u' },
    { "version",          no_argument, NULL, 'v' },
    { "show-extended",    no_argument, NULL, 'x' },
    { "one-only",         no_argument, NULL, '1' },
    { "cylinders",        required_argument, NULL, 'C' },
    { "heads",            required_argument, NULL, 'H' },
    { "sectors",          required_argument, NULL, 'S' },
    { "show-pt-geometry", no_argument, NULL, 'G' },
    { "activate",         optional_argument, NULL, 'A' },
    { "DOS",              no_argument, NULL, 'D' },
    { "DOS-extended",     no_argument, NULL, 'E' },
    { "Linux",            no_argument, NULL, 'L' },
    { "re-read",          no_argument, NULL, 'R' },
    { "list-types",       no_argument, NULL, 'T' },
    { "unhide",           optional_argument, NULL, 'U' },
    { "no-reread",        no_argument, NULL, OPT_NO_REREAD },
    { "IBM",              no_argument, NULL, OPT_LEAVE_LAST },
    { "leave-last",       no_argument, NULL, OPT_LEAVE_LAST },
/* dangerous flags - not all completely implemented */
    { "in-order",         no_argument, NULL, OPT_IN_ORDER },
    { "not-in-order",     no_argument, NULL, OPT_NOT_IN_ORDER },
    { "inside-outer",     no_argument, NULL, OPT_INSIDE_OUTER },
    { "not-inside-outer", no_argument, NULL, OPT_NOT_INSIDE_OUTER },
    { "nested",           no_argument, NULL, OPT_NESTED },
    { "chained",          no_argument, NULL, OPT_CHAINED },
    { "onesector",        no_argument, NULL, OPT_ONESECTOR },
    { NULL, 0, NULL, 0 }
};

static int is_ide_cdrom_or_tape(char *device)
{
	int fd, ret;

	if ((fd = open(device, O_RDONLY)) < 0)
		return 0;
	ret = blkdev_is_cdrom(fd);

	close(fd);
	return ret;
}

static char *
nextproc(FILE * procf) {
    static char devname[256];
    char line[1024], ptname[128];
    int ma, mi;
    unsigned long long sz;

    if (procf == NULL)
	return NULL;
    while (fgets(line, sizeof(line), procf) != NULL) {
	if (sscanf(line, " %d %d %llu %128[^\n ]", &ma, &mi, &sz, ptname) != 4)
	    continue;
	snprintf(devname, sizeof(devname), "/dev/%s", ptname);
	if (!is_whole_disk(devname))
	    continue;
	return canonicalize_path(devname);
    }

    return NULL;
}

static void
gpt_warning(char *dev, int warn_only) {
    if (force)
	warn_only = 1;

    if (dev && gpt_probe_signature_devname(dev)) {
	fflush(stdout);
	fprintf(stderr,
		_("\nWARNING: GPT (GUID Partition Table) detected on '%s'! "
		  "The util sfdisk doesn't support GPT. Use GNU Parted.\n\n"),
		dev);
	if (!warn_only) {
	    fprintf(stderr,
		    _("Use the --force flag to overrule this check.\n"));
	    exit(1);
	}
    }
}

static void do_list(char *dev, int silent);
static void do_size(char *dev, int silent);
static void do_geom(char *dev, int silent);
static void do_pt_geom(char *dev, int silent);
static void do_fdisk(char *dev);
static void do_reread(char *dev);
static void do_change_id(char *dev, char *part, char *id);
static void do_unhide(char **av, int ac, char *arg);
static void do_activate(char **av, int ac, char *arg);

unsigned long long total_size;

int
main(int argc, char **argv) {
    char *progn;
    int c;
    char *dev;
    int opt_size = 0;
    int opt_out_geom = 0;
    int opt_out_pt_geom = 0;
    int opt_reread = 0;
    int activate = 0;
    int do_id = 0;
    int unhide = 0;
    int fdisk = 0;
    char *activatearg = 0;
    char *unhidearg = 0;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    if (argc < 1)
	errx(EXIT_FAILURE, _("no command?"));
    if ((progn = strrchr(argv[0], '/')) == NULL)
	progn = argv[0];
    else
	progn++;
    if (!strcmp(progn, "activate"))
	activate = 1;		/* equivalent to `sfdisk -A' */
#if 0				/* not important enough to deserve a name */
    else if (!strcmp(progn, "unhide"))
	unhide = 1;		/* equivalent to `sfdisk -U' */
#endif
    else
	fdisk = 1;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
	switch (c) {
	case 'f':
	    force = 1;
	    break;		/* does not imply quiet */
	case 'g':
	    opt_out_geom = 1;
	    break;
	case 'G':
	    opt_out_pt_geom = 1;
	    break;
	case 'i':
	    increment = 1;
	    break;
	case 'c':
	case 'c' + PRINT_ID:
	case 'c' + CHANGE_ID:
	    do_id = c;
	    break;
	case 'd':
	    dump = 1;		/* fall through */
	case 'l':
	    opt_list = 1;
	    break;
	case 'n':
	    no_write = 1;
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 's':
	    opt_size = 1;
	    break;
	case 'u':
	    set_format(*optarg);
	    break;
	case 'v':
	    printf(_("%s from %s\n"), program_invocation_short_name,
		   PACKAGE_STRING);
	    return EXIT_SUCCESS;
	case 'h':
	    usage(stdout);
	    return EXIT_SUCCESS;
	case 'x':
	    show_extended = 1;
	    break;
	case 'A':
	    activatearg = optarg;
	    activate = 1;
	    break;
	case 'C':
	    U.cylinders = atoi(optarg);
	    break;
	case 'D':
	    DOS = 1;
	    break;
	case 'E':
	    DOS_extended = 1;
	    break;
	case 'H':
	    U.heads = atoi(optarg);
	    break;
	case 'L':
	    Linux = 1;
	    break;
	case 'N':
	    one_only = atoi(optarg);
	    break;
	case 'I':
	    restore_sector_file = optarg;
	    break;
	case 'O':
	    save_sector_file = optarg;
	    break;
	case 'R':
	    opt_reread = 1;
	    break;
	case 'S':
	    U.sectors = atoi(optarg);
	    break;
	case 'T':
	    list_types();
	    exit(0);
	case 'U':
	    unhidearg = optarg;
	    unhide = 1;
	    break;
	case 'V':
	    verify = 1;
	    break;
	default:
	    usage(stderr);
	    break;

	    /* dangerous flags */
	case OPT_IN_ORDER:
	    partitions_in_order = 1;
	    break;
	case OPT_NOT_IN_ORDER:
	    partitions_in_order = 0;
	    break;
	case OPT_INSIDE_OUTER:
	    all_logicals_inside_outermost_extended = 1;
	    break;
	case OPT_NOT_INSIDE_OUTER:
	    all_logicals_inside_outermost_extended = 0;
	    break;
	case OPT_NESTED:
	    boxes = NESTED;
	    break;
	case OPT_CHAINED:
	    boxes = CHAINED;
	    break;
	case OPT_ONESECTOR:
	    boxes = ONESECTOR;
	    break;

	    /* more flags */
	case OPT_NO_REREAD:
	    no_reread = 1;
	    break;
	case OPT_LEAVE_LAST:
	    leave_last = 1;
	    break;
	}
    }

    if (optind == argc &&
	(opt_list || opt_out_geom || opt_out_pt_geom || opt_size || verify)) {
	FILE *procf;

	/* try all known devices */
	total_size = 0;

	procf = fopen(_PATH_PROC_PARTITIONS, "r");
	if (!procf)
	    fprintf(stderr, _("cannot open %s\n"), _PATH_PROC_PARTITIONS);
	else {
	    while ((dev = nextproc(procf)) != NULL) {
		if (!is_ide_cdrom_or_tape(dev)) {
		    gpt_warning(dev, 1);
		    if (opt_out_geom)
			do_geom(dev, 1);
		    if (opt_out_pt_geom)
			do_pt_geom(dev, 1);
		    if (opt_size)
			do_size(dev, 1);
		    if (opt_list || verify)
			do_list(dev, 1);
		}
		free(dev);
	    }
	    fclose(procf);
	}

	if (opt_size)
	    printf(_("total: %llu blocks\n"), total_size);

	exit(exit_status);
    }

    if (optind == argc) {
	if (activate)
	    activate_usage(fdisk ? "sfdisk -A" : progn);
	else if (unhide)
	    unhide_usage(fdisk ? "sfdisk -U" : progn);
	else
	    usage(stderr);
    }

    if (opt_list || opt_out_geom || opt_out_pt_geom || opt_size || verify) {
	while (optind < argc) {
	    gpt_warning(argv[optind], 1);
	    if (opt_out_geom)
		do_geom(argv[optind], 0);
	    if (opt_out_pt_geom)
		do_pt_geom(argv[optind], 0);
	    if (opt_size)
		do_size(argv[optind], 0);
	    if (opt_list || verify)
		do_list(argv[optind], 0);
	    optind++;
	}
	exit(exit_status);
    }

    if (optind != argc - 1)
	gpt_warning(argv[optind], 0);

    if (activate) {
	do_activate(argv + optind, argc - optind, activatearg);
	exit(exit_status);
    }
    if (unhide) {
	do_unhide(argv + optind, argc - optind, unhidearg);
	exit(exit_status);
    }
    if (do_id) {
	if ((do_id & PRINT_ID) != 0 && optind != argc - 2)
	    errx(EXIT_FAILURE, _("usage: sfdisk --print-id device partition-number"));
	else if ((do_id & CHANGE_ID) != 0 && optind != argc - 3)
	    errx(EXIT_FAILURE, _("usage: sfdisk --change-id device partition-number Id"));
	else if (optind != argc - 3 && optind != argc - 2)
	    errx(EXIT_FAILURE, _("usage: sfdisk --id device partition-number [Id]"));
	do_change_id(argv[optind], argv[optind + 1],
		     (optind == argc - 2) ? 0 : argv[optind + 2]);
	exit(exit_status);
    }

    if (optind != argc - 1)
	errx(EXIT_FAILURE, _("can specify only one device (except with -l or -s)"));
    dev = argv[optind];

    if (opt_reread)
	do_reread(dev);
    else if (restore_sector_file)
	restore_sectors(dev);
    else
	do_fdisk(dev);

    return 0;
}

/*
 *  H. Listing the current situation
 */

static int
my_open(char *dev, int rw, int silent) {
    int fd, mode;

    mode = (rw ? O_RDWR : O_RDONLY);
    fd = open(dev, mode);
    if (fd < 0 && !silent) {
	perror(dev);
	if (rw)
	    errx(EXIT_FAILURE, _("cannot open %s read-write"), dev);
	else
	    errx(EXIT_FAILURE, _("cannot open %s for reading"), dev);
    }
    return fd;
}

static void
do_list(char *dev, int silent) {
    int fd;
    struct disk_desc *z;

    fd = my_open(dev, 0, silent);
    if (fd < 0)
	return;

    z = &oldp;

    free_sectors();
    get_cylindersize(dev, fd, dump ? 1 : opt_list ? 0 : 1);
    get_partitions(dev, fd, z);

    if (opt_list)
	out_partitions(dev, z);

    if (verify) {
	if (partitions_ok(fd, z))
	    my_warn(_("%s: OK\n"), dev);
	else
	    exit_status = 1;
    }

    close(fd);
}

static void
do_geom(char *dev, int silent) {
    int fd;
    struct geometry R;

    fd = my_open(dev, 0, silent);
    if (fd < 0)
	return;

    R = get_geometry(dev, fd, silent);
    if (R.cylinders)
	printf(_("%s: %ld cylinders, %ld heads, %ld sectors/track\n"),
	       dev, R.cylinders, R.heads, R.sectors);

    close(fd);
}

static void
do_pt_geom(char *dev, int silent) {
    int fd;
    struct disk_desc *z;
    struct geometry R;

    fd = my_open(dev, 0, silent);
    if (fd < 0)
	return;

    z = &oldp;

    free_sectors();
    get_cylindersize(dev, fd, 1);
    get_partitions(dev, fd, z);

    R = B;

    if (z->partno != 0 && get_fdisk_geometry(z)) {
	R.heads = F.heads;
	R.sectors = F.sectors;
	R.cylindersize = R.heads * R.sectors;
	R.cylinders = (R.cylindersize == 0) ? 0 : R.total_size / R.cylindersize;
    }

    if (R.cylinders)
	printf(_("%s: %ld cylinders, %ld heads, %ld sectors/track\n"),
	       dev, R.cylinders, R.heads, R.sectors);

    close(fd);
}

/* for compatibility with earlier fdisk: provide option -s */
static void
do_size(char *dev, int silent) {
    int fd;
    unsigned long long size;

    fd = my_open(dev, 0, silent);
    if (fd < 0)
	return;

    if (blkdev_get_sectors(fd, &size) == -1) {
	if (!silent) {
	    perror(dev);
	    errx(EXIT_FAILURE, _("Cannot get size of %s"), dev);
	}
	goto done;
    }

    size /= 2;			/* convert sectors to blocks */

    /* a CDROM drive without mounted CD yields MAXINT */
    if (silent && size == ((1 << 30) - 1))
	goto done;

    if (silent)
	printf("%s: %9llu\n", dev, size);
    else
	printf("%llu\n", size);

    total_size += size;

done:
    close(fd);
}

/*
 * Activate: usually one wants to have a single primary partition
 * to be active. OS/2 fdisk makes non-bootable logical partitions
 * active - I don't know what that means to OS/2 Boot Manager.
 *
 * Call: activate /dev/hda 2 5 7	make these partitions active
 *					and the remaining ones inactive
 * Or:   sfdisk -A /dev/hda 2 5 7
 *
 * If only a single partition must be active, one may also use the form
 *       sfdisk -A2 /dev/hda
 *
 * With "activate /dev/hda" or "sfdisk -A /dev/hda" the active partitions
 * are listed but not changed. To get zero active partitions, use
 * "activate /dev/hda none" or "sfdisk -A /dev/hda none".
 * Use something like `echo ",,,*" | sfdisk -N2 /dev/hda' to only make
 * /dev/hda2 active, without changing other partitions.
 *
 * A warning will be given if after the change not precisely one primary
 * partition is active.
 *
 * The present syntax was chosen to be (somewhat) compatible with the
 * activate from the LILO package.
 */
static void
set_active(struct disk_desc *z, char *pnam) {
    int pno;

    pno = asc_to_index(pnam, z);
    if (z->partitions[pno].ptype == DOS_TYPE)
	z->partitions[pno].p.bootable = 0x80;
}

static void
do_activate(char **av, int ac, char *arg) {
    char *dev = av[0];
    int fd;
    int rw, i, pno, lpno;
    struct disk_desc *z;

    z = &oldp;

    rw = (!no_write && (arg || ac > 1));
    fd = my_open(dev, rw, 0);

    free_sectors();
    get_cylindersize(dev, fd, 1);
    get_partitions(dev, fd, z);

    if (!arg && ac == 1) {
	/* list active partitions */
	for (pno = 0; pno < z->partno; pno++) {
	    if (z->partitions[pno].p.bootable) {
		lpno = index_to_linux(pno, z);
		if (pno == linux_to_index(lpno, z))
		    printf("%s\n", partname(dev, lpno, 0));
		else
		    printf("%s#%d\n", dev, pno);
		if (z->partitions[pno].p.bootable != 0x80)
		    my_warn(_("bad active byte: 0x%x instead of 0x80\n"),
			    z->partitions[pno].p.bootable);
	    }
	}
    } else {
	/* clear `active byte' everywhere */
	for (pno = 0; pno < z->partno; pno++)
	    if (z->partitions[pno].ptype == DOS_TYPE)
		z->partitions[pno].p.bootable = 0;

	/* then set where desired */
	if (ac == 1)
	    set_active(z, arg);
	else
	    for (i = 1; i < ac; i++)
		set_active(z, av[i]);

	/* then write to disk */
	if (write_partitions(dev, fd, z))
	    my_warn(_("Done\n\n"));
	else
	    exit_status = 1;
    }
    i = 0;
    for (pno = 0; pno < z->partno && pno < 4; pno++)
	if (z->partitions[pno].p.bootable)
	    i++;
    if (i != 1)
	my_warn(_("You have %d active primary partitions. This does not matter for LILO,\n"
		 "but the DOS MBR will only boot a disk with 1 active partition.\n"),
		i);

    close(fd);
}

static void
set_unhidden(struct disk_desc *z, char *pnam) {
    int pno;
    unsigned char id;

    pno = asc_to_index(pnam, z);
    id = z->partitions[pno].p.sys_type;
    if (id == 0x11 || id == 0x14 || id == 0x16 || id == 0x17)
	id -= 0x10;
    else
	errx(EXIT_FAILURE, _("partition %s has id %x and is not hidden"), pnam, id);
    z->partitions[pno].p.sys_type = id;
}

/*
 * maybe remove and make part of --change-id
 */
static void
do_unhide(char **av, int ac, char *arg) {
    char *dev = av[0];
    int fd, rw, i;
    struct disk_desc *z;

    z = &oldp;

    rw = !no_write;
    fd = my_open(dev, rw, 0);

    free_sectors();
    get_cylindersize(dev, fd, 1);
    get_partitions(dev, fd, z);

    /* unhide where desired */
    if (ac == 1)
	set_unhidden(z, arg);
    else
	for (i = 1; i < ac; i++)
	    set_unhidden(z, av[i]);

    /* then write to disk */
    if (write_partitions(dev, fd, z))
	my_warn(_("Done\n\n"));
    else
	exit_status = 1;

    close(fd);
}

static void
do_change_id(char *dev, char *pnam, char *id) {
    int fd, rw, pno;
    struct disk_desc *z;
    unsigned long i;

    z = &oldp;

    rw = !no_write;
    fd = my_open(dev, rw, 0);

    free_sectors();
    get_cylindersize(dev, fd, 1);
    get_partitions(dev, fd, z);

    pno = asc_to_index(pnam, z);
    if (id == 0) {
	printf("%x\n", z->partitions[pno].p.sys_type);
	goto done;
    }
    i = strtoul(id, NULL, 16);
    if (i > 255)
	errx(EXIT_FAILURE, _("Bad Id %lx"), i);
    z->partitions[pno].p.sys_type = i;

    if (write_partitions(dev, fd, z))
	my_warn(_("Done\n\n"));
    else
	exit_status = 1;

done:
    close(fd);
}

static void
do_reread(char *dev) {
    int fd;

    fd = my_open(dev, 0, 0);
    if (reread_ioctl(fd)) {
	warnx(_("This disk is currently in use.\n"));
	exit(1);
    }

    close(fd);
}

/*
 *  I. Writing the new situation
 */

static void
do_fdisk(char *dev) {
    int fd;
    int c, answer;
    struct stat statbuf;
    int interactive = isatty(0);
    struct disk_desc *z;

    if (stat(dev, &statbuf) < 0) {
	perror(dev);
	errx(EXIT_FAILURE, _("Fatal error: cannot find %s"), dev);
    }
    if (!S_ISBLK(statbuf.st_mode)) {
	warnx(_("Warning: %s is not a block device\n"), dev);
	no_reread = 1;
    }
    fd = my_open(dev, !no_write, 0);

    if (!no_write && !no_reread) {
	my_warn(_("Checking that no-one is using this disk right now ...\n"));
	if (reread_ioctl(fd)) {
	    warnx(_("\nThis disk is currently in use - repartitioning is probably a bad idea.\n"
		      "Umount all file systems, and swapoff all swap partitions on this disk.\n"
		      "Use the --no-reread flag to suppress this check.\n"));
	    if (!force) {
		warnx(_("Use the --force flag to overrule all checks.\n"));
		exit(1);
	    }
	} else
	    my_warn(_("OK\n"));
    }

    z = &oldp;

    free_sectors();
    get_cylindersize(dev, fd, 0);
    get_partitions(dev, fd, z);

    printf(_("Old situation:\n"));
    out_partitions(dev, z);

    if (one_only && (one_only_pno = linux_to_index(one_only, z)) < 0)
        errx(EXIT_FAILURE, _("Partition %d does not exist, cannot change it"), one_only);

    z = &newp;

    while (1) {

	read_input(dev, interactive, z);

	printf(_("New situation:\n"));
	out_partitions(dev, z);

	if (!partitions_ok(fd, z) && !force) {
	    if (!interactive)
		errx(EXIT_FAILURE, _("I don't like these partitions - nothing changed.\n"
					 "(If you really want this, use the --force option.)"));
	    else
		warnx(_("I don't like this - probably you should answer No\n"));
	}
 ask:
	if (interactive) {
	    if (no_write)
		printf(_("Are you satisfied with this? [ynq] "));
	    else
		printf(_("Do you want to write this to disk? [ynq] "));
	    answer = c = getchar();
	    while (c != '\n' && c != EOF)
		c = getchar();
	    if (c == EOF)
		printf(_("\nsfdisk: premature end of input\n"));
	    if (c == EOF || answer == 'q' || answer == 'Q') {
		errx(EXIT_FAILURE, _("Quitting - nothing changed"));
	    } else if (answer == 'n' || answer == 'N') {
		continue;
	    } else if (answer == 'y' || answer == 'Y') {
		break;
	    } else {
		printf(_("Please answer one of y,n,q\n"));
		goto ask;
	    }
	} else
	    break;
    }

    if (write_partitions(dev, fd, z))
	printf(_("Successfully wrote the new partition table\n\n"));
    else
	exit_status = 1;

    if (!reread_disk_partition(dev, fd)) {	/* close fd on success */
	close(fd);
	exit_status = 1;
    }
    my_warn(_("If you created or changed a DOS partition, /dev/foo7, say, then use dd(1)\n"
	      "to zero the first 512 bytes:  dd if=/dev/zero of=/dev/foo7 bs=512 count=1\n"
	      "(See fdisk(8).)\n"));

    sync();			/* superstition */
    exit(exit_status);
}

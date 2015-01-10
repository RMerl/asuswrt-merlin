/****************************************************************************
 *
 *     CFDISK
 *
 * cfdisk is a curses based disk drive partitioning program that can
 * create partitions for a wide variety of operating systems including
 * Linux, MS-DOS and OS/2.
 *
 * cfdisk was inspired by the fdisk program, by A. V. Le Blanc
 * (LeBlanc@mcc.ac.uk).
 *
 *     Copyright (C) 1994 Kevin E. Martin (martin@cs.unc.edu)
 *
 * cfdisk is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cfdisk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cfdisk; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Created:	Fri Jan 28 22:46:58 1994, martin@cs.unc.edu
 * >2GB patches: Sat Feb 11 09:08:10 1995, faith@cs.unc.edu
 * Prettier menus: Sat Feb 11 09:08:25 1995, Janne Kukonlehto
 *                                           <jtklehto@stekt.oulu.fi>
 * Versions 0.8e-p: aeb@cwi.nl
 * Rebaptised 2.9p, following util-linux versioning.
 *
 *  Recognition of NTFS / HPFS difference inspired by patches
 *  from Marty Leisner <leisner@sdsp.mc.xerox.com>
 *  Exit codes by Enrique Zanardi <ezanardi@ull.es>:
 *     0: all went well
 *     1: command line error, out of memory
 *     2: hardware problems [Cannot open/seek/read/write disk drive].
 *     3: ioctl(fd, HDIO_GETGEO,...) failed. (Probably it is not a disk.)
 *     4: bad partition table on disk. [Bad primary/logical partition].
 *
 * Sat, 23 Jan 1999 19:34:45 +0100 <Vincent.Renardias@ldsol.com>
 *  Internationalized + provided initial French translation.
 * Sat Mar 20 09:26:34 EST 1999 <acme@conectiva.com.br>
 *  Some more i18n.
 * Sun Jul 18 03:19:42 MEST 1999 <aeb@cwi.nl>
 *  Terabyte-sized disks.
 * Sat Jun 30 05:23:19 EST 2001 <nathans@sgi.com>
 *  XFS label recognition.
 * Thu Nov 22 15:42:56 CET 2001 <flavio.stanchina@tin.it>
 *  ext3 and ReiserFS recognition.
 * Sun Oct 12 17:43:43 CEST 2003 <flavio.stanchina@tin.it>
 *  JFS recognition; ReiserFS label recognition.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>

#ifdef HAVE_SLANG_H
#include <slang.h>
#elif defined(HAVE_SLANG_SLANG_H)
#include <slang/slang.h>
#endif

#ifdef HAVE_SLCURSES_H
#include <slcurses.h>
#elif defined(HAVE_SLANG_SLCURSES_H)
#include <slang/slcurses.h>
#elif defined(HAVE_NCURSESW_NCURSES_H) && defined(HAVE_WIDECHAR)
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#include <signal.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef HAVE_LIBBLKID
#include <blkid.h>
#endif

#ifdef HAVE_WIDECHAR
#include <wctype.h>
#endif

#include "nls.h"
#include "rpmatch.h"
#include "blkdev.h"
#include "strutils.h"
#include "common.h"
#include "gpt.h"
#include "mbsalign.h"
#include "widechar.h"

#ifdef __GNU__
#define DEFAULT_DEVICE "/dev/hd0"
#define ALTERNATE_DEVICE "/dev/sd0"
#elif defined(__FreeBSD__)
#define DEFAULT_DEVICE "/dev/ad0"
#define ALTERNATE_DEVICE "/dev/da0"
#else
#define DEFAULT_DEVICE "/dev/hda"
#define ALTERNATE_DEVICE "/dev/sda"
#endif

/* With K=1024 we have `binary' megabytes, gigabytes, etc.
   Some misguided hackers like that.
   With K=1000 we have MB and GB that follow the standards
   [SI, ATA, IEEE etc] and the disk manufacturers and the law. */
#define K	1000

#define LINE_LENGTH 80
#define MAXIMUM_PARTS 60

#define SECTOR_SIZE 512

#define MAX_HEADS 256
#define MAX_SECTORS 63

#define ACTIVE_FLAG 0x80
#define PART_TABLE_FLAG0 0x55
#define PART_TABLE_FLAG1 0xAA

#define UNUSABLE -1
#define FREE_SPACE     0x00
#define DOS_EXTENDED   0x05
#define OS2_OR_NTFS    0x07
#define WIN98_EXTENDED 0x0f
#define LINUX_EXTENDED 0x85
#define LINUX_MINIX    0x81
#define LINUX_SWAP     0x82
#define LINUX          0x83

#define PRI_OR_LOG -1
#define PRIMARY -2
#define LOGICAL -3

#define COL_ID_WIDTH 25

#define ESC '\033'
#define DEL '\177'
#define BELL '\007'
#define REDRAWKEY '\014'	/* ^L */

/* Display units */
#define GIGABYTES 1
#define MEGABYTES 2
#define SECTORS 3
#define CYLINDERS 4

#define GS_DEFAULT -1
#define GS_ESCAPE -2

#define PRINT_RAW_TABLE 1
#define PRINT_SECTOR_TABLE 2
#define PRINT_PARTITION_TABLE 4

#define IS_PRIMARY(p) ((p) >= 0 && (p) < 4)
#define IS_LOGICAL(p) ((p) > 3)

#define round_int(d) ((double)((int)(d+0.5)))
#define ceiling(d) ((double)(((d) != (int)(d)) ? (int)(d+1.0) : (int)(d)))

struct partition {
        unsigned char boot_ind;         /* 0x80 - active */
        unsigned char head;             /* starting head */
        unsigned char sector;           /* starting sector */
        unsigned char cyl;              /* starting cylinder */
        unsigned char sys_ind;          /* What partition type */
        unsigned char end_head;         /* end head */
        unsigned char end_sector;       /* end sector */
        unsigned char end_cyl;          /* end cylinder */
        unsigned char start4[4];        /* starting sector counting from 0 */
        unsigned char size4[4];         /* nr of sectors in partition */
};

int heads = 0;
int sectors = 0;
long long cylinders = 0;
int cylinder_size = 0;		/* heads * sectors */
long long actual_size = 0;	/* (in 512-byte sectors) - set using ioctl */
				/* explicitly given user values */
int user_heads = 0, user_sectors = 0;
long long user_cylinders = 0;
				/* kernel values; ignore the cylinders */
int kern_heads = 0, kern_sectors = 0;
				/* partition-table derived values */
int pt_heads = 0, pt_sectors = 0;


static void
set_hsc0(unsigned char *h, unsigned char *s, int *c, long long sector) {
	*s = sector % sectors + 1;
	sector /= sectors;
	*h = sector % heads;
	sector /= heads;
	*c = sector;
}

static void
set_hsc(unsigned char *h, unsigned char *s, unsigned char *c,
	long long sector) {
	int cc;

	if (sector >= 1024*cylinder_size)
		sector = 1024*cylinder_size - 1;
	set_hsc0(h, s, &cc, sector);
	*c = cc & 0xFF;
	*s |= (cc >> 2) & 0xC0;
}

static void
set_hsc_begin(struct partition *p, long long sector) {
	set_hsc(& p->head, & p->sector, & p->cyl, sector);
}

static void
set_hsc_end(struct partition *p, long long sector) {
	set_hsc(& p->end_head, & p->end_sector, & p->end_cyl, sector);
}

#define is_extended(x)	((x) == DOS_EXTENDED || (x) == WIN98_EXTENDED || \
			 (x) == LINUX_EXTENDED)

/* start_sect and nr_sects are stored little endian on all machines */
/* moreover, they are not aligned correctly */
static void
store4_little_endian(unsigned char *cp, unsigned int val) {
	cp[0] = (val & 0xff);
	cp[1] = ((val >> 8) & 0xff);
	cp[2] = ((val >> 16) & 0xff);
	cp[3] = ((val >> 24) & 0xff);
}

static unsigned int
read4_little_endian(unsigned char *cp) {
	return (unsigned int)(cp[0]) + ((unsigned int)(cp[1]) << 8)
		+ ((unsigned int)(cp[2]) << 16)
		+ ((unsigned int)(cp[3]) << 24);
}

static void
set_start_sect(struct partition *p, unsigned int start_sect) {
	store4_little_endian(p->start4, start_sect);
}

static unsigned int
get_start_sect(struct partition *p) {
	return read4_little_endian(p->start4);
}

static void
set_nr_sects(struct partition *p, unsigned int nr_sects) {
	store4_little_endian(p->size4, nr_sects);
}

static unsigned int
get_nr_sects(struct partition *p) {
	return read4_little_endian(p->size4);
}

#define ALIGNMENT 2
typedef union {
    struct {
	unsigned char align[ALIGNMENT];
	unsigned char b[SECTOR_SIZE];
    } c;
    struct {
	unsigned char align[ALIGNMENT];
	unsigned char buffer[0x1BE];
	struct partition part[4];
	unsigned char magicflag[2];
    } p;
} partition_table;

typedef struct {
    long long first_sector;	/* first sector in partition */
    long long last_sector;	/* last sector in partition */
    long offset;		/* offset from first sector to start of data */
    int flags;		/* active == 0x80 */
    int id;		/* filesystem type */
    int num;		/* number of partition -- primary vs. logical */
#define LABELSZ 16
    char volume_label[LABELSZ+1];
#define OSTYPESZ 8
    char ostype[OSTYPESZ+1];
#define FSTYPESZ 12
    char fstype[FSTYPESZ+1];
} partition_info;

char *disk_device = DEFAULT_DEVICE;
int fd;
int changed = FALSE;
int opened = FALSE;
int opentype;
int curses_started = 0;

partition_info p_info[MAXIMUM_PARTS];
partition_info ext_info;
int num_parts = 0;

int logical = 0;
long long logical_sectors[MAXIMUM_PARTS];

__sighandler_t old_SIGINT, old_SIGTERM;

int arrow_cursor = FALSE;
int display_units = MEGABYTES;
int zero_table = FALSE;
int use_partition_table_geometry = FALSE;
int print_only = 0;

/* Curses screen information */
int cur_part = 0;
int warning_last_time = FALSE;
int defined = FALSE;
int COLUMNS = 80;
int NUM_ON_SCREEN = 1;

/* Y coordinates */
int HEADER_START = 0;
int DISK_TABLE_START = 6;
int WARNING_START = 23;
int COMMAND_LINE_Y = 21;

/* X coordinates */
int NAME_START = 4;
int FLAGS_START = 16;
int PTYPE_START = 28;
int FSTYPE_START = 38;
int LABEL_START = 54;
int SIZE_START = 68;
int COMMAND_LINE_X = 5;

static void die_x(int ret);
static void draw_screen(void);

/* Guaranteed alloc */
static void *
xmalloc (size_t size) {
     void *t;

     if (size == 0)
          return NULL;

     t = malloc (size);
     if (t == NULL) {
          fprintf (stderr, _("%s: Out of memory!\n"), "cfdisk");
	  die_x(1);
     }
     return t;
}

/* Some libc's have their own basename() */
static char *
my_basename(char *devname) {
    char *s = strrchr(devname, '/');
    return s ? s+1 : devname;
}

static char *
partition_type_name(unsigned char type) {
    struct systypes *s = i386_sys_types;

    while(s->name && s->type != type)
	    s++;
    return s->name;
}

static char *
partition_type_text(int i) {
    if (p_info[i].id == UNUSABLE)
	 return _("Unusable");
    else if (p_info[i].id == FREE_SPACE)
	 return _("Free Space");
    else if (*p_info[i].fstype)
	 return p_info[i].fstype;
    else
	 return _(partition_type_name(p_info[i].id));
}

static void
fdexit(int ret) {
    if (opened) {
	if (changed)
		fsync(fd);
	close(fd);
    }
    if (changed) {
	fprintf(stderr, _("Disk has been changed.\n"));
#if 0
	fprintf(stderr, _("Reboot the system to ensure the partition "
			"table is correctly updated.\n"));
#endif

	fprintf( stderr, _("\nWARNING: If you have created or modified any\n"
		         "DOS 6.x partitions, please see the cfdisk manual\n"
		         "page for additional information.\n") );
    }

    exit(ret);
}

/*
 * Note that @len is size of @str buffer.
 *
 * Returns number of read bytes (without \0).
 */
static int
get_string(char *str, int len, char *def) {
    size_t cells = 0;
    ssize_t i = 0;
    int x, y;
    int use_def = FALSE;
    wint_t c;

    getyx(stdscr, y, x);
    clrtoeol();

    str[i] = 0;

    if (def != NULL) {
	mvaddstr(y, x, def);
	move(y, x);
	use_def = TRUE;
    }

    refresh();

    while (1) {
#if !defined(HAVE_SLCURSES_H) && !defined(HAVE_SLANG_SLCURSES_H) && \
    defined(HAVE_LIBNCURSESW) && defined(HAVE_WIDECHAR)
	if (get_wch(&c) == ERR) {
#else
	if ((c = getch()) == ERR) {
#endif
		if (!isatty(STDIN_FILENO))
			exit(2);
		else
			break;
	}
	if (c == '\r' || c == '\n' || c == KEY_ENTER)
		break;

	switch (c) {
	case ESC:
	    move(y, x);
	    clrtoeol();
	    refresh();
	    return GS_ESCAPE;
	case DEL:
	case '\b':
	case KEY_BACKSPACE:
	    if (i > 0) {
		cells--;
		i = mbs_truncate(str, &cells);
		if (i < 0)
			return GS_ESCAPE;
		mvaddch(y, x + cells, ' ');
		move(y, x + cells);
	    } else if (use_def) {
		clrtoeol();
		use_def = FALSE;
	    } else
		putchar(BELL);
	    break;
	default:
#if defined(HAVE_LIBNCURSESW) && defined(HAVE_WIDECHAR)
	    if (i + 1 < len && iswprint(c)) {
		wchar_t wc = (wchar_t) c;
		char s[MB_CUR_MAX + 1];
		int  sz = wctomb(s, wc);

		if (sz > 0 && sz + i < len) {
			s[sz] = '\0';
			mvaddnstr(y, x + cells, s, sz);
			if (use_def) {
			    clrtoeol();
			    use_def = FALSE;
			}
			memcpy(str + i, s, sz);
			i += sz;
			str[i] = '\0';
			cells += wcwidth(wc);
		} else
			putchar(BELL);
	    }
#else
	    if (i + 1 < len && isprint(c)) {
	        mvaddch(y, x + cells, c);
		if (use_def) {
		    clrtoeol();
		    use_def = FALSE;
		}
		str[i++] = c;
		str[i] = 0;
		cells++;
	    }
#endif
	    else
		putchar(BELL);
	}
	refresh();
    }

    if (use_def)
	return GS_DEFAULT;
    else
	return i;
}

static void
clear_warning(void) {
    int i;

    if (!curses_started || !warning_last_time)
	return;

    move(WARNING_START,0);
    for (i = 0; i < COLS; i++)
	addch(' ');

    warning_last_time = FALSE;
}

static void
print_warning(char *s) {
    if (!curses_started) {
	 fprintf(stderr, "%s\n", s);
    } else {
	mvaddstr(WARNING_START, (COLS-strlen(s))/2, s);
	putchar(BELL); /* CTRL-G */

	warning_last_time = TRUE;
    }
}

static void
fatal(char *s, int ret) {
    char *err1 = _("FATAL ERROR");
    char *err2 = _("Press any key to exit cfdisk");

    if (curses_started) {
	 char *str = xmalloc(strlen(s) + strlen(err1) + strlen(err2) + 10);

	 sprintf(str, "%s: %s", err1, s);
	 if (strlen(str) > (size_t) COLS)
	     str[COLS] = 0;
	 mvaddstr(WARNING_START, (COLS-strlen(str))/2, str);
	 sprintf(str, "%s", err2);
	 if (strlen(str) > (size_t) COLS)
	     str[COLS] = 0;
	 mvaddstr(WARNING_START+1, (COLS-strlen(str))/2, str);
	 putchar(BELL); /* CTRL-G */
	 refresh();
	 (void)getch();
	 die_x(ret);
    } else {
	 fprintf(stderr, "%s: %s\n", err1, s);
	 exit(ret);
    }
}

static void
die(int dummy __attribute__((__unused__))) {
    die_x(0);
}

static void
die_x(int ret) {
    signal(SIGINT, old_SIGINT);
    signal(SIGTERM, old_SIGTERM);
#if defined(HAVE_SLCURSES_H) || defined(HAVE_SLANG_SLCURSES_H)
    SLsmg_gotorc(LINES-1, 0);
    SLsmg_refresh();
#else
    mvcur(0, COLS-1, LINES-1, 0);
#endif
    nl();
    endwin();
    printf("\n");
    fdexit(ret);
}

static void
read_sector(unsigned char *buffer, long long sect_num) {
    if (lseek(fd, sect_num*SECTOR_SIZE, SEEK_SET) < 0)
	fatal(_("Cannot seek on disk drive"), 2);
    if (read(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE)
	fatal(_("Cannot read disk drive"), 2);
}

static void
write_sector(unsigned char *buffer, long long sect_num) {
    if (lseek(fd, sect_num*SECTOR_SIZE, SEEK_SET) < 0)
	fatal(_("Cannot seek on disk drive"), 2);
    if (write(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE)
	fatal(_("Cannot write disk drive"), 2);
}

#ifdef HAVE_LIBBLKID
static void
get_fsinfo(int i)
{
	blkid_probe pr;
	blkid_loff_t offset, size;
	const char *data;

	offset = (p_info[i].first_sector + p_info[i].offset) * SECTOR_SIZE;
	size = (p_info[i].last_sector - p_info[i].first_sector + 1) * SECTOR_SIZE;
	pr = blkid_new_probe();
	if (!pr)
		return;
	blkid_probe_enable_superblocks(pr, 1);
	blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_LABEL |
					      BLKID_SUBLKS_TYPE);
	if (blkid_probe_set_device(pr, fd, offset, size))
		goto done;
	if (blkid_do_safeprobe(pr))
		goto done;

	if (!blkid_probe_lookup_value(pr, "TYPE", &data, 0))
		strncpy(p_info[i].fstype, data, FSTYPESZ);

	if (!blkid_probe_lookup_value(pr, "LABEL", &data, 0))
		strncpy(p_info[i].volume_label, data, LABELSZ);
done:
	blkid_free_probe(pr);
}
#endif

static void
check_part_info(void) {
    int i, pri = 0, log = 0;

    for (i = 0; i < num_parts; i++)
	if (p_info[i].id > 0 && IS_PRIMARY(p_info[i].num))
	    pri++;
	else if (p_info[i].id > 0 && IS_LOGICAL(p_info[i].num))
	    log++;
    if (is_extended(ext_info.id)) {
	if (log > 0)
	    pri++;
	else {
	    ext_info.first_sector = 0;
	    ext_info.last_sector = 0;
	    ext_info.offset = 0;
	    ext_info.flags = 0;
	    ext_info.id = FREE_SPACE;
	    ext_info.num = PRIMARY;
	}
    }

    if (pri >= 4) {
	for (i = 0; i < num_parts; i++)
	    if (p_info[i].id == FREE_SPACE || p_info[i].id == UNUSABLE) {
		if (is_extended(ext_info.id)) {
		    if (p_info[i].first_sector >= ext_info.first_sector &&
			p_info[i].last_sector <= ext_info.last_sector) {
			p_info[i].id = FREE_SPACE;
			p_info[i].num = LOGICAL;
		    } else if (i > 0 &&
			       p_info[i-1].first_sector >=
			       ext_info.first_sector &&
			       p_info[i-1].last_sector <=
			       ext_info.last_sector) {
			p_info[i].id = FREE_SPACE;
			p_info[i].num = LOGICAL;
		    } else if (i < num_parts-1 &&
			       p_info[i+1].first_sector >=
			       ext_info.first_sector &&
			       p_info[i+1].last_sector <=
			       ext_info.last_sector) {
			p_info[i].id = FREE_SPACE;
			p_info[i].num = LOGICAL;
		    } else
			p_info[i].id = UNUSABLE;
		} else /* if (!is_extended(ext_info.id)) */
		    p_info[i].id = UNUSABLE;
	    } else /* if (p_info[i].id > 0) */
		while (0); /* Leave these alone */
    } else { /* if (pri < 4) */
	for (i = 0; i < num_parts; i++) {
	    if (p_info[i].id == UNUSABLE)
		p_info[i].id = FREE_SPACE;
	    if (p_info[i].id == FREE_SPACE) {
		if (is_extended(ext_info.id)) {
		    if (p_info[i].first_sector >= ext_info.first_sector &&
			p_info[i].last_sector <= ext_info.last_sector)
			p_info[i].num = LOGICAL;
		    else if (i > 0 &&
			     p_info[i-1].first_sector >=
			     ext_info.first_sector &&
			     p_info[i-1].last_sector <=
			     ext_info.last_sector)
			p_info[i].num = PRI_OR_LOG;
		    else if (i < num_parts-1 &&
			     p_info[i+1].first_sector >=
			     ext_info.first_sector &&
			     p_info[i+1].last_sector <=
			     ext_info.last_sector)
			p_info[i].num = PRI_OR_LOG;
		    else
			p_info[i].num = PRIMARY;
		} else /* if (!is_extended(ext_info.id)) */
		    p_info[i].num = PRI_OR_LOG;
	    } else /* if (p_info[i].id > 0) */
		while (0); /* Leave these alone */
	}
    }
}

static void
remove_part(int i) {
    int p;

    for (p = i; p < num_parts; p++)
	p_info[p] = p_info[p+1];

    num_parts--;
    if (cur_part == num_parts)
	cur_part--;
}

static void
insert_empty_part(int i, long long first, long long last) {
    int p;

    for (p = num_parts; p > i; p--)
	 p_info[p] = p_info[p-1];

    p_info[i].first_sector = first;
    p_info[i].last_sector = last;
    p_info[i].offset = 0;
    p_info[i].flags = 0;
    p_info[i].id = FREE_SPACE;
    p_info[i].num = PRI_OR_LOG;
    p_info[i].volume_label[0] = 0;
    p_info[i].fstype[0] = 0;
    p_info[i].ostype[0] = 0;

    num_parts++;
}

static void
del_part(int i) {
    int num = p_info[i].num;

    if (i > 0 && (p_info[i-1].id == FREE_SPACE ||
		  p_info[i-1].id == UNUSABLE)) {
	/* Merge with previous partition */
	p_info[i-1].last_sector = p_info[i].last_sector;
	remove_part(i--);
    }

    if (i < num_parts - 1 && (p_info[i+1].id == FREE_SPACE ||
			      p_info[i+1].id == UNUSABLE)) {
	/* Merge with next partition */
	p_info[i+1].first_sector = p_info[i].first_sector;
	remove_part(i);
    }

    if (i > 0)
	p_info[i].first_sector = p_info[i-1].last_sector + 1;
    else
	p_info[i].first_sector = 0;

    if (i < num_parts - 1)
	p_info[i].last_sector = p_info[i+1].first_sector - 1;
    else
	p_info[i].last_sector = actual_size - 1;

    p_info[i].offset = 0;
    p_info[i].flags = 0;
    p_info[i].id = FREE_SPACE;
    p_info[i].num = PRI_OR_LOG;

    if (IS_LOGICAL(num)) {
	/* We have a logical partition --> shrink the extended partition
	 * if (1) this is the first logical drive, or (2) this is the
	 * last logical drive; and if there are any other logical drives
	 * then renumber the ones after "num".
	 */
	if (i == 0 || (i > 0 && IS_PRIMARY(p_info[i-1].num))) {
	    ext_info.first_sector = p_info[i].last_sector + 1;
	    ext_info.offset = 0;
	}
	if (i == num_parts-1 ||
	    (i < num_parts-1 && IS_PRIMARY(p_info[i+1].num)))
	    ext_info.last_sector = p_info[i].first_sector - 1;
	for (i = 0; i < num_parts; i++)
	    if (p_info[i].num > num)
		p_info[i].num--;
    }

    /* Clean up the rest of the partitions */
    check_part_info();
}

static int
add_part(int num, int id, int flags, long long first, long long last,
	 long long offset, int want_label, char **errmsg) {
    int i, pri = 0, log = 0;

    if (num_parts == MAXIMUM_PARTS) {
	*errmsg = _("Too many partitions");
	return -1;
    }

    if (first < 0) {
	*errmsg = _("Partition begins before sector 0");
	return -1;
    }

    if (last < 0) {
	*errmsg = _("Partition ends before sector 0");
	return -1;
    }

    if (first >= actual_size) {
	*errmsg = _("Partition begins after end-of-disk");
	return -1;
    }

    if (last >= actual_size) {
	*errmsg = _("Partition ends after end-of-disk");
	return -1;
    }

    for (i = 0; i < num_parts; i++) {
	if (p_info[i].id > 0 && IS_PRIMARY(p_info[i].num))
	    pri++;
	else if (p_info[i].id > 0 && IS_LOGICAL(p_info[i].num))
	    log++;
    }
    if (is_extended(ext_info.id) && log > 0)
	pri++;

    if (IS_PRIMARY(num)) {
	if (pri >= 4) {
	    return -1;		/* no room for more */
	} else
	    pri++;
    }

    for (i = 0; i < num_parts && p_info[i].last_sector < first; i++);

    if (i < num_parts && p_info[i].id != FREE_SPACE) {
	 if (last < p_info[i].first_sector)
	      *errmsg = _("logical partitions not in disk order");
	 else if (first + offset <= p_info[i].last_sector &&
		  p_info[i].first_sector + p_info[i].offset <= last)
	      *errmsg = _("logical partitions overlap");
	 else
	      /* the enlarged logical partition starts at the
		 partition table sector that defines it */
	      *errmsg = _("enlarged logical partitions overlap");
	 return -1;
    }

    if (i == num_parts || last > p_info[i].last_sector) {
	return -1;
    }

    if (is_extended(id)) {
	if (ext_info.id != FREE_SPACE) {
	    return -1;		/* second extended */
	}
	else if (IS_PRIMARY(num)) {
	    ext_info.first_sector = first;
	    ext_info.last_sector = last;
	    ext_info.offset = offset;
	    ext_info.flags = flags;
	    ext_info.id = id;
	    ext_info.num = num;
	    ext_info.volume_label[0] = 0;
	    ext_info.fstype[0] = 0;
	    ext_info.ostype[0] = 0;
	    return 0;
	} else {
	    return -1;		/* explicit extended logical */
	}
    }

    if (IS_LOGICAL(num)) {
	if (!is_extended(ext_info.id)) {
	    print_warning(_("!!!! Internal error creating logical "
			  "drive with no extended partition !!!!"));
	} else {
	    /* We might have a logical partition outside of the extended
	     * partition's range --> we have to extend the extended
	     * partition's range to encompass this new partition, but we
	     * must make sure that there are no primary partitions between
	     * it and the closest logical drive in extended partition.
	     */
	    if (first < ext_info.first_sector) {
		if (i < num_parts-1 && IS_PRIMARY(p_info[i+1].num)) {
		    print_warning(_("Cannot create logical drive here -- would create two extended partitions"));
		    return -1;
		} else {
		    if (first == 0) {
			ext_info.first_sector = 0;
			ext_info.offset = first = offset;
		    } else {
			ext_info.first_sector = first;
		    }
		}
	    } else if (last > ext_info.last_sector) {
		if (i > 0 && IS_PRIMARY(p_info[i-1].num)) {
		    print_warning(_("Cannot create logical drive here -- would create two extended partitions"));
		    return -1;
		} else {
		    ext_info.last_sector = last;
		}
	    }
	}
    }

    if (first != p_info[i].first_sector &&
	!(IS_LOGICAL(num) && first == offset)) {
	insert_empty_part(i, p_info[i].first_sector, first-1);
	i++;
    }

    if (last != p_info[i].last_sector)
	insert_empty_part(i+1, last+1, p_info[i].last_sector);

    p_info[i].first_sector = first;
    p_info[i].last_sector = last;
    p_info[i].offset = offset;
    p_info[i].flags = flags;
    p_info[i].id = id;
    p_info[i].num = num;
    p_info[i].volume_label[0] = 0;
    p_info[i].fstype[0] = 0;
    p_info[i].ostype[0] = 0;

#ifdef HAVE_LIBBLKID
    if (want_label)
	 get_fsinfo(i);
#endif
    check_part_info();

    return 0;
}

static int
find_primary(void) {
    int num = 0, cur = 0;

    while (cur < num_parts && IS_PRIMARY(num))
	if ((p_info[cur].id > 0 && p_info[cur].num == num) ||
	    (is_extended(ext_info.id) && ext_info.num == num)) {
	    num++;
	    cur = 0;
	} else
	    cur++;

    if (!IS_PRIMARY(num))
	return -1;
    else
	return num;
}

static int
find_logical(int i) {
    int num = -1;
    int j;

    for (j = i; j < num_parts && num == -1; j++)
	if (p_info[j].id > 0 && IS_LOGICAL(p_info[j].num))
	    num = p_info[j].num;

    if (num == -1) {
	num = 4;
	for (j = 0; j < num_parts; j++)
	    if (p_info[j].id > 0 && p_info[j].num == num)
		num++;
    }

    return num;
}

/*
 * Command menu support by Janne Kukonlehto <jtklehto@phoenix.oulu.fi>
 * September 1994
 */

/* Constants for menuType parameter of menuSelect function */
#define MENU_ACCEPT_OTHERS 4
#define MENU_BUTTON 8
/* Miscellenous constants */
#define MENU_SPACING 2
#define MENU_MAX_ITEMS 256 /* for simpleMenu function */

struct MenuItem
{
    int key; /* Keyboard shortcut; if zero, then there is no more items in the menu item table */
    char *name; /* Item name, should be eight characters with current implementation */
    char *desc; /* Item description to be printed when item is selected */
};

/*
 * Actual function which prints the button bar and highlights the active button
 * Should not be called directly. Call function menuSelect instead.
 */

static int
menuUpdate( int y, int x, struct MenuItem *menuItems, int itemLength,
	    char *available, int menuType, int current ) {
    int i, lmargin = x;
    char *mcd;

    /* Print available buttons */
    move( y, x ); clrtoeol();

    for( i = 0; menuItems[i].key; i++ ) {
        char buff[20];
        int lenName;
	const char *mi;

        /* Search next available button */
        while( menuItems[i].key && !strchr(available, menuItems[i].key) )
            i++;

        if( !menuItems[i].key ) break; /* No more menu items */

        /* If selected item is not available and we have bypassed it,
	   make current item selected */
        if( current < i && menuItems[current].key < 0 ) current = i;

        /* If current item is selected, highlight it */
        if( current == i ) /*attron( A_REVERSE )*/ standout ();

        /* Print item */
	/* Because of a bug in gettext() we must not translate empty strings */
	if (menuItems[i].name[0])
		mi = _(menuItems[i].name);
	else
		mi = "";
        lenName = strlen( mi );
#if 0
        if(lenName > itemLength || lenName >= sizeof(buff))
            print_warning(_("Menu item too long. Menu may look odd."));
#endif
	if ((size_t) lenName >= sizeof(buff)) {	/* truncate ridiculously long string */
	    xstrncpy(buff, mi, sizeof(buff));
	} else if (lenName >= itemLength) {
            snprintf(buff, sizeof(buff),
		     (menuType & MENU_BUTTON) ? "[%s]" : "%s", mi);
	} else {
            snprintf(buff, sizeof(buff),
		     (menuType & MENU_BUTTON) ? "[%*s%-*s]" : "%*s%-*s",
		     (itemLength - lenName) / 2, "",
		     (itemLength - lenName + 1) / 2 + lenName, mi);
	}
        mvaddstr( y, x, buff );

        /* Lowlight after selected item */
        if( current == i ) /*attroff( A_REVERSE )*/ standend ();

        /* Calculate position for the next item */
            x += itemLength + MENU_SPACING;
            if( menuType & MENU_BUTTON ) x += 2;
            if( x > COLUMNS - lmargin - 12 )
            {
                x = lmargin;
                y ++ ;
            }
    }

    /* Print the description of selected item */
    mcd = _(menuItems[current].desc);
    mvaddstr( WARNING_START + 1, (COLUMNS - strlen( mcd )) / 2, mcd );
    return y;
}

/* This function takes a list of menu items, lets the user choose one *
 * and returns the keyboard shortcut value of the selected menu item  */

static int
menuSelect( int y, int x, struct MenuItem *menuItems, int itemLength,
	    char *available, int menuType, int menuDefault ) {
    int i, ylast = y, key = 0, current = menuDefault;

    /* Make sure that the current is one of the available items */
    while( !strchr(available, menuItems[current].key) ) {
        current ++ ;
        if( !menuItems[current].key ) current = 0;
    }

    keypad(stdscr, TRUE);

    /* Repeat until allowable choice has been made */
    while( !key ) {
        /* Display the menu and read a command */
        ylast = menuUpdate( y, x, menuItems, itemLength, available,
			    menuType, current );
        refresh();
        key = getch();

	if (key == ERR)
		if (!isatty(STDIN_FILENO))
			exit(2);

        /* Clear out all prompts and such */
        clear_warning();
        for (i = y; i < ylast; i++) {
            move(i, x);
            clrtoeol();
        }
        move( WARNING_START + 1, 0 );
        clrtoeol();

	switch (key) {
	case KEY_RIGHT:
	case '\t':
		/* Select next menu item */
		do {
			current++;
			if (!menuItems[current].key)
				current = 0;
		} while (!strchr(available, menuItems[current].key));
		key = 0;
		break;
	case KEY_LEFT:
#ifdef KEY_BTAB
	case KEY_BTAB:	/* Back tab */
#endif
		/* Select previous menu item */
		do {
			current--;
			if (current < 0) {
				while (menuItems[current + 1].key)
					current++;
			}
		} while (!strchr(available, menuItems[current].key));
		key = 0;
		break;
	case KEY_ENTER:
	case '\n':
	case '\r':
		/* Enter equals the keyboard shortcut of current menu item */
		key = menuItems[current].key;
		break;
	}

        /* Should all keys to be accepted? */
        if( key && (menuType & MENU_ACCEPT_OTHERS) ) break;

        /* Is pressed key among acceptable ones? */
        if( key && (strchr(available, tolower(key)) || strchr(available, key)))
	    break;

        /* The key has not been accepted so far -> let's reject it */
        if (key) {
            key = 0;
            putchar( BELL );
            print_warning(_("Illegal key"));
        }
    }

    keypad(stdscr, FALSE);

    /* Clear out prompts and such */
    clear_warning();
    for( i = y; i <= ylast; i ++ ) {
        move( i, x );
        clrtoeol();
    }
    move( WARNING_START + 1, 0 );
    clrtoeol();
    return key;
}

/* A function which displays "Press a key to continue"                  *
 * and waits for a keypress.                                            *
 * Perhaps calling function menuSelect is a bit overkill but who cares? */

static void
menuContinue(void) {
    static struct MenuItem menuContinueBtn[]=
    {
        { 'c', "", N_("Press a key to continue") },
        { 0, NULL, NULL }
    };

    menuSelect(COMMAND_LINE_Y, COMMAND_LINE_X,
	menuContinueBtn, 0, "c", MENU_ACCEPT_OTHERS, 0 );
}

/* Function menuSelect takes way too many parameters  *
 * Luckily, most of time we can do with this function */

static int
menuSimple(struct MenuItem *menuItems, int menuDefault) {
    int i, j, itemLength = 0;
    char available[MENU_MAX_ITEMS];

    for(i = 0; menuItems[i].key; i++)
    {
        j = strlen( _(menuItems[i].name) );
        if( j > itemLength ) itemLength = j;
        available[i] = menuItems[i].key;
    }
    available[i] = 0;
    return menuSelect(COMMAND_LINE_Y, COMMAND_LINE_X, menuItems, itemLength,
        available, MENU_BUTTON, menuDefault);
}

/* End of command menu support code */

static void
new_part(int i) {
    char response[LINE_LENGTH], def[LINE_LENGTH];
    char c;
    long long first = p_info[i].first_sector;
    long long last = p_info[i].last_sector;
    long long offset = 0;
    int flags = 0;
    int id = LINUX;
    int num = -1;
    long long num_sects = last - first + 1;
    int len, ext, j;
    char *errmsg;
    double sectors_per_MB = K*K / 512.0;

    if (p_info[i].num == PRI_OR_LOG) {
        static struct MenuItem menuPartType[]=
        {
            { 'p', N_("Primary"), N_("Create a new primary partition") },
            { 'l', N_("Logical"), N_("Create a new logical partition") },
            { ESC, N_("Cancel"), N_("Don't create a partition") },
            { 0, NULL, NULL }
        };

	c = menuSimple( menuPartType, 0 );
	if (toupper(c) == 'P')
	    num = find_primary();
	else if (toupper(c) == 'L')
	    num = find_logical(i);
	else
	    return;
    } else if (p_info[i].num == PRIMARY)
	num = find_primary();
    else if (p_info[i].num == LOGICAL)
	num = find_logical(i);
    else
	print_warning(_("!!! Internal error !!!"));

    snprintf(def, sizeof(def), "%.2f", num_sects/sectors_per_MB);
    mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X, _("Size (in MB): "));
    if ((len = get_string(response, LINE_LENGTH, def)) <= 0 &&
	len != GS_DEFAULT)
	return;
    else if (len > 0) {
#define num_cyls(bytes) (round_int(bytes/SECTOR_SIZE/cylinder_size))
	for (j = 0;
	     j < len-1 && (isdigit(response[j]) || response[j] == '.');
	     j++);
	if (toupper(response[j]) == 'K') {
	    num_sects = num_cyls(atof(response)*K)*cylinder_size;
	} else if (toupper(response[j]) == 'M') {
	    num_sects = num_cyls(atof(response)*K*K)*cylinder_size;
	} else if (toupper(response[j]) == 'G') {
	    num_sects = num_cyls(atof(response)*K*K*K)*cylinder_size;
	} else if (toupper(response[j]) == 'C') {
	    num_sects = round_int(atof(response))*cylinder_size;
	} else if (toupper(response[j]) == 'S') {
	    num_sects = round_int(atof(response));
	} else {
	    num_sects = num_cyls(atof(response)*K*K)*cylinder_size;
	}
    }

    if (num_sects <= 0 ||
	num_sects > p_info[i].last_sector - p_info[i].first_sector + 1)
	return;

    move( COMMAND_LINE_Y, COMMAND_LINE_X ); clrtoeol();
    if (num_sects < p_info[i].last_sector - p_info[i].first_sector + 1) {
	/* Determine where inside free space to put partition.
	 */
	static struct MenuItem menuPlace[]=
	{
	    { 'b', N_("Beginning"), N_("Add partition at beginning of free space") },
	    { 'e', N_("End"), N_("Add partition at end of free space") },
	    { ESC, N_("Cancel"), N_("Don't create a partition") },
	    { 0, NULL, NULL }
	};
	c = menuSimple( menuPlace, 0 );
	if (toupper(c) == 'B')
	    last = first + num_sects - 1;
	else if (toupper(c) == 'E')
	    first = last - num_sects + 1;
	else
	    return;
    }

    if (IS_LOGICAL(num) && !is_extended(ext_info.id)) {
	/* We want to add a logical partition, but need to create an
	 * extended partition first.
	 */
	if ((ext = find_primary()) < 0) {
	    print_warning(_("No room to create the extended partition"));
	    return;
	}
	errmsg = 0;
	if (add_part(ext, DOS_EXTENDED, 0, first, last,
		     (first == 0 ? sectors : 0), 0, &errmsg) && errmsg)
		print_warning(errmsg);
	first = ext_info.first_sector + ext_info.offset;
    }

    /* increment number of all partitions past this one */
    if (IS_LOGICAL(num)) {
#if 0
	/* original text - ok, but fails when partitions not in disk order */
	for (j = i; j < num_parts; j++)
	    if (p_info[j].id > 0 && IS_LOGICAL(p_info[j].num))
		p_info[j].num++;
#else
	/* always ok */
	for (j = 0; j < num_parts; j++)
	    if (p_info[j].id > 0 && p_info[j].num >= num)
		p_info[j].num++;
#endif
    }

    /* Now we have a complete partition to ourselves */
    if (first == 0 || IS_LOGICAL(num))
	offset = sectors;

    errmsg = 0;
    if (add_part(num, id, flags, first, last, offset, 0, &errmsg) && errmsg)
	    print_warning(errmsg);
}

static void
get_kernel_geometry(void) {
#ifdef HDIO_GETGEO
    struct hd_geometry geometry;

    if (!ioctl(fd, HDIO_GETGEO, &geometry)) {
	kern_heads = geometry.heads;
	kern_sectors = geometry.sectors;
    }
#endif
}

static int
said_yes(char answer) {
	char reply[2];

	reply[0] = answer;
	reply[1] = 0;

	return (rpmatch(reply) == 1) ? 1 : 0;
}

static void
get_partition_table_geometry(partition_table *bufp) {
	struct partition *p;
	int i,h,s,hh,ss;
	int first = TRUE;
	int bad = FALSE;

	for (i=0; i<66; i++)
		if (bufp->c.b[446+i])
			goto nonz;

	/* zero table */
	if (!curses_started) {
		fatal(_("No partition table.\n"), 3);
		return;
	} else {
		mvaddstr(WARNING_START, 0,
			 _("No partition table. Starting with zero table."));
		putchar(BELL);
		refresh();
		zero_table = TRUE;
		return;
	}
 nonz:
	if (bufp->p.magicflag[0] != PART_TABLE_FLAG0 ||
	    bufp->p.magicflag[1] != PART_TABLE_FLAG1) {
		if (!curses_started)
			fatal(_("Bad signature on partition table"), 3);

		/* Matthew Wilcox */
		mvaddstr(WARNING_START, 0,
			 _("Unknown partition table type"));
		mvaddstr(WARNING_START+1, 0,
			 _("Do you wish to start with a zero table [y/N] ?"));
		putchar(BELL);
		refresh();
		{
			int cont = getch();
			if (cont == EOF || !said_yes(cont))
				die_x(3);
		}
		zero_table = TRUE;
		return;
	}

	hh = ss = 0;
	for (i=0; i<4; i++) {
		p = &(bufp->p.part[i]);
		if (p->sys_ind != 0) {
			h = p->end_head + 1;
			s = (p->end_sector & 077);
			if (first) {
				hh = h;
				ss = s;
				first = FALSE;
			} else if (hh != h || ss != s)
				bad = TRUE;
		}
	}

	if (!first && !bad) {
		pt_heads = hh;
		pt_sectors = ss;
	}
}

static void
decide_on_geometry(void) {
    heads = (user_heads ? user_heads :
	     pt_heads ? pt_heads :
	     kern_heads ? kern_heads : 255);
    sectors = (user_sectors ? user_sectors :
	       pt_sectors ? pt_sectors :
	       kern_sectors ? kern_sectors : 63);
    cylinder_size = heads*sectors;
    cylinders = actual_size/cylinder_size;
    if (user_cylinders > 0)
	    cylinders = user_cylinders;

    if (cylinder_size * cylinders > actual_size)
	    print_warning(_("You specified more cylinders than fit on disk"));
}

static void
clear_p_info(void) {
    num_parts = 1;
    p_info[0].first_sector = 0;
    p_info[0].last_sector = actual_size - 1;
    p_info[0].offset = 0;
    p_info[0].flags = 0;
    p_info[0].id = FREE_SPACE;
    p_info[0].num = PRI_OR_LOG;

    ext_info.first_sector = 0;
    ext_info.last_sector = 0;
    ext_info.offset = 0;
    ext_info.flags = 0;
    ext_info.id = FREE_SPACE;
    ext_info.num = PRIMARY;
}

static void
fill_p_info(void) {
    int pn, i;
    long long bs, bsz;
    unsigned long long llsectors;
    struct partition *p;
    partition_table buffer;
    partition_info tmp_ext;

    memset(&tmp_ext, 0, sizeof tmp_ext);
    tmp_ext.id = FREE_SPACE;
    tmp_ext.num = PRIMARY;

    if ((fd = open(disk_device, O_RDWR)) < 0) {
	 if ((fd = open(disk_device, O_RDONLY)) < 0)
	      fatal(_("Cannot open disk drive"), 2);
	 opentype = O_RDONLY;
	 print_warning(_("Opened disk read-only - you have no permission to write"));
	 if (curses_started) {
	      refresh();
	      getch();
	      clear_warning();
	 }
    } else
	 opentype = O_RDWR;
    opened = TRUE;

    if (gpt_probe_signature_fd(fd)) {
	 print_warning(_("Warning!!  Unsupported GPT (GUID Partition Table) detected. Use GNU Parted."));
	 refresh();
	 getch();
	 clear_warning();
    }

#ifdef BLKFLSBUF
    /* Blocks are visible in more than one way:
       e.g. as block on /dev/hda and as block on /dev/hda3
       By a bug in the Linux buffer cache, we will see the old
       contents of /dev/hda when the change was made to /dev/hda3.
       In order to avoid this, discard all blocks on /dev/hda.
       Note that partition table blocks do not live in /dev/hdaN,
       so this only plays a role if we want to show volume labels. */
    ioctl(fd, BLKFLSBUF);	/* ignore errors */
				/* e.g. Permission Denied */
#endif

    if (blkdev_get_sectors(fd, &llsectors) == -1)
	    fatal(_("Cannot get disk size"), 3);
    actual_size = llsectors;

    read_sector(buffer.c.b, 0);

    get_kernel_geometry();

    if (!zero_table || use_partition_table_geometry)
	get_partition_table_geometry(& buffer);

    decide_on_geometry();

    clear_p_info();

    if (!zero_table) {
	char *errmsg = "";

	for (i = 0; i < 4; i++) {
	    p = & buffer.p.part[i];
	    bs = get_start_sect(p);
	    bsz = get_nr_sects(p);

	    if (p->sys_ind > 0 &&
		add_part(i, p->sys_ind, p->boot_ind,
			 ((bs <= sectors) ? 0 : bs), bs + bsz - 1,
			 ((bs <= sectors) ? bs : 0), 1, &errmsg)) {
		    char *bad = _("Bad primary partition");
		    char *msg = (char *) xmalloc(strlen(bad) + strlen(errmsg) + 30);
		    sprintf(msg, "%s %d: %s", bad, i + 1, errmsg);
		    fatal(msg, 4);
	    }
	    if (is_extended(buffer.p.part[i].sys_ind))
		tmp_ext = ext_info;
	}

	if (is_extended(tmp_ext.id)) {
	    ext_info = tmp_ext;
	    logical_sectors[logical] =
		 ext_info.first_sector + ext_info.offset;
	    read_sector(buffer.c.b, logical_sectors[logical++]);
	    i = 4;
	    do {
		for (pn = 0;
		     pn < 4 && (!buffer.p.part[pn].sys_ind ||
			       is_extended(buffer.p.part[pn].sys_ind));
		     pn++);

		if (pn < 4) {
			p = & buffer.p.part[pn];
			bs = get_start_sect(p);
			bsz = get_nr_sects(p);

			if (add_part(i++, p->sys_ind, p->boot_ind,
			     logical_sectors[logical-1],
			     logical_sectors[logical-1] + bs + bsz - 1,
			     bs, 1, &errmsg)) {
				char *bad = _("Bad logical partition");
				char *msg = (char *) xmalloc(strlen(bad) + strlen(errmsg) + 30);
				sprintf(msg, "%s %d: %s", bad, i, errmsg);
				fatal(msg, 4);
			}
		}

		for (pn = 0;
		     pn < 4 && !is_extended(buffer.p.part[pn].sys_ind);
		     pn++);
		if (pn < 4) {
			p = & buffer.p.part[pn];
			bs = get_start_sect(p);
			logical_sectors[logical] = ext_info.first_sector
				+ ext_info.offset + bs;
			read_sector(buffer.c.b, logical_sectors[logical++]);
		}
	    } while (pn < 4 && logical < MAXIMUM_PARTS-4);
	}
    }
}

static void
fill_part_table(struct partition *p, partition_info *pi) {
    long long begin;

    p->boot_ind = pi->flags;
    p->sys_ind = pi->id;
    begin = pi->first_sector + pi->offset;
    if (IS_LOGICAL(pi->num))
	set_start_sect(p,pi->offset);
    else
	set_start_sect(p,begin);
    set_nr_sects(p, pi->last_sector - begin + 1);
    set_hsc_begin(p, begin);
    set_hsc_end(p, pi->last_sector);
}

static void
fill_primary_table(partition_table *buffer) {
    int i;

    /* Zero out existing table */
    for (i = 0x1BE; i < SECTOR_SIZE; i++)
	buffer->c.b[i] = 0;

    for (i = 0; i < num_parts; i++)
	if (IS_PRIMARY(p_info[i].num))
	    fill_part_table(&(buffer->p.part[p_info[i].num]), &(p_info[i]));

    if (is_extended(ext_info.id))
	fill_part_table(&(buffer->p.part[ext_info.num]), &ext_info);

    buffer->p.magicflag[0] = PART_TABLE_FLAG0;
    buffer->p.magicflag[1] = PART_TABLE_FLAG1;
}

static void
fill_logical_table(partition_table *buffer, partition_info *pi) {
    struct partition *p;
    int i;

    for (i = 0; i < logical && pi->first_sector != logical_sectors[i]; i++);
    if (i == logical || buffer->p.magicflag[0] != PART_TABLE_FLAG0
	             || buffer->p.magicflag[1] != PART_TABLE_FLAG1)
	for (i = 0; i < SECTOR_SIZE; i++)
	    buffer->c.b[i] = 0;

    /* Zero out existing table */
    for (i = 0x1BE; i < SECTOR_SIZE; i++)
	buffer->c.b[i] = 0;

    fill_part_table(&(buffer->p.part[0]), pi);

    for (i = 0;
	 i < num_parts && pi->num != p_info[i].num - 1;
	 i++);

    if (i < num_parts) {
	p = &(buffer->p.part[1]);
	pi = &(p_info[i]);

	p->boot_ind = 0;
	p->sys_ind = DOS_EXTENDED;
	set_start_sect(p, pi->first_sector - ext_info.first_sector - ext_info.offset);
	set_nr_sects(p, pi->last_sector - pi->first_sector + 1);
	set_hsc_begin(p, pi->first_sector);
	set_hsc_end(p, pi->last_sector);
    }

    buffer->p.magicflag[0] = PART_TABLE_FLAG0;
    buffer->p.magicflag[1] = PART_TABLE_FLAG1;
}

static void
write_part_table(void) {
    int i, ct, done = FALSE, len;
    partition_table buffer;
    struct stat s;
    int is_bdev;
    char response[LINE_LENGTH];

    if (opentype == O_RDONLY) {
	 print_warning(_("Opened disk read-only - you have no permission to write"));
	 refresh();
	 getch();
	 clear_warning();
	 return;
    }

    is_bdev = 0;
    if(fstat(fd, &s) == 0 && S_ISBLK(s.st_mode))
	 is_bdev = 1;

    if (is_bdev) {
	 print_warning(_("Warning!!  This may destroy data on your disk!"));

	 while (!done) {
	      mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		       _("Are you sure you want to write the partition table "
		       "to disk? (yes or no): "));
	      len = get_string(response, LINE_LENGTH, NULL);
	      clear_warning();
	      if (len == GS_ESCAPE)
		   return;
	      else if (strcasecmp(response, _("no")) == 0 ||
		       strcasecmp(response, "no") == 0) {
		   print_warning(_("Did not write partition table to disk"));
		   return;
	      } else if (strcasecmp(response, _("yes")) == 0 ||
			 strcasecmp(response, "yes") == 0)
		   done = TRUE;
	      else
		   print_warning(_("Please enter `yes' or `no'"));
	 }

	 clear_warning();
	 print_warning(_("Writing partition table to disk..."));
	 refresh();
    }

    read_sector(buffer.c.b, 0);
    fill_primary_table(&buffer);
    write_sector(buffer.c.b, 0);

    for (i = 0; i < num_parts; i++)
	if (IS_LOGICAL(p_info[i].num)) {
	    read_sector(buffer.c.b, p_info[i].first_sector);
	    fill_logical_table(&buffer, &(p_info[i]));
	    write_sector(buffer.c.b, p_info[i].first_sector);
	}

    if (is_bdev) {
#ifdef BLKRRPART
	 sync();
	 if (!ioctl(fd,BLKRRPART))
	      changed = TRUE;
#endif
	 sync();

	 clear_warning();
	 if (changed)
	      print_warning(_("Wrote partition table to disk"));
	 else
	      print_warning(_("Wrote partition table, but re-read table failed.  Run partprobe(8), kpartx(8) or reboot to update table."));
    } else
	 print_warning(_("Wrote partition table to disk"));

    /* Check: unique bootable primary partition? */
    ct = 0;
    for (i = 0; i < num_parts; i++)
	if (IS_PRIMARY(i) && p_info[i].flags == ACTIVE_FLAG)
	    ct++;
    if (ct == 0)
	print_warning(_("No primary partitions are marked bootable. DOS MBR cannot boot this."));
    if (ct > 1)
	print_warning(_("More than one primary partition is marked bootable. DOS MBR cannot boot this."));
}

static void
fp_printf(FILE *fp, char *format, ...) {
    va_list args;
    char buf[1024];
    int y, x __attribute__((unused));

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (fp == NULL) {
	/* The following works best if the string to be printed has at
           most only one newline. */
	printw("%s", buf);
	getyx(stdscr, y, x);
	if (y >= COMMAND_LINE_Y-2) {
	    menuContinue();
	    erase();
	    move(0, 0);
	}
    } else
	fprintf(fp, "%s", buf);
}

#define MAX_PER_LINE 16
static void
print_file_buffer(FILE *fp, unsigned char *buffer) {
    int i,l;

    for (i = 0, l = 0; i < SECTOR_SIZE; i++, l++) {
	if (l == 0)
	    fp_printf(fp, "0x%03X:", i);
	fp_printf(fp, " %02X", buffer[i]);
	if (l == MAX_PER_LINE - 1) {
	    fp_printf(fp, "\n");
	    l = -1;
	}
    }
    if (l > 0)
	fp_printf(fp, "\n");
    fp_printf(fp, "\n");
}

static void
print_raw_table(void) {
    int i, to_file;
    partition_table buffer;
    char fname[LINE_LENGTH];
    FILE *fp;

    if (print_only) {
	fp = stdout;
	to_file = TRUE;
    } else {
	mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		 _("Enter filename or press RETURN to display on screen: "));

	if ((to_file = get_string(fname, LINE_LENGTH, NULL)) < 0)
	    return;

	if (to_file) {
	    if ((fp = fopen(fname, "w")) == NULL) {
		char errstr[LINE_LENGTH];
		snprintf(errstr, sizeof(errstr),
			 _("Cannot open file '%s'"), fname);
		print_warning(errstr);
		return;
	    }
	} else {
	    fp = NULL;
	    erase();
	    move(0, 0);
	}
    }

    fp_printf(fp, _("Disk Drive: %s\n"), disk_device);

    fp_printf(fp, _("Sector 0:\n"));
    read_sector(buffer.c.b, 0);
    fill_primary_table(&buffer);
    print_file_buffer(fp, buffer.c.b);

    for (i = 0; i < num_parts; i++)
	if (IS_LOGICAL(p_info[i].num)) {
	    fp_printf(fp, _("Sector %d:\n"), p_info[i].first_sector);
	    read_sector(buffer.c.b, p_info[i].first_sector);
	    fill_logical_table(&buffer, &(p_info[i]));
	    print_file_buffer(fp, buffer.c.b);
	}

    if (to_file) {
	if (!print_only)
	    fclose(fp);
    } else {
        menuContinue();
    }
}

static void
print_p_info_entry(FILE *fp, partition_info *p) {
    long long size;
    char part_str[40];

    if (p->id == UNUSABLE)
	fp_printf(fp, _("   None   "));
    else if (p->id == FREE_SPACE && p->num == PRI_OR_LOG)
	fp_printf(fp, _("   Pri/Log"));
    else if (p->id == FREE_SPACE && p->num == PRIMARY)
	fp_printf(fp, _("   Primary"));
    else if (p->id == FREE_SPACE && p->num == LOGICAL)
	fp_printf(fp, _("   Logical"));
    else
	fp_printf(fp, "%2d %-7.7s", p->num+1,
		  IS_LOGICAL(p->num) ? _("Logical") : _("Primary"));

    fp_printf(fp, " ");

    fp_printf(fp, "%11lld%c", p->first_sector,
	      ((p->first_sector/cylinder_size) !=
	       ((float)p->first_sector/cylinder_size) ?
	       '*' : ' '));

    fp_printf(fp, "%11lld%c", p->last_sector,
	      (((p->last_sector+1)/cylinder_size) !=
	       ((float)(p->last_sector+1)/cylinder_size) ?
	       '*' : ' '));

    fp_printf(fp, "%6ld%c", p->offset,
	      ((((p->first_sector == 0 || IS_LOGICAL(p->num)) &&
		 (p->offset != sectors)) ||
		(p->first_sector != 0 && IS_PRIMARY(p->num) &&
		 p->offset != 0)) ?
	       '#' : ' '));

    size = p->last_sector - p->first_sector + 1;
    fp_printf(fp, "%11lld%c", size,
	      ((size/cylinder_size) != ((float)size/cylinder_size) ?
	       '*' : ' '));

    /* fp_printf(fp, " "); */

    if (p->id == UNUSABLE)
	sprintf(part_str, "%.15s", _("Unusable"));
    else if (p->id == FREE_SPACE)
	sprintf(part_str, "%.15s", _("Free Space"));
    else if (partition_type_name(p->id))
	sprintf(part_str, "%.15s (%02X)", _(partition_type_name(p->id)), p->id);
    else
	sprintf(part_str, "%.15s (%02X)", _("Unknown"), p->id);
    fp_printf(fp, "%-20.20s", part_str);

    fp_printf(fp, " ");

    if (p->flags == ACTIVE_FLAG)
	fp_printf(fp, _("Boot"), p->flags);
    else if (p->flags != 0)
	fp_printf(fp, _("(%02X)"), p->flags);
    else
	fp_printf(fp, _("None"), p->flags);

    fp_printf(fp, "\n");
}

static void
print_p_info(void) {
    char fname[LINE_LENGTH];
    FILE *fp;
    int i, to_file, pext = is_extended(ext_info.id);

    if (print_only) {
	fp = stdout;
	to_file = TRUE;
    } else {
	mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		 _("Enter filename or press RETURN to display on screen: "));

	if ((to_file = get_string(fname, LINE_LENGTH, NULL)) < 0)
	    return;

	if (to_file) {
	    if ((fp = fopen(fname, "w")) == NULL) {
		char errstr[LINE_LENGTH];
		snprintf(errstr, LINE_LENGTH, _("Cannot open file '%s'"), fname);
		print_warning(errstr);
		return;
	    }
	} else {
	    fp = NULL;
	    erase();
	    move(0, 0);
	}
    }

    fp_printf(fp, _("Partition Table for %s\n"), disk_device);
    fp_printf(fp, "\n");
    fp_printf(fp, _("               First       Last\n"));
    fp_printf(fp, _(" # Type       Sector      Sector   Offset    Length   Filesystem Type (ID) Flag\n"));
    fp_printf(fp, _("-- ------- ----------- ----------- ------ ----------- -------------------- ----\n"));

    for (i = 0; i < num_parts; i++) {
	if (pext && (p_info[i].first_sector >= ext_info.first_sector)) {
	    print_p_info_entry(fp,&ext_info);
	    pext = FALSE;
	}
	print_p_info_entry(fp, &(p_info[i]));
    }

    if (to_file) {
	if (!print_only)
	    fclose(fp);
    } else {
        menuContinue();
    }
}

static void
print_part_entry(FILE *fp, int num, partition_info *pi) {
    long long first = 0, start = 0, end = 0, size = 0;
    unsigned char ss, es, sh, eh;
    int sc, ec;
    int flags = 0, id = 0;

    ss = sh = es = eh = 0;
    sc = ec = 0;

    if (pi != NULL) {
	flags = pi->flags;
	id = pi->id;

	if (IS_LOGICAL(num))
	    first = pi->offset;
	else
	    first = pi->first_sector + pi->offset;

	start = pi->first_sector + pi->offset;
	end = pi->last_sector;
	size = end - start + 1;

	set_hsc0(&sh, &ss, &sc, start);
	set_hsc0(&eh, &es, &ec, end);
    }

    fp_printf(fp, "%2d  0x%02X %4d %4d %5d 0x%02X %4d %4d %5d %11lld %11lld\n",
	      num+1, flags, sh, ss, sc, id, eh, es, ec, first, size);
}


static void
print_part_table(void) {
    int i, j, to_file;
    char fname[LINE_LENGTH];
    FILE *fp;

    if (print_only) {
	fp = stdout;
	to_file = TRUE;
    } else {
	mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		 _("Enter filename or press RETURN to display on screen: "));

	if ((to_file = get_string(fname, LINE_LENGTH, NULL)) < 0)
	    return;

	if (to_file) {
	    if ((fp = fopen(fname, "w")) == NULL) {
		char errstr[LINE_LENGTH];
		snprintf(errstr, LINE_LENGTH, _("Cannot open file '%s'"), fname);
		print_warning(errstr);
		return;
	    }
	} else {
	    fp = NULL;
	    erase();
	    move(0, 0);
	}
    }

    fp_printf(fp, _("Partition Table for %s\n"), disk_device);
    fp_printf(fp, "\n");
    /* Three-line heading. Read "Start Sector" etc vertically. */
    fp_printf(fp, _("         ---Starting----      ----Ending-----    Start     Number of\n"));
    fp_printf(fp, _(" # Flags Head Sect  Cyl   ID  Head Sect  Cyl     Sector    Sectors\n"));
    fp_printf(fp, _("-- ----- ---- ---- ----- ---- ---- ---- ----- ----------- -----------\n"));

    for (i = 0; i < 4; i++) {
	for (j = 0;
	     j < num_parts && (p_info[j].id <= 0 || p_info[j].num != i);
	     j++);
	if (j < num_parts) {
	    print_part_entry(fp, i, &(p_info[j]));
	} else if (is_extended(ext_info.id) && ext_info.num == i) {
	    print_part_entry(fp, i, &ext_info);
	} else {
	    print_part_entry(fp, i, NULL);
	}
    }

    for (i = 0; i < num_parts; i++)
	if (IS_LOGICAL(p_info[i].num))
	    print_part_entry(fp, p_info[i].num, &(p_info[i]));

    if (to_file) {
	if (!print_only)
	    fclose(fp);
    } else {
        menuContinue();
    }
}

static void
print_tables(void) {
    int done = FALSE;

    static struct MenuItem menuFormat[]=
    {
        { 'r', N_("Raw"), N_("Print the table using raw data format") },
        { 's', N_("Sectors"), N_("Print the table ordered by sectors") },
        { 't', N_("Table"), N_("Just print the partition table") },
        { ESC, N_("Cancel"), N_("Don't print the table") },
        { 0, NULL, NULL }
    };

    while (!done)
	switch ( toupper(menuSimple( menuFormat, 2)) ) {
	case 'R':
	    print_raw_table();
	    done = TRUE;
	    break;
	case 'S':
	    print_p_info();
	    done = TRUE;
	    break;
	case 'T':
	    print_part_table();
	    done = TRUE;
	    break;
	case ESC:
	    done = TRUE;
	    break;
	}
}

#define END_OF_HELP "EOHS!"
static void
display_help(void) {
    char *help_text[] = {
	N_("Help Screen for cfdisk"),
	"",
	N_("This is cfdisk, a curses based disk partitioning program, which"),
	N_("allows you to create, delete and modify partitions on your hard"),
	N_("disk drive."),
	"",
	N_("Copyright (C) 1994-1999 Kevin E. Martin & aeb"),
	"",
	N_("Command      Meaning"),
	N_("-------      -------"),
	N_("  b          Toggle bootable flag of the current partition"),
	N_("  d          Delete the current partition"),
	N_("  g          Change cylinders, heads, sectors-per-track parameters"),
	N_("             WARNING: This option should only be used by people who"),
	N_("             know what they are doing."),
	N_("  h          Print this screen"),
	N_("  m          Maximize disk usage of the current partition"),
	N_("             Note: This may make the partition incompatible with"),
	N_("             DOS, OS/2, ..."),
	N_("  n          Create new partition from free space"),
	N_("  p          Print partition table to the screen or to a file"),
	N_("             There are several different formats for the partition"),
	N_("             that you can choose from:"),
	N_("                r - Raw data (exactly what would be written to disk)"),
	N_("                s - Table ordered by sectors"),
	N_("                t - Table in raw format"),
	N_("  q          Quit program without writing partition table"),
	N_("  t          Change the filesystem type"),
	N_("  u          Change units of the partition size display"),
	N_("             Rotates through MB, sectors and cylinders"),
	N_("  W          Write partition table to disk (must enter upper case W)"),
        N_("             Since this might destroy data on the disk, you must"),
	N_("             either confirm or deny the write by entering `yes' or"),
	N_("             `no'"),
	N_("Up Arrow     Move cursor to the previous partition"),
	N_("Down Arrow   Move cursor to the next partition"),
	N_("CTRL-L       Redraws the screen"),
	N_("  ?          Print this screen"),
	"",
	N_("Note: All of the commands can be entered with either upper or lower"),
	N_("case letters (except for Writes)."),
	END_OF_HELP
    };

    int cur_line = 0;
    FILE *fp = NULL;

    erase();
    move(0, 0);
    while (strcmp(help_text[cur_line], END_OF_HELP)) {
	if (help_text[cur_line][0])
	    fp_printf(fp, "%s\n", _(help_text[cur_line]));
	else
	    fp_printf(fp, "\n");
	cur_line++;
    }
    menuContinue();
}

static int
change_geometry(void) {
    int ret_val = FALSE;
    int done = FALSE;
    char def[LINE_LENGTH];
    char response[LINE_LENGTH];
    long long tmp_val;
    int i;

    while (!done) {
        static struct MenuItem menuGeometry[]=
        {
            { 'c', N_("Cylinders"), N_("Change cylinder geometry") },
            { 'h', N_("Heads"), N_("Change head geometry") },
            { 's', N_("Sectors"), N_("Change sector geometry") },
            { 'd', N_("Done"), N_("Done with changing geometry") },
            { 0, NULL, NULL }
        };
	move(COMMAND_LINE_Y, COMMAND_LINE_X);
	clrtoeol();
	refresh();

	clear_warning();

	switch (toupper( menuSimple(menuGeometry, 3) )) {
	case 'C':
	    sprintf(def, "%llu", actual_size/cylinder_size);
	    mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		     _("Enter the number of cylinders: "));
	    i = get_string(response, LINE_LENGTH, def);
	    if (i == GS_DEFAULT) {
		user_cylinders = actual_size/cylinder_size;
		ret_val = TRUE;
	    } else if (i > 0) {
		tmp_val = atoll(response);
		if (tmp_val > 0) {
		    user_cylinders = tmp_val;
		    ret_val = TRUE;
		} else
		    print_warning(_("Illegal cylinders value"));
	    }
	    break;
	case 'H':
	    sprintf(def, "%d", heads);
	    mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		     _("Enter the number of heads: "));
	    if (get_string(response, LINE_LENGTH, def) > 0) {
		tmp_val = atoll(response);
		if (tmp_val > 0 && tmp_val <= MAX_HEADS) {
		    user_heads = tmp_val;
		    ret_val = TRUE;
		} else
		    print_warning(_("Illegal heads value"));
	    }
	    break;
	case 'S':
	    sprintf(def, "%d", sectors);
	    mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X,
		     _("Enter the number of sectors per track: "));
	    if (get_string(response, LINE_LENGTH, def) > 0) {
		tmp_val = atoll(response);
		if (tmp_val > 0 && tmp_val <= MAX_SECTORS) {
		    user_sectors = tmp_val;
		    ret_val = TRUE;
		} else
		    print_warning(_("Illegal sectors value"));
	    }
	    break;
	case ESC:
	case 'D':
	    done = TRUE;
	    break;
	default:
	    putchar(BELL);
	    break;
	}

	if (ret_val) {
	    decide_on_geometry();
	    draw_screen();
	}
    }

    if (ret_val) {
	long long disk_end;

	disk_end = actual_size-1;

	if (p_info[num_parts-1].last_sector > disk_end) {
	    while (p_info[num_parts-1].first_sector > disk_end) {
		if (p_info[num_parts-1].id == FREE_SPACE ||
		    p_info[num_parts-1].id == UNUSABLE)
		    remove_part(num_parts-1);
		else
		    del_part(num_parts-1);
	    }

	    p_info[num_parts-1].last_sector = disk_end;

	    if (ext_info.last_sector > disk_end)
		ext_info.last_sector = disk_end;
	} else if (p_info[num_parts-1].last_sector < disk_end) {
	    if (p_info[num_parts-1].id == FREE_SPACE ||
		p_info[num_parts-1].id == UNUSABLE) {
		p_info[num_parts-1].last_sector = disk_end;
	    } else {
		insert_empty_part(num_parts,
				  p_info[num_parts-1].last_sector+1,
				  disk_end);
	    }
	}

	/* Make sure the partitions are correct */
	check_part_info();
    }

    return ret_val;
}

static void
change_id(int i) {
    char id[LINE_LENGTH], def[LINE_LENGTH];
    int num_types = 0;
    int num_across, num_down;
    int len, new_id = ((p_info[i].id == LINUX) ? LINUX_SWAP : LINUX);
    int y_start, y_end, row, row_min, row_max, row_offset, j, needmore;

    for (j = 1; i386_sys_types[j].name; j++) ;
    num_types = j-1;		/* do not count the Empty type */

    num_across = COLS/COL_ID_WIDTH;
    num_down = (((float)num_types)/num_across + 1);
    y_start = COMMAND_LINE_Y - 1 - num_down;
    if (y_start < 1) {
	y_start = 1;
	y_end = COMMAND_LINE_Y - 2;
    } else {
	if (y_start > DISK_TABLE_START+cur_part+4)
	    y_start = DISK_TABLE_START+cur_part+4;
	y_end = y_start + num_down - 1;
    }

    row_min = 1;
    row_max = COMMAND_LINE_Y - 2;
    row_offset = 0;
    do {
	for (j = y_start - 1; j <= y_end + 1; j++) {
	    move(j, 0);
	    clrtoeol();
	}
	needmore = 0;
	for (j = 1; i386_sys_types[j].name; j++) {
	    row = y_start + (j-1) % num_down - row_offset;
	    if (row >= row_min && row <= row_max) {
		move(row, ((j-1)/num_down)*COL_ID_WIDTH + 1);
		printw("%02X %-20.20s",
		       i386_sys_types[j].type,
		       _(i386_sys_types[j].name));
	    }
	    if (row > row_max)
		needmore = 1;
	}
	if (needmore)
		menuContinue();
	row_offset += (row_max - row_min + 1);
    } while(needmore);

    sprintf(def, "%02X", new_id);
    mvaddstr(COMMAND_LINE_Y, COMMAND_LINE_X, _("Enter filesystem type: "));
    if ((len = get_string(id, 3, def)) <= 0 && len != GS_DEFAULT)
	return;

    if (len != GS_DEFAULT) {
	if (!isxdigit(id[0]))
	    return;
	new_id = (isdigit(id[0]) ? id[0] - '0' : tolower(id[0]) - 'a' + 10);
	if (len == 2) {
	    if (isxdigit(id[1]))
		new_id = new_id*16 +
		    (isdigit(id[1]) ? id[1] - '0' : tolower(id[1]) - 'a' + 10);
	    else
		return;
	}
    }

    if (new_id == 0)
	print_warning(_("Cannot change FS Type to empty"));
    else if (is_extended(new_id))
	print_warning(_("Cannot change FS Type to extended"));
    else
	p_info[i].id = new_id;
}

static void
draw_partition(int i) {
    int j;
    int y = i + DISK_TABLE_START + 2 - (cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN;
    char *t;
    long long size;
    double fsize;

    if (!arrow_cursor) {
	move(y, 0);
	for (j = 0; j < COLS; j++)
	    addch(' ');
    }

    if (p_info[i].id > 0) {
	char *dbn = my_basename(disk_device);
	int l = strlen(dbn);
	int digit_last = isdigit(dbn[l-1]);

	mvprintw(y, NAME_START,
		 "%s%s%d", dbn, (digit_last ? "p" : ""),
		 p_info[i].num+1);
	if (p_info[i].flags) {
	    if (p_info[i].flags == ACTIVE_FLAG)
		mvaddstr(y, FLAGS_START, _("Boot"));
	    else
		mvprintw(y, FLAGS_START, _("Unk(%02X)"), p_info[i].flags);
	    if (p_info[i].first_sector == 0 || IS_LOGICAL(p_info[i].num)) {
		if (p_info[i].offset != sectors)
		    addstr(_(", NC"));
	    } else {
		if (p_info[i].offset != 0)
		    addstr(_(", NC"));
	    }
	} else {
	    if (p_info[i].first_sector == 0 || IS_LOGICAL(p_info[i].num)) {
		if (p_info[i].offset != sectors)
		    mvaddstr(y, FLAGS_START, _("NC"));
	    } else {
		if (p_info[i].offset != 0)
		    mvaddstr(y, FLAGS_START, _("NC"));
	    }
	}
    }
    mvaddstr(y, PTYPE_START,
	     (p_info[i].id == UNUSABLE ? "" :
	      (IS_LOGICAL(p_info[i].num) ? _("Logical") :
	       (p_info[i].num >= 0 ? _("Primary") :
		(p_info[i].num == PRI_OR_LOG ? _("Pri/Log") :
		 (p_info[i].num == PRIMARY ? _("Primary") : _("Logical")))))));

    t = partition_type_text(i);
    if (t)
	 mvaddstr(y, FSTYPE_START, t);
    else
	 mvprintw(y, FSTYPE_START, _("Unknown (%02X)"), p_info[i].id);

    if (p_info[i].volume_label[0]) {
	int l = strlen(p_info[i].volume_label);
	int s = SIZE_START-5-l;
	mvprintw(y, (s > LABEL_START) ? LABEL_START : s,
		 " [%s]  ", p_info[i].volume_label);
    }

    size = p_info[i].last_sector - p_info[i].first_sector + 1;
    fsize = (double) size * SECTOR_SIZE;
    if (display_units == SECTORS)
	mvprintw(y, SIZE_START, "%11lld", size);
    else if (display_units == CYLINDERS)
	mvprintw(y, SIZE_START, "%11lld", size/cylinder_size);
    else if (display_units == MEGABYTES)
	mvprintw(y, SIZE_START, "%11.2f", ceiling((100*fsize)/(K*K))/100);
    else if (display_units == GIGABYTES)
	mvprintw(y, SIZE_START, "%11.2f", ceiling((100*fsize)/(K*K*K))/100);
    if (size % cylinder_size != 0 ||
	p_info[i].first_sector % cylinder_size != 0)
	mvprintw(y, COLUMNS-1, "*");
}

static void
init_const(void) {
    if (!defined) {
	NAME_START = (((float)NAME_START)/COLUMNS)*COLS;
	FLAGS_START = (((float)FLAGS_START)/COLUMNS)*COLS;
	PTYPE_START = (((float)PTYPE_START)/COLUMNS)*COLS;
	FSTYPE_START = (((float)FSTYPE_START)/COLUMNS)*COLS;
	LABEL_START = (((float)LABEL_START)/COLUMNS)*COLS;
	SIZE_START = (((float)SIZE_START)/COLUMNS)*COLS;
	COMMAND_LINE_X = (((float)COMMAND_LINE_X)/COLUMNS)*COLS;

	COMMAND_LINE_Y = LINES - 4;
	WARNING_START = LINES - 2;

	if ((NUM_ON_SCREEN = COMMAND_LINE_Y - DISK_TABLE_START - 3) <= 0)
	    NUM_ON_SCREEN = 1;

	COLUMNS = COLS;
	defined = TRUE;
    }
}

static void
draw_screen(void) {
    int i;
    char *line;

    line = (char *) xmalloc((COLS+1)*sizeof(char));

    if (warning_last_time) {
	for (i = 0; i < COLS; i++) {
	    move(WARNING_START, i);
	    line[i] = inch();
	}
	line[COLS] = 0;
    }

    erase();

    if (warning_last_time)
	mvaddstr(WARNING_START, 0, line);


    snprintf(line, COLS+1, "cfdisk (%s)", PACKAGE_STRING);
    mvaddstr(HEADER_START, (COLS-strlen(line))/2, line);
    snprintf(line, COLS+1, _("Disk Drive: %s"), disk_device);
    mvaddstr(HEADER_START+2, (COLS-strlen(line))/2, line);
    {
	    long long bytes = actual_size*(long long) SECTOR_SIZE;
	    long long megabytes = bytes/(K*K);

	    if (megabytes < 10000)
		    sprintf(line, _("Size: %lld bytes, %lld MB"),
			    bytes, megabytes);
	    else
		    sprintf(line, _("Size: %lld bytes, %lld.%lld GB"),
			    bytes, megabytes/K, (10*megabytes/K)%10);
    }
    mvaddstr(HEADER_START+3, (COLS-strlen(line))/2, line);
    snprintf(line, COLS+1, _("Heads: %d   Sectors per Track: %d   Cylinders: %lld"),
	    heads, sectors, cylinders);
    mvaddstr(HEADER_START+4, (COLS-strlen(line))/2, line);

    mvaddstr(DISK_TABLE_START, NAME_START, _("Name"));
    mvaddstr(DISK_TABLE_START, FLAGS_START, _("Flags"));
    mvaddstr(DISK_TABLE_START, PTYPE_START-1, _("Part Type"));
    mvaddstr(DISK_TABLE_START, FSTYPE_START, _("FS Type"));
    mvaddstr(DISK_TABLE_START, LABEL_START+1, _("[Label]"));
    if (display_units == SECTORS)
	mvaddstr(DISK_TABLE_START, SIZE_START, _("    Sectors"));
    else if (display_units == CYLINDERS)
	mvaddstr(DISK_TABLE_START, SIZE_START, _("  Cylinders"));
    else if (display_units == MEGABYTES)
	mvaddstr(DISK_TABLE_START, SIZE_START, _("  Size (MB)"));
    else if (display_units == GIGABYTES)
	mvaddstr(DISK_TABLE_START, SIZE_START, _("  Size (GB)"));

    move(DISK_TABLE_START+1, 1);
    for (i = 1; i < COLS-1; i++)
	addch('-');

    if (NUM_ON_SCREEN >= num_parts)
	for (i = 0; i < num_parts; i++)
	    draw_partition(i);
    else
	for (i = (cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN;
	     i < NUM_ON_SCREEN + (cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN &&
	     i < num_parts;
	     i++)
	    draw_partition(i);

    free(line);
}

static void
draw_cursor(int move) {
    if (move != 0 && (cur_part + move < 0 || cur_part + move >= num_parts)) {
	print_warning(_("No more partitions"));
	return;
    }

    if (arrow_cursor)
	mvaddstr(DISK_TABLE_START + cur_part + 2
		 - (cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN, 0, "   ");
    else
	draw_partition(cur_part);

    cur_part += move;

    if (((cur_part - move)/NUM_ON_SCREEN)*NUM_ON_SCREEN !=
	(cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN)
	draw_screen();

    if (arrow_cursor)
	mvaddstr(DISK_TABLE_START + cur_part + 2
		 - (cur_part/NUM_ON_SCREEN)*NUM_ON_SCREEN, 0, "-->");
    else {
	standout();
	draw_partition(cur_part);
	standend();
    }
}

static void
do_curses_fdisk(void) {
    int done = FALSE;
    int command;
    int is_first_run = TRUE;

    static struct MenuItem menuMain[] = {
        { 'b', N_("Bootable"), N_("Toggle bootable flag of the current partition") },
        { 'd', N_("Delete"), N_("Delete the current partition") },
        { 'g', N_("Geometry"), N_("Change disk geometry (experts only)") },
        { 'h', N_("Help"), N_("Print help screen") },
        { 'm', N_("Maximize"), N_("Maximize disk usage of the current partition (experts only)") },
        { 'n', N_("New"), N_("Create new partition from free space") },
        { 'p', N_("Print"), N_("Print partition table to the screen or to a file") },
        { 'q', N_("Quit"), N_("Quit program without writing partition table") },
        { 't', N_("Type"), N_("Change the filesystem type (DOS, Linux, OS/2 and so on)") },
        { 'u', N_("Units"), N_("Change units of the partition size display (MB, sect, cyl)") },
        { 'W', N_("Write"), N_("Write partition table to disk (this might destroy data)") },
        { 0, NULL, NULL }
    };
    curses_started = 1;
    initscr();
    init_const();

    old_SIGINT = signal(SIGINT, die);
    old_SIGTERM = signal(SIGTERM, die);
#ifdef DEBUG
    signal(SIGINT, old_SIGINT);
    signal(SIGTERM, old_SIGTERM);
#endif

    cbreak();
    noecho();
    nonl();

    fill_p_info();

    draw_screen();

    while (!done) {
	char *s;

	draw_cursor(0);

	if (p_info[cur_part].id == FREE_SPACE) {
	    s = ((opentype == O_RDWR) ? "hnpquW" : "hnpqu");
	    command = menuSelect(COMMAND_LINE_Y, COMMAND_LINE_X, menuMain, 10,
	        s, MENU_BUTTON | MENU_ACCEPT_OTHERS, 5);
	} else if (p_info[cur_part].id > 0) {
	    s = ((opentype == O_RDWR) ? "bdhmpqtuW" : "bdhmpqtu");
	    command = menuSelect(COMMAND_LINE_Y, COMMAND_LINE_X, menuMain, 10,
	        s, MENU_BUTTON | MENU_ACCEPT_OTHERS, is_first_run ? 7 : 0);
	} else {
	    s = ((opentype == O_RDWR) ? "hpquW" : "hpqu");
	    command = menuSelect(COMMAND_LINE_Y, COMMAND_LINE_X, menuMain, 10,
	        s, MENU_BUTTON | MENU_ACCEPT_OTHERS, 0);
	}
	is_first_run = FALSE;
	switch ( command ) {
	case 'B':
	case 'b':
	    if (p_info[cur_part].id > 0)
		p_info[cur_part].flags ^= 0x80;
	    else
		print_warning(_("Cannot make this partition bootable"));
	    break;
	case 'D':
	case 'd':
	    if (p_info[cur_part].id > 0) {
		del_part(cur_part);
		if (cur_part >= num_parts)
		    cur_part = num_parts - 1;
		draw_screen();
	    } else
		print_warning(_("Cannot delete an empty partition"));
	    break;
	case 'G':
	case 'g':
	    if (change_geometry())
		draw_screen();
	    break;
	case 'M':
	case 'm':
	    if (p_info[cur_part].id > 0) {
		if (p_info[cur_part].first_sector == 0 ||
		    IS_LOGICAL(p_info[cur_part].num)) {
		    if (p_info[cur_part].offset == sectors)
			p_info[cur_part].offset = 1;
		    else
			p_info[cur_part].offset = sectors;
		    draw_screen();
		} else if (p_info[cur_part].offset != 0)
		    p_info[cur_part].offset = 0;
		else
		    print_warning(_("Cannot maximize this partition"));
	    } else
		print_warning(_("Cannot maximize this partition"));
	    break;
	case 'N':
	case 'n':
	    if (p_info[cur_part].id == FREE_SPACE) {
		new_part(cur_part);
		draw_screen();
	    } else if (p_info[cur_part].id == UNUSABLE)
		print_warning(_("This partition is unusable"));
	    else
		print_warning(_("This partition is already in use"));
	    break;
	case 'P':
	case 'p':
	    print_tables();
	    draw_screen();
	    break;
	case 'Q':
	case 'q':
	    done = TRUE;
	    break;
	case 'T':
	case 't':
	    if (p_info[cur_part].id > 0) {
		change_id(cur_part);
		draw_screen();
	    } else
		print_warning(_("Cannot change the type of an empty partition"));
	    break;
	case 'U':
	case 'u':
	    if (display_units == GIGABYTES)
		display_units = MEGABYTES;
	    else if (display_units == MEGABYTES)
		display_units = SECTORS;
	    else if (display_units == SECTORS)
		display_units = CYLINDERS;
	    else if (display_units == CYLINDERS)
		display_units = MEGABYTES; 	/* not yet GIGA */
	    draw_screen();
	    break;
	case 'W':
	    write_part_table();
	    break;
	case 'H':
	case 'h':
	case '?':
	    display_help();
	    draw_screen();
	    break;
	case KEY_UP:	/* Up arrow key */
	case '\020':	/* ^P */
	case 'k':	/* Vi-like alternative */
	    draw_cursor(-1);
	    break;
	case KEY_DOWN:	/* Down arrow key */
	case '\016':	/* ^N */
	case 'j':	/* Vi-like alternative */
	    draw_cursor(1);
	    break;
	case REDRAWKEY:
	    clear();
	    draw_screen();
	    break;
	case KEY_HOME:
		draw_cursor(-cur_part);
		break;
	case KEY_END:
		draw_cursor(num_parts - cur_part - 1);
		break;
	default:
	    print_warning(_("Illegal command"));
	    putchar(BELL); /* CTRL-G */
	}
    }

    die_x(0);
}

static void
copyright(void) {
    fprintf(stderr, _("Copyright (C) 1994-2002 Kevin E. Martin & aeb\n"));
}

static void
usage(char *prog_name) {
    /* Unfortunately, xgettext does not handle multi-line strings */
    /* so, let's use explicit \n's instead */
    fprintf(stderr, _("\n"
"Usage:\n"
"Print version:\n"
"        %s -v\n"
"Print partition table:\n"
"        %s -P {r|s|t} [options] device\n"
"Interactive use:\n"
"        %s [options] device\n"
"\n"
"Options:\n"
"-a: Use arrow instead of highlighting;\n"
"-z: Start with a zero partition table, instead of reading the pt from disk;\n"
"-c C -h H -s S: Override the kernel's idea of the number of cylinders,\n"
"                the number of heads and the number of sectors/track.\n\n"),
    prog_name, prog_name, prog_name);

    copyright();
}

int
main(int argc, char **argv)
{
    int c;
    int i, len;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    while ((c = getopt(argc, argv, "ac:gh:s:vzP:")) != -1)
	switch (c) {
	case 'a':
	    arrow_cursor = TRUE;
	    break;
	case 'c':
	    user_cylinders = cylinders = atoll(optarg);
	    if (cylinders <= 0) {
		fprintf(stderr, "%s: %s\n", argv[0], _("Illegal cylinders value"));
		exit(1);
	    }
	    break;
	case 'g':
	    use_partition_table_geometry = TRUE;
	    break;
	case 'h':
	    user_heads = heads = atoi(optarg);
	    if (heads <= 0 || heads > MAX_HEADS) {
		fprintf(stderr, "%s: %s\n", argv[0], _("Illegal heads value"));
		exit(1);
	    }
	    break;
	case 's':
	    user_sectors = sectors = atoi(optarg);
	    if (sectors <= 0 || sectors > MAX_SECTORS) {
		fprintf(stderr, "%s: %s\n", argv[0], _("Illegal sectors value"));
		exit(1);
	    }
	    break;
	case 'v':
	    fprintf(stderr, "cfdisk (%s)\n", PACKAGE_STRING);
	    copyright();
	    exit(0);
	case 'z':
	    zero_table = TRUE;
	    break;
	case 'P':
	    len = strlen(optarg);
	    for (i = 0; i < len; i++) {
		switch (optarg[i]) {
		case 'r':
		    print_only |= PRINT_RAW_TABLE;
		    break;
		case 's':
		    print_only |= PRINT_SECTOR_TABLE;
		    break;
		case 't':
		    print_only |= PRINT_PARTITION_TABLE;
		    break;
		default:
		    usage(argv[0]);
		    exit(1);
		}
	    }
	    break;
	default:
	    usage(argv[0]);
	    exit(1);
	}

    if (argc-optind == 1)
	disk_device = argv[optind];
    else if (argc-optind != 0) {
	usage(argv[0]);
	exit(1);
    } else if ((fd = open(DEFAULT_DEVICE, O_RDONLY)) < 0)
	disk_device = ALTERNATE_DEVICE;
    else close(fd);

    if (print_only) {
	fill_p_info();
	if (print_only & PRINT_RAW_TABLE)
	    print_raw_table();
	if (print_only & PRINT_SECTOR_TABLE)
	    print_p_info();
	if (print_only & PRINT_PARTITION_TABLE)
	    print_part_table();
    } else
	do_curses_fdisk();

    return 0;
}

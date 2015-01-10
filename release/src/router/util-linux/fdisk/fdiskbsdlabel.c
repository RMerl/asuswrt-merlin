/*
   NetBSD disklabel editor for Linux fdisk
   Written by Bernhard Fastenrath (fasten@informatik.uni-bonn.de)
   with code from the NetBSD disklabel command:
  
   Copyright (c) 1987, 1988 Regents of the University of California.
   All rights reserved.
  
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:
  	This product includes software developed by the University of
  	California, Berkeley and its contributors.
   4. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
  
   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Changes:
   19990319 - Arnaldo Carvalho de Melo <acme@conectiva.com.br> - i18n/nls

   20000101 - David Huggins-Daines <dhuggins@linuxcare.com> - Better
   support for OSF/1 disklabels on Alpha.
   Also fixed unaligned accesses in alpha_bootblock_checksum()
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include "nls.h"

#include <sys/param.h>

#include "common.h"
#include "fdisk.h"
#define FREEBSD_PARTITION	0xa5
#define NETBSD_PARTITION	0xa9
#define DKTYPENAMES
#include "fdiskbsdlabel.h"

static void xbsd_delete_part (void);
static void xbsd_new_part (void);
static void xbsd_write_disklabel (void);
static int xbsd_create_disklabel (void);
static void xbsd_edit_disklabel (void);
static void xbsd_write_bootstrap (void);
static void xbsd_change_fstype (void);
static int xbsd_get_part_index (int max);
static int xbsd_check_new_partition (int *i);
static void xbsd_list_types (void);
static unsigned short xbsd_dkcksum (struct xbsd_disklabel *lp);
static int xbsd_initlabel  (struct partition *p, struct xbsd_disklabel *d,
			    int pindex);
static int xbsd_readlabel  (struct partition *p, struct xbsd_disklabel *d);
static int xbsd_writelabel (struct partition *p, struct xbsd_disklabel *d);
static void sync_disks (void);

#if defined (__alpha__)
void alpha_bootblock_checksum (char *boot);
#endif

#if !defined (__alpha__)
static int xbsd_translate_fstype (int linux_type);
static void xbsd_link_part (void);
static struct partition *xbsd_part;
static int xbsd_part_index;
#endif

#if defined (__alpha__)
/* We access this through a u_int64_t * when checksumming */
static char disklabelbuffer[BSD_BBSIZE] __attribute__((aligned(8)));
#else
static char disklabelbuffer[BSD_BBSIZE];
#endif

static struct xbsd_disklabel xbsd_dlabel;

#define bsd_cround(n) \
	(display_in_cyl_units ? ((n)/xbsd_dlabel.d_secpercyl) + 1 : (n))

/*
 * Test whether the whole disk has BSD disk label magic.
 *
 * Note: often reformatting with DOS-type label leaves the BSD magic,
 * so this does not mean that there is a BSD disk label.
 */
int
check_osf_label(void) {
	if (xbsd_readlabel (NULL, &xbsd_dlabel) == 0)
		return 0;
	return 1;
}

int
btrydev (char * dev) {
	if (xbsd_readlabel (NULL, &xbsd_dlabel) == 0)
		return -1;
	printf(_("\nBSD label for device: %s\n"), dev);
	xbsd_print_disklabel (0);
	return 0;
}

#if !defined (__alpha__)
static int
hidden(int type) {
	return type ^ 0x10;
}

static int
is_bsd_partition_type(int type) {
	return (type == FREEBSD_PARTITION ||
		type == hidden(FREEBSD_PARTITION) ||
		type == NETBSD_PARTITION ||
		type == hidden(NETBSD_PARTITION));
}
#endif

void
bsd_command_prompt (void)
{
#if !defined (__alpha__)
  int t, ss;
  struct partition *p;

  for (t=0; t<4; t++) {
    p = get_part_table(t);
    if (p && is_bsd_partition_type(p->sys_ind)) {
      xbsd_part = p;
      xbsd_part_index = t;
      ss = get_start_sect(xbsd_part);
      if (ss == 0) {
	fprintf (stderr, _("Partition %s has invalid starting sector 0.\n"),
		 partname(disk_device, t+1, 0));
	return;
      }
      printf (_("Reading disklabel of %s at sector %d.\n"),
	      partname(disk_device, t+1, 0), ss + BSD_LABELSECTOR);
      if (xbsd_readlabel (xbsd_part, &xbsd_dlabel) == 0)
	if (xbsd_create_disklabel () == 0)
	  return;
      break;
    }
  }

  if (t == 4) {
    printf (_("There is no *BSD partition on %s.\n"), disk_device);
    return;
  }

#elif defined (__alpha__)

  if (xbsd_readlabel (NULL, &xbsd_dlabel) == 0)
    if (xbsd_create_disklabel () == 0)
      exit ( EXIT_SUCCESS );

#endif

  while (1) {
    putchar ('\n');
    switch (tolower (read_char (_("BSD disklabel command (m for help): ")))) {
      case 'd':
	xbsd_delete_part ();
	break;
      case 'e':
	xbsd_edit_disklabel ();
	break;
      case 'i':
	xbsd_write_bootstrap ();
	break;
      case 'l':
	xbsd_list_types ();
	break;
      case 'n':
	xbsd_new_part ();
	break;
      case 'p':
	xbsd_print_disklabel (0);
	break;
      case 'q':
	close (fd);
	exit ( EXIT_SUCCESS );
      case 'r':
	return;
      case 's':
	xbsd_print_disklabel (1);
	break;
      case 't':
	xbsd_change_fstype ();
	break;
      case 'u':
	change_units();
	break;
      case 'w':
	xbsd_write_disklabel ();
	break;
#if !defined (__alpha__)
      case 'x':
	xbsd_link_part ();
	break;
#endif
      default:
	print_menu(MAIN_MENU);
	break;
    }
  }
}

static void
xbsd_delete_part (void)
{
  int i;

  i = xbsd_get_part_index (xbsd_dlabel.d_npartitions);
  xbsd_dlabel.d_partitions[i].p_size   = 0;
  xbsd_dlabel.d_partitions[i].p_offset = 0;
  xbsd_dlabel.d_partitions[i].p_fstype = BSD_FS_UNUSED;
  if (xbsd_dlabel.d_npartitions == i + 1)
    while (xbsd_dlabel.d_partitions[xbsd_dlabel.d_npartitions-1].p_size == 0)
      xbsd_dlabel.d_npartitions--;
}

static void
xbsd_new_part (void)
{
  unsigned int begin, end;
  char mesg[256];
  int i;

  if (!xbsd_check_new_partition (&i))
    return;

#if !defined (__alpha__) && !defined (__powerpc__) && !defined (__hppa__)
  begin = get_start_sect(xbsd_part);
  end = begin + get_nr_sects(xbsd_part) - 1;
#else
  begin = 0;
  end = xbsd_dlabel.d_secperunit - 1;
#endif

  snprintf (mesg, sizeof(mesg), _("First %s"), str_units(SINGULAR));
  begin = read_int (bsd_cround (begin), bsd_cround (begin), bsd_cround (end),
		    0, mesg);

  if (display_in_cyl_units)
    begin = (begin - 1) * xbsd_dlabel.d_secpercyl;

  snprintf (mesg, sizeof(mesg), _("Last %s or +size or +sizeM or +sizeK"),
	   str_units(SINGULAR));
  end = read_int (bsd_cround (begin), bsd_cround (end), bsd_cround (end),
		  bsd_cround (begin), mesg);

  if (display_in_cyl_units)
    end = end * xbsd_dlabel.d_secpercyl - 1;

  xbsd_dlabel.d_partitions[i].p_size   = end - begin + 1;
  xbsd_dlabel.d_partitions[i].p_offset = begin;
  xbsd_dlabel.d_partitions[i].p_fstype = BSD_FS_UNUSED;
}

void
xbsd_print_disklabel (int show_all) {
  struct xbsd_disklabel *lp = &xbsd_dlabel;
  struct xbsd_partition *pp;
  FILE *f = stdout;
  int i, j;

  if (show_all) {
#if defined (__alpha__)
    fprintf(f, "# %s:\n", disk_device);
#else
    fprintf(f, "# %s:\n", partname(disk_device, xbsd_part_index+1, 0));
#endif
    if ((unsigned) lp->d_type < BSD_DKMAXTYPES)
      fprintf(f, _("type: %s\n"), xbsd_dktypenames[lp->d_type]);
    else
      fprintf(f, _("type: %d\n"), lp->d_type);
    fprintf(f, _("disk: %.*s\n"), (int) sizeof(lp->d_typename), lp->d_typename);
    fprintf(f, _("label: %.*s\n"), (int) sizeof(lp->d_packname), lp->d_packname);
    fprintf(f, _("flags:"));
    if (lp->d_flags & BSD_D_REMOVABLE)
      fprintf(f, _(" removable"));
    if (lp->d_flags & BSD_D_ECC)
      fprintf(f, _(" ecc"));
    if (lp->d_flags & BSD_D_BADSECT)
      fprintf(f, _(" badsect"));
    fprintf(f, "\n");
    /* On various machines the fields of *lp are short/int/long */
    /* In order to avoid problems, we cast them all to long. */
    fprintf(f, _("bytes/sector: %ld\n"), (long) lp->d_secsize);
    fprintf(f, _("sectors/track: %ld\n"), (long) lp->d_nsectors);
    fprintf(f, _("tracks/cylinder: %ld\n"), (long) lp->d_ntracks);
    fprintf(f, _("sectors/cylinder: %ld\n"), (long) lp->d_secpercyl);
    fprintf(f, _("cylinders: %ld\n"), (long) lp->d_ncylinders);
    fprintf(f, _("rpm: %d\n"), lp->d_rpm);
    fprintf(f, _("interleave: %d\n"), lp->d_interleave);
    fprintf(f, _("trackskew: %d\n"), lp->d_trackskew);
    fprintf(f, _("cylinderskew: %d\n"), lp->d_cylskew);
    fprintf(f, _("headswitch: %ld\t\t# milliseconds\n"),
	    (long) lp->d_headswitch);
    fprintf(f, _("track-to-track seek: %ld\t# milliseconds\n"),
	    (long) lp->d_trkseek);
    fprintf(f, _("drivedata: "));
    for (i = NDDATA - 1; i >= 0; i--)
      if (lp->d_drivedata[i])
	break;
    if (i < 0)
      i = 0;
    for (j = 0; j <= i; j++)
      fprintf(f, "%ld ", (long) lp->d_drivedata[j]);
  }
  fprintf (f, _("\n%d partitions:\n"), lp->d_npartitions);
  fprintf (f, _("#       start       end      size     fstype   [fsize bsize   cpg]\n"));
  pp = lp->d_partitions;
  for (i = 0; i < lp->d_npartitions; i++, pp++) {
    if (pp->p_size) {
      if (display_in_cyl_units && lp->d_secpercyl) {
	fprintf(f, "  %c: %8ld%c %8ld%c %8ld%c  ",
		'a' + i,
		(long) pp->p_offset / lp->d_secpercyl + 1,
		(pp->p_offset % lp->d_secpercyl) ? '*' : ' ',
		(long) (pp->p_offset + pp->p_size + lp->d_secpercyl - 1)
			/ lp->d_secpercyl,
		((pp->p_offset + pp->p_size) % lp->d_secpercyl) ? '*' : ' ',
		(long) pp->p_size / lp->d_secpercyl,
		(pp->p_size % lp->d_secpercyl) ? '*' : ' ');
      } else {
	fprintf(f, "  %c: %8ld  %8ld  %8ld   ",
		'a' + i,
		(long) pp->p_offset,
		(long) pp->p_offset + pp->p_size - 1,
		(long) pp->p_size);
      }
      if ((unsigned) pp->p_fstype < BSD_FSMAXTYPES)
	fprintf(f, "%8.8s", xbsd_fstypes[pp->p_fstype].name);
      else
	fprintf(f, "%8x", pp->p_fstype);
      switch (pp->p_fstype) {
	case BSD_FS_UNUSED:
	  fprintf(f, "    %5ld %5ld %5.5s ",
		  (long) pp->p_fsize, (long) pp->p_fsize * pp->p_frag, "");
	  break;
	  
	case BSD_FS_BSDFFS:
	  fprintf(f, "    %5ld %5ld %5d ",
		  (long) pp->p_fsize, (long) pp->p_fsize * pp->p_frag,
		  pp->p_cpg);
	  break;
	  
	default:
	  fprintf(f, "%22.22s", "");
	  break;
      }
      fprintf(f, "\n");
    }
  }
}

static void
xbsd_write_disklabel (void) {
#if defined (__alpha__)
	printf (_("Writing disklabel to %s.\n"), disk_device);
	xbsd_writelabel (NULL, &xbsd_dlabel);
#else
	printf (_("Writing disklabel to %s.\n"),
		partname(disk_device, xbsd_part_index+1, 0));
	xbsd_writelabel (xbsd_part, &xbsd_dlabel);
#endif
	reread_partition_table(0);	/* no exit yet */
}

static int
xbsd_create_disklabel (void) {
	char c;

#if defined (__alpha__)
	fprintf (stderr, _("%s contains no disklabel.\n"), disk_device);
#else
	fprintf (stderr, _("%s contains no disklabel.\n"),
		 partname(disk_device, xbsd_part_index+1, 0));
#endif

	while (1) {
		c = read_char (_("Do you want to create a disklabel? (y/n) "));
		if (tolower(c) == 'y') {
			if (xbsd_initlabel (
#if defined (__alpha__) || defined (__powerpc__) || defined (__hppa__) || \
    defined (__s390__) || defined (__s390x__)
				NULL, &xbsd_dlabel, 0
#else
				xbsd_part, &xbsd_dlabel, xbsd_part_index
#endif
				) == 1) {
				xbsd_print_disklabel (1);
				return 1;
			} else
				return 0;
		} else if (c == 'n')
			return 0;
	}
}

static int
edit_int (int def, char *mesg)
{
  do {
    fputs (mesg, stdout);
    printf (" (%d): ", def);
    if (!read_line (NULL))
      return def;
  }
  while (!isdigit (*line_ptr));
  return atoi (line_ptr);
} 

static void
xbsd_edit_disklabel (void)
{
  struct xbsd_disklabel *d;

  d = &xbsd_dlabel;

#if defined (__alpha__) || defined (__ia64__)
  d -> d_secsize    = (unsigned long) edit_int ((unsigned long) d -> d_secsize     ,_("bytes/sector"));
  d -> d_nsectors   = (unsigned long) edit_int ((unsigned long) d -> d_nsectors    ,_("sectors/track"));
  d -> d_ntracks    = (unsigned long) edit_int ((unsigned long) d -> d_ntracks     ,_("tracks/cylinder"));
  d -> d_ncylinders = (unsigned long) edit_int ((unsigned long) d -> d_ncylinders  ,_("cylinders"));
#endif

  /* d -> d_secpercyl can be != d -> d_nsectors * d -> d_ntracks */
  while (1)
  {
    d -> d_secpercyl = (unsigned long) edit_int ((unsigned long) d -> d_nsectors * d -> d_ntracks,
					  _("sectors/cylinder"));
    if (d -> d_secpercyl <= d -> d_nsectors * d -> d_ntracks)
      break;

    printf (_("Must be <= sectors/track * tracks/cylinder (default).\n"));
  }
  d -> d_rpm        = (unsigned short) edit_int ((unsigned short) d -> d_rpm       ,_("rpm"));
  d -> d_interleave = (unsigned short) edit_int ((unsigned short) d -> d_interleave,_("interleave"));
  d -> d_trackskew  = (unsigned short) edit_int ((unsigned short) d -> d_trackskew ,_("trackskew"));
  d -> d_cylskew    = (unsigned short) edit_int ((unsigned short) d -> d_cylskew   ,_("cylinderskew"));
  d -> d_headswitch = (unsigned long) edit_int ((unsigned long) d -> d_headswitch  ,_("headswitch"));
  d -> d_trkseek    = (unsigned long) edit_int ((unsigned long) d -> d_trkseek     ,_("track-to-track seek"));

  d -> d_secperunit = d -> d_secpercyl * d -> d_ncylinders;
}

static int
xbsd_get_bootstrap (char *path, void *ptr, int size)
{
  int fd;

  if ((fd = open (path, O_RDONLY)) < 0)
  {
    perror (path);
    return 0;
  }
  if (read (fd, ptr, size) < 0)
  {
    perror (path);
    close (fd);
    return 0;
  }
  printf (" ... %s\n", path);
  close (fd);
  return 1;
}

static void
xbsd_write_bootstrap (void)
{
  char *bootdir = BSD_LINUX_BOOTDIR;
  char path[sizeof(BSD_LINUX_BOOTDIR) + 1 + 2 + 4];  /* BSD_LINUX_BOOTDIR + / + {sd,wd} + boot */
  char *dkbasename;
  struct xbsd_disklabel dl;
  char *d, *p, *e;
  int sector;

  if (xbsd_dlabel.d_type == BSD_DTYPE_SCSI)
    dkbasename = "sd";
  else
    dkbasename = "wd";

  printf (_("Bootstrap: %sboot -> boot%s (%s): "),
	  dkbasename, dkbasename, dkbasename);
  if (read_line (NULL)) {
    line_ptr[strlen (line_ptr)-1] = '\0';
    dkbasename = line_ptr;
  }
  snprintf (path, sizeof(path), "%s/%sboot", bootdir, dkbasename);
  if (!xbsd_get_bootstrap (path, disklabelbuffer, (int) xbsd_dlabel.d_secsize))
    return;

  /* We need a backup of the disklabel (xbsd_dlabel might have changed). */
  d = &disklabelbuffer[BSD_LABELSECTOR * SECTOR_SIZE];
  memmove (&dl, d, sizeof (struct xbsd_disklabel));

  /* The disklabel will be overwritten by 0's from bootxx anyway */
  memset (d, 0, sizeof (struct xbsd_disklabel));

  snprintf (path, sizeof(path), "%s/boot%s", bootdir, dkbasename);
  if (!xbsd_get_bootstrap (path, &disklabelbuffer[xbsd_dlabel.d_secsize],
			  (int) xbsd_dlabel.d_bbsize - xbsd_dlabel.d_secsize))
    return;

  e = d + sizeof (struct xbsd_disklabel);
  for (p=d; p < e; p++)
    if (*p) {
      fprintf (stderr, _("Bootstrap overlaps with disk label!\n"));
      exit ( EXIT_FAILURE );
    }

  memmove (d, &dl, sizeof (struct xbsd_disklabel));

#if defined (__powerpc__) || defined (__hppa__)
  sector = 0;
#elif defined (__alpha__)
  sector = 0;
  alpha_bootblock_checksum (disklabelbuffer);
#else
  sector = get_start_sect(xbsd_part);
#endif

  if (lseek (fd, (off_t) sector * SECTOR_SIZE, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (BSD_BBSIZE != write (fd, disklabelbuffer, BSD_BBSIZE))
    fatal (unable_to_write);

#if defined (__alpha__)
  printf (_("Bootstrap installed on %s.\n"), disk_device);
#else
  printf (_("Bootstrap installed on %s.\n"),
    partname (disk_device, xbsd_part_index+1, 0));
#endif

  sync_disks ();
}

static void
xbsd_change_fstype (void)
{
  int i;

  i = xbsd_get_part_index (xbsd_dlabel.d_npartitions);
  xbsd_dlabel.d_partitions[i].p_fstype = read_hex (xbsd_fstypes);
}

static int
xbsd_get_part_index (int max)
{
  char prompt[256];
  char l;

  snprintf (prompt, sizeof(prompt), _("Partition (a-%c): "), 'a' + max - 1);
  do
     l = tolower (read_char (prompt));
  while (l < 'a' || l > 'a' + max - 1);
  return l - 'a';
}

static int
xbsd_check_new_partition (int *i) {

	/* room for more? various BSD flavours have different maxima */
	if (xbsd_dlabel.d_npartitions == BSD_MAXPARTITIONS) {
		int t;

		for (t = 0; t < BSD_MAXPARTITIONS; t++)
			if (xbsd_dlabel.d_partitions[t].p_size == 0)
				break;

		if (t == BSD_MAXPARTITIONS) {
			fprintf (stderr, _("The maximum number of partitions "
					   "has been created\n"));
			return 0;
		}
	}

	*i = xbsd_get_part_index (BSD_MAXPARTITIONS);

	if (*i >= xbsd_dlabel.d_npartitions)
		xbsd_dlabel.d_npartitions = (*i) + 1;

	if (xbsd_dlabel.d_partitions[*i].p_size != 0) {
		fprintf (stderr, _("This partition already exists.\n"));
		return 0;
	}

	return 1;
}

static void
xbsd_list_types (void) {
	list_types (xbsd_fstypes);
}

static unsigned short
xbsd_dkcksum (struct xbsd_disklabel *lp) {
	unsigned short *start, *end;
	unsigned short sum = 0;
  
	start = (unsigned short *) lp;
	end = (unsigned short *) &lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return sum;
}

static int
xbsd_initlabel (struct partition *p, struct xbsd_disklabel *d,
		int pindex __attribute__((__unused__))) {
	struct xbsd_partition *pp;
	struct geom g;

	get_geometry (fd, &g);
	memset (d, 0, sizeof (struct xbsd_disklabel));

	d -> d_magic = BSD_DISKMAGIC;

	if (strncmp (disk_device, "/dev/sd", 7) == 0)
		d -> d_type = BSD_DTYPE_SCSI;
	else
		d -> d_type = BSD_DTYPE_ST506;

#if 0 /* not used (at least not written to disk) by NetBSD/i386 1.0 */
	d -> d_subtype = BSD_DSTYPE_INDOSPART & pindex;
#endif

#if !defined (__alpha__)
	d -> d_flags = BSD_D_DOSPART;
#else
	d -> d_flags = 0;
#endif
	d -> d_secsize = SECTOR_SIZE;		/* bytes/sector  */
	d -> d_nsectors = g.sectors;		/* sectors/track */
	d -> d_ntracks = g.heads;		/* tracks/cylinder (heads) */
	d -> d_ncylinders = g.cylinders;
	d -> d_secpercyl  = g.sectors * g.heads;/* sectors/cylinder */
	if (d -> d_secpercyl == 0)
		d -> d_secpercyl = 1;		/* avoid segfaults */
	d -> d_secperunit = d -> d_secpercyl * d -> d_ncylinders;

	d -> d_rpm = 3600;
	d -> d_interleave = 1;
	d -> d_trackskew = 0;
	d -> d_cylskew = 0;
	d -> d_headswitch = 0;
	d -> d_trkseek = 0;

	d -> d_magic2 = BSD_DISKMAGIC;
	d -> d_bbsize = BSD_BBSIZE;
	d -> d_sbsize = BSD_SBSIZE;

#if !defined (__alpha__)
	d -> d_npartitions = 4;
	pp = &d -> d_partitions[2];		/* Partition C should be
						   the NetBSD partition */
	pp -> p_offset = get_start_sect(p);
	pp -> p_size   = get_nr_sects(p);
	pp -> p_fstype = BSD_FS_UNUSED;
	pp = &d -> d_partitions[3];		/* Partition D should be
						   the whole disk */
	pp -> p_offset = 0;
	pp -> p_size   = d -> d_secperunit;
	pp -> p_fstype = BSD_FS_UNUSED;
#elif defined (__alpha__)
	d -> d_npartitions = 3;
	pp = &d -> d_partitions[2];		/* Partition C should be
						   the whole disk */
	pp -> p_offset = 0;
	pp -> p_size   = d -> d_secperunit;
	pp -> p_fstype = BSD_FS_UNUSED;  
#endif

	return 1;
}

/*
 * Read a xbsd_disklabel from sector 0 or from the starting sector of p.
 * If it has the right magic, return 1.
 */
static int
xbsd_readlabel (struct partition *p, struct xbsd_disklabel *d)
{
	int t, sector;

	/* p is used only to get the starting sector */
#if !defined (__alpha__)
	sector = (p ? get_start_sect(p) : 0);
#elif defined (__alpha__)
	sector = 0;
#endif

	if (lseek (fd, (off_t) sector * SECTOR_SIZE, SEEK_SET) == -1)
		fatal (unable_to_seek);
	if (BSD_BBSIZE != read (fd, disklabelbuffer, BSD_BBSIZE))
		fatal (unable_to_read);

	memmove (d,
	         &disklabelbuffer[BSD_LABELSECTOR * SECTOR_SIZE + BSD_LABELOFFSET],
	         sizeof (struct xbsd_disklabel));

	if (d -> d_magic != BSD_DISKMAGIC || d -> d_magic2 != BSD_DISKMAGIC)
		return 0;

	for (t = d -> d_npartitions; t < BSD_MAXPARTITIONS; t++) {
		d -> d_partitions[t].p_size   = 0;
		d -> d_partitions[t].p_offset = 0;
		d -> d_partitions[t].p_fstype = BSD_FS_UNUSED;  
	}

	if (d -> d_npartitions > BSD_MAXPARTITIONS)
		fprintf (stderr, _("Warning: too many partitions "
				   "(%d, maximum is %d).\n"),
			 d -> d_npartitions, BSD_MAXPARTITIONS);
	return 1;
}

static int
xbsd_writelabel (struct partition *p, struct xbsd_disklabel *d)
{
  unsigned int sector;

#if !defined (__alpha__) && !defined (__powerpc__) && !defined (__hppa__)
  sector = get_start_sect(p) + BSD_LABELSECTOR;
#else
  sector = BSD_LABELSECTOR;
#endif

  d -> d_checksum = 0;
  d -> d_checksum = xbsd_dkcksum (d);

  /* This is necessary if we want to write the bootstrap later,
     otherwise we'd write the old disklabel with the bootstrap.
  */
  memmove (&disklabelbuffer[BSD_LABELSECTOR * SECTOR_SIZE + BSD_LABELOFFSET], d,
           sizeof (struct xbsd_disklabel));

#if defined (__alpha__) && BSD_LABELSECTOR == 0
  alpha_bootblock_checksum (disklabelbuffer);
  if (lseek (fd, (off_t) 0, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (BSD_BBSIZE != write (fd, disklabelbuffer, BSD_BBSIZE))
    fatal (unable_to_write);
#else
  if (lseek (fd, (off_t) sector * SECTOR_SIZE + BSD_LABELOFFSET,
		   SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (sizeof (struct xbsd_disklabel) != write (fd, d, sizeof (struct xbsd_disklabel)))
    fatal (unable_to_write);
#endif

  sync_disks ();

  return 1;
}

static void
sync_disks (void)
{
  printf (_("\nSyncing disks.\n"));
  sync ();
  sleep (4);
}

#if !defined (__alpha__)
static int
xbsd_translate_fstype (int linux_type)
{
  switch (linux_type)
  {
    case 0x01: /* DOS 12-bit FAT   */
    case 0x04: /* DOS 16-bit <32M  */
    case 0x06: /* DOS 16-bit >=32M */
    case 0xe1: /* DOS access       */
    case 0xe3: /* DOS R/O          */
    case 0xf2: /* DOS secondary    */
      return BSD_FS_MSDOS;
    case 0x07: /* OS/2 HPFS        */
      return BSD_FS_HPFS;
    default:
      return BSD_FS_OTHER;
  }
}

static void
xbsd_link_part (void)
{
  int k, i;
  struct partition *p;

  k = get_partition (1, partitions);

  if (!xbsd_check_new_partition (&i))
    return;

  p = get_part_table(k);

  xbsd_dlabel.d_partitions[i].p_size   = get_nr_sects(p);
  xbsd_dlabel.d_partitions[i].p_offset = get_start_sect(p);
  xbsd_dlabel.d_partitions[i].p_fstype = xbsd_translate_fstype(p->sys_ind);
}
#endif

#if defined (__alpha__)

#if !defined(__GLIBC__)
typedef unsigned long long u_int64_t;
#endif

void
alpha_bootblock_checksum (char *boot)
{
  u_int64_t *dp, sum;
  int i;
  
  dp = (u_int64_t *)boot;
  sum = 0;
  for (i = 0; i < 63; i++)
    sum += dp[i];
  dp[63] = sum;
}
#endif /* __alpha__ */

/*
 * fdisksunlabel.c
 *
 * I think this is mostly, or entirely, due to
 * 	Jakub Jelinek (jj@sunsite.mff.cuni.cz), July 1996
 *
 * Merged with fdisk for other architectures, aeb, June 1998.
 *
 * Sat Mar 20 EST 1999 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *      Internationalization
 */

#include <stdio.h>		/* stderr */
#include <stdlib.h>		/* qsort */
#include <string.h>		/* strstr */
#include <unistd.h>		/* write */
#include <sys/ioctl.h>		/* ioctl */
#include <sys/stat.h>		/* stat */
#include <sys/sysmacros.h>	/* major */

#include "nls.h"
#include "blkdev.h"

#include <endian.h>
#ifdef HAVE_LINUX_MAJOR_H
#include <linux/major.h>	/* FLOPPY_MAJOR */
#endif

#include "common.h"
#include "fdisk.h"
#include "fdisksunlabel.h"

static int     other_endian = 0;
static int     scsi_disk = 0;
static int     floppy = 0;

struct systypes sun_sys_types[] = {
	{SUN_TAG_UNASSIGNED, N_("Unassigned")},
	{SUN_TAG_BOOT, N_("Boot")},
	{SUN_TAG_ROOT, N_("SunOS root")},
	{SUN_TAG_SWAP, N_("SunOS swap")},
	{SUN_TAG_USR, N_("SunOS usr")},
	{SUN_TAG_BACKUP, N_("Whole disk")},
	{SUN_TAG_STAND, N_("SunOS stand")},
	{SUN_TAG_VAR, N_("SunOS var")},
	{SUN_TAG_HOME, N_("SunOS home")},
	{SUN_TAG_ALTSCTR, N_("SunOS alt sectors")},
	{SUN_TAG_CACHE, N_("SunOS cachefs")},
	{SUN_TAG_RESERVED, N_("SunOS reserved")},
	{SUN_TAG_LINUX_SWAP, N_("Linux swap")},
	{SUN_TAG_LINUX_NATIVE, N_("Linux native")},
	{SUN_TAG_LINUX_LVM, N_("Linux LVM")},
	{SUN_TAG_LINUX_RAID, N_("Linux raid autodetect")},
	{ 0, NULL }
};

static inline unsigned short __swap16(unsigned short x) {
        return (((uint16_t)(x) & 0xFF) << 8) | (((uint16_t)(x) & 0xFF00) >> 8);
}
static inline uint32_t __swap32(uint32_t x) {
        return (((uint32_t)(x) & 0xFF) << 24) | (((uint32_t)(x) & 0xFF00) << 8) | (((uint32_t)(x) & 0xFF0000) >> 8) | (((uint32_t)(x) & 0xFF000000) >> 24);
}

#define SSWAP16(x) (other_endian ? __swap16(x) \
				 : (uint16_t)(x))
#define SSWAP32(x) (other_endian ? __swap32(x) \
				 : (uint32_t)(x))

#ifndef FLOPPY_MAJOR
#define FLOPPY_MAJOR 2
#endif
#ifndef IDE0_MAJOR
#define IDE0_MAJOR 3
#endif
#ifndef IDE1_MAJOR
#define IDE1_MAJOR 22
#endif
void guess_device_type(int fd)
{
	struct stat bootstat;

	if (fstat (fd, &bootstat) < 0) {
                scsi_disk = 0;
                floppy = 0;
	} else if (S_ISBLK(bootstat.st_mode)
		   && (major(bootstat.st_rdev) == IDE0_MAJOR ||
		       major(bootstat.st_rdev) == IDE1_MAJOR)) {
                scsi_disk = 0;
                floppy = 0;
	} else if (S_ISBLK(bootstat.st_mode)
		   && major(bootstat.st_rdev) == FLOPPY_MAJOR) {
                scsi_disk = 0;
                floppy = 1;
	} else {
                scsi_disk = 1;
                floppy = 0;
	}
}

static void set_sun_partition(int i, uint32_t start, uint32_t stop, uint16_t sysid)
{
	sunlabel->part_tags[i].tag = SSWAP16(sysid);
	sunlabel->part_tags[i].flag = SSWAP16(0);
	sunlabel->partitions[i].start_cylinder =
		SSWAP32(start / (heads * sectors));
	sunlabel->partitions[i].num_sectors =
		SSWAP32(stop - start);
	set_changed(i);
}

void sun_nolabel(void)
{
	sunlabel->magic = 0;
	partitions = 4;
}

int check_sun_label(void)
{
	unsigned short *ush;
	int csum;

	if (sunlabel->magic != SUN_LABEL_MAGIC &&
	    sunlabel->magic != SUN_LABEL_MAGIC_SWAPPED) {
		other_endian = 0;
		return 0;
	}
	other_endian = (sunlabel->magic == SUN_LABEL_MAGIC_SWAPPED);

	ush = ((unsigned short *) (sunlabel + 1)) - 1;
	for (csum = 0; ush >= (unsigned short *)sunlabel;)
		csum ^= *ush--;

	if (csum) {
		fprintf(stderr,_("Detected sun disklabel with wrong checksum.\n"
				"Probably you'll have to set all the values,\n"
				"e.g. heads, sectors, cylinders and partitions\n"
				"or force a fresh label (s command in main menu)\n"));
	} else {
		int need_fixing = 0;

		heads = SSWAP16(sunlabel->nhead);
		cylinders = SSWAP16(sunlabel->ncyl);
		sectors = SSWAP16(sunlabel->nsect);

		if (sunlabel->version != SSWAP32(SUN_LABEL_VERSION)) {
			fprintf(stderr,_("Detected sun disklabel with wrong version [0x%08x].\n"),
				SSWAP32(sunlabel->version));
			need_fixing = 1;
		}
		if (sunlabel->sanity != SSWAP32(SUN_LABEL_SANE)) {
			fprintf(stderr,_("Detected sun disklabel with wrong sanity [0x%08x].\n"),
				SSWAP32(sunlabel->sanity));
			need_fixing = 1;
		}
		if (sunlabel->num_partitions != SSWAP16(SUN_NUM_PARTITIONS)) {
			fprintf(stderr,_("Detected sun disklabel with wrong num_partitions [%u].\n"),
				SSWAP16(sunlabel->num_partitions));
			need_fixing = 1;
		}
		if (need_fixing) {
			fprintf(stderr, _("Warning: Wrong values need to be "
					  "fixed up and will be corrected "
					  "by w(rite)\n"));
			sunlabel->version = SSWAP32(SUN_LABEL_VERSION);
			sunlabel->sanity = SSWAP32(SUN_LABEL_SANE);
			sunlabel->num_partitions = SSWAP16(SUN_NUM_PARTITIONS);

			ush = (unsigned short *)sunlabel;
			csum = 0;
			while(ush < (unsigned short *)(&sunlabel->cksum))
				csum ^= *ush++;
			sunlabel->cksum = csum;

			set_changed(0);
		}
	}
	update_units();
	disklabel = SUN_LABEL;
	partitions = SUN_NUM_PARTITIONS;
	return 1;
}

void create_sunlabel(void)
{
	struct hd_geometry geometry;
	unsigned long long llsectors, llcyls;
	unsigned int ndiv, sec_fac;
	int res;

	fprintf(stderr,
	_("Building a new sun disklabel. Changes will remain in memory only,\n"
       	"until you decide to write them. After that, of course, the previous\n"
	"content won't be recoverable.\n\n"));
#if BYTE_ORDER == LITTLE_ENDIAN
	other_endian = 1;
#else
	other_endian = 0;
#endif
	zeroize_mbr_buffer();
	sunlabel->magic = SSWAP16(SUN_LABEL_MAGIC);
	sunlabel->sanity = SSWAP32(SUN_LABEL_SANE);
	sunlabel->version = SSWAP32(SUN_LABEL_VERSION);
	sunlabel->num_partitions = SSWAP16(SUN_NUM_PARTITIONS);

	res = blkdev_get_sectors(fd, &llsectors);
	sec_fac = sector_size / 512;

#ifdef HDIO_GETGEO
	if (!ioctl(fd, HDIO_GETGEO, &geometry)) {
	        heads = geometry.heads;
	        sectors = geometry.sectors;
		if (res == 0) {
			llcyls = llsectors / (heads * sectors * sec_fac);
			cylinders = llcyls;
			if (cylinders != llcyls)
				cylinders = ~0;
		} else {
			cylinders = geometry.cylinders;
			fprintf(stderr,
				_("Warning:  BLKGETSIZE ioctl failed on %s.  "
				  "Using geometry cylinder value of %d.\n"
				  "This value may be truncated for devices"
				  " > 33.8 GB.\n"), disk_device, cylinders);
		}
	} else
#endif
	{
	        heads = read_int(1,1,1024,0,_("Heads"));
		sectors = read_int(1,1,1024,0,_("Sectors/track"));
		cylinders = read_int(1,1,65535,0,_("Cylinders"));
	}

	sunlabel->acyl   = SSWAP16(2);
	sunlabel->pcyl   = SSWAP16(cylinders);
	sunlabel->ncyl   = SSWAP16(cylinders - 2);
	sunlabel->rpm    = SSWAP16(5400);
	sunlabel->intrlv = SSWAP16(1);
	sunlabel->apc    = SSWAP16(0);

	sunlabel->nhead = SSWAP16(heads);
	sunlabel->nsect = SSWAP16(sectors);
	sunlabel->ncyl = SSWAP16(cylinders);

	snprintf(sunlabel->label_id, sizeof(sunlabel->label_id),
		 "Linux cyl %d alt %d hd %d sec %llu",
		 cylinders, SSWAP16(sunlabel->acyl), heads, sectors);

	if (cylinders * heads * sectors >= 150 * 2048) {
	        ndiv = cylinders - (50 * 2048 / (heads * sectors)); /* 50M swap */
	} else
	        ndiv = cylinders * 2 / 3;

	set_sun_partition(0, 0, ndiv * heads * sectors,
			  SUN_TAG_LINUX_NATIVE);
	set_sun_partition(1, ndiv * heads * sectors,
			  cylinders * heads * sectors,
			  SUN_TAG_LINUX_SWAP);
	sunlabel->part_tags[1].flag |= SSWAP16(SUN_FLAG_UNMNT);

	set_sun_partition(2, 0, cylinders * heads * sectors, SUN_TAG_BACKUP);

	{
		unsigned short *ush = (unsigned short *)sunlabel;
		unsigned short csum = 0;
		while(ush < (unsigned short *)(&sunlabel->cksum))
			csum ^= *ush++;
		sunlabel->cksum = csum;
	}

	set_all_unchanged();
	get_boot(create_empty_sun);
	set_changed(0);
}

void toggle_sunflags(int i, uint16_t mask)
{
	struct sun_tag_flag *p = &sunlabel->part_tags[i];

	p->flag ^= SSWAP16(mask);

	set_changed(i);
}

static void fetch_sun(uint32_t *starts, uint32_t *lens, uint32_t *start, uint32_t *stop)
{
	int i, continuous = 1;

	*start = 0;
	*stop = cylinders * heads * sectors;

	for (i = 0; i < partitions; i++) {
		struct sun_partition *part = &sunlabel->partitions[i];
		struct sun_tag_flag *tag = &sunlabel->part_tags[i];

		if (part->num_sectors &&
		    tag->tag != SSWAP16(SUN_TAG_UNASSIGNED) &&
		    tag->tag != SSWAP16(SUN_TAG_BACKUP)) {
			starts[i] = (SSWAP32(part->start_cylinder) *
				     heads * sectors);
			lens[i] = SSWAP32(part->num_sectors);
			if (continuous) {
				if (starts[i] == *start)
					*start += lens[i];
				else if (starts[i] + lens[i] >= *stop)
					*stop = starts[i];
				else
					continuous = 0;
				        /* There will be probably more gaps
					  than one, so lets check afterwards */
			}
		} else {
			starts[i] = 0;
			lens[i] = 0;
		}
	}
}

static unsigned int *verify_sun_starts;

static int verify_sun_cmp(int *a, int *b)
{
    if (*a == -1)
	    return 1;
    if (*b == -1)
	    return -1;
    if (verify_sun_starts[*a] > verify_sun_starts[*b])
	    return 1;
    return -1;
}

void verify_sun(void)
{
    uint32_t starts[SUN_NUM_PARTITIONS], lens[SUN_NUM_PARTITIONS], start, stop;
    uint32_t i,j,k,starto,endo;
    int array[SUN_NUM_PARTITIONS];

    verify_sun_starts = starts;

    fetch_sun(starts, lens, &start, &stop);

    for (k = 0; k < 7; k++) {
	for (i = 0; i < SUN_NUM_PARTITIONS; i++) {
	    if (k && (lens[i] % (heads * sectors))) {
	        printf(_("Partition %d doesn't end on cylinder boundary\n"), i+1);
	    }
	    if (lens[i]) {
	        for (j = 0; j < i; j++)
	            if (lens[j]) {
	                if (starts[j] == starts[i]+lens[i]) {
	                    starts[j] = starts[i]; lens[j] += lens[i];
	                    lens[i] = 0;
	                } else if (starts[i] == starts[j]+lens[j]){
	                    lens[j] += lens[i];
	                    lens[i] = 0;
	                } else if (!k) {
	                    if (starts[i] < starts[j]+lens[j] &&
				starts[j] < starts[i]+lens[i]) {
	                        starto = starts[i];
	                        if (starts[j] > starto)
					starto = starts[j];
	                        endo = starts[i]+lens[i];
	                        if (starts[j]+lens[j] < endo)
					endo = starts[j]+lens[j];
	                        printf(_("Partition %d overlaps with others in "
				       "sectors %d-%d\n"), i+1, starto, endo);
	                    }
	                }
	            }
	    }
	}
    }
    for (i = 0; i < SUN_NUM_PARTITIONS; i++) {
        if (lens[i])
            array[i] = i;
        else
            array[i] = -1;
    }
    qsort(array,ARRAY_SIZE(array),sizeof(array[0]),
	  (int (*)(const void *,const void *)) verify_sun_cmp);
    if (array[0] == -1) {
    	printf(_("No partitions defined\n"));
    	return;
    }
    stop = cylinders * heads * sectors;
    if (starts[array[0]])
        printf(_("Unused gap - sectors 0-%d\n"), starts[array[0]]);
    for (i = 0; i < 7 && array[i+1] != -1; i++) {
        printf(_("Unused gap - sectors %d-%d\n"),
	       (starts[array[i]] + lens[array[i]]),
	       starts[array[i+1]]);
    }
    start = (starts[array[i]] + lens[array[i]]);
    if (start < stop)
        printf(_("Unused gap - sectors %d-%d\n"), start, stop);
}

void add_sun_partition(int n, int sys)
{
	uint32_t starts[SUN_NUM_PARTITIONS], lens[SUN_NUM_PARTITIONS];
	struct sun_partition *part = &sunlabel->partitions[n];
	struct sun_tag_flag *tag = &sunlabel->part_tags[n];
	uint32_t start, stop, stop2;
	int whole_disk = 0;
		
	char mesg[256];
	int i;
	unsigned int first, last;

	if (part->num_sectors && tag->tag != SSWAP16(SUN_TAG_UNASSIGNED)) {
		printf(_("Partition %d is already defined.  Delete "
			"it before re-adding it.\n"), n + 1);
		return;
	}
	
	fetch_sun(starts, lens, &start, &stop);
	if (stop <= start) {
		if (n == 2)
			whole_disk = 1;
		else {
			printf(_("Other partitions already cover the whole disk.\nDelete "
			       "some/shrink them before retry.\n"));
			return;
		}
	}
	snprintf(mesg, sizeof(mesg), _("First %s"), str_units(SINGULAR));
	for (;;) {
		if (whole_disk)
			first = read_int(0, 0, 0, 0, mesg);
		else
			first = read_int(scround(start), scround(stop)+1,
					 scround(stop), 0, mesg);
		if (display_in_cyl_units)
			first *= units_per_sector;
		else {
			/* Starting sector has to be properly aligned */
			int cs = heads * sectors;
			int x = first % cs;

			if (x)
				first += cs - x;
		}
		if (n == 2 && first != 0)
			printf (_("\
It is highly recommended that the third partition covers the whole disk\n\
and is of type `Whole disk'\n"));
		/* ewt asks to add: "don't start a partition at cyl 0"
		   However, edmundo@rano.demon.co.uk writes:
		   "In addition to having a Sun partition table, to be able to
		   boot from the disc, the first partition, /dev/sdX1, must
		   start at cylinder 0. This means that /dev/sdX1 contains
		   the partition table and the boot block, as these are the
		   first two sectors of the disc. Therefore you must be
		   careful what you use /dev/sdX1 for. In particular, you must
		   not use a partition starting at cylinder 0 for Linux swap,
		   as that would overwrite the partition table and the boot
		   block. You may, however, use such a partition for a UFS
		   or EXT2 file system, as these file systems leave the first
		   1024 bytes undisturbed. */
		/* On the other hand, one should not use partitions
		   starting at block 0 in an md, or the label will
		   be trashed. */
		for (i = 0; i < partitions; i++)
			if (lens[i] && starts[i] <= first
			            && starts[i] + lens[i] > first)
				break;
		if (i < partitions && !whole_disk) {
			if (n == 2 && !first) {
			    whole_disk = 1;
			    break;
			}
			printf(_("Sector %d is already allocated\n"), first);
		} else
			break;
	}
	stop = cylinders * heads * sectors;	/* ancient */
	stop2 = stop;
	for (i = 0; i < partitions; i++) {
		if (starts[i] > first && starts[i] < stop)
			stop = starts[i];
	}
	snprintf(mesg, sizeof(mesg),
		 _("Last %s or +size or +sizeM or +sizeK"),
		 str_units(SINGULAR));
	if (whole_disk)
		last = read_int(scround(stop2), scround(stop2), scround(stop2),
				0, mesg);
	else if (n == 2 && !first)
		last = read_int(scround(first), scround(stop2), scround(stop2),
				scround(first), mesg);
	else
		last = read_int(scround(first), scround(stop), scround(stop),
				scround(first), mesg);
	if (display_in_cyl_units)
		last *= units_per_sector;
	if (n == 2 && !first) {
		if (last >= stop2) {
		    whole_disk = 1;
		    last = stop2;
		} else if (last > stop) {
		    printf (
   _("You haven't covered the whole disk with the 3rd partition, but your value\n"
     "%d %s covers some other partition. Your entry has been changed\n"
     "to %d %s\n"),
			scround(last), str_units(SINGULAR),
			scround(stop), str_units(SINGULAR));
		    last = stop;
		}
	} else if (!whole_disk && last > stop)
		last = stop;

	if (whole_disk)
		sys = SUN_TAG_BACKUP;

	set_sun_partition(n, first, last, sys);
}

void sun_delete_partition(int i)
{
	struct sun_partition *part = &sunlabel->partitions[i];
	struct sun_tag_flag *tag = &sunlabel->part_tags[i];
	unsigned int nsec;

	if (i == 2 &&
	    tag->tag == SSWAP16(SUN_TAG_BACKUP) &&
	    !part->start_cylinder &&
	    (nsec = SSWAP32(part->num_sectors))
	      == heads * sectors * cylinders)
		printf(_("If you want to maintain SunOS/Solaris compatibility, "
		       "consider leaving this\n"
		       "partition as Whole disk (5), starting at 0, with %u "
		       "sectors\n"), nsec);
	tag->tag = SSWAP16(SUN_TAG_UNASSIGNED);
	part->num_sectors = 0;
}

int sun_change_sysid(int i, uint16_t sys)
{
	struct sun_partition *part = &sunlabel->partitions[i];
	struct sun_tag_flag *tag = &sunlabel->part_tags[i];

	if (sys == SUN_TAG_LINUX_SWAP && !part->start_cylinder) {
	    read_chars(
	      _("It is highly recommended that the partition at offset 0\n"
	      "is UFS, EXT2FS filesystem or SunOS swap. Putting Linux swap\n"
	      "there may destroy your partition table and bootblock.\n"
	      "Type YES if you're very sure you would like that partition\n"
	      "tagged with 82 (Linux swap): "));
	    if (strcmp (line_ptr, _("YES\n")))
		    return 0;
	}
	switch (sys) {
	case SUN_TAG_SWAP:
	case SUN_TAG_LINUX_SWAP:
		/* swaps are not mountable by default */
		tag->flag |= SSWAP16(SUN_FLAG_UNMNT);
		break;
	default:
		/* assume other types are mountable;
		   user can change it anyway */
		tag->flag &= ~SSWAP16(SUN_FLAG_UNMNT);
		break;
	}
	tag->tag = SSWAP16(sys);
	return 1;
}

void sun_list_table(int xtra)
{
	int i, w;
	char *type;

	w = strlen(disk_device);
	if (xtra)
		printf(
		_("\nDisk %s (Sun disk label): %u heads, %llu sectors, %d rpm\n"
		"%u cylinders, %d alternate cylinders, %d physical cylinders\n"
		"%d extra sects/cyl, interleave %d:1\n"
		"Label ID: %s\n"
		"Volume ID: %s\n"
		"Units = %s of %d * 512 bytes\n\n"),
		       disk_device, heads, sectors, SSWAP16(sunlabel->rpm),
		       cylinders, SSWAP16(sunlabel->acyl),
		       SSWAP16(sunlabel->pcyl),
		       SSWAP16(sunlabel->apc),
		       SSWAP16(sunlabel->intrlv),
		       sunlabel->label_id,
		       sunlabel->volume_id,
		       str_units(PLURAL), units_per_sector);
	else
		printf(
	_("\nDisk %s (Sun disk label): %u heads, %llu sectors, %u cylinders\n"
	"Units = %s of %d * 512 bytes\n\n"),
		       disk_device, heads, sectors, cylinders,
		       str_units(PLURAL), units_per_sector);

	printf(_("%*s Flag    Start       End    Blocks   Id  System\n"),
	       w + 1, _("Device"));
	for (i = 0 ; i < partitions; i++) {
		struct sun_partition *part = &sunlabel->partitions[i];
		struct sun_tag_flag *tag = &sunlabel->part_tags[i];

		if (part->num_sectors) {
			uint32_t start = SSWAP32(part->start_cylinder) * heads * sectors;
			uint32_t len = SSWAP32(part->num_sectors);
			printf(
			    "%s %c%c %9lu %9lu %9lu%c  %2x  %s\n",
/* device */		  partname(disk_device, i+1, w),
/* flags */		  (tag->flag & SSWAP16(SUN_FLAG_UNMNT)) ? 'u' : ' ',
			  (tag->flag & SSWAP16(SUN_FLAG_RONLY)) ? 'r' : ' ',
/* start */		  (unsigned long) scround(start),
/* end */		  (unsigned long) scround(start+len),
/* odd flag on end */	  (unsigned long) len / 2, len & 1 ? '+' : ' ',
/* type id */		  SSWAP16(tag->tag),
/* type name */		  (type = partition_type(SSWAP16(tag->tag)))
			        ? type : _("Unknown"));
		}
	}
}

void sun_set_alt_cyl(void)
{
	sunlabel->acyl =
		SSWAP16(read_int(0,SSWAP16(sunlabel->acyl), 65535, 0,
				 _("Number of alternate cylinders")));
}

void sun_set_ncyl(int cyl)
{
	sunlabel->ncyl = SSWAP16(cyl);
}

void sun_set_xcyl(void)
{
	sunlabel->apc =
		SSWAP16(read_int(0, SSWAP16(sunlabel->apc), sectors, 0,
				 _("Extra sectors per cylinder")));
}

void sun_set_ilfact(void)
{
	sunlabel->intrlv =
		SSWAP16(read_int(1, SSWAP16(sunlabel->intrlv), 32, 0,
				 _("Interleave factor")));
}

void sun_set_rspeed(void)
{
	sunlabel->rpm =
		SSWAP16(read_int(1, SSWAP16(sunlabel->rpm), 100000, 0,
				 _("Rotation speed (rpm)")));
}

void sun_set_pcylcount(void)
{
	sunlabel->pcyl =
		SSWAP16(read_int(0, SSWAP16(sunlabel->pcyl), 65535, 0,
				 _("Number of physical cylinders")));
}

void sun_write_table(void)
{
	unsigned short *ush = (unsigned short *)sunlabel;
	unsigned short csum = 0;

	while(ush < (unsigned short *)(&sunlabel->cksum))
		csum ^= *ush++;
	sunlabel->cksum = csum;
	if (lseek(fd, 0, SEEK_SET) < 0)
		fatal(unable_to_seek);
	if (write(fd, sunlabel, SECTOR_SIZE) != SECTOR_SIZE)
		fatal(unable_to_write);
}

int sun_get_sysid(int i)
{
	return SSWAP16(sunlabel->part_tags[i].tag);
}

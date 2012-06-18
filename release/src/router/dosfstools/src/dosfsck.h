/* dosfsck.h  -  Common data structures and global variables

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.

   On Debian systems, the complete text of the GNU General Public License
   can be found in /usr/share/common-licenses/GPL-3 file.
*/

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#ifndef _DOSFSCK_H
#define _DOSFSCK_H

#include <sys/types.h>
#define _LINUX_STAT_H		/* hack to avoid inclusion of <linux/stat.h> */
#define _LINUX_STRING_H_	/* hack to avoid inclusion of <linux/string.h> */
#define _LINUX_FS_H		/* hack to avoid inclusion of <linux/fs.h> */

#include <asm/types.h>
#include <asm/byteorder.h>

#include <linux/msdos_fs.h>

#undef CF_LE_W
#undef CF_LE_L
#undef CT_LE_W
#undef CT_LE_L

#if __BYTE_ORDER == __BIG_ENDIAN
#include <byteswap.h>
#define CF_LE_W(v) bswap_16(v)
#define CF_LE_L(v) bswap_32(v)
#define CT_LE_W(v) CF_LE_W(v)
#define CT_LE_L(v) CF_LE_L(v)
#else
#define CF_LE_W(v) (v)
#define CF_LE_L(v) (v)
#define CT_LE_W(v) (v)
#define CT_LE_L(v) (v)
#endif /* __BIG_ENDIAN */

#define VFAT_LN_ATTR (ATTR_RO | ATTR_HIDDEN | ATTR_SYS | ATTR_VOLUME)

/* ++roman: Use own definition of boot sector structure -- the kernel headers'
 * name for it is msdos_boot_sector in 2.0 and fat_boot_sector in 2.1 ... */
struct boot_sector {
    __u8 ignored[3];		/* Boot strap short or near jump */
    __u8 system_id[8];		/* Name - can be used to special case
				   partition manager volumes */
    __u8 sector_size[2];	/* bytes per logical sector */
    __u8 cluster_size;		/* sectors/cluster */
    __u16 reserved;		/* reserved sectors */
    __u8 fats;			/* number of FATs */
    __u8 dir_entries[2];	/* root directory entries */
    __u8 sectors[2];		/* number of sectors */
    __u8 media;			/* media code (unused) */
    __u16 fat_length;		/* sectors/FAT */
    __u16 secs_track;		/* sectors per track */
    __u16 heads;		/* number of heads */
    __u32 hidden;		/* hidden sectors (unused) */
    __u32 total_sect;		/* number of sectors (if sectors == 0) */

    /* The following fields are only used by FAT32 */
    __u32 fat32_length;		/* sectors/FAT */
    __u16 flags;		/* bit 8: fat mirroring, low 4: active fat */
    __u8 version[2];		/* major, minor filesystem version */
    __u32 root_cluster;		/* first cluster in root directory */
    __u16 info_sector;		/* filesystem info sector */
    __u16 backup_boot;		/* backup boot sector */
    __u8 reserved2[12];		/* Unused */

    __u8 drive_number;		/* Logical Drive Number */
    __u8 reserved3;		/* Unused */

    __u8 extended_sig;		/* Extended Signature (0x29) */
    __u32 serial;		/* Serial number */
    __u8 label[11];		/* FS label */
    __u8 fs_type[8];		/* FS Type */

    /* fill up to 512 bytes */
    __u8 junk[422];
} __attribute__ ((packed));

struct boot_sector_16 {
    __u8 ignored[3];		/* Boot strap short or near jump */
    __u8 system_id[8];		/* Name - can be used to special case
				   partition manager volumes */
    __u8 sector_size[2];	/* bytes per logical sector */
    __u8 cluster_size;		/* sectors/cluster */
    __u16 reserved;		/* reserved sectors */
    __u8 fats;			/* number of FATs */
    __u8 dir_entries[2];	/* root directory entries */
    __u8 sectors[2];		/* number of sectors */
    __u8 media;			/* media code (unused) */
    __u16 fat_length;		/* sectors/FAT */
    __u16 secs_track;		/* sectors per track */
    __u16 heads;		/* number of heads */
    __u32 hidden;		/* hidden sectors (unused) */
    __u32 total_sect;		/* number of sectors (if sectors == 0) */

    __u8 drive_number;		/* Logical Drive Number */
    __u8 reserved2;		/* Unused */

    __u8 extended_sig;		/* Extended Signature (0x29) */
    __u32 serial;		/* Serial number */
    __u8 label[11];		/* FS label */
    __u8 fs_type[8];		/* FS Type */

    /* fill up to 512 bytes */
    __u8 junk[450];
} __attribute__ ((packed));

struct info_sector {
    __u32 magic;		/* Magic for info sector ('RRaA') */
    __u8 junk[0x1dc];
    __u32 reserved1;		/* Nothing as far as I can tell */
    __u32 signature;		/* 0x61417272 ('rrAa') */
    __u32 free_clusters;	/* Free cluster count.  -1 if unknown */
    __u32 next_cluster;		/* Most recently allocated cluster. */
    __u32 reserved2[3];
    __u16 reserved3;
    __u16 boot_sign;
};

typedef struct {
    __u8 name[8], ext[3];	/* name and extension */
    __u8 attr;			/* attribute bits */
    __u8 lcase;			/* Case for base and extension */
    __u8 ctime_ms;		/* Creation time, milliseconds */
    __u16 ctime;		/* Creation time */
    __u16 cdate;		/* Creation date */
    __u16 adate;		/* Last access date */
    __u16 starthi;		/* High 16 bits of cluster in FAT32 */
    __u16 time, date, start;	/* time, date and first cluster */
    __u32 size;			/* file size (in bytes) */
} __attribute__ ((packed)) DIR_ENT;

typedef struct _dos_file {
    DIR_ENT dir_ent;
    char *lfn;
    loff_t offset;
    loff_t lfn_offset;
    struct _dos_file *parent;	/* parent directory */
    struct _dos_file *next;	/* next entry */
    struct _dos_file *first;	/* first entry (directory only) */
} DOS_FILE;

typedef struct {
    unsigned long value;
    unsigned long reserved;
} FAT_ENTRY;

typedef struct {
    int nfats;
    loff_t fat_start;
    unsigned int fat_size;	/* unit is bytes */
    unsigned int fat_bits;	/* size of a FAT entry */
    unsigned int eff_fat_bits;	/* # of used bits in a FAT entry */
    unsigned long root_cluster;	/* 0 for old-style root dir */
    loff_t root_start;
    unsigned int root_entries;
    loff_t data_start;
    unsigned int cluster_size;
    unsigned long clusters;
    loff_t fsinfo_start;	/* 0 if not present */
    long free_clusters;
    loff_t backupboot_start;	/* 0 if not present */
    unsigned char *fat;
    DOS_FILE **cluster_owner;
    char *label;
} DOS_FS;

#ifndef offsetof
#define offsetof(t,e)	((int)&(((t *)0)->e))
#endif

extern int interactive, rw, list, verbose, test, write_immed;
extern int atari_format;
extern unsigned n_files;
extern void *mem_queue;

/* value to use as end-of-file marker */
#define FAT_EOF(fs)	((atari_format ? 0xfff : 0xff8) | FAT_EXTD(fs))
#define FAT_IS_EOF(fs,v) ((unsigned long)(v) >= (0xff8|FAT_EXTD(fs)))
/* value to mark bad clusters */
#define FAT_BAD(fs)	(0xff7 | FAT_EXTD(fs))
/* range of values used for bad clusters */
#define FAT_MIN_BAD(fs)	((atari_format ? 0xff0 : 0xff7) | FAT_EXTD(fs))
#define FAT_MAX_BAD(fs)	((atari_format ? 0xff7 : 0xff7) | FAT_EXTD(fs))
#define FAT_IS_BAD(fs,v) ((v) >= FAT_MIN_BAD(fs) && (v) <= FAT_MAX_BAD(fs))

/* return -16 as a number with fs->fat_bits bits */
#define FAT_EXTD(fs)	(((1 << fs->eff_fat_bits)-1) & ~0xf)

/* marker for files with no 8.3 name */
#define FAT_NO_83NAME 32

#endif

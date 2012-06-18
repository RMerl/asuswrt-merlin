/*
 * layout.h - Ntfs on-disk layout structures.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2005 Anton Altaparmakov
 * Copyright (c)      2005 Yura Pakhuchiy
 * Copyright (c) 2005-2006 Szabolcs Szakacsits
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_LAYOUT_H
#define _NTFS_LAYOUT_H

#include "types.h"
#include "endians.h"
#include "support.h"

/* The NTFS oem_id */
#define magicNTFS	const_cpu_to_le64(0x202020205346544e)	/* "NTFS    " */
#define NTFS_SB_MAGIC	0x5346544e				/* 'NTFS' */

/*
 * Location of bootsector on partition:
 *	The standard NTFS_BOOT_SECTOR is on sector 0 of the partition.
 *	On NT4 and above there is one backup copy of the boot sector to
 *	be found on the last sector of the partition (not normally accessible
 *	from within Windows as the bootsector contained number of sectors
 *	value is one less than the actual value!).
 *	On versions of NT 3.51 and earlier, the backup copy was located at
 *	number of sectors/2 (integer divide), i.e. in the middle of the volume.
 */

/**
 * struct BIOS_PARAMETER_BLOCK - BIOS parameter block (bpb) structure.
 */
typedef struct {
	u16 bytes_per_sector;		/* Size of a sector in bytes. */
	u8  sectors_per_cluster;	/* Size of a cluster in sectors. */
	u16 reserved_sectors;		/* zero */
	u8  fats;			/* zero */
	u16 root_entries;		/* zero */
	u16 sectors;			/* zero */
	u8  media_type;			/* 0xf8 = hard disk */
	u16 sectors_per_fat;		/* zero */
/*0x0d*/u16 sectors_per_track;		/* Required to boot Windows. */
/*0x0f*/u16 heads;			/* Required to boot Windows. */
/*0x11*/u32 hidden_sectors;		/* Offset to the start of the partition
					   relative to the disk in sectors.
					   Required to boot Windows. */
/*0x15*/u32 large_sectors;		/* zero */
/* sizeof() = 25 (0x19) bytes */
} __attribute__((__packed__)) BIOS_PARAMETER_BLOCK;

/**
 * struct NTFS_BOOT_SECTOR - NTFS boot sector structure.
 */
typedef struct {
	u8  jump[3];			/* Irrelevant (jump to boot up code).*/
	u64 oem_id;			/* Magic "NTFS    ". */
/*0x0b*/BIOS_PARAMETER_BLOCK bpb;	/* See BIOS_PARAMETER_BLOCK. */
	u8 physical_drive;		/* 0x00 floppy, 0x80 hard disk */
	u8 current_head;		/* zero */
	u8 extended_boot_signature; 	/* 0x80 */
	u8 reserved2;			/* zero */
/*0x28*/s64 number_of_sectors;		/* Number of sectors in volume. Gives
					   maximum volume size of 2^63 sectors.
					   Assuming standard sector size of 512
					   bytes, the maximum byte size is
					   approx. 4.7x10^21 bytes. (-; */
	s64 mft_lcn;			/* Cluster location of mft data. */
	s64 mftmirr_lcn;		/* Cluster location of copy of mft. */
	s8  clusters_per_mft_record;	/* Mft record size in clusters. */
	u8  reserved0[3];		/* zero */
	s8  clusters_per_index_record;	/* Index block size in clusters. */
	u8  reserved1[3];		/* zero */
	u64 volume_serial_number;	/* Irrelevant (serial number). */
	u32 checksum;			/* Boot sector checksum. */
/*0x54*/u8  bootstrap[426];		/* Irrelevant (boot up code). */
	u16 end_of_sector_marker;	/* End of bootsector magic. Always is
					   0xaa55 in little endian. */
/* sizeof() = 512 (0x200) bytes */
} __attribute__((__packed__)) NTFS_BOOT_SECTOR;

/**
 * enum NTFS_RECORD_TYPES -
 *
 * Magic identifiers present at the beginning of all ntfs record containing
 * records (like mft records for example).
 */
typedef enum {
	/* Found in $MFT/$DATA. */
	magic_FILE = const_cpu_to_le32(0x454c4946), /* Mft entry. */
	magic_INDX = const_cpu_to_le32(0x58444e49), /* Index buffer. */
	magic_HOLE = const_cpu_to_le32(0x454c4f48), /* ? (NTFS 3.0+?) */

	/* Found in $LogFile/$DATA. */
	magic_RSTR = const_cpu_to_le32(0x52545352), /* Restart page. */
	magic_RCRD = const_cpu_to_le32(0x44524352), /* Log record page. */

	/* Found in $LogFile/$DATA.  (May be found in $MFT/$DATA, also?) */
	magic_CHKD = const_cpu_to_le32(0x444b4843), /* Modified by chkdsk. */

	/* Found in all ntfs record containing records. */
	magic_BAAD = const_cpu_to_le32(0x44414142), /* Failed multi sector
						       transfer was detected. */

	/*
	 * Found in $LogFile/$DATA when a page is full or 0xff bytes and is
	 * thus not initialized.  User has to initialize the page before using
	 * it.
	 */
	magic_empty = const_cpu_to_le32(0xffffffff),/* Record is empty and has
						       to be initialized before
						       it can be used. */
} NTFS_RECORD_TYPES;

/*
 * Generic magic comparison macros. Finally found a use for the ## preprocessor
 * operator! (-8
 */
#define ntfs_is_magic(x, m)	(   (u32)(x) == (u32)magic_##m )
#define ntfs_is_magicp(p, m)	( *(u32*)(p) == (u32)magic_##m )

/*
 * Specialised magic comparison macros for the NTFS_RECORD_TYPES defined above.
 */
#define ntfs_is_file_record(x)	( ntfs_is_magic (x, FILE) )
#define ntfs_is_file_recordp(p)	( ntfs_is_magicp(p, FILE) )
#define ntfs_is_mft_record(x)	( ntfs_is_file_record(x) )
#define ntfs_is_mft_recordp(p)	( ntfs_is_file_recordp(p) )
#define ntfs_is_indx_record(x)	( ntfs_is_magic (x, INDX) )
#define ntfs_is_indx_recordp(p)	( ntfs_is_magicp(p, INDX) )
#define ntfs_is_hole_record(x)	( ntfs_is_magic (x, HOLE) )
#define ntfs_is_hole_recordp(p)	( ntfs_is_magicp(p, HOLE) )

#define ntfs_is_rstr_record(x)	( ntfs_is_magic (x, RSTR) )
#define ntfs_is_rstr_recordp(p)	( ntfs_is_magicp(p, RSTR) )
#define ntfs_is_rcrd_record(x)	( ntfs_is_magic (x, RCRD) )
#define ntfs_is_rcrd_recordp(p)	( ntfs_is_magicp(p, RCRD) )

#define ntfs_is_chkd_record(x)	( ntfs_is_magic (x, CHKD) )
#define ntfs_is_chkd_recordp(p)	( ntfs_is_magicp(p, CHKD) )

#define ntfs_is_baad_record(x)	( ntfs_is_magic (x, BAAD) )
#define ntfs_is_baad_recordp(p)	( ntfs_is_magicp(p, BAAD) )

#define ntfs_is_empty_record(x)		( ntfs_is_magic (x, empty) )
#define ntfs_is_empty_recordp(p)	( ntfs_is_magicp(p, empty) )


#define NTFS_BLOCK_SIZE		512
#define NTFS_BLOCK_SIZE_BITS	9

/**
 * struct NTFS_RECORD -
 *
 * The Update Sequence Array (usa) is an array of the u16 values which belong
 * to the end of each sector protected by the update sequence record in which
 * this array is contained. Note that the first entry is the Update Sequence
 * Number (usn), a cyclic counter of how many times the protected record has
 * been written to disk. The values 0 and -1 (ie. 0xffff) are not used. All
 * last u16's of each sector have to be equal to the usn (during reading) or
 * are set to it (during writing). If they are not, an incomplete multi sector
 * transfer has occurred when the data was written.
 * The maximum size for the update sequence array is fixed to:
 *	maximum size = usa_ofs + (usa_count * 2) = 510 bytes
 * The 510 bytes comes from the fact that the last u16 in the array has to
 * (obviously) finish before the last u16 of the first 512-byte sector.
 * This formula can be used as a consistency check in that usa_ofs +
 * (usa_count * 2) has to be less than or equal to 510.
 */
typedef struct {
	NTFS_RECORD_TYPES magic;/* A four-byte magic identifying the
				   record type and/or status. */
	u16 usa_ofs;		/* Offset to the Update Sequence Array (usa)
				   from the start of the ntfs record. */
	u16 usa_count;		/* Number of u16 sized entries in the usa
				   including the Update Sequence Number (usn),
				   thus the number of fixups is the usa_count
				   minus 1. */
} __attribute__((__packed__)) NTFS_RECORD;

/**
 * enum NTFS_SYSTEM_FILES - System files mft record numbers.
 *
 * All these files are always marked as used in the bitmap attribute of the
 * mft; presumably in order to avoid accidental allocation for random other
 * mft records. Also, the sequence number for each of the system files is
 * always equal to their mft record number and it is never modified.
 */
typedef enum {
	FILE_MFT	= 0,	/* Master file table (mft). Data attribute
				   contains the entries and bitmap attribute
				   records which ones are in use (bit==1). */
	FILE_MFTMirr	= 1,	/* Mft mirror: copy of first four mft records
				   in data attribute. If cluster size > 4kiB,
				   copy of first N mft records, with
					N = cluster_size / mft_record_size. */
	FILE_LogFile	= 2,	/* Journalling log in data attribute. */
	FILE_Volume	= 3,	/* Volume name attribute and volume information
				   attribute (flags and ntfs version). Windows
				   refers to this file as volume DASD (Direct
				   Access Storage Device). */
	FILE_AttrDef	= 4,	/* Array of attribute definitions in data
				   attribute. */
	FILE_root	= 5,	/* Root directory. */
	FILE_Bitmap	= 6,	/* Allocation bitmap of all clusters (lcns) in
				   data attribute. */
	FILE_Boot	= 7,	/* Boot sector (always at cluster 0) in data
				   attribute. */
	FILE_BadClus	= 8,	/* Contains all bad clusters in the non-resident
				   data attribute. */
	FILE_Secure	= 9,	/* Shared security descriptors in data attribute
				   and two indexes into the descriptors.
				   Appeared in Windows 2000. Before that, this
				   file was named $Quota but was unused. */
	FILE_UpCase	= 10,	/* Uppercase equivalents of all 65536 Unicode
				   characters in data attribute. */
	FILE_Extend	= 11,	/* Directory containing other system files (eg.
				   $ObjId, $Quota, $Reparse and $UsnJrnl). This
				   is new to NTFS3.0. */
	FILE_reserved12	= 12,	/* Reserved for future use (records 12-15). */
	FILE_reserved13	= 13,
	FILE_reserved14	= 14,
	FILE_reserved15	= 15,
	FILE_first_user	= 16,	/* First user file, used as test limit for
				   whether to allow opening a file or not. */
} NTFS_SYSTEM_FILES;

/**
 * enum MFT_RECORD_FLAGS -
 *
 * These are the so far known MFT_RECORD_* flags (16-bit) which contain
 * information about the mft record in which they are present.
 * 
 * MFT_RECORD_IS_4 exists on all $Extend sub-files.
 * It seems that it marks it is a metadata file with MFT record >24, however,
 * it is unknown if it is limited to metadata files only.
 *
 * MFT_RECORD_IS_VIEW_INDEX exists on every metafile with a non directory
 * index, that means an INDEX_ROOT and an INDEX_ALLOCATION with a name other
 * than "$I30". It is unknown if it is limited to metadata files only.
 */
typedef enum {
	MFT_RECORD_IN_USE		= const_cpu_to_le16(0x0001),
	MFT_RECORD_IS_DIRECTORY		= const_cpu_to_le16(0x0002),
	MFT_RECORD_IS_4			= const_cpu_to_le16(0x0004),
	MFT_RECORD_IS_VIEW_INDEX	= const_cpu_to_le16(0x0008),
	MFT_REC_SPACE_FILLER		= 0xffff, /* Just to make flags
						     16-bit. */
} __attribute__((__packed__)) MFT_RECORD_FLAGS;

/*
 * mft references (aka file references or file record segment references) are
 * used whenever a structure needs to refer to a record in the mft.
 *
 * A reference consists of a 48-bit index into the mft and a 16-bit sequence
 * number used to detect stale references.
 *
 * For error reporting purposes we treat the 48-bit index as a signed quantity.
 *
 * The sequence number is a circular counter (skipping 0) describing how many
 * times the referenced mft record has been (re)used. This has to match the
 * sequence number of the mft record being referenced, otherwise the reference
 * is considered stale and removed (FIXME: only ntfsck or the driver itself?).
 *
 * If the sequence number is zero it is assumed that no sequence number
 * consistency checking should be performed.
 *
 * FIXME: Since inodes are 32-bit as of now, the driver needs to always check
 * for high_part being 0 and if not either BUG(), cause a panic() or handle
 * the situation in some other way. This shouldn't be a problem as a volume has
 * to become HUGE in order to need more than 32-bits worth of mft records.
 * Assuming the standard mft record size of 1kb only the records (never mind
 * the non-resident attributes, etc.) would require 4Tb of space on their own
 * for the first 32 bits worth of records. This is only if some strange person
 * doesn't decide to foul play and make the mft sparse which would be a really
 * horrible thing to do as it would trash our current driver implementation. )-:
 * Do I hear screams "we want 64-bit inodes!" ?!? (-;
 *
 * FIXME: The mft zone is defined as the first 12% of the volume. This space is
 * reserved so that the mft can grow contiguously and hence doesn't become
 * fragmented. Volume free space includes the empty part of the mft zone and
 * when the volume's free 88% are used up, the mft zone is shrunk by a factor
 * of 2, thus making more space available for more files/data. This process is
 * repeated every time there is no more free space except for the mft zone until
 * there really is no more free space.
 */

/*
 * Typedef the MFT_REF as a 64-bit value for easier handling.
 * Also define two unpacking macros to get to the reference (MREF) and
 * sequence number (MSEQNO) respectively.
 * The _LE versions are to be applied on little endian MFT_REFs.
 * Note: The _LE versions will return a CPU endian formatted value!
 */
#define MFT_REF_MASK_CPU 0x0000ffffffffffffULL
#define MFT_REF_MASK_LE const_cpu_to_le64(MFT_REF_MASK_CPU)

typedef u64 MFT_REF;

#define MK_MREF(m, s)	((MFT_REF)(((MFT_REF)(s) << 48) |		\
					((MFT_REF)(m) & MFT_REF_MASK_CPU)))
#define MK_LE_MREF(m, s) const_cpu_to_le64(((MFT_REF)(((MFT_REF)(s) << 48) | \
					((MFT_REF)(m) & MFT_REF_MASK_CPU))))

#define MREF(x)		((u64)((x) & MFT_REF_MASK_CPU))
#define MSEQNO(x)	((u16)(((x) >> 48) & 0xffff))
#define MREF_LE(x)	((u64)(const_le64_to_cpu(x) & MFT_REF_MASK_CPU))
#define MSEQNO_LE(x)	((u16)((const_le64_to_cpu(x) >> 48) & 0xffff))

#define IS_ERR_MREF(x)	(((x) & 0x0000800000000000ULL) ? 1 : 0)
#define ERR_MREF(x)	((u64)((s64)(x)))
#define MREF_ERR(x)	((int)((s64)(x)))

/**
 * struct MFT_RECORD - An MFT record layout (NTFS 3.1+)
 *
 * The mft record header present at the beginning of every record in the mft.
 * This is followed by a sequence of variable length attribute records which
 * is terminated by an attribute of type AT_END which is a truncated attribute
 * in that it only consists of the attribute type code AT_END and none of the
 * other members of the attribute structure are present.
 */
typedef struct {
/*Ofs*/
/*  0	NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
	NTFS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
	u16 usa_ofs;		/* See NTFS_RECORD definition above. */
	u16 usa_count;		/* See NTFS_RECORD definition above. */

/*  8*/	LSN lsn;		/* $LogFile sequence number for this record.
				   Changed every time the record is modified. */
/* 16*/	u16 sequence_number;	/* Number of times this mft record has been
				   reused. (See description for MFT_REF
				   above.) NOTE: The increment (skipping zero)
				   is done when the file is deleted. NOTE: If
				   this is zero it is left zero. */
/* 18*/	u16 link_count;		/* Number of hard links, i.e. the number of
				   directory entries referencing this record.
				   NOTE: Only used in mft base records.
				   NOTE: When deleting a directory entry we
				   check the link_count and if it is 1 we
				   delete the file. Otherwise we delete the
				   FILE_NAME_ATTR being referenced by the
				   directory entry from the mft record and
				   decrement the link_count.
				   FIXME: Careful with Win32 + DOS names! */
/* 20*/	u16 attrs_offset;	/* Byte offset to the first attribute in this
				   mft record from the start of the mft record.
				   NOTE: Must be aligned to 8-byte boundary. */
/* 22*/	MFT_RECORD_FLAGS flags;	/* Bit array of MFT_RECORD_FLAGS. When a file
				   is deleted, the MFT_RECORD_IN_USE flag is
				   set to zero. */
/* 24*/	u32 bytes_in_use;	/* Number of bytes used in this mft record.
				   NOTE: Must be aligned to 8-byte boundary. */
/* 28*/	u32 bytes_allocated;	/* Number of bytes allocated for this mft
				   record. This should be equal to the mft
				   record size. */
/* 32*/	MFT_REF base_mft_record; /* This is zero for base mft records.
				   When it is not zero it is a mft reference
				   pointing to the base mft record to which
				   this record belongs (this is then used to
				   locate the attribute list attribute present
				   in the base record which describes this
				   extension record and hence might need
				   modification when the extension record
				   itself is modified, also locating the
				   attribute list also means finding the other
				   potential extents, belonging to the non-base
				   mft record). */
/* 40*/	u16 next_attr_instance; /* The instance number that will be
				   assigned to the next attribute added to this
				   mft record. NOTE: Incremented each time
				   after it is used. NOTE: Every time the mft
				   record is reused this number is set to zero.
				   NOTE: The first instance number is always 0.
				 */
/* The below fields are specific to NTFS 3.1+ (Windows XP and above): */
/* 42*/ u16 reserved;		/* Reserved/alignment. */
/* 44*/ u32 mft_record_number;	/* Number of this mft record. */
/* sizeof() = 48 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} __attribute__((__packed__)) MFT_RECORD;

/**
 * struct MFT_RECORD_OLD - An MFT record layout (NTFS <=3.0)
 *
 * This is the version without the NTFS 3.1+ specific fields.
 */
typedef struct {
/*Ofs*/
/*  0	NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
	NTFS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
	u16 usa_ofs;		/* See NTFS_RECORD definition above. */
	u16 usa_count;		/* See NTFS_RECORD definition above. */

/*  8*/	LSN lsn;		/* $LogFile sequence number for this record.
				   Changed every time the record is modified. */
/* 16*/	u16 sequence_number;	/* Number of times this mft record has been
				   reused. (See description for MFT_REF
				   above.) NOTE: The increment (skipping zero)
				   is done when the file is deleted. NOTE: If
				   this is zero it is left zero. */
/* 18*/	u16 link_count;		/* Number of hard links, i.e. the number of
				   directory entries referencing this record.
				   NOTE: Only used in mft base records.
				   NOTE: When deleting a directory entry we
				   check the link_count and if it is 1 we
				   delete the file. Otherwise we delete the
				   FILE_NAME_ATTR being referenced by the
				   directory entry from the mft record and
				   decrement the link_count.
				   FIXME: Careful with Win32 + DOS names! */
/* 20*/	u16 attrs_offset;	/* Byte offset to the first attribute in this
				   mft record from the start of the mft record.
				   NOTE: Must be aligned to 8-byte boundary. */
/* 22*/	MFT_RECORD_FLAGS flags;	/* Bit array of MFT_RECORD_FLAGS. When a file
				   is deleted, the MFT_RECORD_IN_USE flag is
				   set to zero. */
/* 24*/	u32 bytes_in_use;	/* Number of bytes used in this mft record.
				   NOTE: Must be aligned to 8-byte boundary. */
/* 28*/	u32 bytes_allocated;	/* Number of bytes allocated for this mft
				   record. This should be equal to the mft
				   record size. */
/* 32*/	MFT_REF base_mft_record; /* This is zero for base mft records.
				   When it is not zero it is a mft reference
				   pointing to the base mft record to which
				   this record belongs (this is then used to
				   locate the attribute list attribute present
				   in the base record which describes this
				   extension record and hence might need
				   modification when the extension record
				   itself is modified, also locating the
				   attribute list also means finding the other
				   potential extents, belonging to the non-base
				   mft record). */
/* 40*/	u16 next_attr_instance; /* The instance number that will be
				   assigned to the next attribute added to this
				   mft record. NOTE: Incremented each time
				   after it is used. NOTE: Every time the mft
				   record is reused this number is set to zero.
				   NOTE: The first instance number is always 0.
				 */
/* sizeof() = 42 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} __attribute__((__packed__)) MFT_RECORD_OLD;

/**
 * enum ATTR_TYPES - System defined attributes (32-bit).
 *
 * Each attribute type has a corresponding attribute name (Unicode string of
 * maximum 64 character length) as described by the attribute definitions
 * present in the data attribute of the $AttrDef system file.
 * 
 * On NTFS 3.0 volumes the names are just as the types are named in the below
 * enum exchanging AT_ for the dollar sign ($). If that isn't a revealing
 * choice of symbol... (-;
 */
typedef enum {
	AT_UNUSED			= const_cpu_to_le32(         0),
	AT_STANDARD_INFORMATION		= const_cpu_to_le32(      0x10),
	AT_ATTRIBUTE_LIST		= const_cpu_to_le32(      0x20),
	AT_FILE_NAME			= const_cpu_to_le32(      0x30),
	AT_OBJECT_ID			= const_cpu_to_le32(      0x40),
	AT_SECURITY_DESCRIPTOR		= const_cpu_to_le32(      0x50),
	AT_VOLUME_NAME			= const_cpu_to_le32(      0x60),
	AT_VOLUME_INFORMATION		= const_cpu_to_le32(      0x70),
	AT_DATA				= const_cpu_to_le32(      0x80),
	AT_INDEX_ROOT			= const_cpu_to_le32(      0x90),
	AT_INDEX_ALLOCATION		= const_cpu_to_le32(      0xa0),
	AT_BITMAP			= const_cpu_to_le32(      0xb0),
	AT_REPARSE_POINT		= const_cpu_to_le32(      0xc0),
	AT_EA_INFORMATION		= const_cpu_to_le32(      0xd0),
	AT_EA				= const_cpu_to_le32(      0xe0),
	AT_PROPERTY_SET			= const_cpu_to_le32(      0xf0),
	AT_LOGGED_UTILITY_STREAM	= const_cpu_to_le32(     0x100),
	AT_FIRST_USER_DEFINED_ATTRIBUTE	= const_cpu_to_le32(    0x1000),
	AT_END				= const_cpu_to_le32(0xffffffff),
} ATTR_TYPES;

/**
 * enum COLLATION_RULES - The collation rules for sorting views/indexes/etc
 * (32-bit).
 *
 * COLLATION_UNICODE_STRING - Collate Unicode strings by comparing their binary
 *	Unicode values, except that when a character can be uppercased, the
 *	upper case value collates before the lower case one.
 * COLLATION_FILE_NAME - Collate file names as Unicode strings. The collation
 *	is done very much like COLLATION_UNICODE_STRING. In fact I have no idea
 *	what the difference is. Perhaps the difference is that file names
 *	would treat some special characters in an odd way (see
 *	unistr.c::ntfs_collate_names() and unistr.c::legal_ansi_char_array[]
 *	for what I mean but COLLATION_UNICODE_STRING would not give any special
 *	treatment to any characters at all, but this is speculation.
 * COLLATION_NTOFS_ULONG - Sorting is done according to ascending u32 key
 *	values. E.g. used for $SII index in FILE_Secure, which sorts by
 *	security_id (u32).
 * COLLATION_NTOFS_SID - Sorting is done according to ascending SID values.
 *	E.g. used for $O index in FILE_Extend/$Quota.
 * COLLATION_NTOFS_SECURITY_HASH - Sorting is done first by ascending hash
 *	values and second by ascending security_id values. E.g. used for $SDH
 *	index in FILE_Secure.
 * COLLATION_NTOFS_ULONGS - Sorting is done according to a sequence of ascending
 *	u32 key values. E.g. used for $O index in FILE_Extend/$ObjId, which
 *	sorts by object_id (16-byte), by splitting up the object_id in four
 *	u32 values and using them as individual keys. E.g. take the following
 *	two security_ids, stored as follows on disk:
 *		1st: a1 61 65 b7 65 7b d4 11 9e 3d 00 e0 81 10 42 59
 *		2nd: 38 14 37 d2 d2 f3 d4 11 a5 21 c8 6b 79 b1 97 45
 *	To compare them, they are split into four u32 values each, like so:
 *		1st: 0xb76561a1 0x11d47b65 0xe0003d9e 0x59421081
 *		2nd: 0xd2371438 0x11d4f3d2 0x6bc821a5 0x4597b179
 *	Now, it is apparent why the 2nd object_id collates after the 1st: the
 *	first u32 value of the 1st object_id is less than the first u32 of
 *	the 2nd object_id. If the first u32 values of both object_ids were
 *	equal then the second u32 values would be compared, etc.
 */
typedef enum {
	COLLATION_BINARY	 = const_cpu_to_le32(0), /* Collate by binary
					compare where the first byte is most
					significant. */
	COLLATION_FILE_NAME	 = const_cpu_to_le32(1), /* Collate file names
					as Unicode strings. */
	COLLATION_UNICODE_STRING = const_cpu_to_le32(2), /* Collate Unicode
					strings by comparing their binary
					Unicode values, except that when a
					character can be uppercased, the upper
					case value collates before the lower
					case one. */
	COLLATION_NTOFS_ULONG		= const_cpu_to_le32(16),
	COLLATION_NTOFS_SID		= const_cpu_to_le32(17),
	COLLATION_NTOFS_SECURITY_HASH	= const_cpu_to_le32(18),
	COLLATION_NTOFS_ULONGS		= const_cpu_to_le32(19),
} COLLATION_RULES;

/**
 * enum ATTR_DEF_FLAGS -
 *
 * The flags (32-bit) describing attribute properties in the attribute
 * definition structure.  FIXME: This information is based on Regis's
 * information and, according to him, it is not certain and probably
 * incomplete.  The INDEXABLE flag is fairly certainly correct as only the file
 * name attribute has this flag set and this is the only attribute indexed in
 * NT4.
 */
typedef enum {
	ATTR_DEF_INDEXABLE	= const_cpu_to_le32(0x02), /* Attribute can be
					indexed. */
	ATTR_DEF_MULTIPLE	= const_cpu_to_le32(0x04), /* Attribute type
					can be present multiple times in the
					mft records of an inode. */
	ATTR_DEF_NOT_ZERO	= const_cpu_to_le32(0x08), /* Attribute value
					must contain at least one non-zero
					byte. */
	ATTR_DEF_INDEXED_UNIQUE	= const_cpu_to_le32(0x10), /* Attribute must be
					indexed and the attribute value must be
					unique for the attribute type in all of
					the mft records of an inode. */
	ATTR_DEF_NAMED_UNIQUE	= const_cpu_to_le32(0x20), /* Attribute must be
					named and the name must be unique for
					the attribute type in all of the mft
					records of an inode. */
	ATTR_DEF_RESIDENT	= const_cpu_to_le32(0x40), /* Attribute must be
					resident. */
	ATTR_DEF_ALWAYS_LOG	= const_cpu_to_le32(0x80), /* Always log
					modifications to this attribute,
					regardless of whether it is resident or
					non-resident.  Without this, only log
					modifications if the attribute is
					resident. */
} ATTR_DEF_FLAGS;

/**
 * struct ATTR_DEF -
 *
 * The data attribute of FILE_AttrDef contains a sequence of attribute
 * definitions for the NTFS volume. With this, it is supposed to be safe for an
 * older NTFS driver to mount a volume containing a newer NTFS version without
 * damaging it (that's the theory. In practice it's: not damaging it too much).
 * Entries are sorted by attribute type. The flags describe whether the
 * attribute can be resident/non-resident and possibly other things, but the
 * actual bits are unknown.
 */
typedef struct {
/*hex ofs*/
/*  0*/	ntfschar name[0x40];		/* Unicode name of the attribute. Zero
					   terminated. */
/* 80*/	ATTR_TYPES type;		/* Type of the attribute. */
/* 84*/	u32 display_rule;		/* Default display rule.
					   FIXME: What does it mean? (AIA) */
/* 88*/ COLLATION_RULES collation_rule;	/* Default collation rule. */
/* 8c*/	ATTR_DEF_FLAGS flags;		/* Flags describing the attribute. */
/* 90*/	s64 min_size;			/* Optional minimum attribute size. */
/* 98*/	s64 max_size;			/* Maximum size of attribute. */
/* sizeof() = 0xa0 or 160 bytes */
} __attribute__((__packed__)) ATTR_DEF;

/**
 * enum ATTR_FLAGS - Attribute flags (16-bit).
 */
typedef enum {
	ATTR_IS_COMPRESSED	= const_cpu_to_le16(0x0001),
	ATTR_COMPRESSION_MASK	= const_cpu_to_le16(0x00ff),  /* Compression
						method mask. Also, first
						illegal value. */
	ATTR_IS_ENCRYPTED	= const_cpu_to_le16(0x4000),
	ATTR_IS_SPARSE		= const_cpu_to_le16(0x8000),
} __attribute__((__packed__)) ATTR_FLAGS;

/*
 * Attribute compression.
 *
 * Only the data attribute is ever compressed in the current ntfs driver in
 * Windows. Further, compression is only applied when the data attribute is
 * non-resident. Finally, to use compression, the maximum allowed cluster size
 * on a volume is 4kib.
 *
 * The compression method is based on independently compressing blocks of X
 * clusters, where X is determined from the compression_unit value found in the
 * non-resident attribute record header (more precisely: X = 2^compression_unit
 * clusters). On Windows NT/2k, X always is 16 clusters (compression_unit = 4).
 *
 * There are three different cases of how a compression block of X clusters
 * can be stored:
 *
 *   1) The data in the block is all zero (a sparse block):
 *	  This is stored as a sparse block in the runlist, i.e. the runlist
 *	  entry has length = X and lcn = -1. The mapping pairs array actually
 *	  uses a delta_lcn value length of 0, i.e. delta_lcn is not present at
 *	  all, which is then interpreted by the driver as lcn = -1.
 *	  NOTE: Even uncompressed files can be sparse on NTFS 3.0 volumes, then
 *	  the same principles apply as above, except that the length is not
 *	  restricted to being any particular value.
 *
 *   2) The data in the block is not compressed:
 *	  This happens when compression doesn't reduce the size of the block
 *	  in clusters. I.e. if compression has a small effect so that the
 *	  compressed data still occupies X clusters, then the uncompressed data
 *	  is stored in the block.
 *	  This case is recognised by the fact that the runlist entry has
 *	  length = X and lcn >= 0. The mapping pairs array stores this as
 *	  normal with a run length of X and some specific delta_lcn, i.e.
 *	  delta_lcn has to be present.
 *
 *   3) The data in the block is compressed:
 *	  The common case. This case is recognised by the fact that the run
 *	  list entry has length L < X and lcn >= 0. The mapping pairs array
 *	  stores this as normal with a run length of X and some specific
 *	  delta_lcn, i.e. delta_lcn has to be present. This runlist entry is
 *	  immediately followed by a sparse entry with length = X - L and
 *	  lcn = -1. The latter entry is to make up the vcn counting to the
 *	  full compression block size X.
 *
 * In fact, life is more complicated because adjacent entries of the same type
 * can be coalesced. This means that one has to keep track of the number of
 * clusters handled and work on a basis of X clusters at a time being one
 * block. An example: if length L > X this means that this particular runlist
 * entry contains a block of length X and part of one or more blocks of length
 * L - X. Another example: if length L < X, this does not necessarily mean that
 * the block is compressed as it might be that the lcn changes inside the block
 * and hence the following runlist entry describes the continuation of the
 * potentially compressed block. The block would be compressed if the
 * following runlist entry describes at least X - L sparse clusters, thus
 * making up the compression block length as described in point 3 above. (Of
 * course, there can be several runlist entries with small lengths so that the
 * sparse entry does not follow the first data containing entry with
 * length < X.)
 *
 * NOTE: At the end of the compressed attribute value, there most likely is not
 * just the right amount of data to make up a compression block, thus this data
 * is not even attempted to be compressed. It is just stored as is, unless
 * the number of clusters it occupies is reduced when compressed in which case
 * it is stored as a compressed compression block, complete with sparse
 * clusters at the end.
 */

/**
 * enum RESIDENT_ATTR_FLAGS - Flags of resident attributes (8-bit).
 */
typedef enum {
	RESIDENT_ATTR_IS_INDEXED = 0x01, /* Attribute is referenced in an index
					    (has implications for deleting and
					    modifying the attribute). */
} __attribute__((__packed__)) RESIDENT_ATTR_FLAGS;

/**
 * struct ATTR_RECORD - Attribute record header.
 *
 * Always aligned to 8-byte boundary.
 */
typedef struct {
/*Ofs*/
/*  0*/	ATTR_TYPES type;	/* The (32-bit) type of the attribute. */
/*  4*/	u32 length;		/* Byte size of the resident part of the
				   attribute (aligned to 8-byte boundary).
				   Used to get to the next attribute. */
/*  8*/	u8 non_resident;	/* If 0, attribute is resident.
				   If 1, attribute is non-resident. */
/*  9*/	u8 name_length;		/* Unicode character size of name of attribute.
				   0 if unnamed. */
/* 10*/	u16 name_offset;	/* If name_length != 0, the byte offset to the
				   beginning of the name from the attribute
				   record. Note that the name is stored as a
				   Unicode string. When creating, place offset
				   just at the end of the record header. Then,
				   follow with attribute value or mapping pairs
				   array, resident and non-resident attributes
				   respectively, aligning to an 8-byte
				   boundary. */
/* 12*/	ATTR_FLAGS flags;	/* Flags describing the attribute. */
/* 14*/	u16 instance;		/* The instance of this attribute record. This
				   number is unique within this mft record (see
				   MFT_RECORD/next_attribute_instance notes
				   above for more details). */
/* 16*/	union {
		/* Resident attributes. */
		struct {
/* 16 */		u32 value_length; /* Byte size of attribute value. */
/* 20 */		u16 value_offset; /* Byte offset of the attribute
					       value from the start of the
					       attribute record. When creating,
					       align to 8-byte boundary if we
					       have a name present as this might
					       not have a length of a multiple
					       of 8-bytes. */
/* 22 */		RESIDENT_ATTR_FLAGS resident_flags; /* See above. */
/* 23 */		s8 reservedR;	    /* Reserved/alignment to 8-byte
					       boundary. */
/* 24 */		void *resident_end[0]; /* Use offsetof(ATTR_RECORD,
						  resident_end) to get size of
						  a resident attribute. */
		} __attribute__((__packed__));
		/* Non-resident attributes. */
		struct {
/* 16*/			VCN lowest_vcn;	/* Lowest valid virtual cluster number
				for this portion of the attribute value or
				0 if this is the only extent (usually the
				case). - Only when an attribute list is used
				does lowest_vcn != 0 ever occur. */
/* 24*/			VCN highest_vcn; /* Highest valid vcn of this extent of
				the attribute value. - Usually there is only one
				portion, so this usually equals the attribute
				value size in clusters minus 1. Can be -1 for
				zero length files. Can be 0 for "single extent"
				attributes. */
/* 32*/			u16 mapping_pairs_offset; /* Byte offset from the
				beginning of the structure to the mapping pairs
				array which contains the mappings between the
				vcns and the logical cluster numbers (lcns).
				When creating, place this at the end of this
				record header aligned to 8-byte boundary. */
/* 34*/			u8 compression_unit; /* The compression unit expressed
				as the log to the base 2 of the number of
				clusters in a compression unit. 0 means not
				compressed. (This effectively limits the
				compression unit size to be a power of two
				clusters.) WinNT4 only uses a value of 4. */
/* 35*/			u8 reserved1[5];	/* Align to 8-byte boundary. */
/* The sizes below are only used when lowest_vcn is zero, as otherwise it would
   be difficult to keep them up-to-date.*/
/* 40*/			s64 allocated_size;	/* Byte size of disk space
				allocated to hold the attribute value. Always
				is a multiple of the cluster size. When a file
				is compressed, this field is a multiple of the
				compression block size (2^compression_unit) and
				it represents the logically allocated space
				rather than the actual on disk usage. For this
				use the compressed_size (see below). */
/* 48*/			s64 data_size;	/* Byte size of the attribute
				value. Can be larger than allocated_size if
				attribute value is compressed or sparse. */
/* 56*/			s64 initialized_size;	/* Byte size of initialized
				portion of the attribute value. Usually equals
				data_size. */
/* 64 */		void *non_resident_end[0]; /* Use offsetof(ATTR_RECORD,
						      non_resident_end) to get
						      size of a non resident
						      attribute. */
/* sizeof(uncompressed attr) = 64*/
/* 64*/			s64 compressed_size;	/* Byte size of the attribute
				value after compression. Only present when
				compressed. Always is a multiple of the
				cluster size. Represents the actual amount of
				disk space being used on the disk. */
/* 72 */		void *compressed_end[0];
				/* Use offsetof(ATTR_RECORD, compressed_end) to
				   get size of a compressed attribute. */
/* sizeof(compressed attr) = 72*/
		} __attribute__((__packed__));
	} __attribute__((__packed__));
} __attribute__((__packed__)) ATTR_RECORD;

typedef ATTR_RECORD ATTR_REC;

/**
 * enum FILE_ATTR_FLAGS - File attribute flags (32-bit).
 */
typedef enum {
	/*
	 * These flags are only present in the STANDARD_INFORMATION attribute
	 * (in the field file_attributes).
	 */
	FILE_ATTR_READONLY		= const_cpu_to_le32(0x00000001),
	FILE_ATTR_HIDDEN		= const_cpu_to_le32(0x00000002),
	FILE_ATTR_SYSTEM		= const_cpu_to_le32(0x00000004),
	/* Old DOS volid. Unused in NT.	= cpu_to_le32(0x00000008), */

	FILE_ATTR_DIRECTORY		= const_cpu_to_le32(0x00000010),
	/* FILE_ATTR_DIRECTORY is not considered valid in NT. It is reserved
	   for the DOS SUBDIRECTORY flag. */
	FILE_ATTR_ARCHIVE		= const_cpu_to_le32(0x00000020),
	FILE_ATTR_DEVICE		= const_cpu_to_le32(0x00000040),
	FILE_ATTR_NORMAL		= const_cpu_to_le32(0x00000080),

	FILE_ATTR_TEMPORARY		= const_cpu_to_le32(0x00000100),
	FILE_ATTR_SPARSE_FILE		= const_cpu_to_le32(0x00000200),
	FILE_ATTR_REPARSE_POINT		= const_cpu_to_le32(0x00000400),
	FILE_ATTR_COMPRESSED		= const_cpu_to_le32(0x00000800),

	FILE_ATTR_OFFLINE		= const_cpu_to_le32(0x00001000),
	FILE_ATTR_NOT_CONTENT_INDEXED	= const_cpu_to_le32(0x00002000),
	FILE_ATTR_ENCRYPTED		= const_cpu_to_le32(0x00004000),

	FILE_ATTR_VALID_FLAGS		= const_cpu_to_le32(0x00007fb7),
	/* FILE_ATTR_VALID_FLAGS masks out the old DOS VolId and the
	   FILE_ATTR_DEVICE and preserves everything else. This mask
	   is used to obtain all flags that are valid for reading. */
	FILE_ATTR_VALID_SET_FLAGS	= const_cpu_to_le32(0x000031a7),
	/* FILE_ATTR_VALID_SET_FLAGS masks out the old DOS VolId, the
	   FILE_ATTR_DEVICE, FILE_ATTR_DIRECTORY, FILE_ATTR_SPARSE_FILE,
	   FILE_ATTR_REPARSE_POINT, FILE_ATRE_COMPRESSED and FILE_ATTR_ENCRYPTED
	   and preserves the rest. This mask is used to to obtain all flags that
	   are valid for setting. */

	/**
	 * FILE_ATTR_I30_INDEX_PRESENT - Is it a directory?
	 *
	 * This is a copy of the MFT_RECORD_IS_DIRECTORY bit from the mft
	 * record, telling us whether this is a directory or not, i.e. whether
	 * it has an index root attribute named "$I30" or not.
	 * 
	 * This flag is only present in the FILE_NAME attribute (in the 
	 * file_attributes field).
	 */
	FILE_ATTR_I30_INDEX_PRESENT	= const_cpu_to_le32(0x10000000),
	
	/**
	 * FILE_ATTR_VIEW_INDEX_PRESENT - Does have a non-directory index?
	 * 
	 * This is a copy of the MFT_RECORD_IS_VIEW_INDEX bit from the mft
	 * record, telling us whether this file has a view index present (eg.
	 * object id index, quota index, one of the security indexes and the
	 * reparse points index).
	 *
	 * This flag is only present in the $STANDARD_INFORMATION and
	 * $FILE_NAME attributes.
	 */
	FILE_ATTR_VIEW_INDEX_PRESENT	= const_cpu_to_le32(0x20000000),
} __attribute__((__packed__)) FILE_ATTR_FLAGS;

/*
 * NOTE on times in NTFS: All times are in MS standard time format, i.e. they
 * are the number of 100-nanosecond intervals since 1st January 1601, 00:00:00
 * universal coordinated time (UTC). (In Linux time starts 1st January 1970,
 * 00:00:00 UTC and is stored as the number of 1-second intervals since then.)
 */

/**
 * struct STANDARD_INFORMATION - Attribute: Standard information (0x10).
 *
 * NOTE: Always resident.
 * NOTE: Present in all base file records on a volume.
 * NOTE: There is conflicting information about the meaning of each of the time
 *	 fields but the meaning as defined below has been verified to be
 *	 correct by practical experimentation on Windows NT4 SP6a and is hence
 *	 assumed to be the one and only correct interpretation.
 */
typedef struct {
/*Ofs*/
/*  0*/	s64 creation_time;		/* Time file was created. Updated when
					   a filename is changed(?). */
/*  8*/	s64 last_data_change_time;	/* Time the data attribute was last
					   modified. */
/* 16*/	s64 last_mft_change_time;	/* Time this mft record was last
					   modified. */
/* 24*/	s64 last_access_time;		/* Approximate time when the file was
					   last accessed (obviously this is not
					   updated on read-only volumes). In
					   Windows this is only updated when
					   accessed if some time delta has
					   passed since the last update. Also,
					   last access times updates can be
					   disabled altogether for speed. */
/* 32*/	FILE_ATTR_FLAGS file_attributes; /* Flags describing the file. */
/* 36*/	union {
		/* NTFS 1.2 (and previous, presumably) */
		struct {
		/* 36 */ u8 reserved12[12];	/* Reserved/alignment to 8-byte
						   boundary. */
		/* 48 */ void *v1_end[0];	/* Marker for offsetof(). */
		} __attribute__((__packed__));
/* sizeof() = 48 bytes */
		/* NTFS 3.0 */
		struct {
/*
 * If a volume has been upgraded from a previous NTFS version, then these
 * fields are present only if the file has been accessed since the upgrade.
 * Recognize the difference by comparing the length of the resident attribute
 * value. If it is 48, then the following fields are missing. If it is 72 then
 * the fields are present. Maybe just check like this:
 *	if (resident.ValueLength < sizeof(STANDARD_INFORMATION)) {
 *		Assume NTFS 1.2- format.
 *		If (volume version is 3.0+)
 *			Upgrade attribute to NTFS 3.0 format.
 *		else
 *			Use NTFS 1.2- format for access.
 *	} else
 *		Use NTFS 3.0 format for access.
 * Only problem is that it might be legal to set the length of the value to
 * arbitrarily large values thus spoiling this check. - But chkdsk probably
 * views that as a corruption, assuming that it behaves like this for all
 * attributes.
 */
		/* 36*/	u32 maximum_versions;	/* Maximum allowed versions for
				file. Zero if version numbering is disabled. */
		/* 40*/	u32 version_number;	/* This file's version (if any).
				Set to zero if maximum_versions is zero. */
		/* 44*/	u32 class_id;		/* Class id from bidirectional
				class id index (?). */
		/* 48*/	u32 owner_id;		/* Owner_id of the user owning
				the file. Translate via $Q index in FILE_Extend
				/$Quota to the quota control entry for the user
				owning the file. Zero if quotas are disabled. */
		/* 52*/	u32 security_id;	/* Security_id for the file.
				Translate via $SII index and $SDS data stream
				in FILE_Secure to the security descriptor. */
		/* 56*/	u64 quota_charged;	/* Byte size of the charge to
				the quota for all streams of the file. Note: Is
				zero if quotas are disabled. */
		/* 64*/	u64 usn;		/* Last update sequence number
				of the file. This is a direct index into the
				change (aka usn) journal file. It is zero if
				the usn journal is disabled.
				NOTE: To disable the journal need to delete
				the journal file itself and to then walk the
				whole mft and set all Usn entries in all mft
				records to zero! (This can take a while!)
				The journal is FILE_Extend/$UsnJrnl. Win2k
				will recreate the journal and initiate
				logging if necessary when mounting the
				partition. This, in contrast to disabling the
				journal is a very fast process, so the user
				won't even notice it. */
		/* 72*/ void *v3_end[0]; /* Marker for offsetof(). */
		} __attribute__((__packed__));
	} __attribute__((__packed__));
/* sizeof() = 72 bytes (NTFS 3.0) */
} __attribute__((__packed__)) STANDARD_INFORMATION;

/**
 * struct ATTR_LIST_ENTRY - Attribute: Attribute list (0x20).
 *
 * - Can be either resident or non-resident.
 * - Value consists of a sequence of variable length, 8-byte aligned,
 * ATTR_LIST_ENTRY records.
 * - The attribute list attribute contains one entry for each attribute of
 * the file in which the list is located, except for the list attribute
 * itself. The list is sorted: first by attribute type, second by attribute
 * name (if present), third by instance number. The extents of one
 * non-resident attribute (if present) immediately follow after the initial
 * extent. They are ordered by lowest_vcn and have their instance set to zero.
 * It is not allowed to have two attributes with all sorting keys equal.
 * - Further restrictions:
 *	- If not resident, the vcn to lcn mapping array has to fit inside the
 *	  base mft record.
 *	- The attribute list attribute value has a maximum size of 256kb. This
 *	  is imposed by the Windows cache manager.
 * - Attribute lists are only used when the attributes of mft record do not
 * fit inside the mft record despite all attributes (that can be made
 * non-resident) having been made non-resident. This can happen e.g. when:
 *	- File has a large number of hard links (lots of file name
 *	  attributes present).
 *	- The mapping pairs array of some non-resident attribute becomes so
 *	  large due to fragmentation that it overflows the mft record.
 *	- The security descriptor is very complex (not applicable to
 *	  NTFS 3.0 volumes).
 *	- There are many named streams.
 */
typedef struct {
/*Ofs*/
/*  0*/	ATTR_TYPES type;	/* Type of referenced attribute. */
/*  4*/	u16 length;		/* Byte size of this entry. */
/*  6*/	u8 name_length;		/* Size in Unicode chars of the name of the
				   attribute or 0 if unnamed. */
/*  7*/	u8 name_offset;		/* Byte offset to beginning of attribute name
				   (always set this to where the name would
				   start even if unnamed). */
/*  8*/	VCN lowest_vcn;		/* Lowest virtual cluster number of this portion
				   of the attribute value. This is usually 0. It
				   is non-zero for the case where one attribute
				   does not fit into one mft record and thus
				   several mft records are allocated to hold
				   this attribute. In the latter case, each mft
				   record holds one extent of the attribute and
				   there is one attribute list entry for each
				   extent. NOTE: This is DEFINITELY a signed
				   value! The windows driver uses cmp, followed
				   by jg when comparing this, thus it treats it
				   as signed. */
/* 16*/	MFT_REF mft_reference;	/* The reference of the mft record holding
				   the ATTR_RECORD for this portion of the
				   attribute value. */
/* 24*/	u16 instance;		/* If lowest_vcn = 0, the instance of the
				   attribute being referenced; otherwise 0. */
/* 26*/	ntfschar name[0];	/* Use when creating only. When reading use
				   name_offset to determine the location of the
				   name. */
/* sizeof() = 26 + (attribute_name_length * 2) bytes */
} __attribute__((__packed__)) ATTR_LIST_ENTRY;

/*
 * The maximum allowed length for a file name.
 */
#define NTFS_MAX_NAME_LEN	255

/**
 * enum FILE_NAME_TYPE_FLAGS - Possible namespaces for filenames in ntfs.
 * (8-bit).
 */
typedef enum {
	FILE_NAME_POSIX			= 0x00,
		/* This is the largest namespace. It is case sensitive and
		   allows all Unicode characters except for: '\0' and '/'.
		   Beware that in WinNT/2k files which eg have the same name
		   except for their case will not be distinguished by the
		   standard utilities and thus a "del filename" will delete
		   both "filename" and "fileName" without warning. */
	FILE_NAME_WIN32			= 0x01,
		/* The standard WinNT/2k NTFS long filenames. Case insensitive.
		   All Unicode chars except: '\0', '"', '*', '/', ':', '<',
		   '>', '?', '\' and '|'. Further, names cannot end with a '.'
		   or a space. */
	FILE_NAME_DOS			= 0x02,
		/* The standard DOS filenames (8.3 format). Uppercase only.
		   All 8-bit characters greater space, except: '"', '*', '+',
		   ',', '/', ':', ';', '<', '=', '>', '?' and '\'. */
	FILE_NAME_WIN32_AND_DOS		= 0x03,
		/* 3 means that both the Win32 and the DOS filenames are
		   identical and hence have been saved in this single filename
		   record. */
} __attribute__((__packed__)) FILE_NAME_TYPE_FLAGS;

/**
 * struct FILE_NAME_ATTR - Attribute: Filename (0x30).
 *
 * NOTE: Always resident.
 * NOTE: All fields, except the parent_directory, are only updated when the
 *	 filename is changed. Until then, they just become out of sync with
 *	 reality and the more up to date values are present in the standard
 *	 information attribute.
 * NOTE: There is conflicting information about the meaning of each of the time
 *	 fields but the meaning as defined below has been verified to be
 *	 correct by practical experimentation on Windows NT4 SP6a and is hence
 *	 assumed to be the one and only correct interpretation.
 */
typedef struct {
/*hex ofs*/
/*  0*/	MFT_REF parent_directory;	/* Directory this filename is
					   referenced from. */
/*  8*/	s64 creation_time;		/* Time file was created. */
/* 10*/	s64 last_data_change_time;	/* Time the data attribute was last
					   modified. */
/* 18*/	s64 last_mft_change_time;	/* Time this mft record was last
					   modified. */
/* 20*/	s64 last_access_time;		/* Last time this mft record was
					   accessed. */
/* 28*/	s64 allocated_size;		/* Byte size of on-disk allocated space
					   for the data attribute.  So for
					   normal $DATA, this is the
					   allocated_size from the unnamed
					   $DATA attribute and for compressed
					   and/or sparse $DATA, this is the
					   compressed_size from the unnamed
					   $DATA attribute.  NOTE: This is a
					   multiple of the cluster size. */
/* 30*/	s64 data_size;			/* Byte size of actual data in data
					   attribute. */
/* 38*/	FILE_ATTR_FLAGS file_attributes;	/* Flags describing the file. */
/* 3c*/	union {
	/* 3c*/	struct {
		/* 3c*/	u16 packed_ea_size;	/* Size of the buffer needed to
						   pack the extended attributes
						   (EAs), if such are present.*/
		/* 3e*/	u16 reserved;		/* Reserved for alignment. */
		} __attribute__((__packed__));
	/* 3c*/	u32 reparse_point_tag;		/* Type of reparse point,
						   present only in reparse
						   points and only if there are
						   no EAs. */
	} __attribute__((__packed__));
/* 40*/	u8 file_name_length;			/* Length of file name in
						   (Unicode) characters. */
/* 41*/	FILE_NAME_TYPE_FLAGS file_name_type;	/* Namespace of the file name.*/
/* 42*/	ntfschar file_name[0];			/* File name in Unicode. */
} __attribute__((__packed__)) FILE_NAME_ATTR;

/**
 * struct GUID - GUID structures store globally unique identifiers (GUID).
 *
 * A GUID is a 128-bit value consisting of one group of eight hexadecimal
 * digits, followed by three groups of four hexadecimal digits each, followed
 * by one group of twelve hexadecimal digits. GUIDs are Microsoft's
 * implementation of the distributed computing environment (DCE) universally
 * unique identifier (UUID).
 *
 * Example of a GUID:
 *	1F010768-5A73-BC91-0010-A52216A7227B
 */
typedef struct {
	u32 data1;	/* The first eight hexadecimal digits of the GUID. */
	u16 data2;	/* The first group of four hexadecimal digits. */
	u16 data3;	/* The second group of four hexadecimal digits. */
	u8 data4[8];	/* The first two bytes are the third group of four
			   hexadecimal digits. The remaining six bytes are the
			   final 12 hexadecimal digits. */
} __attribute__((__packed__)) GUID;

/**
 * struct OBJ_ID_INDEX_DATA - FILE_Extend/$ObjId contains an index named $O.
 *
 * This index contains all object_ids present on the volume as the index keys
 * and the corresponding mft_record numbers as the index entry data parts.
 *
 * The data part (defined below) also contains three other object_ids:
 *	birth_volume_id - object_id of FILE_Volume on which the file was first
 *			  created. Optional (i.e. can be zero).
 *	birth_object_id - object_id of file when it was first created. Usually
 *			  equals the object_id. Optional (i.e. can be zero).
 *	domain_id	- Reserved (always zero).
 */
typedef struct {
	MFT_REF mft_reference;	/* Mft record containing the object_id in
				   the index entry key. */
	union {
		struct {
			GUID birth_volume_id;
			GUID birth_object_id;
			GUID domain_id;
		} __attribute__((__packed__));
		u8 extended_info[48];
	} __attribute__((__packed__));
} __attribute__((__packed__)) OBJ_ID_INDEX_DATA;

/**
 * struct OBJECT_ID_ATTR - Attribute: Object id (NTFS 3.0+) (0x40).
 *
 * NOTE: Always resident.
 */
typedef struct {
	GUID object_id;				/* Unique id assigned to the
						   file.*/
	/* The following fields are optional. The attribute value size is 16
	   bytes, i.e. sizeof(GUID), if these are not present at all. Note,
	   the entries can be present but one or more (or all) can be zero
	   meaning that that particular value(s) is(are) not defined. Note,
	   when the fields are missing here, it is well possible that they are
	   to be found within the $Extend/$ObjId system file indexed under the
	   above object_id. */
	union {
		struct {
			GUID birth_volume_id;	/* Unique id of volume on which
						   the file was first created.*/
			GUID birth_object_id;	/* Unique id of file when it was
						   first created. */
			GUID domain_id;		/* Reserved, zero. */
		} __attribute__((__packed__));
		u8 extended_info[48];
	} __attribute__((__packed__));
} __attribute__((__packed__)) OBJECT_ID_ATTR;

#if 0
/**
 * enum IDENTIFIER_AUTHORITIES -
 *
 * The pre-defined IDENTIFIER_AUTHORITIES used as SID_IDENTIFIER_AUTHORITY in
 * the SID structure (see below).
 */
typedef enum {					/* SID string prefix. */
	SECURITY_NULL_SID_AUTHORITY	= {0, 0, 0, 0, 0, 0},	/* S-1-0 */
	SECURITY_WORLD_SID_AUTHORITY	= {0, 0, 0, 0, 0, 1},	/* S-1-1 */
	SECURITY_LOCAL_SID_AUTHORITY	= {0, 0, 0, 0, 0, 2},	/* S-1-2 */
	SECURITY_CREATOR_SID_AUTHORITY	= {0, 0, 0, 0, 0, 3},	/* S-1-3 */
	SECURITY_NON_UNIQUE_AUTHORITY	= {0, 0, 0, 0, 0, 4},	/* S-1-4 */
	SECURITY_NT_SID_AUTHORITY	= {0, 0, 0, 0, 0, 5},	/* S-1-5 */
} IDENTIFIER_AUTHORITIES;
#endif

/**
 * enum RELATIVE_IDENTIFIERS -
 *
 * These relative identifiers (RIDs) are used with the above identifier
 * authorities to make up universal well-known SIDs.
 *
 * Note: The relative identifier (RID) refers to the portion of a SID, which
 * identifies a user or group in relation to the authority that issued the SID.
 * For example, the universal well-known SID Creator Owner ID (S-1-3-0) is
 * made up of the identifier authority SECURITY_CREATOR_SID_AUTHORITY (3) and
 * the relative identifier SECURITY_CREATOR_OWNER_RID (0).
 */
typedef enum {					/* Identifier authority. */
	SECURITY_NULL_RID		  = 0,	/* S-1-0 */
	SECURITY_WORLD_RID		  = 0,	/* S-1-1 */
	SECURITY_LOCAL_RID		  = 0,	/* S-1-2 */

	SECURITY_CREATOR_OWNER_RID	  = 0,	/* S-1-3 */
	SECURITY_CREATOR_GROUP_RID	  = 1,	/* S-1-3 */

	SECURITY_CREATOR_OWNER_SERVER_RID = 2,	/* S-1-3 */
	SECURITY_CREATOR_GROUP_SERVER_RID = 3,	/* S-1-3 */

	SECURITY_DIALUP_RID		  = 1,
	SECURITY_NETWORK_RID		  = 2,
	SECURITY_BATCH_RID		  = 3,
	SECURITY_INTERACTIVE_RID	  = 4,
	SECURITY_SERVICE_RID		  = 6,
	SECURITY_ANONYMOUS_LOGON_RID	  = 7,
	SECURITY_PROXY_RID		  = 8,
	SECURITY_ENTERPRISE_CONTROLLERS_RID=9,
	SECURITY_SERVER_LOGON_RID	  = 9,
	SECURITY_PRINCIPAL_SELF_RID	  = 0xa,
	SECURITY_AUTHENTICATED_USER_RID	  = 0xb,
	SECURITY_RESTRICTED_CODE_RID	  = 0xc,
	SECURITY_TERMINAL_SERVER_RID	  = 0xd,

	SECURITY_LOGON_IDS_RID		  = 5,
	SECURITY_LOGON_IDS_RID_COUNT	  = 3,

	SECURITY_LOCAL_SYSTEM_RID	  = 0x12,

	SECURITY_NT_NON_UNIQUE		  = 0x15,

	SECURITY_BUILTIN_DOMAIN_RID	  = 0x20,

	/*
	 * Well-known domain relative sub-authority values (RIDs).
	 */

	/* Users. */
	DOMAIN_USER_RID_ADMIN		  = 0x1f4,
	DOMAIN_USER_RID_GUEST		  = 0x1f5,
	DOMAIN_USER_RID_KRBTGT		  = 0x1f6,

	/* Groups. */
	DOMAIN_GROUP_RID_ADMINS		  = 0x200,
	DOMAIN_GROUP_RID_USERS		  = 0x201,
	DOMAIN_GROUP_RID_GUESTS		  = 0x202,
	DOMAIN_GROUP_RID_COMPUTERS	  = 0x203,
	DOMAIN_GROUP_RID_CONTROLLERS	  = 0x204,
	DOMAIN_GROUP_RID_CERT_ADMINS	  = 0x205,
	DOMAIN_GROUP_RID_SCHEMA_ADMINS	  = 0x206,
	DOMAIN_GROUP_RID_ENTERPRISE_ADMINS= 0x207,
	DOMAIN_GROUP_RID_POLICY_ADMINS	  = 0x208,

	/* Aliases. */
	DOMAIN_ALIAS_RID_ADMINS		  = 0x220,
	DOMAIN_ALIAS_RID_USERS		  = 0x221,
	DOMAIN_ALIAS_RID_GUESTS		  = 0x222,
	DOMAIN_ALIAS_RID_POWER_USERS	  = 0x223,

	DOMAIN_ALIAS_RID_ACCOUNT_OPS	  = 0x224,
	DOMAIN_ALIAS_RID_SYSTEM_OPS	  = 0x225,
	DOMAIN_ALIAS_RID_PRINT_OPS	  = 0x226,
	DOMAIN_ALIAS_RID_BACKUP_OPS	  = 0x227,

	DOMAIN_ALIAS_RID_REPLICATOR	  = 0x228,
	DOMAIN_ALIAS_RID_RAS_SERVERS	  = 0x229,
	DOMAIN_ALIAS_RID_PREW2KCOMPACCESS = 0x22a,
} RELATIVE_IDENTIFIERS;

/*
 * The universal well-known SIDs:
 *
 *	NULL_SID			S-1-0-0
 *	WORLD_SID			S-1-1-0
 *	LOCAL_SID			S-1-2-0
 *	CREATOR_OWNER_SID		S-1-3-0
 *	CREATOR_GROUP_SID		S-1-3-1
 *	CREATOR_OWNER_SERVER_SID	S-1-3-2
 *	CREATOR_GROUP_SERVER_SID	S-1-3-3
 *
 *	(Non-unique IDs)		S-1-4
 *
 * NT well-known SIDs:
 *
 *	NT_AUTHORITY_SID	S-1-5
 *	DIALUP_SID		S-1-5-1
 *
 *	NETWORD_SID		S-1-5-2
 *	BATCH_SID		S-1-5-3
 *	INTERACTIVE_SID		S-1-5-4
 *	SERVICE_SID		S-1-5-6
 *	ANONYMOUS_LOGON_SID	S-1-5-7		(aka null logon session)
 *	PROXY_SID		S-1-5-8
 *	SERVER_LOGON_SID	S-1-5-9		(aka domain controller account)
 *	SELF_SID		S-1-5-10	(self RID)
 *	AUTHENTICATED_USER_SID	S-1-5-11
 *	RESTRICTED_CODE_SID	S-1-5-12	(running restricted code)
 *	TERMINAL_SERVER_SID	S-1-5-13	(running on terminal server)
 *
 *	(Logon IDs)		S-1-5-5-X-Y
 *
 *	(NT non-unique IDs)	S-1-5-0x15-...
 *
 *	(Built-in domain)	S-1-5-0x20
 */

/**
 * union SID_IDENTIFIER_AUTHORITY - A 48-bit value used in the SID structure
 *
 * NOTE: This is stored as a big endian number.
 */
typedef union {
	struct {
		u16 high_part;		/* High 16-bits. */
		u32 low_part;		/* Low 32-bits. */
	} __attribute__((__packed__));
	u8 value[6];			/* Value as individual bytes. */
} __attribute__((__packed__)) SID_IDENTIFIER_AUTHORITY;

/**
 * struct SID -
 *
 * The SID structure is a variable-length structure used to uniquely identify
 * users or groups. SID stands for security identifier.
 *
 * The standard textual representation of the SID is of the form:
 *	S-R-I-S-S...
 * Where:
 *    - The first "S" is the literal character 'S' identifying the following
 *	digits as a SID.
 *    - R is the revision level of the SID expressed as a sequence of digits
 *	in decimal.
 *    - I is the 48-bit identifier_authority, expressed as digits in decimal,
 *	if I < 2^32, or hexadecimal prefixed by "0x", if I >= 2^32.
 *    - S... is one or more sub_authority values, expressed as digits in
 *	decimal.
 *
 * Example SID; the domain-relative SID of the local Administrators group on
 * Windows NT/2k:
 *	S-1-5-32-544
 * This translates to a SID with:
 *	revision = 1,
 *	sub_authority_count = 2,
 *	identifier_authority = {0,0,0,0,0,5},	// SECURITY_NT_AUTHORITY
 *	sub_authority[0] = 32,			// SECURITY_BUILTIN_DOMAIN_RID
 *	sub_authority[1] = 544			// DOMAIN_ALIAS_RID_ADMINS
 */
typedef struct {
	u8 revision;
	u8 sub_authority_count;
	SID_IDENTIFIER_AUTHORITY identifier_authority;
	u32 sub_authority[1];		/* At least one sub_authority. */
} __attribute__((__packed__)) SID;

/**
 * enum SID_CONSTANTS - Current constants for SIDs.
 */
typedef enum {
	SID_REVISION			=  1,	/* Current revision level. */
	SID_MAX_SUB_AUTHORITIES		= 15,	/* Maximum number of those. */
	SID_RECOMMENDED_SUB_AUTHORITIES	=  1,	/* Will change to around 6 in
						   a future revision. */
} SID_CONSTANTS;

/**
 * enum ACE_TYPES - The predefined ACE types (8-bit, see below).
 */
typedef enum {
	ACCESS_MIN_MS_ACE_TYPE		= 0,
	ACCESS_ALLOWED_ACE_TYPE		= 0,
	ACCESS_DENIED_ACE_TYPE		= 1,
	SYSTEM_AUDIT_ACE_TYPE		= 2,
	SYSTEM_ALARM_ACE_TYPE		= 3, /* Not implemented as of Win2k. */
	ACCESS_MAX_MS_V2_ACE_TYPE	= 3,

	ACCESS_ALLOWED_COMPOUND_ACE_TYPE= 4,
	ACCESS_MAX_MS_V3_ACE_TYPE	= 4,

	/* The following are Win2k only. */
	ACCESS_MIN_MS_OBJECT_ACE_TYPE	= 5,
	ACCESS_ALLOWED_OBJECT_ACE_TYPE	= 5,
	ACCESS_DENIED_OBJECT_ACE_TYPE	= 6,
	SYSTEM_AUDIT_OBJECT_ACE_TYPE	= 7,
	SYSTEM_ALARM_OBJECT_ACE_TYPE	= 8,
	ACCESS_MAX_MS_OBJECT_ACE_TYPE	= 8,

	ACCESS_MAX_MS_V4_ACE_TYPE	= 8,

	/* This one is for WinNT&2k. */
	ACCESS_MAX_MS_ACE_TYPE		= 8,
} __attribute__((__packed__)) ACE_TYPES;

/**
 * enum ACE_FLAGS - The ACE flags (8-bit) for audit and inheritance.
 *
 * SUCCESSFUL_ACCESS_ACE_FLAG is only used with system audit and alarm ACE
 * types to indicate that a message is generated (in Windows!) for successful
 * accesses.
 *
 * FAILED_ACCESS_ACE_FLAG is only used with system audit and alarm ACE types
 * to indicate that a message is generated (in Windows!) for failed accesses.
 */
typedef enum {
	/* The inheritance flags. */
	OBJECT_INHERIT_ACE		= 0x01,
	CONTAINER_INHERIT_ACE		= 0x02,
	NO_PROPAGATE_INHERIT_ACE	= 0x04,
	INHERIT_ONLY_ACE		= 0x08,
	INHERITED_ACE			= 0x10,	/* Win2k only. */
	VALID_INHERIT_FLAGS		= 0x1f,

	/* The audit flags. */
	SUCCESSFUL_ACCESS_ACE_FLAG	= 0x40,
	FAILED_ACCESS_ACE_FLAG		= 0x80,
} __attribute__((__packed__)) ACE_FLAGS;

/**
 * struct ACE_HEADER -
 *
 * An ACE is an access-control entry in an access-control list (ACL).
 * An ACE defines access to an object for a specific user or group or defines
 * the types of access that generate system-administration messages or alarms
 * for a specific user or group. The user or group is identified by a security
 * identifier (SID).
 *
 * Each ACE starts with an ACE_HEADER structure (aligned on 4-byte boundary),
 * which specifies the type and size of the ACE. The format of the subsequent
 * data depends on the ACE type.
 */
typedef struct {
	ACE_TYPES type;		/* Type of the ACE. */
	ACE_FLAGS flags;	/* Flags describing the ACE. */
	u16 size;		/* Size in bytes of the ACE. */
} __attribute__((__packed__)) ACE_HEADER;

/**
 * enum ACCESS_MASK - The access mask (32-bit).
 *
 * Defines the access rights.
 */
typedef enum {
	/*
	 * The specific rights (bits 0 to 15). Depend on the type of the
	 * object being secured by the ACE.
	 */

	/* Specific rights for files and directories are as follows: */

	/* Right to read data from the file. (FILE) */
	FILE_READ_DATA			= const_cpu_to_le32(0x00000001),
	/* Right to list contents of a directory. (DIRECTORY) */
	FILE_LIST_DIRECTORY		= const_cpu_to_le32(0x00000001),

	/* Right to write data to the file. (FILE) */
	FILE_WRITE_DATA			= const_cpu_to_le32(0x00000002),
	/* Right to create a file in the directory. (DIRECTORY) */
	FILE_ADD_FILE			= const_cpu_to_le32(0x00000002),

	/* Right to append data to the file. (FILE) */
	FILE_APPEND_DATA		= const_cpu_to_le32(0x00000004),
	/* Right to create a subdirectory. (DIRECTORY) */
	FILE_ADD_SUBDIRECTORY		= const_cpu_to_le32(0x00000004),

	/* Right to read extended attributes. (FILE/DIRECTORY) */
	FILE_READ_EA			= const_cpu_to_le32(0x00000008),

	/* Right to write extended attributes. (FILE/DIRECTORY) */
	FILE_WRITE_EA			= const_cpu_to_le32(0x00000010),

	/* Right to execute a file. (FILE) */
	FILE_EXECUTE			= const_cpu_to_le32(0x00000020),
	/* Right to traverse the directory. (DIRECTORY) */
	FILE_TRAVERSE			= const_cpu_to_le32(0x00000020),

	/*
	 * Right to delete a directory and all the files it contains (its
	 * children), even if the files are read-only. (DIRECTORY)
	 */
	FILE_DELETE_CHILD		= const_cpu_to_le32(0x00000040),

	/* Right to read file attributes. (FILE/DIRECTORY) */
	FILE_READ_ATTRIBUTES		= const_cpu_to_le32(0x00000080),

	/* Right to change file attributes. (FILE/DIRECTORY) */
	FILE_WRITE_ATTRIBUTES		= const_cpu_to_le32(0x00000100),

	/*
	 * The standard rights (bits 16 to 23). Are independent of the type of
	 * object being secured.
	 */

	/* Right to delete the object. */
	DELETE				= const_cpu_to_le32(0x00010000),

	/*
	 * Right to read the information in the object's security descriptor,
	 * not including the information in the SACL. I.e. right to read the
	 * security descriptor and owner.
	 */
	READ_CONTROL			= const_cpu_to_le32(0x00020000),

	/* Right to modify the DACL in the object's security descriptor. */
	WRITE_DAC			= const_cpu_to_le32(0x00040000),

	/* Right to change the owner in the object's security descriptor. */
	WRITE_OWNER			= const_cpu_to_le32(0x00080000),

	/*
	 * Right to use the object for synchronization. Enables a process to
	 * wait until the object is in the signalled state. Some object types
	 * do not support this access right.
	 */
	SYNCHRONIZE			= const_cpu_to_le32(0x00100000),

	/*
	 * The following STANDARD_RIGHTS_* are combinations of the above for
	 * convenience and are defined by the Win32 API.
	 */

	/* These are currently defined to READ_CONTROL. */
	STANDARD_RIGHTS_READ		= const_cpu_to_le32(0x00020000),
	STANDARD_RIGHTS_WRITE		= const_cpu_to_le32(0x00020000),
	STANDARD_RIGHTS_EXECUTE		= const_cpu_to_le32(0x00020000),

	/* Combines DELETE, READ_CONTROL, WRITE_DAC, and WRITE_OWNER access. */
	STANDARD_RIGHTS_REQUIRED	= const_cpu_to_le32(0x000f0000),

	/*
	 * Combines DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, and
	 * SYNCHRONIZE access.
	 */
	STANDARD_RIGHTS_ALL		= const_cpu_to_le32(0x001f0000),

	/*
	 * The access system ACL and maximum allowed access types (bits 24 to
	 * 25, bits 26 to 27 are reserved).
	 */
	ACCESS_SYSTEM_SECURITY		= const_cpu_to_le32(0x01000000),
	MAXIMUM_ALLOWED			= const_cpu_to_le32(0x02000000),

	/*
	 * The generic rights (bits 28 to 31). These map onto the standard and
	 * specific rights.
	 */

	/* Read, write, and execute access. */
	GENERIC_ALL			= const_cpu_to_le32(0x10000000),

	/* Execute access. */
	GENERIC_EXECUTE			= const_cpu_to_le32(0x20000000),

	/*
	 * Write access. For files, this maps onto:
	 *	FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
	 *	FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE
	 * For directories, the mapping has the same numerical value. See
	 * above for the descriptions of the rights granted.
	 */
	GENERIC_WRITE			= const_cpu_to_le32(0x40000000),

	/*
	 * Read access. For files, this maps onto:
	 *	FILE_READ_ATTRIBUTES | FILE_READ_DATA | FILE_READ_EA |
	 *	STANDARD_RIGHTS_READ | SYNCHRONIZE
	 * For directories, the mapping has the same numerical value. See
	 * above for the descriptions of the rights granted.
	 */
	GENERIC_READ			= const_cpu_to_le32(0x80000000),
} ACCESS_MASK;

/**
 * struct GENERIC_MAPPING -
 *
 * The generic mapping array. Used to denote the mapping of each generic
 * access right to a specific access mask.
 *
 * FIXME: What exactly is this and what is it for? (AIA)
 */
typedef struct {
	ACCESS_MASK generic_read;
	ACCESS_MASK generic_write;
	ACCESS_MASK generic_execute;
	ACCESS_MASK generic_all;
} __attribute__((__packed__)) GENERIC_MAPPING;

/*
 * The predefined ACE type structures are as defined below.
 */

/**
 * struct ACCESS_DENIED_ACE -
 *
 * ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE, SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE
 */
typedef struct {
/*  0	ACE_HEADER; -- Unfolded here as gcc doesn't like unnamed structs. */
	ACE_TYPES type;		/* Type of the ACE. */
	ACE_FLAGS flags;	/* Flags describing the ACE. */
	u16 size;		/* Size in bytes of the ACE. */

/*  4*/	ACCESS_MASK mask;	/* Access mask associated with the ACE. */
/*  8*/	SID sid;		/* The SID associated with the ACE. */
} __attribute__((__packed__)) ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE,
			       SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE;

/**
 * enum OBJECT_ACE_FLAGS - The object ACE flags (32-bit).
 */
typedef enum {
	ACE_OBJECT_TYPE_PRESENT			= const_cpu_to_le32(1),
	ACE_INHERITED_OBJECT_TYPE_PRESENT	= const_cpu_to_le32(2),
} OBJECT_ACE_FLAGS;

/**
 * struct ACCESS_ALLOWED_OBJECT_ACE -
 */
typedef struct {
/*  0	ACE_HEADER; -- Unfolded here as gcc doesn't like unnamed structs. */
	ACE_TYPES type;		/* Type of the ACE. */
	ACE_FLAGS flags;	/* Flags describing the ACE. */
	u16 size;		/* Size in bytes of the ACE. */

/*  4*/	ACCESS_MASK mask;	/* Access mask associated with the ACE. */
/*  8*/	OBJECT_ACE_FLAGS object_flags;	/* Flags describing the object ACE. */
/* 12*/	GUID object_type;
/* 28*/	GUID inherited_object_type;
/* 44*/	SID sid;		/* The SID associated with the ACE. */
} __attribute__((__packed__)) ACCESS_ALLOWED_OBJECT_ACE,
			       ACCESS_DENIED_OBJECT_ACE,
			       SYSTEM_AUDIT_OBJECT_ACE,
			       SYSTEM_ALARM_OBJECT_ACE;

/**
 * struct ACL - An ACL is an access-control list (ACL).
 *
 * An ACL starts with an ACL header structure, which specifies the size of
 * the ACL and the number of ACEs it contains. The ACL header is followed by
 * zero or more access control entries (ACEs). The ACL as well as each ACE
 * are aligned on 4-byte boundaries.
 */
typedef struct {
	u8 revision;	/* Revision of this ACL. */
	u8 alignment1;
	u16 size;	/* Allocated space in bytes for ACL. Includes this
			   header, the ACEs and the remaining free space. */
	u16 ace_count;	/* Number of ACEs in the ACL. */
	u16 alignment2;
/* sizeof() = 8 bytes */
} __attribute__((__packed__)) ACL;

/**
 * enum ACL_CONSTANTS - Current constants for ACLs.
 */
typedef enum {
	/* Current revision. */
	ACL_REVISION		= 2,
	ACL_REVISION_DS		= 4,

	/* History of revisions. */
	ACL_REVISION1		= 1,
	MIN_ACL_REVISION	= 2,
	ACL_REVISION2		= 2,
	ACL_REVISION3		= 3,
	ACL_REVISION4		= 4,
	MAX_ACL_REVISION	= 4,
} ACL_CONSTANTS;

/**
 * enum SECURITY_DESCRIPTOR_CONTROL -
 *
 * The security descriptor control flags (16-bit).
 *
 * SE_OWNER_DEFAULTED - This boolean flag, when set, indicates that the
 *	SID pointed to by the Owner field was provided by a
 *	defaulting mechanism rather than explicitly provided by the
 *	original provider of the security descriptor.  This may
 *	affect the treatment of the SID with respect to inheritance
 *	of an owner.
 *
 * SE_GROUP_DEFAULTED - This boolean flag, when set, indicates that the
 *	SID in the Group field was provided by a defaulting mechanism
 *	rather than explicitly provided by the original provider of
 *	the security descriptor.  This may affect the treatment of
 *	the SID with respect to inheritance of a primary group.
 *
 * SE_DACL_PRESENT - This boolean flag, when set, indicates that the
 *	security descriptor contains a discretionary ACL.  If this
 *	flag is set and the Dacl field of the SECURITY_DESCRIPTOR is
 *	null, then a null ACL is explicitly being specified.
 *
 * SE_DACL_DEFAULTED - This boolean flag, when set, indicates that the
 *	ACL pointed to by the Dacl field was provided by a defaulting
 *	mechanism rather than explicitly provided by the original
 *	provider of the security descriptor.  This may affect the
 *	treatment of the ACL with respect to inheritance of an ACL.
 *	This flag is ignored if the DaclPresent flag is not set.
 *
 * SE_SACL_PRESENT - This boolean flag, when set,  indicates that the
 *	security descriptor contains a system ACL pointed to by the
 *	Sacl field.  If this flag is set and the Sacl field of the
 *	SECURITY_DESCRIPTOR is null, then an empty (but present)
 *	ACL is being specified.
 *
 * SE_SACL_DEFAULTED - This boolean flag, when set, indicates that the
 *	ACL pointed to by the Sacl field was provided by a defaulting
 *	mechanism rather than explicitly provided by the original
 *	provider of the security descriptor.  This may affect the
 *	treatment of the ACL with respect to inheritance of an ACL.
 *	This flag is ignored if the SaclPresent flag is not set.
 *
 * SE_SELF_RELATIVE - This boolean flag, when set, indicates that the
 *	security descriptor is in self-relative form.  In this form,
 *	all fields of the security descriptor are contiguous in memory
 *	and all pointer fields are expressed as offsets from the
 *	beginning of the security descriptor.
 */
typedef enum {
	SE_OWNER_DEFAULTED		= const_cpu_to_le16(0x0001),
	SE_GROUP_DEFAULTED		= const_cpu_to_le16(0x0002),
	SE_DACL_PRESENT			= const_cpu_to_le16(0x0004),
	SE_DACL_DEFAULTED		= const_cpu_to_le16(0x0008),
	SE_SACL_PRESENT			= const_cpu_to_le16(0x0010),
	SE_SACL_DEFAULTED		= const_cpu_to_le16(0x0020),
	SE_DACL_AUTO_INHERIT_REQ	= const_cpu_to_le16(0x0100),
	SE_SACL_AUTO_INHERIT_REQ	= const_cpu_to_le16(0x0200),
	SE_DACL_AUTO_INHERITED		= const_cpu_to_le16(0x0400),
	SE_SACL_AUTO_INHERITED		= const_cpu_to_le16(0x0800),
	SE_DACL_PROTECTED		= const_cpu_to_le16(0x1000),
	SE_SACL_PROTECTED		= const_cpu_to_le16(0x2000),
	SE_RM_CONTROL_VALID		= const_cpu_to_le16(0x4000),
	SE_SELF_RELATIVE		= const_cpu_to_le16(0x8000),
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_CONTROL;

/**
 * struct SECURITY_DESCRIPTOR_RELATIVE -
 *
 * Self-relative security descriptor. Contains the owner and group SIDs as well
 * as the sacl and dacl ACLs inside the security descriptor itself.
 */
typedef struct {
	u8 revision;	/* Revision level of the security descriptor. */
	u8 alignment;
	SECURITY_DESCRIPTOR_CONTROL control; /* Flags qualifying the type of
			   the descriptor as well as the following fields. */
	u32 owner;	/* Byte offset to a SID representing an object's
			   owner. If this is NULL, no owner SID is present in
			   the descriptor. */
	u32 group;	/* Byte offset to a SID representing an object's
			   primary group. If this is NULL, no primary group
			   SID is present in the descriptor. */
	u32 sacl;	/* Byte offset to a system ACL. Only valid, if
			   SE_SACL_PRESENT is set in the control field. If
			   SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
			   is specified. */
	u32 dacl;	/* Byte offset to a discretionary ACL. Only valid, if
			   SE_DACL_PRESENT is set in the control field. If
			   SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
			   (unconditionally granting access) is specified. */
/* sizeof() = 0x14 bytes */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_RELATIVE;

/**
 * struct SECURITY_DESCRIPTOR - Absolute security descriptor.
 *
 * Does not contain the owner and group SIDs, nor the sacl and dacl ACLs inside
 * the security descriptor. Instead, it contains pointers to these structures
 * in memory. Obviously, absolute security descriptors are only useful for in
 * memory representations of security descriptors.
 *
 * On disk, a self-relative security descriptor is used.
 */
typedef struct {
	u8 revision;	/* Revision level of the security descriptor. */
	u8 alignment;
	SECURITY_DESCRIPTOR_CONTROL control;	/* Flags qualifying the type of
			   the descriptor as well as the following fields. */
	SID *owner;	/* Points to a SID representing an object's owner. If
			   this is NULL, no owner SID is present in the
			   descriptor. */
	SID *group;	/* Points to a SID representing an object's primary
			   group. If this is NULL, no primary group SID is
			   present in the descriptor. */
	ACL *sacl;	/* Points to a system ACL. Only valid, if
			   SE_SACL_PRESENT is set in the control field. If
			   SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
			   is specified. */
	ACL *dacl;	/* Points to a discretionary ACL. Only valid, if
			   SE_DACL_PRESENT is set in the control field. If
			   SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
			   (unconditionally granting access) is specified. */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR;

/**
 * enum SECURITY_DESCRIPTOR_CONSTANTS -
 *
 * Current constants for security descriptors.
 */
typedef enum {
	/* Current revision. */
	SECURITY_DESCRIPTOR_REVISION	= 1,
	SECURITY_DESCRIPTOR_REVISION1	= 1,

	/* The sizes of both the absolute and relative security descriptors is
	   the same as pointers, at least on ia32 architecture are 32-bit. */
	SECURITY_DESCRIPTOR_MIN_LENGTH	= sizeof(SECURITY_DESCRIPTOR),
} SECURITY_DESCRIPTOR_CONSTANTS;

/*
 * Attribute: Security descriptor (0x50).
 *
 * A standard self-relative security descriptor.
 *
 * NOTE: Can be resident or non-resident.
 * NOTE: Not used in NTFS 3.0+, as security descriptors are stored centrally
 * in FILE_Secure and the correct descriptor is found using the security_id
 * from the standard information attribute.
 */
typedef SECURITY_DESCRIPTOR_RELATIVE SECURITY_DESCRIPTOR_ATTR;

/*
 * On NTFS 3.0+, all security descriptors are stored in FILE_Secure. Only one
 * referenced instance of each unique security descriptor is stored.
 *
 * FILE_Secure contains no unnamed data attribute, i.e. it has zero length. It
 * does, however, contain two indexes ($SDH and $SII) as well as a named data
 * stream ($SDS).
 *
 * Every unique security descriptor is assigned a unique security identifier
 * (security_id, not to be confused with a SID). The security_id is unique for
 * the NTFS volume and is used as an index into the $SII index, which maps
 * security_ids to the security descriptor's storage location within the $SDS
 * data attribute. The $SII index is sorted by ascending security_id.
 *
 * A simple hash is computed from each security descriptor. This hash is used
 * as an index into the $SDH index, which maps security descriptor hashes to
 * the security descriptor's storage location within the $SDS data attribute.
 * The $SDH index is sorted by security descriptor hash and is stored in a B+
 * tree. When searching $SDH (with the intent of determining whether or not a
 * new security descriptor is already present in the $SDS data stream), if a
 * matching hash is found, but the security descriptors do not match, the
 * search in the $SDH index is continued, searching for a next matching hash.
 *
 * When a precise match is found, the security_id corresponding to the security
 * descriptor in the $SDS attribute is read from the found $SDH index entry and
 * is stored in the $STANDARD_INFORMATION attribute of the file/directory to
 * which the security descriptor is being applied. The $STANDARD_INFORMATION
 * attribute is present in all base mft records (i.e. in all files and
 * directories).
 *
 * If a match is not found, the security descriptor is assigned a new unique
 * security_id and is added to the $SDS data attribute. Then, entries
 * referencing the this security descriptor in the $SDS data attribute are
 * added to the $SDH and $SII indexes.
 *
 * Note: Entries are never deleted from FILE_Secure, even if nothing
 * references an entry any more.
 */

/**
 * struct SECURITY_DESCRIPTOR_HEADER -
 *
 * This header precedes each security descriptor in the $SDS data stream.
 * This is also the index entry data part of both the $SII and $SDH indexes.
 */
typedef struct {
	u32 hash;	   /* Hash of the security descriptor. */
	u32 security_id;   /* The security_id assigned to the descriptor. */
	u64 offset;	   /* Byte offset of this entry in the $SDS stream. */
	u32 length;	   /* Size in bytes of this entry in $SDS stream. */
} __attribute__((__packed__)) SECURITY_DESCRIPTOR_HEADER;

/**
 * struct SDH_INDEX_DATA -
 */
typedef struct {
	u32 hash;          /* Hash of the security descriptor. */
	u32 security_id;   /* The security_id assigned to the descriptor. */
	u64 offset;	   /* Byte offset of this entry in the $SDS stream. */
	u32 length;	   /* Size in bytes of this entry in $SDS stream. */
	u32 reserved_II;   /* Padding - always unicode "II" or zero. This field
			      isn't counted in INDEX_ENTRY's data_length. */
} __attribute__((__packed__)) SDH_INDEX_DATA;

/**
 * struct SII_INDEX_DATA -
 */
typedef SECURITY_DESCRIPTOR_HEADER SII_INDEX_DATA;

/**
 * struct SDS_ENTRY -
 *
 * The $SDS data stream contains the security descriptors, aligned on 16-byte
 * boundaries, sorted by security_id in a B+ tree. Security descriptors cannot
 * cross 256kib boundaries (this restriction is imposed by the Windows cache
 * manager). Each security descriptor is contained in a SDS_ENTRY structure.
 * Also, each security descriptor is stored twice in the $SDS stream with a
 * fixed offset of 0x40000 bytes (256kib, the Windows cache manager's max size)
 * between them; i.e. if a SDS_ENTRY specifies an offset of 0x51d0, then the
 * the first copy of the security descriptor will be at offset 0x51d0 in the
 * $SDS data stream and the second copy will be at offset 0x451d0.
 */
typedef struct {
/*  0	SECURITY_DESCRIPTOR_HEADER; -- Unfolded here as gcc doesn't like
				       unnamed structs. */
	u32 hash;	   /* Hash of the security descriptor. */
	u32 security_id;   /* The security_id assigned to the descriptor. */
	u64 offset;	   /* Byte offset of this entry in the $SDS stream. */
	u32 length;	   /* Size in bytes of this entry in $SDS stream. */
/* 20*/	SECURITY_DESCRIPTOR_RELATIVE sid; /* The self-relative security
					     descriptor. */
} __attribute__((__packed__)) SDS_ENTRY;

/**
 * struct SII_INDEX_KEY - The index entry key used in the $SII index.
 *
 * The collation type is COLLATION_NTOFS_ULONG.
 */
typedef struct {
	u32 security_id;   /* The security_id assigned to the descriptor. */
} __attribute__((__packed__)) SII_INDEX_KEY;

/**
 * struct SDH_INDEX_KEY - The index entry key used in the $SDH index.
 *
 * The keys are sorted first by hash and then by security_id.
 * The collation rule is COLLATION_NTOFS_SECURITY_HASH.
 */
typedef struct {
	u32 hash;	   /* Hash of the security descriptor. */
	u32 security_id;   /* The security_id assigned to the descriptor. */
} __attribute__((__packed__)) SDH_INDEX_KEY;

/**
 * struct VOLUME_NAME - Attribute: Volume name (0x60).
 *
 * NOTE: Always resident.
 * NOTE: Present only in FILE_Volume.
 */
typedef struct {
	ntfschar name[0];	/* The name of the volume in Unicode. */
} __attribute__((__packed__)) VOLUME_NAME;

/**
 * enum VOLUME_FLAGS - Possible flags for the volume (16-bit).
 */
typedef enum {
	VOLUME_IS_DIRTY			= const_cpu_to_le16(0x0001),
	VOLUME_RESIZE_LOG_FILE		= const_cpu_to_le16(0x0002),
	VOLUME_UPGRADE_ON_MOUNT		= const_cpu_to_le16(0x0004),
	VOLUME_MOUNTED_ON_NT4		= const_cpu_to_le16(0x0008),
	VOLUME_DELETE_USN_UNDERWAY	= const_cpu_to_le16(0x0010),
	VOLUME_REPAIR_OBJECT_ID		= const_cpu_to_le16(0x0020),
	VOLUME_CHKDSK_UNDERWAY		= const_cpu_to_le16(0x4000),
	VOLUME_MODIFIED_BY_CHKDSK	= const_cpu_to_le16(0x8000),
	VOLUME_FLAGS_MASK		= const_cpu_to_le16(0xc03f),
} __attribute__((__packed__)) VOLUME_FLAGS;

/**
 * struct VOLUME_INFORMATION - Attribute: Volume information (0x70).
 *
 * NOTE: Always resident.
 * NOTE: Present only in FILE_Volume.
 * NOTE: Windows 2000 uses NTFS 3.0 while Windows NT4 service pack 6a uses
 *	 NTFS 1.2. I haven't personally seen other values yet.
 */
typedef struct {
	u64 reserved;		/* Not used (yet?). */
	u8 major_ver;		/* Major version of the ntfs format. */
	u8 minor_ver;		/* Minor version of the ntfs format. */
	VOLUME_FLAGS flags;	/* Bit array of VOLUME_* flags. */
} __attribute__((__packed__)) VOLUME_INFORMATION;

/**
 * struct DATA_ATTR - Attribute: Data attribute (0x80).
 *
 * NOTE: Can be resident or non-resident.
 *
 * Data contents of a file (i.e. the unnamed stream) or of a named stream.
 */
typedef struct {
	u8 data[0];		/* The file's data contents. */
} __attribute__((__packed__)) DATA_ATTR;

/**
 * enum INDEX_HEADER_FLAGS - Index header flags (8-bit).
 */
typedef enum {
	/* When index header is in an index root attribute: */
	SMALL_INDEX	= 0, /* The index is small enough to fit inside the
				index root attribute and there is no index
				allocation attribute present. */
	LARGE_INDEX	= 1, /* The index is too large to fit in the index
				root attribute and/or an index allocation
				attribute is present. */
	/*
	 * When index header is in an index block, i.e. is part of index
	 * allocation attribute:
	 */
	LEAF_NODE	= 0, /* This is a leaf node, i.e. there are no more
				nodes branching off it. */
	INDEX_NODE	= 1, /* This node indexes other nodes, i.e. is not a
				leaf node. */
	NODE_MASK	= 1, /* Mask for accessing the *_NODE bits. */
} __attribute__((__packed__)) INDEX_HEADER_FLAGS;

/**
 * struct INDEX_HEADER -
 *
 * This is the header for indexes, describing the INDEX_ENTRY records, which
 * follow the INDEX_HEADER. Together the index header and the index entries
 * make up a complete index.
 *
 * IMPORTANT NOTE: The offset, length and size structure members are counted
 * relative to the start of the index header structure and not relative to the
 * start of the index root or index allocation structures themselves.
 */
typedef struct {
/*  0*/	u32 entries_offset;	/* Byte offset from the INDEX_HEADER to first
				   INDEX_ENTRY, aligned to 8-byte boundary.  */
/*  4*/	u32 index_length;	/* Data size in byte of the INDEX_ENTRY's,
				   including the INDEX_HEADER, aligned to 8. */
/*  8*/	u32 allocated_size;	/* Allocated byte size of this index (block),
				   multiple of 8 bytes. See more below.      */
	/* 
	   For the index root attribute, the above two numbers are always
	   equal, as the attribute is resident and it is resized as needed.
	   
	   For the index allocation attribute, the attribute is not resident 
	   and the allocated_size is equal to the index_block_size specified 
	   by the corresponding INDEX_ROOT attribute minus the INDEX_BLOCK 
	   size not counting the INDEX_HEADER part (i.e. minus -24).
	 */
/* 12*/	INDEX_HEADER_FLAGS ih_flags;	/* Bit field of INDEX_HEADER_FLAGS.  */
/* 13*/	u8 reserved[3];			/* Reserved/align to 8-byte boundary.*/
/* sizeof() == 16 */
} __attribute__((__packed__)) INDEX_HEADER;

/**
 * struct INDEX_ROOT - Attribute: Index root (0x90).
 *
 * NOTE: Always resident.
 *
 * This is followed by a sequence of index entries (INDEX_ENTRY structures)
 * as described by the index header.
 *
 * When a directory is small enough to fit inside the index root then this
 * is the only attribute describing the directory. When the directory is too
 * large to fit in the index root, on the other hand, two additional attributes
 * are present: an index allocation attribute, containing sub-nodes of the B+
 * directory tree (see below), and a bitmap attribute, describing which virtual
 * cluster numbers (vcns) in the index allocation attribute are in use by an
 * index block.
 *
 * NOTE: The root directory (FILE_root) contains an entry for itself. Other
 * directories do not contain entries for themselves, though.
 */
typedef struct {
/*  0*/	ATTR_TYPES type;		/* Type of the indexed attribute. Is
					   $FILE_NAME for directories, zero
					   for view indexes. No other values
					   allowed. */
/*  4*/	COLLATION_RULES collation_rule;	/* Collation rule used to sort the
					   index entries. If type is $FILE_NAME,
					   this must be COLLATION_FILE_NAME. */
/*  8*/	u32 index_block_size;		/* Size of index block in bytes (in
					   the index allocation attribute). */
/* 12*/	s8 clusters_per_index_block;	/* Size of index block in clusters (in
					   the index allocation attribute), when
					   an index block is >= than a cluster,
					   otherwise sectors per index block. */
/* 13*/	u8 reserved[3];			/* Reserved/align to 8-byte boundary. */
/* 16*/	INDEX_HEADER index;		/* Index header describing the
					   following index entries. */
/* sizeof()= 32 bytes */
} __attribute__((__packed__)) INDEX_ROOT;

/**
 * struct INDEX_BLOCK - Attribute: Index allocation (0xa0).
 *
 * NOTE: Always non-resident (doesn't make sense to be resident anyway!).
 *
 * This is an array of index blocks. Each index block starts with an
 * INDEX_BLOCK structure containing an index header, followed by a sequence of
 * index entries (INDEX_ENTRY structures), as described by the INDEX_HEADER.
 */
typedef struct {
/*  0	NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
	NTFS_RECORD_TYPES magic;/* Magic is "INDX". */
	u16 usa_ofs;		/* See NTFS_RECORD definition. */
	u16 usa_count;		/* See NTFS_RECORD definition. */

/*  8*/	LSN lsn;		/* $LogFile sequence number of the last
				   modification of this index block. */
/* 16*/	VCN index_block_vcn;	/* Virtual cluster number of the index block. */
/* 24*/	INDEX_HEADER index;	/* Describes the following index entries. */
/* sizeof()= 40 (0x28) bytes */
/*
 * When creating the index block, we place the update sequence array at this
 * offset, i.e. before we start with the index entries. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading use the data from the ntfs record header.
 */
} __attribute__((__packed__)) INDEX_BLOCK;

typedef INDEX_BLOCK INDEX_ALLOCATION;

/**
 * struct REPARSE_INDEX_KEY -
 *
 * The system file FILE_Extend/$Reparse contains an index named $R listing
 * all reparse points on the volume. The index entry keys are as defined
 * below. Note, that there is no index data associated with the index entries.
 *
 * The index entries are sorted by the index key file_id. The collation rule is
 * COLLATION_NTOFS_ULONGS. FIXME: Verify whether the reparse_tag is not the
 * primary key / is not a key at all. (AIA)
 */
typedef struct {
	u32 reparse_tag;	/* Reparse point type (inc. flags). */
	MFT_REF file_id;	/* Mft record of the file containing the
				   reparse point attribute. */
} __attribute__((__packed__)) REPARSE_INDEX_KEY;

/**
 * enum QUOTA_FLAGS - Quota flags (32-bit).
 */
typedef enum {
	/* The user quota flags. Names explain meaning. */
	QUOTA_FLAG_DEFAULT_LIMITS	= const_cpu_to_le32(0x00000001),
	QUOTA_FLAG_LIMIT_REACHED	= const_cpu_to_le32(0x00000002),
	QUOTA_FLAG_ID_DELETED		= const_cpu_to_le32(0x00000004),

	QUOTA_FLAG_USER_MASK		= const_cpu_to_le32(0x00000007),
		/* Bit mask for user quota flags. */

	/* These flags are only present in the quota defaults index entry,
	   i.e. in the entry where owner_id = QUOTA_DEFAULTS_ID. */
	QUOTA_FLAG_TRACKING_ENABLED	= const_cpu_to_le32(0x00000010),
	QUOTA_FLAG_ENFORCEMENT_ENABLED	= const_cpu_to_le32(0x00000020),
	QUOTA_FLAG_TRACKING_REQUESTED	= const_cpu_to_le32(0x00000040),
	QUOTA_FLAG_LOG_THRESHOLD	= const_cpu_to_le32(0x00000080),
	QUOTA_FLAG_LOG_LIMIT		= const_cpu_to_le32(0x00000100),
	QUOTA_FLAG_OUT_OF_DATE		= const_cpu_to_le32(0x00000200),
	QUOTA_FLAG_CORRUPT		= const_cpu_to_le32(0x00000400),
	QUOTA_FLAG_PENDING_DELETES	= const_cpu_to_le32(0x00000800),
} QUOTA_FLAGS;

/**
 * struct QUOTA_CONTROL_ENTRY -
 *
 * The system file FILE_Extend/$Quota contains two indexes $O and $Q. Quotas
 * are on a per volume and per user basis.
 *
 * The $Q index contains one entry for each existing user_id on the volume. The
 * index key is the user_id of the user/group owning this quota control entry,
 * i.e. the key is the owner_id. The user_id of the owner of a file, i.e. the
 * owner_id, is found in the standard information attribute. The collation rule
 * for $Q is COLLATION_NTOFS_ULONG.
 *
 * The $O index contains one entry for each user/group who has been assigned
 * a quota on that volume. The index key holds the SID of the user_id the
 * entry belongs to, i.e. the owner_id. The collation rule for $O is
 * COLLATION_NTOFS_SID.
 *
 * The $O index entry data is the user_id of the user corresponding to the SID.
 * This user_id is used as an index into $Q to find the quota control entry
 * associated with the SID.
 *
 * The $Q index entry data is the quota control entry and is defined below.
 */
typedef struct {
	u32 version;		/* Currently equals 2. */
	QUOTA_FLAGS flags;	/* Flags describing this quota entry. */
	u64 bytes_used;		/* How many bytes of the quota are in use. */
	s64 change_time;	/* Last time this quota entry was changed. */
	s64 threshold;		/* Soft quota (-1 if not limited). */
	s64 limit;		/* Hard quota (-1 if not limited). */
	s64 exceeded_time;	/* How long the soft quota has been exceeded. */
/* The below field is NOT present for the quota defaults entry. */
	SID sid;		/* The SID of the user/object associated with
				   this quota entry. If this field is missing
				   then the INDEX_ENTRY is padded with zeros
				   to multiply of 8 which are not counted in
				   the data_length field. If the sid is present
				   then this structure is padded with zeros to
				   multiply of 8 and the padding is counted in
				   the INDEX_ENTRY's data_length. */
} __attribute__((__packed__)) QUOTA_CONTROL_ENTRY;

/**
 * struct QUOTA_O_INDEX_DATA -
 */
typedef struct {
	u32 owner_id;
	u32 unknown;		/* Always 32. Seems to be padding and it's not
				   counted in the INDEX_ENTRY's data_length.
				   This field shouldn't be really here. */
} __attribute__((__packed__)) QUOTA_O_INDEX_DATA;

/**
 * enum PREDEFINED_OWNER_IDS - Predefined owner_id values (32-bit).
 */
typedef enum {
	QUOTA_INVALID_ID	= const_cpu_to_le32(0x00000000),
	QUOTA_DEFAULTS_ID	= const_cpu_to_le32(0x00000001),
	QUOTA_FIRST_USER_ID	= const_cpu_to_le32(0x00000100),
} PREDEFINED_OWNER_IDS;

/**
 * enum INDEX_ENTRY_FLAGS - Index entry flags (16-bit).
 */
typedef enum {
	INDEX_ENTRY_NODE = const_cpu_to_le16(1), /* This entry contains a
					sub-node, i.e. a reference to an index
					block in form of a virtual cluster
					number (see below). */
	INDEX_ENTRY_END  = const_cpu_to_le16(2), /* This signifies the last
					entry in an index block. The index
					entry does not represent a file but it
					can point to a sub-node. */
	INDEX_ENTRY_SPACE_FILLER = 0xffff, /* Just to force 16-bit width. */
} __attribute__((__packed__)) INDEX_ENTRY_FLAGS;

/**
 * struct INDEX_ENTRY_HEADER - This the index entry header (see below).
 * 
 *         ==========================================================
 *         !!!!!  SEE DESCRIPTION OF THE FIELDS AT INDEX_ENTRY  !!!!!
 *         ==========================================================
 */
typedef struct {
/*  0*/	union {
		MFT_REF indexed_file;
		struct {
			u16 data_offset;
			u16 data_length;
			u32 reservedV;
		} __attribute__((__packed__));
	} __attribute__((__packed__));
/*  8*/	u16 length;
/* 10*/	u16 key_length;
/* 12*/	INDEX_ENTRY_FLAGS flags;
/* 14*/	u16 reserved;
/* sizeof() = 16 bytes */
} __attribute__((__packed__)) INDEX_ENTRY_HEADER;

/**
 * struct INDEX_ENTRY - This is an index entry.
 *
 * A sequence of such entries follows each INDEX_HEADER structure. Together
 * they make up a complete index. The index follows either an index root
 * attribute or an index allocation attribute.
 *
 * NOTE: Before NTFS 3.0 only filename attributes were indexed.
 */
typedef struct {
/*  0	INDEX_ENTRY_HEADER; -- Unfolded here as gcc dislikes unnamed structs. */
	union {		/* Only valid when INDEX_ENTRY_END is not set. */
		MFT_REF indexed_file;		/* The mft reference of the file
						   described by this index
						   entry. Used for directory
						   indexes. */
		struct { /* Used for views/indexes to find the entry's data. */
			u16 data_offset;	/* Data byte offset from this
						   INDEX_ENTRY. Follows the
						   index key. */
			u16 data_length;	/* Data length in bytes. */
			u32 reservedV;		/* Reserved (zero). */
		} __attribute__((__packed__));
	} __attribute__((__packed__));
/*  8*/ u16 length;		 /* Byte size of this index entry, multiple of
				    8-bytes. Size includes INDEX_ENTRY_HEADER
				    and the optional subnode VCN. See below. */
/* 10*/ u16 key_length;		 /* Byte size of the key value, which is in the
				    index entry. It follows field reserved. Not
				    multiple of 8-bytes. */
/* 12*/	INDEX_ENTRY_FLAGS ie_flags; /* Bit field of INDEX_ENTRY_* flags. */
/* 14*/	u16 reserved;		 /* Reserved/align to 8-byte boundary. */
/*	End of INDEX_ENTRY_HEADER */
/* 16*/	union {		/* The key of the indexed attribute. NOTE: Only present
			   if INDEX_ENTRY_END bit in flags is not set. NOTE: On
			   NTFS versions before 3.0 the only valid key is the
			   FILE_NAME_ATTR. On NTFS 3.0+ the following
			   additional index keys are defined: */
		FILE_NAME_ATTR file_name;/* $I30 index in directories. */
		SII_INDEX_KEY sii;	/* $SII index in $Secure. */
		SDH_INDEX_KEY sdh;	/* $SDH index in $Secure. */
		GUID object_id;		/* $O index in FILE_Extend/$ObjId: The
					   object_id of the mft record found in
					   the data part of the index. */
		REPARSE_INDEX_KEY reparse;	/* $R index in
						   FILE_Extend/$Reparse. */
		SID sid;		/* $O index in FILE_Extend/$Quota:
					   SID of the owner of the user_id. */
		u32 owner_id;		/* $Q index in FILE_Extend/$Quota:
					   user_id of the owner of the quota
					   control entry in the data part of
					   the index. */
	} __attribute__((__packed__)) key;
	/* The (optional) index data is inserted here when creating.
	VCN vcn;	   If INDEX_ENTRY_NODE bit in ie_flags is set, the last
			   eight bytes of this index entry contain the virtual
			   cluster number of the index block that holds the
			   entries immediately preceding the current entry. 
	
			   If the key_length is zero, then the vcn immediately
			   follows the INDEX_ENTRY_HEADER. 
	
			   The address of the vcn of "ie" INDEX_ENTRY is given by 
			   (char*)ie + le16_to_cpu(ie->length) - sizeof(VCN)
	*/
} __attribute__((__packed__)) INDEX_ENTRY;

/**
 * struct BITMAP_ATTR - Attribute: Bitmap (0xb0).
 *
 * Contains an array of bits (aka a bitfield).
 *
 * When used in conjunction with the index allocation attribute, each bit
 * corresponds to one index block within the index allocation attribute. Thus
 * the number of bits in the bitmap * index block size / cluster size is the
 * number of clusters in the index allocation attribute.
 */
typedef struct {
	u8 bitmap[0];			/* Array of bits. */
} __attribute__((__packed__)) BITMAP_ATTR;

/**
 * enum PREDEFINED_REPARSE_TAGS -
 *
 * The reparse point tag defines the type of the reparse point. It also
 * includes several flags, which further describe the reparse point.
 *
 * The reparse point tag is an unsigned 32-bit value divided in three parts:
 *
 * 1. The least significant 16 bits (i.e. bits 0 to 15) specify the type of
 *    the reparse point.
 * 2. The 13 bits after this (i.e. bits 16 to 28) are reserved for future use.
 * 3. The most significant three bits are flags describing the reparse point.
 *    They are defined as follows:
 *	bit 29: Name surrogate bit. If set, the filename is an alias for
 *		another object in the system.
 *	bit 30: High-latency bit. If set, accessing the first byte of data will
 *		be slow. (E.g. the data is stored on a tape drive.)
 *	bit 31: Microsoft bit. If set, the tag is owned by Microsoft. User
 *		defined tags have to use zero here.
 */
typedef enum {
	IO_REPARSE_TAG_IS_ALIAS		= const_cpu_to_le32(0x20000000),
	IO_REPARSE_TAG_IS_HIGH_LATENCY	= const_cpu_to_le32(0x40000000),
	IO_REPARSE_TAG_IS_MICROSOFT	= const_cpu_to_le32(0x80000000),

	IO_REPARSE_TAG_RESERVED_ZERO	= const_cpu_to_le32(0x00000000),
	IO_REPARSE_TAG_RESERVED_ONE	= const_cpu_to_le32(0x00000001),
	IO_REPARSE_TAG_RESERVED_RANGE	= const_cpu_to_le32(0x00000001),

	IO_REPARSE_TAG_NSS		= const_cpu_to_le32(0x68000005),
	IO_REPARSE_TAG_NSS_RECOVER	= const_cpu_to_le32(0x68000006),
	IO_REPARSE_TAG_SIS		= const_cpu_to_le32(0x68000007),
	IO_REPARSE_TAG_DFS		= const_cpu_to_le32(0x68000008),

	IO_REPARSE_TAG_MOUNT_POINT	= const_cpu_to_le32(0x88000003),

	IO_REPARSE_TAG_HSM		= const_cpu_to_le32(0xa8000004),

	IO_REPARSE_TAG_SYMBOLIC_LINK	= const_cpu_to_le32(0xe8000000),

	IO_REPARSE_TAG_VALID_VALUES	= const_cpu_to_le32(0xe000ffff),
} PREDEFINED_REPARSE_TAGS;

/**
 * struct REPARSE_POINT - Attribute: Reparse point (0xc0).
 *
 * NOTE: Can be resident or non-resident.
 */
typedef struct {
	u32 reparse_tag;		/* Reparse point type (inc. flags). */
	u16 reparse_data_length;	/* Byte size of reparse data. */
	u16 reserved;			/* Align to 8-byte boundary. */
	u8 reparse_data[0];		/* Meaning depends on reparse_tag. */
} __attribute__((__packed__)) REPARSE_POINT;

/**
 * struct EA_INFORMATION - Attribute: Extended attribute information (0xd0).
 *
 * NOTE: Always resident.
 */
typedef struct {
	u16 ea_length;		/* Byte size of the packed extended
				   attributes. */
	u16 need_ea_count;	/* The number of extended attributes which have
				   the NEED_EA bit set. */
	u32 ea_query_length;	/* Byte size of the buffer required to query
				   the extended attributes when calling
				   ZwQueryEaFile() in Windows NT/2k. I.e. the
				   byte size of the unpacked extended
				   attributes. */
} __attribute__((__packed__)) EA_INFORMATION;

/**
 * enum EA_FLAGS - Extended attribute flags (8-bit).
 */
typedef enum {
	NEED_EA	= 0x80,		/* Indicate that the file to which the EA
				   belongs cannot be interpreted without
				   understanding the associated extended
				   attributes. */
} __attribute__((__packed__)) EA_FLAGS;

/**
 * struct EA_ATTR - Attribute: Extended attribute (EA) (0xe0).
 *
 * Like the attribute list and the index buffer list, the EA attribute value is
 * a sequence of EA_ATTR variable length records.
 *
 * FIXME: It appears weird that the EA name is not Unicode. Is it true?
 * FIXME: It seems that name is always uppercased. Is it true?
 */
typedef struct {
	u32 next_entry_offset;	/* Offset to the next EA_ATTR. */
	EA_FLAGS flags;		/* Flags describing the EA. */
	u8 name_length;		/* Length of the name of the extended
				   attribute in bytes. */
	u16 value_length;	/* Byte size of the EA's value. */
	u8 name[0];		/* Name of the EA. */
	u8 value[0];		/* The value of the EA. Immediately
				   follows the name. */
} __attribute__((__packed__)) EA_ATTR;

/**
 * struct PROPERTY_SET - Attribute: Property set (0xf0).
 *
 * Intended to support Native Structure Storage (NSS) - a feature removed from
 * NTFS 3.0 during beta testing.
 */
typedef struct {
	/* Irrelevant as feature unused. */
} __attribute__((__packed__)) PROPERTY_SET;

/**
 * struct LOGGED_UTILITY_STREAM - Attribute: Logged utility stream (0x100).
 *
 * NOTE: Can be resident or non-resident.
 *
 * Operations on this attribute are logged to the journal ($LogFile) like
 * normal metadata changes.
 *
 * Used by the Encrypting File System (EFS).  All encrypted files have this
 * attribute with the name $EFS.  See below for the relevant structures.
 */
typedef struct {
	/* Can be anything the creator chooses. */
} __attribute__((__packed__)) LOGGED_UTILITY_STREAM;

/*
 * $EFS Data Structure:
 *
 * The following information is about the data structures that are contained
 * inside a logged utility stream (0x100) with a name of "$EFS".
 *
 * The stream starts with an instance of EFS_ATTR_HEADER.
 *
 * Next, at offsets offset_to_ddf_array and offset_to_drf_array (unless any of
 * them is 0) there is a EFS_DF_ARRAY_HEADER immediately followed by a sequence
 * of multiple data decryption/recovery fields.
 *
 * Each data decryption/recovery field starts with a EFS_DF_HEADER and the next
 * one (if it exists) can be found by adding EFS_DF_HEADER->df_length bytes to
 * the offset of the beginning of the current EFS_DF_HEADER.
 *
 * The data decryption/recovery field contains an EFS_DF_CERTIFICATE_HEADER, a
 * SID, an optional GUID, an optional container name, a non-optional user name,
 * and the encrypted FEK.
 *
 * Note all the below are best guesses so may have mistakes/inaccuracies.
 * Corrections/clarifications/additions are always welcome!
 *
 * Ntfs.sys takes an EFS value length of <= 0x54 or > 0x40000 to BSOD, i.e. it
 * is invalid.
 */

/**
 * struct EFS_ATTR_HEADER - "$EFS" header.
 *
 * The header of the Logged utility stream (0x100) attribute named "$EFS".
 */
typedef struct {
/*  0*/	u32 length;		/* Length of EFS attribute in bytes. */
	u32 state;		/* Always 0? */
	u32 version;		/* Efs version.  Always 2? */
	u32 crypto_api_version;	/* Always 0? */
/* 16*/	u8 unknown4[16];	/* MD5 hash of decrypted FEK?  This field is
				   created with a call to UuidCreate() so is
				   unlikely to be an MD5 hash and is more
				   likely to be GUID of this encrytped file
				   or something like that. */
/* 32*/	u8 unknown5[16];	/* MD5 hash of DDFs? */
/* 48*/	u8 unknown6[16];	/* MD5 hash of DRFs? */
/* 64*/	u32 offset_to_ddf_array;/* Offset in bytes to the array of data
				   decryption fields (DDF), see below.  Zero if
				   no DDFs are present. */
	u32 offset_to_drf_array;/* Offset in bytes to the array of data
				   recovery fields (DRF), see below.  Zero if
				   no DRFs are present. */
	u32 reserved;		/* Reserved. */
} __attribute__((__packed__)) EFS_ATTR_HEADER;

/**
 * struct EFS_DF_ARRAY_HEADER -
 */
typedef struct {
	u32 df_count;		/* Number of data decryption/recovery fields in
				   the array. */
} __attribute__((__packed__)) EFS_DF_ARRAY_HEADER;

/**
 * struct EFS_DF_HEADER -
 */
typedef struct {
/*  0*/	u32 df_length;		/* Length of this data decryption/recovery
				   field in bytes. */
	u32 cred_header_offset;	/* Offset in bytes to the credential header. */
	u32 fek_size;		/* Size in bytes of the encrypted file
				   encryption key (FEK). */
	u32 fek_offset;		/* Offset in bytes to the FEK from the start of
				   the data decryption/recovery field. */
/* 16*/	u32 unknown1;		/* always 0?  Might be just padding. */
} __attribute__((__packed__)) EFS_DF_HEADER;

/**
 * struct EFS_DF_CREDENTIAL_HEADER -
 */
typedef struct {
/*  0*/	u32 cred_length;	/* Length of this credential in bytes. */
	u32 sid_offset;		/* Offset in bytes to the user's sid from start
				   of this structure.  Zero if no sid is
				   present. */
/*  8*/	u32 type;		/* Type of this credential:
					1 = CryptoAPI container.
					2 = Unexpected type.
					3 = Certificate thumbprint.
					other = Unknown type. */
	union {
		/* CryptoAPI container. */
		struct {
/* 12*/			u32 container_name_offset;	/* Offset in bytes to
				   the name of the container from start of this
				   structure (may not be zero). */
/* 16*/			u32 provider_name_offset;	/* Offset in bytes to
				   the name of the provider from start of this
				   structure (may not be zero). */
			u32 public_key_blob_offset;	/* Offset in bytes to
				   the public key blob from start of this
				   structure. */
/* 24*/			u32 public_key_blob_size;	/* Size in bytes of
				   public key blob. */
		} __attribute__((__packed__));
		/* Certificate thumbprint. */
		struct {
/* 12*/			u32 cert_thumbprint_header_size;	/* Size in
				   bytes of the header of the certificate
				   thumbprint. */
/* 16*/			u32 cert_thumbprint_header_offset;	/* Offset in
				   bytes to the header of the certificate
				   thumbprint from start of this structure. */
			u32 unknown1;	/* Always 0?  Might be padding... */
			u32 unknown2;	/* Always 0?  Might be padding... */
		} __attribute__((__packed__));
	} __attribute__((__packed__));
} __attribute__((__packed__)) EFS_DF_CREDENTIAL_HEADER;

typedef EFS_DF_CREDENTIAL_HEADER EFS_DF_CRED_HEADER;

/**
 * struct EFS_DF_CERTIFICATE_THUMBPRINT_HEADER -
 */
typedef struct {
/*  0*/	u32 thumbprint_offset;		/* Offset in bytes to the thumbprint. */
	u32 thumbprint_size;		/* Size of thumbprint in bytes. */
/*  8*/	u32 container_name_offset;	/* Offset in bytes to the name of the
					   container from start of this
					   structure or 0 if no name present. */
	u32 provider_name_offset;	/* Offset in bytes to the name of the
					   cryptographic provider from start of
					   this structure or 0 if no name
					   present. */
/* 16*/	u32 user_name_offset;		/* Offset in bytes to the user name
					   from start of this structure or 0 if
					   no user name present.  (This is also
					   known as lpDisplayInformation.) */
} __attribute__((__packed__)) EFS_DF_CERTIFICATE_THUMBPRINT_HEADER;

typedef EFS_DF_CERTIFICATE_THUMBPRINT_HEADER EFS_DF_CERT_THUMBPRINT_HEADER;

typedef enum {
	INTX_SYMBOLIC_LINK =
		const_cpu_to_le64(0x014B4E4C78746E49ULL), /* "IntxLNK\1" */
	INTX_CHARACTER_DEVICE =
		const_cpu_to_le64(0x0052484378746E49ULL), /* "IntxCHR\0" */
	INTX_BLOCK_DEVICE =
		const_cpu_to_le64(0x004B4C4278746E49ULL), /* "IntxBLK\0" */
} INTX_FILE_TYPES;

typedef struct {
	INTX_FILE_TYPES magic;		/* Intx file magic. */
	union {
		/* For character and block devices. */
		struct {
			u64 major;		/* Major device number. */
			u64 minor;		/* Minor device number. */
			void *device_end[0];	/* Marker for offsetof(). */
		} __attribute__((__packed__));
		/* For symbolic links. */
		ntfschar target[0];
	} __attribute__((__packed__));
} __attribute__((__packed__)) INTX_FILE;

#endif /* defined _NTFS_LAYOUT_H */

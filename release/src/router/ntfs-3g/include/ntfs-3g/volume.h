/*
 * volume.h - Exports for NTFS volume handling. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2005-2006 Yura Pakhuchiy
 * Copyright (c) 2005-2009 Szabolcs Szakacsits
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

#ifndef _NTFS_VOLUME_H
#define _NTFS_VOLUME_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif

/*
 * Under Cygwin, DJGPP and FreeBSD we do not have MS_RDONLY,
 * so we define them ourselves.
 */
#ifndef MS_RDONLY
#define MS_RDONLY 1
#endif

#define MS_EXCLUSIVE 0x08000000

#ifndef MS_RECOVER
#define MS_RECOVER   0x10000000
#endif

#define MS_IGNORE_HIBERFILE   0x20000000

/* Forward declaration */
typedef struct _ntfs_volume ntfs_volume;

#include "param.h"
#include "types.h"
#include "support.h"
#include "device.h"
#include "inode.h"
#include "attrib.h"
#include "index.h"

/**
 * enum ntfs_mount_flags -
 *
 * Flags returned by the ntfs_check_if_mounted() function.
 */
typedef enum {
	NTFS_MF_MOUNTED		= 1,	/* Device is mounted. */
	NTFS_MF_ISROOT		= 2,	/* Device is mounted as system root. */
	NTFS_MF_READONLY	= 4,	/* Device is mounted read-only. */
} ntfs_mount_flags;

extern int ntfs_check_if_mounted(const char *file, unsigned long *mnt_flags);

typedef enum {
	NTFS_VOLUME_OK			= 0,
	NTFS_VOLUME_SYNTAX_ERROR	= 11,
	NTFS_VOLUME_NOT_NTFS		= 12,
	NTFS_VOLUME_CORRUPT		= 13,
	NTFS_VOLUME_HIBERNATED		= 14,
	NTFS_VOLUME_UNCLEAN_UNMOUNT	= 15,
	NTFS_VOLUME_LOCKED		= 16,
	NTFS_VOLUME_RAID		= 17,
	NTFS_VOLUME_UNKNOWN_REASON	= 18,
	NTFS_VOLUME_NO_PRIVILEGE	= 19,
	NTFS_VOLUME_OUT_OF_MEMORY	= 20,
	NTFS_VOLUME_FUSE_ERROR		= 21,
	NTFS_VOLUME_INSECURE		= 22
} ntfs_volume_status;

/**
 * enum ntfs_volume_state_bits -
 *
 * Defined bits for the state field in the ntfs_volume structure.
 */
typedef enum {
	NV_ReadOnly,		/* 1: Volume is read-only. */
	NV_CaseSensitive,	/* 1: Volume is mounted case-sensitive. */
	NV_LogFileEmpty,	/* 1: $logFile journal is empty. */
	NV_ShowSysFiles,	/* 1: Show NTFS metafiles. */
	NV_ShowHidFiles,	/* 1: Show files marked hidden. */
	NV_HideDotFiles,	/* 1: Set hidden flag on dot files */
	NV_Compression,		/* 1: allow compression */
} ntfs_volume_state_bits;

#define  test_nvol_flag(nv, flag)	 test_bit(NV_##flag, (nv)->state)
#define   set_nvol_flag(nv, flag)	  set_bit(NV_##flag, (nv)->state)
#define clear_nvol_flag(nv, flag)	clear_bit(NV_##flag, (nv)->state)

#define NVolReadOnly(nv)		 test_nvol_flag(nv, ReadOnly)
#define NVolSetReadOnly(nv)		  set_nvol_flag(nv, ReadOnly)
#define NVolClearReadOnly(nv)		clear_nvol_flag(nv, ReadOnly)

#define NVolCaseSensitive(nv)		 test_nvol_flag(nv, CaseSensitive)
#define NVolSetCaseSensitive(nv)	  set_nvol_flag(nv, CaseSensitive)
#define NVolClearCaseSensitive(nv)	clear_nvol_flag(nv, CaseSensitive)

#define NVolLogFileEmpty(nv)		 test_nvol_flag(nv, LogFileEmpty)
#define NVolSetLogFileEmpty(nv)		  set_nvol_flag(nv, LogFileEmpty)
#define NVolClearLogFileEmpty(nv)	clear_nvol_flag(nv, LogFileEmpty)

#define NVolShowSysFiles(nv)		 test_nvol_flag(nv, ShowSysFiles)
#define NVolSetShowSysFiles(nv)		  set_nvol_flag(nv, ShowSysFiles)
#define NVolClearShowSysFiles(nv)	clear_nvol_flag(nv, ShowSysFiles)

#define NVolShowHidFiles(nv)		 test_nvol_flag(nv, ShowHidFiles)
#define NVolSetShowHidFiles(nv)		  set_nvol_flag(nv, ShowHidFiles)
#define NVolClearShowHidFiles(nv)	clear_nvol_flag(nv, ShowHidFiles)

#define NVolHideDotFiles(nv)		 test_nvol_flag(nv, HideDotFiles)
#define NVolSetHideDotFiles(nv)		  set_nvol_flag(nv, HideDotFiles)
#define NVolClearHideDotFiles(nv)	clear_nvol_flag(nv, HideDotFiles)

#define NVolCompression(nv)		 test_nvol_flag(nv, Compression)
#define NVolSetCompression(nv)		  set_nvol_flag(nv, Compression)
#define NVolClearCompression(nv)	clear_nvol_flag(nv, Compression)

/*
 * NTFS version 1.1 and 1.2 are used by Windows NT4.
 * NTFS version 2.x is used by Windows 2000 Beta
 * NTFS version 3.0 is used by Windows 2000.
 * NTFS version 3.1 is used by Windows XP, 2003 and Vista.
 */

#define NTFS_V1_1(major, minor) ((major) == 1 && (minor) == 1)
#define NTFS_V1_2(major, minor) ((major) == 1 && (minor) == 2)
#define NTFS_V2_X(major, minor) ((major) == 2)
#define NTFS_V3_0(major, minor) ((major) == 3 && (minor) == 0)
#define NTFS_V3_1(major, minor) ((major) == 3 && (minor) == 1)

#define NTFS_BUF_SIZE 8192

/**
 * struct _ntfs_volume - structure describing an open volume in memory.
 */
struct _ntfs_volume {
	union {
		struct ntfs_device *dev;	/* NTFS device associated with
						   the volume. */
		void *sb;	/* For kernel porting compatibility. */
	};
	char *vol_name;		/* Name of the volume. */
	unsigned long state;	/* NTFS specific flags describing this volume.
				   See ntfs_volume_state_bits above. */

	ntfs_inode *vol_ni;	/* ntfs_inode structure for FILE_Volume. */
	u8 major_ver;		/* Ntfs major version of volume. */
	u8 minor_ver;		/* Ntfs minor version of volume. */
	le16 flags;		/* Bit array of VOLUME_* flags. */

	u16 sector_size;	/* Byte size of a sector. */
	u8 sector_size_bits;	/* Log(2) of the byte size of a sector. */
	u32 cluster_size;	/* Byte size of a cluster. */
	u32 mft_record_size;	/* Byte size of a mft record. */
	u32 indx_record_size;	/* Byte size of a INDX record. */
	u8 cluster_size_bits;	/* Log(2) of the byte size of a cluster. */
	u8 mft_record_size_bits;/* Log(2) of the byte size of a mft record. */
	u8 indx_record_size_bits;/* Log(2) of the byte size of a INDX record. */

	/* Variables used by the cluster and mft allocators. */
	u8 mft_zone_multiplier;	/* Initial mft zone multiplier. */
	u8 full_zones;		/* cluster zones which are full */
	s64 mft_data_pos;	/* Mft record number at which to allocate the
				   next mft record. */
	LCN mft_zone_start;	/* First cluster of the mft zone. */
	LCN mft_zone_end;	/* First cluster beyond the mft zone. */
	LCN mft_zone_pos;	/* Current position in the mft zone. */
	LCN data1_zone_pos;	/* Current position in the first data zone. */
	LCN data2_zone_pos;	/* Current position in the second data zone. */

	s64 nr_clusters;	/* Volume size in clusters, hence also the
				   number of bits in lcn_bitmap. */
	ntfs_inode *lcnbmp_ni;	/* ntfs_inode structure for FILE_Bitmap. */
	ntfs_attr *lcnbmp_na;	/* ntfs_attr structure for the data attribute
				   of FILE_Bitmap. Each bit represents a
				   cluster on the volume, bit 0 representing
				   lcn 0 and so on. A set bit means that the
				   cluster and vice versa. */

	LCN mft_lcn;		/* Logical cluster number of the data attribute
				   for FILE_MFT. */
	ntfs_inode *mft_ni;	/* ntfs_inode structure for FILE_MFT. */
	ntfs_attr *mft_na;	/* ntfs_attr structure for the data attribute
				   of FILE_MFT. */
	ntfs_attr *mftbmp_na;	/* ntfs_attr structure for the bitmap attribute
				   of FILE_MFT. Each bit represents an mft
				   record in the $DATA attribute, bit 0
				   representing mft record 0 and so on. A set
				   bit means that the mft record is in use and
				   vice versa. */

	ntfs_inode *secure_ni;	/* ntfs_inode structure for FILE $Secure */
	ntfs_index_context *secure_xsii; /* index for using $Secure:$SII */
	ntfs_index_context *secure_xsdh; /* index for using $Secure:$SDH */
	int secure_reentry;  /* check for non-rentries */
	unsigned int secure_flags;  /* flags, see security.h for values */

	int mftmirr_size;	/* Size of the FILE_MFTMirr in mft records. */
	LCN mftmirr_lcn;	/* Logical cluster number of the data attribute
				   for FILE_MFTMirr. */
	ntfs_inode *mftmirr_ni;	/* ntfs_inode structure for FILE_MFTMirr. */
	ntfs_attr *mftmirr_na;	/* ntfs_attr structure for the data attribute
				   of FILE_MFTMirr. */

	ntfschar *upcase;	/* Upper case equivalents of all 65536 2-byte
				   Unicode characters. Obtained from
				   FILE_UpCase. */
	u32 upcase_len;		/* Length in Unicode characters of the upcase
				   table. */
	ntfschar *locase;	/* Lower case equivalents of all 65536 2-byte
				   Unicode characters. Only if option
				   case_ignore is set. */

	ATTR_DEF *attrdef;	/* Attribute definitions. Obtained from
				   FILE_AttrDef. */
	s32 attrdef_len;	/* Size of the attribute definition table in
				   bytes. */

	s64 free_clusters; 	/* Track the number of free clusters which
				   greatly improves statfs() performance */
	s64 free_mft_records; 	/* Same for free mft records (see above) */
	BOOL efs_raw;		/* volume is mounted for raw access to
				   efs-encrypted files */

#if CACHE_INODE_SIZE
	struct CACHE_HEADER *xinode_cache;
#endif
#if CACHE_NIDATA_SIZE
	struct CACHE_HEADER *nidata_cache;
#endif
#if CACHE_LOOKUP_SIZE
	struct CACHE_HEADER *lookup_cache;
#endif
#if CACHE_SECURID_SIZE
	struct CACHE_HEADER *securid_cache;
#endif
#if CACHE_LEGACY_SIZE
	struct CACHE_HEADER *legacy_cache;
#endif

};

extern const char *ntfs_home;

extern ntfs_volume *ntfs_volume_alloc(void);

extern ntfs_volume *ntfs_volume_startup(struct ntfs_device *dev,
		unsigned long flags);

extern ntfs_volume *ntfs_device_mount(struct ntfs_device *dev,
		unsigned long flags);

extern ntfs_volume *ntfs_mount(const char *name, unsigned long flags);
extern int ntfs_umount(ntfs_volume *vol, const BOOL force);

extern int ntfs_version_is_supported(ntfs_volume *vol);
extern int ntfs_volume_check_hiberfile(ntfs_volume *vol, int verbose);
extern int ntfs_logfile_reset(ntfs_volume *vol);

extern int ntfs_volume_write_flags(ntfs_volume *vol, const le16 flags);

extern int ntfs_volume_error(int err);
extern void ntfs_mount_error(const char *vol, const char *mntpoint, int err);

extern int ntfs_volume_get_free_space(ntfs_volume *vol);

extern int ntfs_set_shown_files(ntfs_volume *vol,
		BOOL show_sys_files, BOOL show_hid_files, BOOL hide_dot_files);
extern int ntfs_set_locale(void);
extern int ntfs_set_ignore_case(ntfs_volume *vol);

#endif /* defined _NTFS_VOLUME_H */


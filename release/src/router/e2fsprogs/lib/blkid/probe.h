/*
 * probe.h - constants and on-disk structures for extracting device data
 *
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#ifndef _BLKID_PROBE_H
#define _BLKID_PROBE_H

#include <blkid/blkid_types.h>

struct blkid_magic;

#define SB_BUFFER_SIZE		0x11000

struct blkid_probe {
	int			fd;
	blkid_cache		cache;
	blkid_dev		dev;
	unsigned char		*sbbuf;
	size_t			sb_valid;
	unsigned char		*buf;
	size_t			buf_max;
};

typedef int (*blkid_probe_t)(struct blkid_probe *probe,
			     struct blkid_magic *id, unsigned char *buf);

struct blkid_magic {
	const char	*bim_type;	/* type name for this magic */
	long		bim_kboff;	/* kilobyte offset of superblock */
	unsigned	bim_sboff;	/* byte offset within superblock */
	unsigned	bim_len;	/* length of magic */
	const char	*bim_magic;	/* magic string */
	blkid_probe_t	bim_probe;	/* probe function */
};

/*
 * Structures for each of the content types we want to extract information
 * from.  We do not necessarily need the magic field here, because we have
 * already identified the content type before we get this far.  It may still
 * be useful if there are probe functions which handle multiple content types.
 */
struct ext2_super_block {
	__u32		s_inodes_count;
	__u32		s_blocks_count;
	__u32		s_r_blocks_count;
	__u32		s_free_blocks_count;
	__u32		s_free_inodes_count;
	__u32		s_first_data_block;
	__u32		s_log_block_size;
	__u32		s_dummy3[7];
	unsigned char	s_magic[2];
	__u16		s_state;
	__u32		s_dummy5[8];
	__u32		s_feature_compat;
	__u32		s_feature_incompat;
	__u32		s_feature_ro_compat;
	unsigned char   s_uuid[16];
	char	   s_volume_name[16];
	char	s_last_mounted[64];
	__u32	s_algorithm_usage_bitmap;
	__u8	s_prealloc_blocks;
	__u8	s_prealloc_dir_blocks;
	__u16	s_reserved_gdt_blocks;
	__u8	s_journal_uuid[16];
	__u32	s_journal_inum;
	__u32	s_journal_dev;
	__u32	s_last_orphan;
	__u32	s_hash_seed[4];
	__u8	s_def_hash_version;
	__u8	s_jnl_backup_type;
	__u16	s_reserved_word_pad;
	__u32	s_default_mount_opts;
	__u32	s_first_meta_bg;
	__u32	s_mkfs_time;
	__u32	s_jnl_blocks[17];
	__u32	s_blocks_count_hi;
	__u32	s_r_blocks_count_hi;
	__u32	s_free_blocks_hi;
	__u16	s_min_extra_isize;
	__u16	s_want_extra_isize;
	__u32	s_flags;
	__u16   s_raid_stride;
	__u16   s_mmp_interval;
	__u64   s_mmp_block;
	__u32   s_raid_stripe_width;
	__u32   s_reserved[163];
};

/* for s_flags */
#define EXT2_FLAGS_TEST_FILESYS		0x0004

/* for s_feature_compat */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004

/* for s_feature_ro_compat */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT4_FEATURE_RO_COMPAT_QUOTA		0x0100

/* for s_feature_incompat */
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040 /* extents support */
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200

#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP

#define EXT3_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT3_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT3_FEATURE_INCOMPAT_RECOVER| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_INCOMPAT_UNSUPPORTED	~EXT3_FEATURE_INCOMPAT_SUPP
#define EXT3_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT3_FEATURE_RO_COMPAT_SUPP


struct xfs_super_block {
	unsigned char	xs_magic[4];
	__u32		xs_blocksize;
	__u64		xs_dblocks;
	__u64		xs_rblocks;
	__u32		xs_dummy1[2];
	unsigned char	xs_uuid[16];
	__u32		xs_dummy2[15];
	char		xs_fname[12];
	__u32		xs_dummy3[2];
	__u64		xs_icount;
	__u64		xs_ifree;
	__u64		xs_fdblocks;
};

struct reiserfs_super_block {
	__u32		rs_blocks_count;
	__u32		rs_free_blocks;
	__u32		rs_root_block;
	__u32		rs_journal_block;
	__u32		rs_journal_dev;
	__u32		rs_orig_journal_size;
	__u32		rs_dummy2[5];
	__u16		rs_blocksize;
	__u16		rs_dummy3[3];
	unsigned char	rs_magic[12];
	__u32		rs_dummy4[5];
	unsigned char	rs_uuid[16];
	char		rs_label[16];
};

struct reiser4_super_block {
	unsigned char	rs4_magic[16];
	__u16		rs4_dummy[2];
	unsigned char	rs4_uuid[16];
	unsigned char	rs4_label[16];
	__u64		rs4_dummy2;
};

struct jfs_super_block {
	unsigned char	js_magic[4];
	__u32		js_version;
	__u64		js_size;
	__u32		js_bsize;	/* 4: aggregate block size in bytes */
	__u16		js_l2bsize;	/* 2: log2 of s_bsize */
	__u16		js_l2bfactor;	/* 2: log2(s_bsize/hardware block size) */
	__u32		js_pbsize;	/* 4: hardware/LVM block size in bytes */
	__u16		js_l2pbsize;	/* 2: log2 of s_pbsize */
	__u16 		js_pad;		/* 2: padding necessary for alignment */
	__u32		js_dummy2[26];
	unsigned char	js_uuid[16];
	unsigned char	js_label[16];
	unsigned char	js_loguuid[16];
};

struct romfs_super_block {
	unsigned char	ros_magic[8];
	__u32		ros_dummy1[2];
	unsigned char	ros_volume[16];
};

struct cramfs_super_block {
	__u8		magic[4];
	__u32		size;
	__u32		flags;
	__u32		future;
	__u8		signature[16];
	struct cramfs_info {
		__u32		crc;
		__u32		edition;
		__u32		blocks;
		__u32		files;
	} info;
	__u8		name[16];
};

struct swap_id_block {
/*	unsigned char	sws_boot[1024]; */
	__u32		sws_version;
	__u32		sws_lastpage;
	__u32		sws_nrbad;
	unsigned char	sws_uuid[16];
	char		sws_volume[16];
	unsigned char	sws_pad[117];
	__u32		sws_badpg;
};

/* Yucky misaligned values */
struct vfat_super_block {
/* 00*/	unsigned char	vs_ignored[3];
/* 03*/	unsigned char	vs_sysid[8];
/* 0b*/	unsigned char	vs_sector_size[2];
/* 0d*/	__u8		vs_cluster_size;
/* 0e*/	__u16		vs_reserved;
/* 10*/	__u8		vs_fats;
/* 11*/	unsigned char	vs_dir_entries[2];
/* 13*/	unsigned char	vs_sectors[2];
/* 15*/	unsigned char	vs_media;
/* 16*/	__u16		vs_fat_length;
/* 18*/	__u16		vs_secs_track;
/* 1a*/	__u16		vs_heads;
/* 1c*/	__u32		vs_hidden;
/* 20*/	__u32		vs_total_sect;
/* 24*/	__u32		vs_fat32_length;
/* 28*/	__u16		vs_flags;
/* 2a*/	__u8		vs_version[2];
/* 2c*/	__u32		vs_root_cluster;
/* 30*/	__u16		vs_insfo_sector;
/* 32*/	__u16		vs_backup_boot;
/* 34*/	__u16		vs_reserved2[6];
/* 40*/	unsigned char	vs_unknown[3];
/* 43*/	unsigned char	vs_serno[4];
/* 47*/	unsigned char	vs_label[11];
/* 52*/	unsigned char   vs_magic[8];
/* 5a*/	unsigned char	vs_dummy2[164];
/*1fe*/	unsigned char	vs_pmagic[2];
};

/* Yucky misaligned values */
struct msdos_super_block {
/* 00*/	unsigned char	ms_ignored[3];
/* 03*/	unsigned char	ms_sysid[8];
/* 0b*/	unsigned char	ms_sector_size[2];
/* 0d*/	__u8		ms_cluster_size;
/* 0e*/	__u16		ms_reserved;
/* 10*/	__u8		ms_fats;
/* 11*/	unsigned char	ms_dir_entries[2];
/* 13*/	unsigned char	ms_sectors[2];
/* 15*/	unsigned char	ms_media;
/* 16*/	__u16		ms_fat_length;
/* 18*/	__u16		ms_secs_track;
/* 1a*/	__u16		ms_heads;
/* 1c*/	__u32		ms_hidden;
/* 20*/	__u32		ms_total_sect;
/* 24*/	unsigned char	ms_unknown[3];
/* 27*/	unsigned char	ms_serno[4];
/* 2b*/	unsigned char	ms_label[11];
/* 36*/	unsigned char   ms_magic[8];
/* 3d*/	unsigned char	ms_dummy2[192];
/*1fe*/	unsigned char	ms_pmagic[2];
};

struct vfat_dir_entry {
	__u8	name[11];
	__u8	attr;
	__u16	time_creat;
	__u16	date_creat;
	__u16	time_acc;
	__u16	date_acc;
	__u16	cluster_high;
	__u16	time_write;
	__u16	date_write;
	__u16	cluster_low;
	__u32	size;
};

/* maximum number of clusters */
#define FAT12_MAX 0xFF4
#define FAT16_MAX 0xFFF4
#define FAT32_MAX 0x0FFFFFF6

struct minix_super_block {
	__u16		ms_ninodes;
	__u16		ms_nzones;
	__u16		ms_imap_blocks;
	__u16		ms_zmap_blocks;
	__u16		ms_firstdatazone;
	__u16		ms_log_zone_size;
	__u32		ms_max_size;
	unsigned char	ms_magic[2];
	__u16		ms_state;
	__u32		ms_zones;
};

struct mdp_superblock_s {
	__u32 md_magic;
	__u32 major_version;
	__u32 minor_version;
	__u32 patch_version;
	__u32 gvalid_words;
	__u32 set_uuid0;
	__u32 ctime;
	__u32 level;
	__u32 size;
	__u32 nr_disks;
	__u32 raid_disks;
	__u32 md_minor;
	__u32 not_persistent;
	__u32 set_uuid1;
	__u32 set_uuid2;
	__u32 set_uuid3;
};

struct hfs_super_block {
	char	h_magic[2];
	char	h_dummy[18];
	__u32	h_blksize;
};

struct ocfs_volume_header {
	unsigned char	minor_version[4];
	unsigned char	major_version[4];
	unsigned char	signature[128];
	char		mount[128];
	unsigned char   mount_len[2];
};

struct ocfs_volume_label {
	unsigned char	disk_lock[48];
	char		label[64];
	unsigned char	label_len[2];
	unsigned char  vol_id[16];
	unsigned char  vol_id_len[2];
};

#define ocfsmajor(o) ((__u32)o.major_version[0] \
                   + (((__u32) o.major_version[1]) << 8) \
                   + (((__u32) o.major_version[2]) << 16) \
                   + (((__u32) o.major_version[3]) << 24))
#define ocfslabellen(o)	((__u32)o.label_len[0] + (((__u32) o.label_len[1]) << 8))
#define ocfsmountlen(o)	((__u32)o.mount_len[0] + (((__u32) o.mount_len[1])<<8))

#define OCFS_MAGIC "OracleCFS"

struct ocfs2_super_block {
	unsigned char  signature[8];
	unsigned char  s_dummy1[184];
	unsigned char  s_dummy2[80];
	char	       s_label[64];
	unsigned char  s_uuid[16];
};

#define OCFS2_MIN_BLOCKSIZE             512
#define OCFS2_MAX_BLOCKSIZE             4096

#define OCFS2_SUPER_BLOCK_BLKNO         2

#define OCFS2_SUPER_BLOCK_SIGNATURE     "OCFSV2"

struct oracle_asm_disk_label {
	char dummy[32];
	char dl_tag[8];
	char dl_id[24];
};

#define ORACLE_ASM_DISK_LABEL_MARKED    "ORCLDISK"
#define ORACLE_ASM_DISK_LABEL_OFFSET    32

struct iso_volume_descriptor {
	unsigned char	vd_type;
	unsigned char	vd_id[5];
	unsigned char	vd_version;
	unsigned char	flags;
	unsigned char	system_id[32];
	unsigned char	volume_id[32];
	unsigned char	unused[8];
	unsigned char	space_size[8];
	unsigned char	escape_sequences[8];
};

/* Common gfs/gfs2 constants: */
#define GFS_MAGIC               0x01161970
#define GFS_DEFAULT_BSIZE       4096
#define GFS_SUPERBLOCK_OFFSET	(0x10 * GFS_DEFAULT_BSIZE)
#define GFS_METATYPE_SB         1
#define GFS_FORMAT_SB           100
#define GFS_LOCKNAME_LEN        64

/* gfs1 constants: */
#define GFS_FORMAT_FS           1309
#define GFS_FORMAT_MULTI        1401
/* gfs2 constants: */
#define GFS2_FORMAT_FS          1801
#define GFS2_FORMAT_MULTI       1900

struct gfs2_meta_header {
	__u32 mh_magic;
	__u32 mh_type;
	__u64 __pad0;          /* Was generation number in gfs1 */
	__u32 mh_format;
	__u32 __pad1;          /* Was incarnation number in gfs1 */
};

struct gfs2_inum {
	__u64 no_formal_ino;
	__u64 no_addr;
};

struct gfs2_sb {
	struct gfs2_meta_header sb_header;

	__u32 sb_fs_format;
	__u32 sb_multihost_format;
	__u32  __pad0;  /* Was superblock flags in gfs1 */

	__u32 sb_bsize;
	__u32 sb_bsize_shift;
	__u32 __pad1;   /* Was journal segment size in gfs1 */

	struct gfs2_inum sb_master_dir; /* Was jindex dinode in gfs1 */
	struct gfs2_inum __pad2; /* Was rindex dinode in gfs1 */
	struct gfs2_inum sb_root_dir;

	char sb_lockproto[GFS_LOCKNAME_LEN];
	char sb_locktable[GFS_LOCKNAME_LEN];
	/* In gfs1, quota and license dinodes followed */
};

struct ntfs_super_block {
	__u8	jump[3];
	__u8	oem_id[8];
	__u8	bios_parameter_block[25];
	__u16	unused[2];
	__u64	number_of_sectors;
	__u64	mft_cluster_location;
	__u64	mft_mirror_cluster_location;
	__s8	cluster_per_mft_record;
	__u8	reserved1[3];
	__s8	cluster_per_index_record;
	__u8	reserved2[3];
	__u64	volume_serial;
	__u16	checksum;
};

struct master_file_table_record {
	__u32	magic;
	__u16	usa_ofs;
	__u16	usa_count;
	__u64	lsn;
	__u16	sequence_number;
	__u16	link_count;
	__u16	attrs_offset;
	__u16	flags;
	__u32	bytes_in_use;
	__u32	bytes_allocated;
} __attribute__((__packed__));

struct file_attribute {
	__u32	type;
	__u32	len;
	__u8	non_resident;
	__u8	name_len;
	__u16	name_offset;
	__u16	flags;
	__u16	instance;
	__u32	value_len;
	__u16	value_offset;
} __attribute__((__packed__));

#define MFT_RECORD_VOLUME			3
#define MFT_RECORD_ATTR_VOLUME_NAME		0x60
#define MFT_RECORD_ATTR_VOLUME_INFO		0x70
#define MFT_RECORD_ATTR_OBJECT_ID		0x40
#define MFT_RECORD_ATTR_END			0xffffffffu

/* HFS / HFS+ */
struct hfs_finder_info {
        __u32        boot_folder;
        __u32        start_app;
        __u32        open_folder;
        __u32        os9_folder;
        __u32        reserved;
        __u32        osx_folder;
        __u8         id[8];
} __attribute__((packed));

struct hfs_mdb {
        __u8         signature[2];
        __u32        cr_date;
        __u32        ls_Mod;
        __u16        atrb;
        __u16        nm_fls;
        __u16        vbm_st;
        __u16        alloc_ptr;
        __u16        nm_al_blks;
        __u32        al_blk_size;
        __u32        clp_size;
        __u16        al_bl_st;
        __u32        nxt_cnid;
        __u16        free_bks;
        __u8         label_len;
        __u8         label[27];
        __u32        vol_bkup;
        __u16        vol_seq_num;
        __u32        wr_cnt;
        __u32        xt_clump_size;
        __u32        ct_clump_size;
        __u16        num_root_dirs;
        __u32        file_count;
        __u32        dir_count;
        struct hfs_finder_info finder_info;
        __u8         embed_sig[2];
        __u16        embed_startblock;
        __u16        embed_blockcount;
} __attribute__((packed));


#define HFS_NODE_LEAF			0xff
#define HFSPLUS_POR_CNID		1

struct hfsplus_bnode_descriptor {
	__u32		next;
	__u32		prev;
	__u8		type;
	__u8		height;
	__u16		num_recs;
	__u16		reserved;
} __attribute__((packed));

struct hfsplus_bheader_record {
	__u16		depth;
	__u32		root;
	__u32		leaf_count;
	__u32		leaf_head;
	__u32		leaf_tail;
	__u16		node_size;
} __attribute__((packed));

struct hfsplus_catalog_key {
	__u16	key_len;
	__u32	parent_id;
	__u16	unicode_len;
	__u8		unicode[255 * 2];
} __attribute__((packed));

struct hfsplus_extent {
	__u32		start_block;
	__u32		block_count;
} __attribute__((packed));

#define HFSPLUS_EXTENT_COUNT		8
struct hfsplus_fork {
	__u64		total_size;
	__u32		clump_size;
	__u32		total_blocks;
	struct hfsplus_extent extents[HFSPLUS_EXTENT_COUNT];
} __attribute__((packed));

struct hfsplus_vol_header {
	__u8		signature[2];
	__u16		version;
	__u32		attributes;
	__u32		last_mount_vers;
	__u32		reserved;
	__u32		create_date;
	__u32		modify_date;
	__u32		backup_date;
	__u32		checked_date;
	__u32		file_count;
	__u32		folder_count;
	__u32		blocksize;
	__u32		total_blocks;
	__u32		free_blocks;
	__u32		next_alloc;
	__u32		rsrc_clump_sz;
	__u32		data_clump_sz;
	__u32		next_cnid;
	__u32		write_count;
	__u64		encodings_bmp;
	struct hfs_finder_info finder_info;
	struct hfsplus_fork alloc_file;
	struct hfsplus_fork ext_file;
	struct hfsplus_fork cat_file;
	struct hfsplus_fork attr_file;
	struct hfsplus_fork start_file;
}  __attribute__((packed));


/* this is lvm's label_header & pv_header combined. */

#define LVM2_ID_LEN 32

struct lvm2_pv_label_header {
	/* label_header */
	__u8	id[8];		/* LABELONE */
	__u64	sector_xl;	/* Sector number of this label */
	__u32	crc_xl;		/* From next field to end of sector */
	__u32	offset_xl;	/* Offset from start of struct to contents */
	__u8	type[8];	/* LVM2 001 */
	/* pv_header */
	__u8	pv_uuid[LVM2_ID_LEN];
} __attribute__ ((packed));


/*
 * this is a very generous portion of the super block, giving us
 * room to translate 14 chunks with 3 stripes each.
 */
#define BTRFS_SYSTEM_CHUNK_ARRAY_SIZE 2048
#define BTRFS_LABEL_SIZE 256
#define BTRFS_UUID_SIZE 16
#define BTRFS_FSID_SIZE 16
#define BTRFS_CSUM_SIZE 32

struct btrfs_dev_item {
	/* the internal btrfs device id */
	__u64 devid;

	/* size of the device */
	__u64 total_bytes;

	/* bytes used */
	__u64 bytes_used;

	/* optimal io alignment for this device */
	__u32 io_align;

	/* optimal io width for this device */
	__u32 io_width;

	/* minimal io size for this device */
	__u32 sector_size;

	/* type and info about this device */
	__u64 type;

	/* expected generation for this device */
	__u64 generation;

	/*
	 * starting byte of this partition on the device,
	 * to allowr for stripe alignment in the future
	 */
	__u64 start_offset;

	/* grouping information for allocation decisions */
	__u32 dev_group;

	/* seek speed 0-100 where 100 is fastest */
	__u8 seek_speed;

	/* bandwidth 0-100 where 100 is fastest */
	__u8 bandwidth;

	/* btrfs generated uuid for this device */
	__u8 uuid[BTRFS_UUID_SIZE];

	/* uuid of FS who owns this device */
	__u8 fsid[BTRFS_UUID_SIZE];
} __attribute__ ((__packed__));

/*
 * the super block basically lists the main trees of the FS
 * it currently lacks any block count etc etc
 */
struct btrfs_super_block {
	__u8 csum[BTRFS_CSUM_SIZE];
	/* the first 3 fields must match struct btrfs_header */
	__u8 fsid[BTRFS_FSID_SIZE];    /* FS specific uuid */
	__u64 bytenr; /* this block number */
	__u64 flags;

	/* allowed to be different from the btrfs_header from here own down */
	__u64 magic;
	__u64 generation;
	__u64 root;
	__u64 chunk_root;
	__u64 log_root;

	/* this will help find the new super based on the log root */
	__u64 log_root_transid;
	__u64 total_bytes;
	__u64 bytes_used;
	__u64 root_dir_objectid;
	__u64 num_devices;
	__u32 sectorsize;
	__u32 nodesize;
	__u32 leafsize;
	__u32 stripesize;
	__u32 sys_chunk_array_size;
	__u64 chunk_root_generation;
	__u64 compat_flags;
	__u64 compat_ro_flags;
	__u64 incompat_flags;
	__u16 csum_type;
	__u8 root_level;
	__u8 chunk_root_level;
	__u8 log_root_level;
	struct btrfs_dev_item dev_item;

	char label[BTRFS_LABEL_SIZE];

	/* future expansion */
	__u64 reserved[32];
	__u8 sys_chunk_array[BTRFS_SYSTEM_CHUNK_ARRAY_SIZE];
} __attribute__ ((__packed__));

/*
 * Byte swap functions
 */
#ifdef __GNUC__
#define _INLINE_ static __inline__
#else				/* For Watcom C */
#define _INLINE_ static inline
#endif

static __u16 blkid_swab16(__u16 val);
static __u32 blkid_swab32(__u32 val);
static __u64 blkid_swab64(__u64 val);

#if ((defined __GNUC__) && \
     (defined(__i386__) || defined(__i486__) || defined(__i586__)))

#define _BLKID_HAVE_ASM_BITOPS_

_INLINE_ __u32 blkid_swab32(__u32 val)
{
#ifdef EXT2FS_REQUIRE_486
	__asm__("bswap %0" : "=r" (val) : "0" (val));
#else
	__asm__("xchgb %b0,%h0\n\t"	/* swap lower bytes	*/
		"rorl $16,%0\n\t"	/* swap words		*/
		"xchgb %b0,%h0"		/* swap higher bytes	*/
		:"=q" (val)
		: "0" (val));
#endif
	return val;
}

_INLINE_ __u16 blkid_swab16(__u16 val)
{
	__asm__("xchgb %b0,%h0"		/* swap bytes		*/ \
		: "=q" (val) \
		:  "0" (val)); \
		return val;
}

_INLINE_ __u64 blkid_swab64(__u64 val)
{
	return (blkid_swab32(val >> 32) |
		(((__u64) blkid_swab32(val & 0xFFFFFFFFUL)) << 32));
}
#endif

#if !defined(_BLKID_HAVE_ASM_BITOPS_)

_INLINE_  __u16 blkid_swab16(__u16 val)
{
	return (val >> 8) | (val << 8);
}

_INLINE_ __u32 blkid_swab32(__u32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
		((val<<8)&0xFF0000) | (val<<24));
}

_INLINE_ __u64 blkid_swab64(__u64 val)
{
	return (blkid_swab32(val >> 32) |
		(((__u64) blkid_swab32(val & 0xFFFFFFFFUL)) << 32));
}
#endif



#ifdef WORDS_BIGENDIAN
#define blkid_le16(x) blkid_swab16(x)
#define blkid_le32(x) blkid_swab32(x)
#define blkid_le64(x) blkid_swab64(x)
#define blkid_be16(x) (x)
#define blkid_be32(x) (x)
#define blkid_be64(x) (x)
#else
#define blkid_le16(x) (x)
#define blkid_le32(x) (x)
#define blkid_le64(x) (x)
#define blkid_be16(x) blkid_swab16(x)
#define blkid_be32(x) blkid_swab32(x)
#define blkid_be64(x) blkid_swab64(x)
#endif

#undef _INLINE_

#endif /* _BLKID_PROBE_H */

/*
 * qcow2.h --- structures and function prototypes for qcow2.c to generate
 * qcow2 formatted disk images.  This format is used originally by QEMU
 * for virtual machines, and stores the filesystem data on disk in a
 * packed format to avoid creating sparse image files that need lots of
 * seeking to read and write.
 *
 * The qcow2 format supports zlib compression, but that is not yet
 * implemented.
 *
 * It is possible to directly mount a qcow2 image using qemu-nbd:
 *
 * [root]# modprobe nbd max_part=63
 * [root]# qemu-nbd -c /dev/nbd0 image.img
 * [root]# mount /dev/nbd0p1 /mnt/qemu
 *
 * Format details at http://people.gnome.org/~markmc/qcow-image-format.html
 *
 * Copyright (C) 2010 Red Hat, Inc., Lukas Czerner <lczerner@redhat.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* Number of l2 tables in memory before writeback */
#define L2_CACHE_PREALLOC	512


#define QCOW_MAGIC (('Q' << 24) | ('F' << 16) | ('I' << 8) | 0xfb)
#define QCOW_VERSION		2
#define QCOW_OFLAG_COPIED	(1LL << 63)
#define QCOW_OFLAG_COMPRESSED	(1LL << 62)

#define QCOW_COMPRESSED		1
#define QCOW_ENCRYPTED		2

struct ext2_qcow2_hdr {
	__u32	magic;
	__u32	version;

	__u64	backing_file_offset;
	__u32	backing_file_size;

	__u32	cluster_bits;
	__u64	size;
	__u32	crypt_method;

	__u32	l1_size;
	__u64	l1_table_offset;

	__u64	refcount_table_offset;
	__u32	refcount_table_clusters;

	__u32	nb_snapshots;
	__u64	snapshots_offset;
};

typedef struct ext2_qcow2_l2_table L2_CACHE_HEAD;

struct ext2_qcow2_l2_table {
	__u32		l1_index;
	__u64		offset;
	__u64		*data;
	L2_CACHE_HEAD	*next;
};

struct ext2_qcow2_l2_cache {
	L2_CACHE_HEAD	*used_head;
	L2_CACHE_HEAD	*used_tail;
	L2_CACHE_HEAD	*free_head;
	__u32		free;
	__u32		count;
	__u64		next_offset;
};

struct ext2_qcow2_refcount {
	__u64	*refcount_table;
	__u64	refcount_table_offset;
	__u64	refcount_block_offset;

	__u32	refcount_table_clusters;
	__u32	refcount_table_index;
	__u32	refcount_block_index;

	__u16	*refcount_block;
};

struct ext2_qcow2_image {
	int	fd;
	struct	ext2_qcow2_hdr		*hdr;
	struct	ext2_qcow2_l2_cache	*l2_cache;
	struct	ext2_qcow2_refcount	refcount;
	__u32	cluster_size;
	__u32	cluster_bits;
	__u32	l1_size;
	__u32	l2_size;

	__u64	*l1_table;
	__u64	l2_offset;
	__u64	l1_offset;
	__u64	image_size;
};

/* Function prototypes */

/* qcow2.c */

/* Functions for converting qcow2 image into raw image */
struct ext2_qcow2_hdr *qcow2_read_header(int);
int qcow2_write_raw_image(int, int, struct ext2_qcow2_hdr *);


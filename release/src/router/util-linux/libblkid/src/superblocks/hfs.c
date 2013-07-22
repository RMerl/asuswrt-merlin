/*
 * Copyright (C) 2004-2008 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "superblocks.h"
#include "md5.h"

/* HFS / HFS+ */
struct hfs_finder_info {
        uint32_t        boot_folder;
        uint32_t        start_app;
        uint32_t        open_folder;
        uint32_t        os9_folder;
        uint32_t        reserved;
        uint32_t        osx_folder;
        uint8_t         id[8];
} __attribute__((packed));

struct hfs_mdb {
        uint8_t         signature[2];
        uint32_t        cr_date;
        uint32_t        ls_Mod;
        uint16_t        atrb;
        uint16_t        nm_fls;
        uint16_t        vbm_st;
        uint16_t        alloc_ptr;
        uint16_t        nm_al_blks;
        uint32_t        al_blk_size;
        uint32_t        clp_size;
        uint16_t        al_bl_st;
        uint32_t        nxt_cnid;
        uint16_t        free_bks;
        uint8_t         label_len;
        uint8_t         label[27];
        uint32_t        vol_bkup;
        uint16_t        vol_seq_num;
        uint32_t        wr_cnt;
        uint32_t        xt_clump_size;
        uint32_t        ct_clump_size;
        uint16_t        num_root_dirs;
        uint32_t        file_count;
        uint32_t        dir_count;
        struct hfs_finder_info finder_info;
        uint8_t         embed_sig[2];
        uint16_t        embed_startblock;
        uint16_t        embed_blockcount;
} __attribute__((packed));


#define HFS_NODE_LEAF			0xff
#define HFSPLUS_POR_CNID		1

struct hfsplus_bnode_descriptor {
	uint32_t		next;
	uint32_t		prev;
	uint8_t		type;
	uint8_t		height;
	uint16_t		num_recs;
	uint16_t		reserved;
} __attribute__((packed));

struct hfsplus_bheader_record {
	uint16_t		depth;
	uint32_t		root;
	uint32_t		leaf_count;
	uint32_t		leaf_head;
	uint32_t		leaf_tail;
	uint16_t		node_size;
} __attribute__((packed));

struct hfsplus_catalog_key {
	uint16_t	key_len;
	uint32_t	parent_id;
	uint16_t	unicode_len;
	uint8_t		unicode[255 * 2];
} __attribute__((packed));

struct hfsplus_extent {
	uint32_t		start_block;
	uint32_t		block_count;
} __attribute__((packed));

#define HFSPLUS_EXTENT_COUNT		8
struct hfsplus_fork {
	uint64_t		total_size;
	uint32_t		clump_size;
	uint32_t		total_blocks;
	struct hfsplus_extent extents[HFSPLUS_EXTENT_COUNT];
} __attribute__((packed));

struct hfsplus_vol_header {
	uint8_t		signature[2];
	uint16_t		version;
	uint32_t		attributes;
	uint32_t		last_mount_vers;
	uint32_t		reserved;
	uint32_t		create_date;
	uint32_t		modify_date;
	uint32_t		backup_date;
	uint32_t		checked_date;
	uint32_t		file_count;
	uint32_t		folder_count;
	uint32_t		blocksize;
	uint32_t		total_blocks;
	uint32_t		free_blocks;
	uint32_t		next_alloc;
	uint32_t		rsrc_clump_sz;
	uint32_t		data_clump_sz;
	uint32_t		next_cnid;
	uint32_t		write_count;
	uint64_t		encodings_bmp;
	struct hfs_finder_info finder_info;
	struct hfsplus_fork alloc_file;
	struct hfsplus_fork ext_file;
	struct hfsplus_fork cat_file;
	struct hfsplus_fork attr_file;
	struct hfsplus_fork start_file;
}  __attribute__((packed));

#define HFSPLUS_SECTOR_SIZE        512

static int hfs_set_uuid(blkid_probe pr, unsigned char const *hfs_info, size_t len)
{
	static unsigned char const hash_init[MD5LENGTH] = {
		0xb3, 0xe2, 0x0f, 0x39, 0xf2, 0x92, 0x11, 0xd6,
		0x97, 0xa4, 0x00, 0x30, 0x65, 0x43, 0xec, 0xac
	};
	unsigned char uuid[MD5LENGTH];
	struct MD5Context md5c;

	if (memcmp(hfs_info, "\0\0\0\0\0\0\0\0", len) == 0)
		return -1;
	MD5Init(&md5c);
	MD5Update(&md5c, hash_init, MD5LENGTH);
	MD5Update(&md5c, hfs_info, len);
	MD5Final(uuid, &md5c);
	uuid[6] = 0x30 | (uuid[6] & 0x0f);
	uuid[8] = 0x80 | (uuid[8] & 0x3f);
	return blkid_probe_set_uuid(pr, uuid);
}

static int probe_hfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct hfs_mdb	*hfs;

	hfs = blkid_probe_get_sb(pr, mag, struct hfs_mdb);
	if (!hfs)
		return -1;

	if ((memcmp(hfs->embed_sig, "H+", 2) == 0) ||
	    (memcmp(hfs->embed_sig, "HX", 2) == 0))
		return 1;	/* Not hfs, but an embedded HFS+ */

	hfs_set_uuid(pr, hfs->finder_info.id, sizeof(hfs->finder_info.id));

	blkid_probe_set_label(pr, hfs->label, hfs->label_len);
	return 0;
}

static int probe_hfsplus(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct hfsplus_extent extents[HFSPLUS_EXTENT_COUNT];
	struct hfsplus_bnode_descriptor *descr;
	struct hfsplus_bheader_record *bnode;
	struct hfsplus_catalog_key *key;
	struct hfsplus_vol_header *hfsplus;
	struct hfs_mdb *sbd;
	unsigned int alloc_block_size;
	unsigned int alloc_first_block;
	unsigned int embed_first_block;
	unsigned int off = 0;
	unsigned int blocksize;
	unsigned int cat_block;
	unsigned int ext_block_start;
	unsigned int ext_block_count;
	unsigned int record_count;
	unsigned int leaf_node_head;
	unsigned int leaf_node_count;
	unsigned int leaf_node_size;
	unsigned int leaf_block;
	int ext;
	uint64_t leaf_off;
	unsigned char *buf;

	sbd = blkid_probe_get_sb(pr, mag, struct hfs_mdb);
	if (!sbd)
		return -1;

	/* Check for a HFS+ volume embedded in a HFS volume */
	if (memcmp(sbd->signature, "BD", 2) == 0) {
		if ((memcmp(sbd->embed_sig, "H+", 2) != 0) &&
		    (memcmp(sbd->embed_sig, "HX", 2) != 0))
			/* This must be an HFS volume, so fail */
			return 1;

		alloc_block_size = be32_to_cpu(sbd->al_blk_size);
		alloc_first_block = be16_to_cpu(sbd->al_bl_st);
		embed_first_block = be16_to_cpu(sbd->embed_startblock);
		off = (alloc_first_block * 512) +
			(embed_first_block * alloc_block_size);

		buf = blkid_probe_get_buffer(pr,
				off + (mag->kboff * 1024),
				sizeof(struct hfsplus_vol_header));
		hfsplus = (struct hfsplus_vol_header *) buf;

	} else
		hfsplus = blkid_probe_get_sb(pr, mag,
				struct hfsplus_vol_header);

	if (!hfsplus)
		return -1;

	if ((memcmp(hfsplus->signature, "H+", 2) != 0) &&
	    (memcmp(hfsplus->signature, "HX", 2) != 0))
		return 1;

	hfs_set_uuid(pr, hfsplus->finder_info.id, sizeof(hfsplus->finder_info.id));

	blocksize = be32_to_cpu(hfsplus->blocksize);
	if (blocksize < HFSPLUS_SECTOR_SIZE)
		return -1;

	memcpy(extents, hfsplus->cat_file.extents, sizeof(extents));
	cat_block = be32_to_cpu(extents[0].start_block);

	buf = blkid_probe_get_buffer(pr,
			off + ((blkid_loff_t) cat_block * blocksize), 0x2000);
	if (!buf)
		return 0;

	bnode = (struct hfsplus_bheader_record *)
		&buf[sizeof(struct hfsplus_bnode_descriptor)];

	leaf_node_head = be32_to_cpu(bnode->leaf_head);
	leaf_node_size = be16_to_cpu(bnode->node_size);
	leaf_node_count = be32_to_cpu(bnode->leaf_count);
	if (leaf_node_count == 0)
		return 0;

	leaf_block = (leaf_node_head * leaf_node_size) / blocksize;

	/* get physical location */
	for (ext = 0; ext < HFSPLUS_EXTENT_COUNT; ext++) {
		ext_block_start = be32_to_cpu(extents[ext].start_block);
		ext_block_count = be32_to_cpu(extents[ext].block_count);
		if (ext_block_count == 0)
			return 0;

		/* this is our extent */
		if (leaf_block < ext_block_count)
			break;

		leaf_block -= ext_block_count;
	}
	if (ext == HFSPLUS_EXTENT_COUNT)
		return 0;

	leaf_off = (ext_block_start + leaf_block) * blocksize;

	buf = blkid_probe_get_buffer(pr,
				(blkid_loff_t) off + leaf_off,
				leaf_node_size);
	if (!buf)
		return 0;

	descr = (struct hfsplus_bnode_descriptor *) buf;
	record_count = be16_to_cpu(descr->num_recs);
	if (record_count == 0)
		return 0;

	if (descr->type != HFS_NODE_LEAF)
		return 0;

	key = (struct hfsplus_catalog_key *)
		&buf[sizeof(struct hfsplus_bnode_descriptor)];

	if (be32_to_cpu(key->parent_id) != HFSPLUS_POR_CNID)
		return 0;

	blkid_probe_set_utf8label(pr, key->unicode,
			be16_to_cpu(key->unicode_len) * 2,
			BLKID_ENC_UTF16BE);
	return 0;
}

const struct blkid_idinfo hfs_idinfo =
{
	.name		= "hfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_hfs,
	.flags		= BLKID_IDINFO_TOLERANT,
	.magics		=
	{
		{ .magic = "BD", .len = 2, .kboff = 1 },
		{ NULL }
	}
};

const struct blkid_idinfo hfsplus_idinfo =
{
	.name		= "hfsplus",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_hfsplus,
	.magics		=
	{
		{ .magic = "BD", .len = 2, .kboff = 1 },
		{ .magic = "H+", .len = 2, .kboff = 1 },
		{ .magic = "HX", .len = 2, .kboff = 1 },
		{ NULL }
	}
};

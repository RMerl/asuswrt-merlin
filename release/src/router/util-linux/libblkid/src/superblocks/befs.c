/*
 * Copyright (C) 2010 Jeroen Oortwijn <oortwijn@gmail.com>
 *
 * Partly based on the Haiku BFS driver by
 *     Axel Dörfler <axeld@pinc-software.de>
 *
 * Also inspired by the Linux BeFS driver by
 *     Will Dyson <will_dyson@pobox.com>, et al.
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

#define B_OS_NAME_LENGTH	0x20
#define SUPER_BLOCK_MAGIC1	0x42465331	/* BFS1 */
#define SUPER_BLOCK_MAGIC2	0xdd121031
#define SUPER_BLOCK_MAGIC3	0x15b6830e
#define SUPER_BLOCK_FS_ENDIAN	0x42494745	/* BIGE */
#define INODE_MAGIC1		0x3bbe0ad9
#define BPLUSTREE_MAGIC		0x69f6c2e8
#define BPLUSTREE_NULL		-1LL
#define NUM_DIRECT_BLOCKS	12
#define B_UINT64_TYPE		0x554c4c47	/* ULLG */
#define KEY_NAME		"be:volume_id"
#define KEY_SIZE		8

#define FS16_TO_CPU(value, fs_is_le) (fs_is_le ? le16_to_cpu(value) \
							: be16_to_cpu(value))
#define FS32_TO_CPU(value, fs_is_le) (fs_is_le ? le32_to_cpu(value) \
							: be32_to_cpu(value))
#define FS64_TO_CPU(value, fs_is_le) (fs_is_le ? le64_to_cpu(value) \
							: be64_to_cpu(value))

typedef struct block_run {
	int32_t		allocation_group;
	uint16_t	start;
	uint16_t	len;
} __attribute__((packed)) block_run, inode_addr;

struct befs_super_block {
	char		name[B_OS_NAME_LENGTH];
	int32_t		magic1;
	int32_t		fs_byte_order;
	uint32_t	block_size;
	uint32_t	block_shift;
	int64_t		num_blocks;
	int64_t		used_blocks;
	int32_t		inode_size;
	int32_t		magic2;
	int32_t		blocks_per_ag;
	int32_t		ag_shift;
	int32_t		num_ags;
	int32_t		flags;
	block_run	log_blocks;
	int64_t		log_start;
	int64_t		log_end;
	int32_t		magic3;
	inode_addr	root_dir;
	inode_addr	indices;
	int32_t		pad[8];
} __attribute__((packed));

typedef struct data_stream {
	block_run	direct[NUM_DIRECT_BLOCKS];
	int64_t		max_direct_range;
	block_run	indirect;
	int64_t		max_indirect_range;
	block_run	double_indirect;
	int64_t		max_double_indirect_range;
	int64_t		size;
} __attribute__((packed)) data_stream;

struct befs_inode {
	int32_t		magic1;
	inode_addr	inode_num;
	int32_t		uid;
	int32_t		gid;
	int32_t		mode;
	int32_t		flags;
	int64_t		create_time;
	int64_t		last_modified_time;
	inode_addr	parent;
	inode_addr	attributes;
	uint32_t	type;
	int32_t		inode_size;
	uint32_t	etc;
	data_stream	data;
	int32_t		pad[4];
	int32_t		small_data[0];
} __attribute__((packed));

struct small_data {
	uint32_t	type;
	uint16_t	name_size;
	uint16_t	data_size;
	char		name[0];
} __attribute__((packed));

struct bplustree_header {
	uint32_t	magic;
	uint32_t	node_size;
	uint32_t	max_number_of_levels;
	uint32_t	data_type;
	int64_t		root_node_pointer;
	int64_t		free_node_pointer;
	int64_t		maximum_size;
} __attribute__((packed));

struct bplustree_node {
	int64_t		left_link;
	int64_t		right_link;
	int64_t		overflow_link;
	uint16_t	all_key_count;
	uint16_t	all_key_length;
	char		name[0];
} __attribute__((packed));

unsigned char *get_block_run(blkid_probe pr, const struct befs_super_block *bs,
					const struct block_run *br, int fs_le)
{
	return blkid_probe_get_buffer(pr,
			((blkid_loff_t) FS32_TO_CPU(br->allocation_group, fs_le)
					<< FS32_TO_CPU(bs->ag_shift, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le))
				+ ((blkid_loff_t) FS16_TO_CPU(br->start, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le)),
			(blkid_loff_t) FS16_TO_CPU(br->len, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le));
}

unsigned char *get_custom_block_run(blkid_probe pr,
				const struct befs_super_block *bs,
				const struct block_run *br,
				int64_t offset, uint32_t length, int fs_le)
{
	if (offset + length > (int64_t) FS16_TO_CPU(br->len, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le))
		return NULL;

	return blkid_probe_get_buffer(pr,
			((blkid_loff_t) FS32_TO_CPU(br->allocation_group, fs_le)
					<< FS32_TO_CPU(bs->ag_shift, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le))
				+ ((blkid_loff_t) FS16_TO_CPU(br->start, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le))
				+ offset,
			length);
}

unsigned char *get_tree_node(blkid_probe pr, const struct befs_super_block *bs,
				const struct data_stream *ds,
				int64_t start, uint32_t length, int fs_le)
{
	if (start < (int64_t) FS64_TO_CPU(ds->max_direct_range, fs_le)) {
		int64_t br_len;
		size_t i;

		for (i = 0; i < NUM_DIRECT_BLOCKS; i++) {
			br_len = (int64_t) FS16_TO_CPU(ds->direct[i].len, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le);
			if (start < br_len)
				return get_custom_block_run(pr, bs,
							&ds->direct[i],
							start, length, fs_le);
			else
				start -= br_len;
		}
	} else if (start < (int64_t) FS64_TO_CPU(ds->max_indirect_range, fs_le)) {
		struct block_run *br;
		int64_t max_br, br_len, i;

		start -= FS64_TO_CPU(ds->max_direct_range, fs_le);
		max_br = ((int64_t) FS16_TO_CPU(ds->indirect.len, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le))
				/ sizeof(struct block_run);

		br = (struct block_run *) get_block_run(pr, bs, &ds->indirect,
									fs_le);
		if (!br)
			return NULL;

		for (i = 0; i < max_br; i++) {
			br_len = (int64_t) FS16_TO_CPU(br[i].len, fs_le)
					<< FS32_TO_CPU(bs->block_shift, fs_le);
			if (start < br_len)
				return get_custom_block_run(pr, bs, &br[i],
							start, length, fs_le);
			else
				start -= br_len;
		}
	} else if (start < (int64_t) FS64_TO_CPU(ds->max_double_indirect_range, fs_le)) {
		struct block_run *br;
		int64_t di_br_size, br_per_di_br, di_index, i_index;

		start -= (int64_t) FS64_TO_CPU(ds->max_indirect_range, fs_le);

		di_br_size = (int64_t) FS16_TO_CPU(ds->double_indirect.len,
				fs_le) << FS32_TO_CPU(bs->block_shift, fs_le);
		if (di_br_size == 0)
			return NULL;

		br_per_di_br = di_br_size / sizeof(struct block_run);
		if (br_per_di_br == 0)
			return NULL;

		di_index = start / (br_per_di_br * di_br_size);
		i_index = (start % (br_per_di_br * di_br_size)) / di_br_size;
		start = (start % (br_per_di_br * di_br_size)) % di_br_size;

		br = (struct block_run *) get_block_run(pr, bs,
						&ds->double_indirect, fs_le);
		if (!br)
			return NULL;

		br = (struct block_run *) get_block_run(pr, bs, &br[di_index],
									fs_le);
		if (!br)
			return NULL;

		return get_custom_block_run(pr, bs, &br[i_index], start, length,
									fs_le);
	}
	return NULL;
}

int32_t compare_keys(const char keys1[], uint16_t keylengths1[], int32_t index,
			const char *key2, uint16_t keylength2, int fs_le)
{
	const char *key1;
	uint16_t keylength1;
	int32_t result;

	key1 = &keys1[index == 0 ? 0 : FS16_TO_CPU(keylengths1[index - 1],
									fs_le)];
	keylength1 = FS16_TO_CPU(keylengths1[index], fs_le)
			- (index == 0 ? 0 : FS16_TO_CPU(keylengths1[index - 1],
									fs_le));

	result = strncmp(key1, key2, min(keylength1, keylength2));

	if (result == 0)
		return keylength1 - keylength2;

	return result;
}

int64_t get_key_value(blkid_probe pr, const struct befs_super_block *bs,
			const struct befs_inode *bi, const char *key, int fs_le)
{
	struct bplustree_header *bh;
	struct bplustree_node *bn;
	uint16_t *keylengths;
	int64_t *values;
	int64_t node_pointer;
	int32_t first, last, mid, cmp;

	bh = (struct bplustree_header *) get_tree_node(pr, bs, &bi->data, 0,
					sizeof(struct bplustree_header), fs_le);
	if (!bh)
		return -1;

	if ((int32_t) FS32_TO_CPU(bh->magic, fs_le) != BPLUSTREE_MAGIC)
		return -1;

	node_pointer = FS64_TO_CPU(bh->root_node_pointer, fs_le);

	do {
		bn = (struct bplustree_node *) get_tree_node(pr, bs, &bi->data,
			node_pointer, FS32_TO_CPU(bh->node_size, fs_le), fs_le);
		if (!bn)
			return -1;

		keylengths = (uint16_t *) ((uint8_t *) bn
				+ ((sizeof(struct bplustree_node)
					+ FS16_TO_CPU(bn->all_key_length, fs_le)
					+ sizeof(int64_t) - 1)
						& ~(sizeof(int64_t) - 1)));
		values = (int64_t *) ((uint8_t *) keylengths
					+ FS16_TO_CPU(bn->all_key_count, fs_le)
						* sizeof(uint16_t));
		first = 0;
		mid = 0;
		last = FS16_TO_CPU(bn->all_key_count, fs_le) - 1;

		cmp = compare_keys(bn->name, keylengths, last, key, strlen(key),
									fs_le);
		if (cmp == 0) {
			if ((int64_t) FS64_TO_CPU(bn->overflow_link, fs_le)
							== BPLUSTREE_NULL)
				return FS64_TO_CPU(values[last], fs_le);
			else
				node_pointer = FS64_TO_CPU(values[last], fs_le);
		} else if (cmp < 0)
			node_pointer = FS64_TO_CPU(bn->overflow_link, fs_le);
		else {
			while (first <= last) {
				mid = (first + last) / 2;

				cmp = compare_keys(bn->name, keylengths, mid,
						key, strlen(key), fs_le);
				if (cmp == 0) {
					if ((int64_t) FS64_TO_CPU(bn->overflow_link,
						fs_le) == BPLUSTREE_NULL)
						return FS64_TO_CPU(values[mid],
									fs_le);
					else
						break;
				} else if (cmp < 0)
					first = mid + 1;
				else
					last = mid - 1;
			}
			if (cmp < 0)
				node_pointer = FS64_TO_CPU(values[mid + 1],
									fs_le);
			else
				node_pointer = FS64_TO_CPU(values[mid], fs_le);
		}
	} while ((int64_t) FS64_TO_CPU(bn->overflow_link, fs_le)
						!= BPLUSTREE_NULL);
	return 0;
}

int get_uuid(blkid_probe pr, const struct befs_super_block *bs,
					uint64_t * const uuid, int fs_le)
{
	struct befs_inode *bi;
	struct small_data *sd;

	bi = (struct befs_inode *) get_block_run(pr, bs, &bs->root_dir, fs_le);
	if (!bi)
		return -1;

	if (FS32_TO_CPU(bi->magic1, fs_le) != INODE_MAGIC1)
		return -1;

	sd = (struct small_data *) bi->small_data;

	do {
		if (FS32_TO_CPU(sd->type, fs_le) == B_UINT64_TYPE
			&& FS16_TO_CPU(sd->name_size, fs_le) == strlen(KEY_NAME)
			&& FS16_TO_CPU(sd->data_size, fs_le) == KEY_SIZE
			&& strcmp(sd->name, KEY_NAME) == 0) {
			*uuid = *(uint64_t *) ((uint8_t *) sd->name
					+ FS16_TO_CPU(sd->name_size, fs_le)
					+ 3);
			break;
		} else if (FS32_TO_CPU(sd->type, fs_le) == 0
				&& FS16_TO_CPU(sd->name_size, fs_le) == 0
				&& FS16_TO_CPU(sd->data_size, fs_le) == 0)
			break;

		sd = (struct small_data *) ((uint8_t *) sd
				+ sizeof(struct small_data)
				+ FS16_TO_CPU(sd->name_size, fs_le) + 3
				+ FS16_TO_CPU(sd->data_size, fs_le) + 1);

	} while ((intptr_t) sd < (intptr_t) bi
				+ (int32_t) FS32_TO_CPU(bi->inode_size, fs_le)
				- (int32_t) sizeof(struct small_data));
	if (*uuid == 0
		&& (FS32_TO_CPU(bi->attributes.allocation_group, fs_le) != 0
			|| FS16_TO_CPU(bi->attributes.start, fs_le) != 0
			|| FS16_TO_CPU(bi->attributes.len, fs_le) != 0)) {
		int64_t value;

		bi = (struct befs_inode *) get_block_run(pr, bs,
							&bi->attributes, fs_le);
		if (!bi)
			return -1;

		if (FS32_TO_CPU(bi->magic1, fs_le) != INODE_MAGIC1)
			return -1;

		value = get_key_value(pr, bs, bi, KEY_NAME, fs_le);

		if (value < 0)
			return value;
		else if (value > 0) {
			bi = (struct befs_inode *) blkid_probe_get_buffer(pr,
				value << FS32_TO_CPU(bs->block_shift, fs_le),
				FS32_TO_CPU(bs->block_size, fs_le));
			if (!bi)
				return -1;

			if (FS32_TO_CPU(bi->magic1, fs_le) != INODE_MAGIC1)
				return -1;

			if (FS32_TO_CPU(bi->type, fs_le) == B_UINT64_TYPE
				&& FS64_TO_CPU(bi->data.size, fs_le) == KEY_SIZE
				&& FS16_TO_CPU(bi->data.direct[0].len, fs_le)
									== 1) {
				uint64_t *attr_data;

				attr_data = (uint64_t *) get_block_run(pr, bs,
						&bi->data.direct[0], fs_le);
				if (!attr_data)
					return -1;

				*uuid = *attr_data;
			}
		}
	}
	return 0;
}

static int probe_befs(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct befs_super_block *bs;
	const char *version = NULL;
	uint64_t volume_id = 0;
	int fs_le, ret;

	bs = (struct befs_super_block *) blkid_probe_get_buffer(pr,
					mag->sboff - B_OS_NAME_LENGTH,
					sizeof(struct befs_super_block));
	if (!bs)
		return -1;

	if (le32_to_cpu(bs->magic1) == SUPER_BLOCK_MAGIC1
		&& le32_to_cpu(bs->magic2) == SUPER_BLOCK_MAGIC2
		&& le32_to_cpu(bs->magic3) == SUPER_BLOCK_MAGIC3
		&& le32_to_cpu(bs->fs_byte_order) == SUPER_BLOCK_FS_ENDIAN) {
		fs_le = 1;
		version = "little-endian";
	} else if (be32_to_cpu(bs->magic1) == SUPER_BLOCK_MAGIC1
		&& be32_to_cpu(bs->magic2) == SUPER_BLOCK_MAGIC2
		&& be32_to_cpu(bs->magic3) == SUPER_BLOCK_MAGIC3
		&& be32_to_cpu(bs->fs_byte_order) == SUPER_BLOCK_FS_ENDIAN) {
		fs_le = 0;
		version = "big-endian";
	} else
		return -1;

	ret = get_uuid(pr, bs, &volume_id, fs_le);

	if (ret < 0)
		return ret;

	/*
	 * all checks pass, set LABEL, VERSION and UUID
	 */
	if (strlen(bs->name))
		blkid_probe_set_label(pr, (unsigned char *) bs->name,
							sizeof(bs->name));
	if (version)
		blkid_probe_set_version(pr, version);

	if (volume_id)
		blkid_probe_sprintf_uuid(pr, (unsigned char *) &volume_id,
					sizeof(volume_id), "%016" PRIx64,
					FS64_TO_CPU(volume_id, fs_le));
	return 0;
}

const struct blkid_idinfo befs_idinfo =
{
	.name		= "befs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_befs,
	.minsz		= 1024 * 1440,
	.magics		= {
		{ .magic = "BFS1", .len = 4, .sboff = B_OS_NAME_LENGTH },
		{ .magic = "1SFB", .len = 4, .sboff = B_OS_NAME_LENGTH },
		{ .magic = "BFS1", .len = 4, .sboff = 0x200 +
							B_OS_NAME_LENGTH },
		{ .magic = "1SFB", .len = 4, .sboff = 0x200 +
							B_OS_NAME_LENGTH },
		{ NULL }
	}
};

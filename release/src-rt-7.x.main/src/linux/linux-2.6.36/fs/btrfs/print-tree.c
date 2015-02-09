/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "ctree.h"
#include "disk-io.h"
#include "print-tree.h"

static void print_chunk(struct extent_buffer *eb, struct btrfs_chunk *chunk)
{
	int num_stripes = btrfs_chunk_num_stripes(eb, chunk);
	int i;
	printk(KERN_INFO "\t\tchunk length %llu owner %llu type %llu "
	       "num_stripes %d\n",
	       (unsigned long long)btrfs_chunk_length(eb, chunk),
	       (unsigned long long)btrfs_chunk_owner(eb, chunk),
	       (unsigned long long)btrfs_chunk_type(eb, chunk),
	       num_stripes);
	for (i = 0 ; i < num_stripes ; i++) {
		printk(KERN_INFO "\t\t\tstripe %d devid %llu offset %llu\n", i,
		      (unsigned long long)btrfs_stripe_devid_nr(eb, chunk, i),
		      (unsigned long long)btrfs_stripe_offset_nr(eb, chunk, i));
	}
}
static void print_dev_item(struct extent_buffer *eb,
			   struct btrfs_dev_item *dev_item)
{
	printk(KERN_INFO "\t\tdev item devid %llu "
	       "total_bytes %llu bytes used %llu\n",
	       (unsigned long long)btrfs_device_id(eb, dev_item),
	       (unsigned long long)btrfs_device_total_bytes(eb, dev_item),
	       (unsigned long long)btrfs_device_bytes_used(eb, dev_item));
}
static void print_extent_data_ref(struct extent_buffer *eb,
				  struct btrfs_extent_data_ref *ref)
{
	printk(KERN_INFO "\t\textent data backref root %llu "
	       "objectid %llu offset %llu count %u\n",
	       (unsigned long long)btrfs_extent_data_ref_root(eb, ref),
	       (unsigned long long)btrfs_extent_data_ref_objectid(eb, ref),
	       (unsigned long long)btrfs_extent_data_ref_offset(eb, ref),
	       btrfs_extent_data_ref_count(eb, ref));
}

static void print_extent_item(struct extent_buffer *eb, int slot)
{
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_data_ref *dref;
	struct btrfs_shared_data_ref *sref;
	struct btrfs_disk_key key;
	unsigned long end;
	unsigned long ptr;
	int type;
	u32 item_size = btrfs_item_size_nr(eb, slot);
	u64 flags;
	u64 offset;

	if (item_size < sizeof(*ei)) {
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		struct btrfs_extent_item_v0 *ei0;
		BUG_ON(item_size != sizeof(*ei0));
		ei0 = btrfs_item_ptr(eb, slot, struct btrfs_extent_item_v0);
		printk(KERN_INFO "\t\textent refs %u\n",
		       btrfs_extent_refs_v0(eb, ei0));
		return;
#else
		BUG();
#endif
	}

	ei = btrfs_item_ptr(eb, slot, struct btrfs_extent_item);
	flags = btrfs_extent_flags(eb, ei);

	printk(KERN_INFO "\t\textent refs %llu gen %llu flags %llu\n",
	       (unsigned long long)btrfs_extent_refs(eb, ei),
	       (unsigned long long)btrfs_extent_generation(eb, ei),
	       (unsigned long long)flags);

	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		struct btrfs_tree_block_info *info;
		info = (struct btrfs_tree_block_info *)(ei + 1);
		btrfs_tree_block_key(eb, info, &key);
		printk(KERN_INFO "\t\ttree block key (%llu %x %llu) "
		       "level %d\n",
		       (unsigned long long)btrfs_disk_key_objectid(&key),
		       key.type,
		       (unsigned long long)btrfs_disk_key_offset(&key),
		       btrfs_tree_block_level(eb, info));
		iref = (struct btrfs_extent_inline_ref *)(info + 1);
	} else {
		iref = (struct btrfs_extent_inline_ref *)(ei + 1);
	}

	ptr = (unsigned long)iref;
	end = (unsigned long)ei + item_size;
	while (ptr < end) {
		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_extent_inline_ref_type(eb, iref);
		offset = btrfs_extent_inline_ref_offset(eb, iref);
		switch (type) {
		case BTRFS_TREE_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\ttree block backref "
				"root %llu\n", (unsigned long long)offset);
			break;
		case BTRFS_SHARED_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\tshared block backref "
				"parent %llu\n", (unsigned long long)offset);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			print_extent_data_ref(eb, dref);
			break;
		case BTRFS_SHARED_DATA_REF_KEY:
			sref = (struct btrfs_shared_data_ref *)(iref + 1);
			printk(KERN_INFO "\t\tshared data backref "
			       "parent %llu count %u\n",
			       (unsigned long long)offset,
			       btrfs_shared_data_ref_count(eb, sref));
			break;
		default:
			BUG();
		}
		ptr += btrfs_extent_inline_ref_size(type);
	}
	WARN_ON(ptr > end);
}

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static void print_extent_ref_v0(struct extent_buffer *eb, int slot)
{
	struct btrfs_extent_ref_v0 *ref0;

	ref0 = btrfs_item_ptr(eb, slot, struct btrfs_extent_ref_v0);
	printk("\t\textent back ref root %llu gen %llu "
		"owner %llu num_refs %lu\n",
		(unsigned long long)btrfs_ref_root_v0(eb, ref0),
		(unsigned long long)btrfs_ref_generation_v0(eb, ref0),
		(unsigned long long)btrfs_ref_objectid_v0(eb, ref0),
		(unsigned long)btrfs_ref_count_v0(eb, ref0));
}
#endif

void btrfs_print_leaf(struct btrfs_root *root, struct extent_buffer *l)
{
	int i;
	u32 type;
	u32 nr = btrfs_header_nritems(l);
	struct btrfs_item *item;
	struct btrfs_root_item *ri;
	struct btrfs_dir_item *di;
	struct btrfs_inode_item *ii;
	struct btrfs_block_group_item *bi;
	struct btrfs_file_extent_item *fi;
	struct btrfs_extent_data_ref *dref;
	struct btrfs_shared_data_ref *sref;
	struct btrfs_dev_extent *dev_extent;
	struct btrfs_key key;
	struct btrfs_key found_key;

	printk(KERN_INFO "leaf %llu total ptrs %d free space %d\n",
		(unsigned long long)btrfs_header_bytenr(l), nr,
		btrfs_leaf_free_space(root, l));
	for (i = 0 ; i < nr ; i++) {
		item = btrfs_item_nr(l, i);
		btrfs_item_key_to_cpu(l, &key, i);
		type = btrfs_key_type(&key);
		printk(KERN_INFO "\titem %d key (%llu %x %llu) itemoff %d "
		       "itemsize %d\n",
			i,
			(unsigned long long)key.objectid, type,
			(unsigned long long)key.offset,
			btrfs_item_offset(l, item), btrfs_item_size(l, item));
		switch (type) {
		case BTRFS_INODE_ITEM_KEY:
			ii = btrfs_item_ptr(l, i, struct btrfs_inode_item);
			printk(KERN_INFO "\t\tinode generation %llu size %llu "
			       "mode %o\n",
			       (unsigned long long)
			       btrfs_inode_generation(l, ii),
			      (unsigned long long)btrfs_inode_size(l, ii),
			       btrfs_inode_mode(l, ii));
			break;
		case BTRFS_DIR_ITEM_KEY:
			di = btrfs_item_ptr(l, i, struct btrfs_dir_item);
			btrfs_dir_item_key_to_cpu(l, di, &found_key);
			printk(KERN_INFO "\t\tdir oid %llu type %u\n",
				(unsigned long long)found_key.objectid,
				btrfs_dir_type(l, di));
			break;
		case BTRFS_ROOT_ITEM_KEY:
			ri = btrfs_item_ptr(l, i, struct btrfs_root_item);
			printk(KERN_INFO "\t\troot data bytenr %llu refs %u\n",
				(unsigned long long)
				btrfs_disk_root_bytenr(l, ri),
				btrfs_disk_root_refs(l, ri));
			break;
		case BTRFS_EXTENT_ITEM_KEY:
			print_extent_item(l, i);
			break;
		case BTRFS_TREE_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\ttree block backref\n");
			break;
		case BTRFS_SHARED_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\tshared block backref\n");
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
			dref = btrfs_item_ptr(l, i,
					      struct btrfs_extent_data_ref);
			print_extent_data_ref(l, dref);
			break;
		case BTRFS_SHARED_DATA_REF_KEY:
			sref = btrfs_item_ptr(l, i,
					      struct btrfs_shared_data_ref);
			printk(KERN_INFO "\t\tshared data backref count %u\n",
			       btrfs_shared_data_ref_count(l, sref));
			break;
		case BTRFS_EXTENT_DATA_KEY:
			fi = btrfs_item_ptr(l, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(l, fi) ==
			    BTRFS_FILE_EXTENT_INLINE) {
				printk(KERN_INFO "\t\tinline extent data "
				       "size %u\n",
				       btrfs_file_extent_inline_len(l, fi));
				break;
			}
			printk(KERN_INFO "\t\textent data disk bytenr %llu "
			       "nr %llu\n",
			       (unsigned long long)
			       btrfs_file_extent_disk_bytenr(l, fi),
			       (unsigned long long)
			       btrfs_file_extent_disk_num_bytes(l, fi));
			printk(KERN_INFO "\t\textent data offset %llu "
			       "nr %llu ram %llu\n",
			       (unsigned long long)
			       btrfs_file_extent_offset(l, fi),
			       (unsigned long long)
			       btrfs_file_extent_num_bytes(l, fi),
			       (unsigned long long)
			       btrfs_file_extent_ram_bytes(l, fi));
			break;
		case BTRFS_EXTENT_REF_V0_KEY:
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
			print_extent_ref_v0(l, i);
#else
			BUG();
#endif
		case BTRFS_BLOCK_GROUP_ITEM_KEY:
			bi = btrfs_item_ptr(l, i,
					    struct btrfs_block_group_item);
			printk(KERN_INFO "\t\tblock group used %llu\n",
			       (unsigned long long)
			       btrfs_disk_block_group_used(l, bi));
			break;
		case BTRFS_CHUNK_ITEM_KEY:
			print_chunk(l, btrfs_item_ptr(l, i,
						      struct btrfs_chunk));
			break;
		case BTRFS_DEV_ITEM_KEY:
			print_dev_item(l, btrfs_item_ptr(l, i,
					struct btrfs_dev_item));
			break;
		case BTRFS_DEV_EXTENT_KEY:
			dev_extent = btrfs_item_ptr(l, i,
						    struct btrfs_dev_extent);
			printk(KERN_INFO "\t\tdev extent chunk_tree %llu\n"
			       "\t\tchunk objectid %llu chunk offset %llu "
			       "length %llu\n",
			       (unsigned long long)
			       btrfs_dev_extent_chunk_tree(l, dev_extent),
			       (unsigned long long)
			       btrfs_dev_extent_chunk_objectid(l, dev_extent),
			       (unsigned long long)
			       btrfs_dev_extent_chunk_offset(l, dev_extent),
			       (unsigned long long)
			       btrfs_dev_extent_length(l, dev_extent));
		};
	}
}

void btrfs_print_tree(struct btrfs_root *root, struct extent_buffer *c)
{
	int i; u32 nr;
	struct btrfs_key key;
	int level;

	if (!c)
		return;
	nr = btrfs_header_nritems(c);
	level = btrfs_header_level(c);
	if (level == 0) {
		btrfs_print_leaf(root, c);
		return;
	}
	printk(KERN_INFO "node %llu level %d total ptrs %d free spc %u\n",
	       (unsigned long long)btrfs_header_bytenr(c),
	      level, nr,
	       (u32)BTRFS_NODEPTRS_PER_BLOCK(root) - nr);
	for (i = 0; i < nr; i++) {
		btrfs_node_key_to_cpu(c, &key, i);
		printk(KERN_INFO "\tkey %d (%llu %u %llu) block %llu\n",
		       i,
		       (unsigned long long)key.objectid,
		       key.type,
		       (unsigned long long)key.offset,
		       (unsigned long long)btrfs_node_blockptr(c, i));
	}
	for (i = 0; i < nr; i++) {
		struct extent_buffer *next = read_tree_block(root,
					btrfs_node_blockptr(c, i),
					btrfs_level_size(root, level - 1),
					btrfs_node_ptr_generation(c, i));
		if (btrfs_is_leaf(next) &&
		   level != 1)
			BUG();
		if (btrfs_header_level(next) !=
		       level - 1)
			BUG();
		btrfs_print_tree(root, next);
		free_extent_buffer(next);
	}
}

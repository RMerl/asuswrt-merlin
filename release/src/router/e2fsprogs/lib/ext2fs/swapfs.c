/*
 * swapfs.c --- swap ext2 filesystem data structures
 *
 * Copyright (C) 1995, 1996, 2002 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"
#include <ext2fs/ext2_ext_attr.h>

#ifdef WORDS_BIGENDIAN
void ext2fs_swap_super(struct ext2_super_block * sb)
{
  	int i;
	sb->s_inodes_count = ext2fs_swab32(sb->s_inodes_count);
	sb->s_blocks_count = ext2fs_swab32(sb->s_blocks_count);
	sb->s_r_blocks_count = ext2fs_swab32(sb->s_r_blocks_count);
	sb->s_free_blocks_count = ext2fs_swab32(sb->s_free_blocks_count);
	sb->s_free_inodes_count = ext2fs_swab32(sb->s_free_inodes_count);
	sb->s_first_data_block = ext2fs_swab32(sb->s_first_data_block);
	sb->s_log_block_size = ext2fs_swab32(sb->s_log_block_size);
	sb->s_log_frag_size = ext2fs_swab32(sb->s_log_frag_size);
	sb->s_blocks_per_group = ext2fs_swab32(sb->s_blocks_per_group);
	sb->s_frags_per_group = ext2fs_swab32(sb->s_frags_per_group);
	sb->s_inodes_per_group = ext2fs_swab32(sb->s_inodes_per_group);
	sb->s_mtime = ext2fs_swab32(sb->s_mtime);
	sb->s_wtime = ext2fs_swab32(sb->s_wtime);
	sb->s_mnt_count = ext2fs_swab16(sb->s_mnt_count);
	sb->s_max_mnt_count = ext2fs_swab16(sb->s_max_mnt_count);
	sb->s_magic = ext2fs_swab16(sb->s_magic);
	sb->s_state = ext2fs_swab16(sb->s_state);
	sb->s_errors = ext2fs_swab16(sb->s_errors);
	sb->s_minor_rev_level = ext2fs_swab16(sb->s_minor_rev_level);
	sb->s_lastcheck = ext2fs_swab32(sb->s_lastcheck);
	sb->s_checkinterval = ext2fs_swab32(sb->s_checkinterval);
	sb->s_creator_os = ext2fs_swab32(sb->s_creator_os);
	sb->s_rev_level = ext2fs_swab32(sb->s_rev_level);
	sb->s_def_resuid = ext2fs_swab16(sb->s_def_resuid);
	sb->s_def_resgid = ext2fs_swab16(sb->s_def_resgid);
	sb->s_first_ino = ext2fs_swab32(sb->s_first_ino);
	sb->s_inode_size = ext2fs_swab16(sb->s_inode_size);
	sb->s_block_group_nr = ext2fs_swab16(sb->s_block_group_nr);
	sb->s_feature_compat = ext2fs_swab32(sb->s_feature_compat);
	sb->s_feature_incompat = ext2fs_swab32(sb->s_feature_incompat);
	sb->s_feature_ro_compat = ext2fs_swab32(sb->s_feature_ro_compat);
	sb->s_algorithm_usage_bitmap = ext2fs_swab32(sb->s_algorithm_usage_bitmap);
	sb->s_reserved_gdt_blocks = ext2fs_swab16(sb->s_reserved_gdt_blocks);
	sb->s_journal_inum = ext2fs_swab32(sb->s_journal_inum);
	sb->s_journal_dev = ext2fs_swab32(sb->s_journal_dev);
	sb->s_last_orphan = ext2fs_swab32(sb->s_last_orphan);
	sb->s_desc_size = ext2fs_swab16(sb->s_desc_size);
	sb->s_default_mount_opts = ext2fs_swab32(sb->s_default_mount_opts);
	sb->s_first_meta_bg = ext2fs_swab32(sb->s_first_meta_bg);
	sb->s_mkfs_time = ext2fs_swab32(sb->s_mkfs_time);
	sb->s_blocks_count_hi = ext2fs_swab32(sb->s_blocks_count_hi);
	sb->s_r_blocks_count_hi = ext2fs_swab32(sb->s_r_blocks_count_hi);
	sb->s_free_blocks_hi = ext2fs_swab32(sb->s_free_blocks_hi);
	sb->s_min_extra_isize = ext2fs_swab16(sb->s_min_extra_isize);
	sb->s_want_extra_isize = ext2fs_swab16(sb->s_want_extra_isize);
	sb->s_flags = ext2fs_swab32(sb->s_flags);
	sb->s_kbytes_written = ext2fs_swab64(sb->s_kbytes_written);
	sb->s_snapshot_inum = ext2fs_swab32(sb->s_snapshot_inum);
	sb->s_snapshot_id = ext2fs_swab32(sb->s_snapshot_id);
	sb->s_snapshot_r_blocks_count =
		ext2fs_swab64(sb->s_snapshot_r_blocks_count);
	sb->s_snapshot_list = ext2fs_swab32(sb->s_snapshot_list);

	for (i=0; i < 4; i++)
		sb->s_hash_seed[i] = ext2fs_swab32(sb->s_hash_seed[i]);

	/* if journal backup is for a valid extent-based journal... */
	if (!ext2fs_extent_header_verify(sb->s_jnl_blocks,
					 sizeof(sb->s_jnl_blocks))) {
		/* ... swap only the journal i_size */
		sb->s_jnl_blocks[16] = ext2fs_swab32(sb->s_jnl_blocks[16]);
		/* and the extent data is not swapped on read */
		return;
	}

	/* direct/indirect journal: swap it all */
	for (i=0; i < 17; i++)
		sb->s_jnl_blocks[i] = ext2fs_swab32(sb->s_jnl_blocks[i]);
}

void ext2fs_swap_group_desc(struct ext2_group_desc *gdp)
{
	gdp->bg_block_bitmap = ext2fs_swab32(gdp->bg_block_bitmap);
	gdp->bg_inode_bitmap = ext2fs_swab32(gdp->bg_inode_bitmap);
	gdp->bg_inode_table = ext2fs_swab32(gdp->bg_inode_table);
	gdp->bg_free_blocks_count = ext2fs_swab16(gdp->bg_free_blocks_count);
	gdp->bg_free_inodes_count = ext2fs_swab16(gdp->bg_free_inodes_count);
	gdp->bg_used_dirs_count = ext2fs_swab16(gdp->bg_used_dirs_count);
	gdp->bg_flags = ext2fs_swab16(gdp->bg_flags);
	gdp->bg_itable_unused = ext2fs_swab16(gdp->bg_itable_unused);
	gdp->bg_checksum = ext2fs_swab16(gdp->bg_checksum);
}

void ext2fs_swap_ext_attr_header(struct ext2_ext_attr_header *to_header,
				 struct ext2_ext_attr_header *from_header)
{
	int n;

	to_header->h_magic    = ext2fs_swab32(from_header->h_magic);
	to_header->h_blocks   = ext2fs_swab32(from_header->h_blocks);
	to_header->h_refcount = ext2fs_swab32(from_header->h_refcount);
	to_header->h_hash     = ext2fs_swab32(from_header->h_hash);
	for (n = 0; n < 4; n++)
		to_header->h_reserved[n] =
			ext2fs_swab32(from_header->h_reserved[n]);
}

void ext2fs_swap_ext_attr_entry(struct ext2_ext_attr_entry *to_entry,
				struct ext2_ext_attr_entry *from_entry)
{
	to_entry->e_value_offs  = ext2fs_swab16(from_entry->e_value_offs);
	to_entry->e_value_block = ext2fs_swab32(from_entry->e_value_block);
	to_entry->e_value_size  = ext2fs_swab32(from_entry->e_value_size);
	to_entry->e_hash	= ext2fs_swab32(from_entry->e_hash);
}

void ext2fs_swap_ext_attr(char *to, char *from, int bufsize, int has_header)
{
	struct ext2_ext_attr_header *from_header =
		(struct ext2_ext_attr_header *)from;
	struct ext2_ext_attr_header *to_header =
		(struct ext2_ext_attr_header *)to;
	struct ext2_ext_attr_entry *from_entry, *to_entry;
	char *from_end = (char *)from_header + bufsize;

	if (to_header != from_header)
		memcpy(to_header, from_header, bufsize);

	if (has_header) {
		ext2fs_swap_ext_attr_header(to_header, from_header);

		from_entry = (struct ext2_ext_attr_entry *)(from_header+1);
		to_entry   = (struct ext2_ext_attr_entry *)(to_header+1);
	} else {
		from_entry = (struct ext2_ext_attr_entry *)from_header;
		to_entry   = (struct ext2_ext_attr_entry *)to_header;
	}

	while ((char *)from_entry < from_end && *(__u32 *)from_entry) {
		ext2fs_swap_ext_attr_entry(to_entry, from_entry);
		from_entry = EXT2_EXT_ATTR_NEXT(from_entry);
		to_entry   = EXT2_EXT_ATTR_NEXT(to_entry);
	}
}

void ext2fs_swap_inode_full(ext2_filsys fs, struct ext2_inode_large *t,
			    struct ext2_inode_large *f, int hostorder,
			    int bufsize)
{
	unsigned i, has_data_blocks, extra_isize, attr_magic;
	int has_extents = 0;
	int islnk = 0;
	__u32 *eaf, *eat;

	if (hostorder && LINUX_S_ISLNK(f->i_mode))
		islnk = 1;
	t->i_mode = ext2fs_swab16(f->i_mode);
	if (!hostorder && LINUX_S_ISLNK(t->i_mode))
		islnk = 1;
	t->i_uid = ext2fs_swab16(f->i_uid);
	t->i_size = ext2fs_swab32(f->i_size);
	t->i_atime = ext2fs_swab32(f->i_atime);
	t->i_ctime = ext2fs_swab32(f->i_ctime);
	t->i_mtime = ext2fs_swab32(f->i_mtime);
	t->i_dtime = ext2fs_swab32(f->i_dtime);
	t->i_gid = ext2fs_swab16(f->i_gid);
	t->i_links_count = ext2fs_swab16(f->i_links_count);
	t->i_file_acl = ext2fs_swab32(f->i_file_acl);
	if (hostorder)
		has_data_blocks = ext2fs_inode_data_blocks(fs,
					   (struct ext2_inode *) f);
	t->i_blocks = ext2fs_swab32(f->i_blocks);
	if (!hostorder)
		has_data_blocks = ext2fs_inode_data_blocks(fs,
					   (struct ext2_inode *) t);
	if (hostorder && (f->i_flags & EXT4_EXTENTS_FL))
		has_extents = 1;
	t->i_flags = ext2fs_swab32(f->i_flags);
	if (!hostorder && (t->i_flags & EXT4_EXTENTS_FL))
		has_extents = 1;
	t->i_dir_acl = ext2fs_swab32(f->i_dir_acl);
	/* extent data are swapped on access, not here */
	if (!has_extents && (!islnk || has_data_blocks)) {
		for (i = 0; i < EXT2_N_BLOCKS; i++)
			t->i_block[i] = ext2fs_swab32(f->i_block[i]);
	} else if (t != f) {
		for (i = 0; i < EXT2_N_BLOCKS; i++)
			t->i_block[i] = f->i_block[i];
	}
	t->i_generation = ext2fs_swab32(f->i_generation);
	t->i_faddr = ext2fs_swab32(f->i_faddr);

	switch (fs->super->s_creator_os) {
	case EXT2_OS_LINUX:
		t->osd1.linux1.l_i_version =
			ext2fs_swab32(f->osd1.linux1.l_i_version);
		t->osd2.linux2.l_i_blocks_hi =
			ext2fs_swab16(f->osd2.linux2.l_i_blocks_hi);
		t->osd2.linux2.l_i_file_acl_high =
			ext2fs_swab16(f->osd2.linux2.l_i_file_acl_high);
		t->osd2.linux2.l_i_uid_high =
		  ext2fs_swab16 (f->osd2.linux2.l_i_uid_high);
		t->osd2.linux2.l_i_gid_high =
		  ext2fs_swab16 (f->osd2.linux2.l_i_gid_high);
		t->osd2.linux2.l_i_reserved2 =
			ext2fs_swab32(f->osd2.linux2.l_i_reserved2);
		break;
	case EXT2_OS_HURD:
		t->osd1.hurd1.h_i_translator =
		  ext2fs_swab32 (f->osd1.hurd1.h_i_translator);
		t->osd2.hurd2.h_i_frag = f->osd2.hurd2.h_i_frag;
		t->osd2.hurd2.h_i_fsize = f->osd2.hurd2.h_i_fsize;
		t->osd2.hurd2.h_i_mode_high =
		  ext2fs_swab16 (f->osd2.hurd2.h_i_mode_high);
		t->osd2.hurd2.h_i_uid_high =
		  ext2fs_swab16 (f->osd2.hurd2.h_i_uid_high);
		t->osd2.hurd2.h_i_gid_high =
		  ext2fs_swab16 (f->osd2.hurd2.h_i_gid_high);
		t->osd2.hurd2.h_i_author =
		  ext2fs_swab32 (f->osd2.hurd2.h_i_author);
		break;
	default:
		break;
	}

	if (bufsize < (int) (sizeof(struct ext2_inode) + sizeof(__u16)))
		return; /* no i_extra_isize field */

	if (hostorder)
		extra_isize = f->i_extra_isize;
	t->i_extra_isize = ext2fs_swab16(f->i_extra_isize);
	if (!hostorder)
		extra_isize = t->i_extra_isize;
	if (extra_isize > EXT2_INODE_SIZE(fs->super) -
				sizeof(struct ext2_inode)) {
		/* this is error case: i_extra_size is too large */
		return;
	}

	i = sizeof(struct ext2_inode) + extra_isize + sizeof(__u32);
	if (bufsize < (int) i)
		return; /* no space for EA magic */

	eaf = (__u32 *) (((char *) f) + sizeof(struct ext2_inode) +
					extra_isize);

	attr_magic = *eaf;
	if (!hostorder)
		attr_magic = ext2fs_swab32(attr_magic);

	if (attr_magic != EXT2_EXT_ATTR_MAGIC)
		return; /* it seems no magic here */

	eat = (__u32 *) (((char *) t) + sizeof(struct ext2_inode) +
					extra_isize);
	*eat = ext2fs_swab32(*eaf);

	/* convert EA(s) */
	ext2fs_swap_ext_attr((char *) (eat + 1), (char *) (eaf + 1),
			     bufsize - sizeof(struct ext2_inode) -
			     extra_isize - sizeof(__u32), 0);

}

void ext2fs_swap_inode(ext2_filsys fs, struct ext2_inode *t,
		       struct ext2_inode *f, int hostorder)
{
	ext2fs_swap_inode_full(fs, (struct ext2_inode_large *) t,
				(struct ext2_inode_large *) f, hostorder,
				sizeof(struct ext2_inode));
}

#endif

/* vi: set sw=4 ts=4: */
/*
 * swapfs.c --- swap ext2 filesystem data structures
 *
 * Copyright (C) 1995, 1996, 2002 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"
#include "ext2_ext_attr.h"

#if BB_BIG_ENDIAN
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
	sb->s_default_mount_opts = ext2fs_swab32(sb->s_default_mount_opts);
	sb->s_first_meta_bg = ext2fs_swab32(sb->s_first_meta_bg);
	sb->s_mkfs_time = ext2fs_swab32(sb->s_mkfs_time);
	for (i=0; i < 4; i++)
		sb->s_hash_seed[i] = ext2fs_swab32(sb->s_hash_seed[i]);
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
}

void ext2fs_swap_ext_attr(char *to, char *from, int bufsize, int has_header)
{
	struct ext2_ext_attr_header *from_header =
		(struct ext2_ext_attr_header *)from;
	struct ext2_ext_attr_header *to_header =
		(struct ext2_ext_attr_header *)to;
	struct ext2_ext_attr_entry *from_entry, *to_entry;
	char *from_end = (char *)from_header + bufsize;
	int n;

	if (to_header != from_header)
		memcpy(to_header, from_header, bufsize);

	from_entry = (struct ext2_ext_attr_entry *)from_header;
	to_entry   = (struct ext2_ext_attr_entry *)to_header;

	if (has_header) {
		to_header->h_magic    = ext2fs_swab32(from_header->h_magic);
		to_header->h_blocks   = ext2fs_swab32(from_header->h_blocks);
		to_header->h_refcount = ext2fs_swab32(from_header->h_refcount);
		for (n=0; n<4; n++)
			to_header->h_reserved[n] =
				ext2fs_swab32(from_header->h_reserved[n]);
		from_entry = (struct ext2_ext_attr_entry *)(from_header+1);
		to_entry   = (struct ext2_ext_attr_entry *)(to_header+1);
	}

	while ((char *)from_entry < from_end && *(__u32 *)from_entry) {
		to_entry->e_value_offs  =
			ext2fs_swab16(from_entry->e_value_offs);
		to_entry->e_value_block =
			ext2fs_swab32(from_entry->e_value_block);
		to_entry->e_value_size  =
			ext2fs_swab32(from_entry->e_value_size);
		from_entry = EXT2_EXT_ATTR_NEXT(from_entry);
		to_entry   = EXT2_EXT_ATTR_NEXT(to_entry);
	}
}

void ext2fs_swap_inode_full(ext2_filsys fs, struct ext2_inode_large *t,
			    struct ext2_inode_large *f, int hostorder,
			    int bufsize)
{
	unsigned i;
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
	t->i_blocks = ext2fs_swab32(f->i_blocks);
	t->i_flags = ext2fs_swab32(f->i_flags);
	t->i_file_acl = ext2fs_swab32(f->i_file_acl);
	t->i_dir_acl = ext2fs_swab32(f->i_dir_acl);
	if (!islnk || ext2fs_inode_data_blocks(fs, (struct ext2_inode *)t)) {
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
		t->osd1.linux1.l_i_reserved1 =
			ext2fs_swab32(f->osd1.linux1.l_i_reserved1);
		t->osd2.linux2.l_i_frag = f->osd2.linux2.l_i_frag;
		t->osd2.linux2.l_i_fsize = f->osd2.linux2.l_i_fsize;
		t->osd2.linux2.i_pad1 = ext2fs_swab16(f->osd2.linux2.i_pad1);
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
	case EXT2_OS_MASIX:
		t->osd1.masix1.m_i_reserved1 =
			ext2fs_swab32(f->osd1.masix1.m_i_reserved1);
		t->osd2.masix2.m_i_frag = f->osd2.masix2.m_i_frag;
		t->osd2.masix2.m_i_fsize = f->osd2.masix2.m_i_fsize;
		t->osd2.masix2.m_pad1 = ext2fs_swab16(f->osd2.masix2.m_pad1);
		t->osd2.masix2.m_i_reserved2[0] =
			ext2fs_swab32(f->osd2.masix2.m_i_reserved2[0]);
		t->osd2.masix2.m_i_reserved2[1] =
			ext2fs_swab32(f->osd2.masix2.m_i_reserved2[1]);
		break;
	}

	if (bufsize < (int) (sizeof(struct ext2_inode) + sizeof(__u16)))
		return; /* no i_extra_isize field */

	t->i_extra_isize = ext2fs_swab16(f->i_extra_isize);
	if (t->i_extra_isize > EXT2_INODE_SIZE(fs->super) -
				sizeof(struct ext2_inode)) {
		/* this is error case: i_extra_size is too large */
		return;
	}

	i = sizeof(struct ext2_inode) + t->i_extra_isize + sizeof(__u32);
	if (bufsize < (int) i)
		return; /* no space for EA magic */

	eaf = (__u32 *) (((char *) f) + sizeof(struct ext2_inode) +
					f->i_extra_isize);

	if (ext2fs_swab32(*eaf) != EXT2_EXT_ATTR_MAGIC)
		return; /* it seems no magic here */

	eat = (__u32 *) (((char *) t) + sizeof(struct ext2_inode) +
					f->i_extra_isize);
	*eat = ext2fs_swab32(*eaf);

	/* convert EA(s) */
	ext2fs_swap_ext_attr((char *) (eat + 1), (char *) (eaf + 1),
			     bufsize - sizeof(struct ext2_inode) -
			     t->i_extra_isize - sizeof(__u32), 0);
}

void ext2fs_swap_inode(ext2_filsys fs, struct ext2_inode *t,
		       struct ext2_inode *f, int hostorder)
{
	ext2fs_swap_inode_full(fs, (struct ext2_inode_large *) t,
				(struct ext2_inode_large *) f, hostorder,
				sizeof(struct ext2_inode));
}

#endif

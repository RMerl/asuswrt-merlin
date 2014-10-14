/*
 * efs_fs.h
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from work (c) 1995,1996 Christian Vogelgsang.
 */

#ifndef __EFS_FS_H__
#define __EFS_FS_H__

#define EFS_VERSION "1.0a"

static const char cprt[] = "EFS: "EFS_VERSION" - (c) 1999 Al Smith <Al.Smith@aeschi.ch.eu.org>";

#include <asm/uaccess.h>

/* 1 block is 512 bytes */
#define	EFS_BLOCKSIZE_BITS	9
#define	EFS_BLOCKSIZE		(1 << EFS_BLOCKSIZE_BITS)

#include <linux/fs.h>
#include <linux/efs_fs_i.h>
#include <linux/efs_fs_sb.h>
#include <linux/efs_dir.h>

static inline struct efs_inode_info *INODE_INFO(struct inode *inode)
{
	return container_of(inode, struct efs_inode_info, vfs_inode);
}

static inline struct efs_sb_info *SUPER_INFO(struct super_block *sb)
{
	return sb->s_fs_info;
}

struct statfs;

extern const struct inode_operations efs_dir_inode_operations;
extern const struct file_operations efs_dir_operations;
extern const struct address_space_operations efs_symlink_aops;

extern void efs_read_inode(struct inode *);
extern efs_block_t efs_map_block(struct inode *, efs_block_t);
extern int efs_get_block(struct inode *, sector_t, struct buffer_head *, int);

extern struct dentry *efs_lookup(struct inode *, struct dentry *, struct nameidata *);
extern struct dentry *efs_get_dentry(struct super_block *sb, void *vobjp);
extern struct dentry *efs_get_parent(struct dentry *);
extern int efs_bmap(struct inode *, int);

#endif /* __EFS_FS_H__ */

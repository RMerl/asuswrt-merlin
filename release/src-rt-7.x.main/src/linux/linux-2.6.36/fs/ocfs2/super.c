/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * super.c
 *
 * load/unload driver, mount/dismount volumes
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/statfs.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
#include <linux/socket.h>
#include <linux/inet.h>
#include <linux/parser.h>
#include <linux/crc32.h>
#include <linux/debugfs.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/quotaops.h>
#include <linux/smp_lock.h>

#define MLOG_MASK_PREFIX ML_SUPER
#include <cluster/masklog.h>

#include "ocfs2.h"

/* this should be the only file to include a version 1 header */
#include "ocfs1_fs_compat.h"

#include "alloc.h"
#include "blockcheck.h"
#include "dlmglue.h"
#include "export.h"
#include "extent_map.h"
#include "heartbeat.h"
#include "inode.h"
#include "journal.h"
#include "localalloc.h"
#include "namei.h"
#include "slot_map.h"
#include "super.h"
#include "sysfile.h"
#include "uptodate.h"
#include "ver.h"
#include "xattr.h"
#include "quota.h"
#include "refcounttree.h"
#include "suballoc.h"

#include "buffer_head_io.h"

static struct kmem_cache *ocfs2_inode_cachep = NULL;
struct kmem_cache *ocfs2_dquot_cachep;
struct kmem_cache *ocfs2_qf_chunk_cachep;

/* OCFS2 needs to schedule several differnt types of work which
 * require cluster locking, disk I/O, recovery waits, etc. Since these
 * types of work tend to be heavy we avoid using the kernel events
 * workqueue and schedule on our own. */
struct workqueue_struct *ocfs2_wq = NULL;

static struct dentry *ocfs2_debugfs_root = NULL;

MODULE_AUTHOR("Oracle");
MODULE_LICENSE("GPL");

struct mount_options
{
	unsigned long	commit_interval;
	unsigned long	mount_opt;
	unsigned int	atime_quantum;
	signed short	slot;
	int		localalloc_opt;
	unsigned int	resv_level;
	int		dir_resv_level;
	char		cluster_stack[OCFS2_STACK_LABEL_LEN + 1];
};

static int ocfs2_parse_options(struct super_block *sb, char *options,
			       struct mount_options *mopt,
			       int is_remount);
static int ocfs2_check_set_options(struct super_block *sb,
				   struct mount_options *options);
static int ocfs2_show_options(struct seq_file *s, struct vfsmount *mnt);
static void ocfs2_put_super(struct super_block *sb);
static int ocfs2_mount_volume(struct super_block *sb);
static int ocfs2_remount(struct super_block *sb, int *flags, char *data);
static void ocfs2_dismount_volume(struct super_block *sb, int mnt_err);
static int ocfs2_initialize_mem_caches(void);
static void ocfs2_free_mem_caches(void);
static void ocfs2_delete_osb(struct ocfs2_super *osb);

static int ocfs2_statfs(struct dentry *dentry, struct kstatfs *buf);

static int ocfs2_sync_fs(struct super_block *sb, int wait);

static int ocfs2_init_global_system_inodes(struct ocfs2_super *osb);
static int ocfs2_init_local_system_inodes(struct ocfs2_super *osb);
static void ocfs2_release_system_inodes(struct ocfs2_super *osb);
static int ocfs2_check_volume(struct ocfs2_super *osb);
static int ocfs2_verify_volume(struct ocfs2_dinode *di,
			       struct buffer_head *bh,
			       u32 sectsize,
			       struct ocfs2_blockcheck_stats *stats);
static int ocfs2_initialize_super(struct super_block *sb,
				  struct buffer_head *bh,
				  int sector_size,
				  struct ocfs2_blockcheck_stats *stats);
static int ocfs2_get_sector(struct super_block *sb,
			    struct buffer_head **bh,
			    int block,
			    int sect_size);
static struct inode *ocfs2_alloc_inode(struct super_block *sb);
static void ocfs2_destroy_inode(struct inode *inode);
static int ocfs2_susp_quotas(struct ocfs2_super *osb, int unsuspend);
static int ocfs2_enable_quotas(struct ocfs2_super *osb);
static void ocfs2_disable_quotas(struct ocfs2_super *osb);

static const struct super_operations ocfs2_sops = {
	.statfs		= ocfs2_statfs,
	.alloc_inode	= ocfs2_alloc_inode,
	.destroy_inode	= ocfs2_destroy_inode,
	.drop_inode	= ocfs2_drop_inode,
	.evict_inode	= ocfs2_evict_inode,
	.sync_fs	= ocfs2_sync_fs,
	.put_super	= ocfs2_put_super,
	.remount_fs	= ocfs2_remount,
	.show_options   = ocfs2_show_options,
	.quota_read	= ocfs2_quota_read,
	.quota_write	= ocfs2_quota_write,
};

enum {
	Opt_barrier,
	Opt_err_panic,
	Opt_err_ro,
	Opt_intr,
	Opt_nointr,
	Opt_hb_none,
	Opt_hb_local,
	Opt_data_ordered,
	Opt_data_writeback,
	Opt_atime_quantum,
	Opt_slot,
	Opt_commit,
	Opt_localalloc,
	Opt_localflocks,
	Opt_stack,
	Opt_user_xattr,
	Opt_nouser_xattr,
	Opt_inode64,
	Opt_acl,
	Opt_noacl,
	Opt_usrquota,
	Opt_grpquota,
	Opt_resv_level,
	Opt_dir_resv_level,
	Opt_err,
};

static const match_table_t tokens = {
	{Opt_barrier, "barrier=%u"},
	{Opt_err_panic, "errors=panic"},
	{Opt_err_ro, "errors=remount-ro"},
	{Opt_intr, "intr"},
	{Opt_nointr, "nointr"},
	{Opt_hb_none, OCFS2_HB_NONE},
	{Opt_hb_local, OCFS2_HB_LOCAL},
	{Opt_data_ordered, "data=ordered"},
	{Opt_data_writeback, "data=writeback"},
	{Opt_atime_quantum, "atime_quantum=%u"},
	{Opt_slot, "preferred_slot=%u"},
	{Opt_commit, "commit=%u"},
	{Opt_localalloc, "localalloc=%d"},
	{Opt_localflocks, "localflocks"},
	{Opt_stack, "cluster_stack=%s"},
	{Opt_user_xattr, "user_xattr"},
	{Opt_nouser_xattr, "nouser_xattr"},
	{Opt_inode64, "inode64"},
	{Opt_acl, "acl"},
	{Opt_noacl, "noacl"},
	{Opt_usrquota, "usrquota"},
	{Opt_grpquota, "grpquota"},
	{Opt_resv_level, "resv_level=%u"},
	{Opt_dir_resv_level, "dir_resv_level=%u"},
	{Opt_err, NULL}
};

#ifdef CONFIG_DEBUG_FS
static int ocfs2_osb_dump(struct ocfs2_super *osb, char *buf, int len)
{
	struct ocfs2_cluster_connection *cconn = osb->cconn;
	struct ocfs2_recovery_map *rm = osb->recovery_map;
	struct ocfs2_orphan_scan *os = &osb->osb_orphan_scan;
	int i, out = 0;

	out += snprintf(buf + out, len - out,
			"%10s => Id: %-s  Uuid: %-s  Gen: 0x%X  Label: %-s\n",
			"Device", osb->dev_str, osb->uuid_str,
			osb->fs_generation, osb->vol_label);

	out += snprintf(buf + out, len - out,
			"%10s => State: %d  Flags: 0x%lX\n", "Volume",
			atomic_read(&osb->vol_state), osb->osb_flags);

	out += snprintf(buf + out, len - out,
			"%10s => Block: %lu  Cluster: %d\n", "Sizes",
			osb->sb->s_blocksize, osb->s_clustersize);

	out += snprintf(buf + out, len - out,
			"%10s => Compat: 0x%X  Incompat: 0x%X  "
			"ROcompat: 0x%X\n",
			"Features", osb->s_feature_compat,
			osb->s_feature_incompat, osb->s_feature_ro_compat);

	out += snprintf(buf + out, len - out,
			"%10s => Opts: 0x%lX  AtimeQuanta: %u\n", "Mount",
			osb->s_mount_opt, osb->s_atime_quantum);

	if (cconn) {
		out += snprintf(buf + out, len - out,
				"%10s => Stack: %s  Name: %*s  "
				"Version: %d.%d\n", "Cluster",
				(*osb->osb_cluster_stack == '\0' ?
				 "o2cb" : osb->osb_cluster_stack),
				cconn->cc_namelen, cconn->cc_name,
				cconn->cc_version.pv_major,
				cconn->cc_version.pv_minor);
	}

	spin_lock(&osb->dc_task_lock);
	out += snprintf(buf + out, len - out,
			"%10s => Pid: %d  Count: %lu  WakeSeq: %lu  "
			"WorkSeq: %lu\n", "DownCnvt",
			(osb->dc_task ?  task_pid_nr(osb->dc_task) : -1),
			osb->blocked_lock_count, osb->dc_wake_sequence,
			osb->dc_work_sequence);
	spin_unlock(&osb->dc_task_lock);

	spin_lock(&osb->osb_lock);
	out += snprintf(buf + out, len - out, "%10s => Pid: %d  Nodes:",
			"Recovery",
			(osb->recovery_thread_task ?
			 task_pid_nr(osb->recovery_thread_task) : -1));
	if (rm->rm_used == 0)
		out += snprintf(buf + out, len - out, " None\n");
	else {
		for (i = 0; i < rm->rm_used; i++)
			out += snprintf(buf + out, len - out, " %d",
					rm->rm_entries[i]);
		out += snprintf(buf + out, len - out, "\n");
	}
	spin_unlock(&osb->osb_lock);

	out += snprintf(buf + out, len - out,
			"%10s => Pid: %d  Interval: %lu  Needs: %d\n", "Commit",
			(osb->commit_task ? task_pid_nr(osb->commit_task) : -1),
			osb->osb_commit_interval,
			atomic_read(&osb->needs_checkpoint));

	out += snprintf(buf + out, len - out,
			"%10s => State: %d  TxnId: %lu  NumTxns: %d\n",
			"Journal", osb->journal->j_state,
			osb->journal->j_trans_id,
			atomic_read(&osb->journal->j_num_trans));

	out += snprintf(buf + out, len - out,
			"%10s => GlobalAllocs: %d  LocalAllocs: %d  "
			"SubAllocs: %d  LAWinMoves: %d  SAExtends: %d\n",
			"Stats",
			atomic_read(&osb->alloc_stats.bitmap_data),
			atomic_read(&osb->alloc_stats.local_data),
			atomic_read(&osb->alloc_stats.bg_allocs),
			atomic_read(&osb->alloc_stats.moves),
			atomic_read(&osb->alloc_stats.bg_extends));

	out += snprintf(buf + out, len - out,
			"%10s => State: %u  Descriptor: %llu  Size: %u bits  "
			"Default: %u bits\n",
			"LocalAlloc", osb->local_alloc_state,
			(unsigned long long)osb->la_last_gd,
			osb->local_alloc_bits, osb->local_alloc_default_bits);

	spin_lock(&osb->osb_lock);
	out += snprintf(buf + out, len - out,
			"%10s => InodeSlot: %d  StolenInodes: %d, "
			"MetaSlot: %d  StolenMeta: %d\n", "Steal",
			osb->s_inode_steal_slot,
			atomic_read(&osb->s_num_inodes_stolen),
			osb->s_meta_steal_slot,
			atomic_read(&osb->s_num_meta_stolen));
	spin_unlock(&osb->osb_lock);

	out += snprintf(buf + out, len - out, "OrphanScan => ");
	out += snprintf(buf + out, len - out, "Local: %u  Global: %u ",
			os->os_count, os->os_seqno);
	out += snprintf(buf + out, len - out, " Last Scan: ");
	if (atomic_read(&os->os_state) == ORPHAN_SCAN_INACTIVE)
		out += snprintf(buf + out, len - out, "Disabled\n");
	else
		out += snprintf(buf + out, len - out, "%lu seconds ago\n",
				(get_seconds() - os->os_scantime.tv_sec));

	out += snprintf(buf + out, len - out, "%10s => %3s  %10s\n",
			"Slots", "Num", "RecoGen");
	for (i = 0; i < osb->max_slots; ++i) {
		out += snprintf(buf + out, len - out,
				"%10s  %c %3d  %10d\n",
				" ",
				(i == osb->slot_num ? '*' : ' '),
				i, osb->slot_recovery_generations[i]);
	}

	return out;
}

static int ocfs2_osb_debug_open(struct inode *inode, struct file *file)
{
	struct ocfs2_super *osb = inode->i_private;
	char *buf = NULL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		goto bail;

	i_size_write(inode, ocfs2_osb_dump(osb, buf, PAGE_SIZE));

	file->private_data = buf;

	return 0;
bail:
	return -ENOMEM;
}

static int ocfs2_debug_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static ssize_t ocfs2_debug_read(struct file *file, char __user *buf,
				size_t nbytes, loff_t *ppos)
{
	return simple_read_from_buffer(buf, nbytes, ppos, file->private_data,
				       i_size_read(file->f_mapping->host));
}
#else
static int ocfs2_osb_debug_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int ocfs2_debug_release(struct inode *inode, struct file *file)
{
	return 0;
}
static ssize_t ocfs2_debug_read(struct file *file, char __user *buf,
				size_t nbytes, loff_t *ppos)
{
	return 0;
}
#endif	/* CONFIG_DEBUG_FS */

static const struct file_operations ocfs2_osb_debug_fops = {
	.open =		ocfs2_osb_debug_open,
	.release =	ocfs2_debug_release,
	.read =		ocfs2_debug_read,
	.llseek =	generic_file_llseek,
};

static int ocfs2_sync_fs(struct super_block *sb, int wait)
{
	int status;
	tid_t target;
	struct ocfs2_super *osb = OCFS2_SB(sb);

	if (ocfs2_is_hard_readonly(osb))
		return -EROFS;

	if (wait) {
		status = ocfs2_flush_truncate_log(osb);
		if (status < 0)
			mlog_errno(status);
	} else {
		ocfs2_schedule_truncate_log_flush(osb, 0);
	}

	if (jbd2_journal_start_commit(OCFS2_SB(sb)->journal->j_journal,
				      &target)) {
		if (wait)
			jbd2_log_wait_commit(OCFS2_SB(sb)->journal->j_journal,
					     target);
	}
	return 0;
}

static int ocfs2_need_system_inode(struct ocfs2_super *osb, int ino)
{
	if (!OCFS2_HAS_RO_COMPAT_FEATURE(osb->sb, OCFS2_FEATURE_RO_COMPAT_USRQUOTA)
	    && (ino == USER_QUOTA_SYSTEM_INODE
		|| ino == LOCAL_USER_QUOTA_SYSTEM_INODE))
		return 0;
	if (!OCFS2_HAS_RO_COMPAT_FEATURE(osb->sb, OCFS2_FEATURE_RO_COMPAT_GRPQUOTA)
	    && (ino == GROUP_QUOTA_SYSTEM_INODE
		|| ino == LOCAL_GROUP_QUOTA_SYSTEM_INODE))
		return 0;
	return 1;
}

static int ocfs2_init_global_system_inodes(struct ocfs2_super *osb)
{
	struct inode *new = NULL;
	int status = 0;
	int i;

	mlog_entry_void();

	new = ocfs2_iget(osb, osb->root_blkno, OCFS2_FI_FLAG_SYSFILE, 0);
	if (IS_ERR(new)) {
		status = PTR_ERR(new);
		mlog_errno(status);
		goto bail;
	}
	osb->root_inode = new;

	new = ocfs2_iget(osb, osb->system_dir_blkno, OCFS2_FI_FLAG_SYSFILE, 0);
	if (IS_ERR(new)) {
		status = PTR_ERR(new);
		mlog_errno(status);
		goto bail;
	}
	osb->sys_root_inode = new;

	for (i = OCFS2_FIRST_ONLINE_SYSTEM_INODE;
	     i <= OCFS2_LAST_GLOBAL_SYSTEM_INODE; i++) {
		if (!ocfs2_need_system_inode(osb, i))
			continue;
		new = ocfs2_get_system_file_inode(osb, i, osb->slot_num);
		if (!new) {
			ocfs2_release_system_inodes(osb);
			status = -EINVAL;
			mlog_errno(status);
			mlog(ML_ERROR, "Unable to load system inode %d, "
			     "possibly corrupt fs?", i);
			goto bail;
		}
		// the array now has one ref, so drop this one
		iput(new);
	}

bail:
	mlog_exit(status);
	return status;
}

static int ocfs2_init_local_system_inodes(struct ocfs2_super *osb)
{
	struct inode *new = NULL;
	int status = 0;
	int i;

	mlog_entry_void();

	for (i = OCFS2_LAST_GLOBAL_SYSTEM_INODE + 1;
	     i < NUM_SYSTEM_INODES;
	     i++) {
		if (!ocfs2_need_system_inode(osb, i))
			continue;
		new = ocfs2_get_system_file_inode(osb, i, osb->slot_num);
		if (!new) {
			ocfs2_release_system_inodes(osb);
			status = -EINVAL;
			mlog(ML_ERROR, "status=%d, sysfile=%d, slot=%d\n",
			     status, i, osb->slot_num);
			goto bail;
		}
		/* the array now has one ref, so drop this one */
		iput(new);
	}

bail:
	mlog_exit(status);
	return status;
}

static void ocfs2_release_system_inodes(struct ocfs2_super *osb)
{
	int i;
	struct inode *inode;

	mlog_entry_void();

	for (i = 0; i < NUM_SYSTEM_INODES; i++) {
		inode = osb->system_inodes[i];
		if (inode) {
			iput(inode);
			osb->system_inodes[i] = NULL;
		}
	}

	inode = osb->sys_root_inode;
	if (inode) {
		iput(inode);
		osb->sys_root_inode = NULL;
	}

	inode = osb->root_inode;
	if (inode) {
		iput(inode);
		osb->root_inode = NULL;
	}

	mlog_exit(0);
}

/* We're allocating fs objects, use GFP_NOFS */
static struct inode *ocfs2_alloc_inode(struct super_block *sb)
{
	struct ocfs2_inode_info *oi;

	oi = kmem_cache_alloc(ocfs2_inode_cachep, GFP_NOFS);
	if (!oi)
		return NULL;

	jbd2_journal_init_jbd_inode(&oi->ip_jinode, &oi->vfs_inode);
	return &oi->vfs_inode;
}

static void ocfs2_destroy_inode(struct inode *inode)
{
	kmem_cache_free(ocfs2_inode_cachep, OCFS2_I(inode));
}

static unsigned long long ocfs2_max_file_offset(unsigned int bbits,
						unsigned int cbits)
{
	unsigned int bytes = 1 << cbits;
	unsigned int trim = bytes;
	unsigned int bitshift = 32;

	/*
	 * i_size and all block offsets in ocfs2 are always 64 bits
	 * wide. i_clusters is 32 bits, in cluster-sized units. So on
	 * 64 bit platforms, cluster size will be the limiting factor.
	 */

#if BITS_PER_LONG == 32
# if defined(CONFIG_LBDAF)
	BUILD_BUG_ON(sizeof(sector_t) != 8);
	/*
	 * We might be limited by page cache size.
	 */
	if (bytes > PAGE_CACHE_SIZE) {
		bytes = PAGE_CACHE_SIZE;
		trim = 1;
		/*
		 * Shift by 31 here so that we don't get larger than
		 * MAX_LFS_FILESIZE
		 */
		bitshift = 31;
	}
# else
	/*
	 * We are limited by the size of sector_t. Use block size, as
	 * that's what we expose to the VFS.
	 */
	bytes = 1 << bbits;
	trim = 1;
	bitshift = 31;
# endif
#endif

	/*
	 * Trim by a whole cluster when we can actually approach the
	 * on-disk limits. Otherwise we can overflow i_clusters when
	 * an extent start is at the max offset.
	 */
	return (((unsigned long long)bytes) << bitshift) - trim;
}

static int ocfs2_remount(struct super_block *sb, int *flags, char *data)
{
	int incompat_features;
	int ret = 0;
	struct mount_options parsed_options;
	struct ocfs2_super *osb = OCFS2_SB(sb);

	lock_kernel();

	if (!ocfs2_parse_options(sb, data, &parsed_options, 1) ||
	    !ocfs2_check_set_options(sb, &parsed_options)) {
		ret = -EINVAL;
		goto out;
	}

	if ((osb->s_mount_opt & OCFS2_MOUNT_HB_LOCAL) !=
	    (parsed_options.mount_opt & OCFS2_MOUNT_HB_LOCAL)) {
		ret = -EINVAL;
		mlog(ML_ERROR, "Cannot change heartbeat mode on remount\n");
		goto out;
	}

	if ((osb->s_mount_opt & OCFS2_MOUNT_DATA_WRITEBACK) !=
	    (parsed_options.mount_opt & OCFS2_MOUNT_DATA_WRITEBACK)) {
		ret = -EINVAL;
		mlog(ML_ERROR, "Cannot change data mode on remount\n");
		goto out;
	}

	/* Probably don't want this on remount; it might
	 * mess with other nodes */
	if (!(osb->s_mount_opt & OCFS2_MOUNT_INODE64) &&
	    (parsed_options.mount_opt & OCFS2_MOUNT_INODE64)) {
		ret = -EINVAL;
		mlog(ML_ERROR, "Cannot enable inode64 on remount\n");
		goto out;
	}

	/* We're going to/from readonly mode. */
	if ((*flags & MS_RDONLY) != (sb->s_flags & MS_RDONLY)) {
		/* Disable quota accounting before remounting RO */
		if (*flags & MS_RDONLY) {
			ret = ocfs2_susp_quotas(osb, 0);
			if (ret < 0)
				goto out;
		}
		/* Lock here so the check of HARD_RO and the potential
		 * setting of SOFT_RO is atomic. */
		spin_lock(&osb->osb_lock);
		if (osb->osb_flags & OCFS2_OSB_HARD_RO) {
			mlog(ML_ERROR, "Remount on readonly device is forbidden.\n");
			ret = -EROFS;
			goto unlock_osb;
		}

		if (*flags & MS_RDONLY) {
			mlog(0, "Going to ro mode.\n");
			sb->s_flags |= MS_RDONLY;
			osb->osb_flags |= OCFS2_OSB_SOFT_RO;
		} else {
			mlog(0, "Making ro filesystem writeable.\n");

			if (osb->osb_flags & OCFS2_OSB_ERROR_FS) {
				mlog(ML_ERROR, "Cannot remount RDWR "
				     "filesystem due to previous errors.\n");
				ret = -EROFS;
				goto unlock_osb;
			}
			incompat_features = OCFS2_HAS_RO_COMPAT_FEATURE(sb, ~OCFS2_FEATURE_RO_COMPAT_SUPP);
			if (incompat_features) {
				mlog(ML_ERROR, "Cannot remount RDWR because "
				     "of unsupported optional features "
				     "(%x).\n", incompat_features);
				ret = -EINVAL;
				goto unlock_osb;
			}
			sb->s_flags &= ~MS_RDONLY;
			osb->osb_flags &= ~OCFS2_OSB_SOFT_RO;
		}
unlock_osb:
		spin_unlock(&osb->osb_lock);
		/* Enable quota accounting after remounting RW */
		if (!ret && !(*flags & MS_RDONLY)) {
			if (sb_any_quota_suspended(sb))
				ret = ocfs2_susp_quotas(osb, 1);
			else
				ret = ocfs2_enable_quotas(osb);
			if (ret < 0) {
				/* Return back changes... */
				spin_lock(&osb->osb_lock);
				sb->s_flags |= MS_RDONLY;
				osb->osb_flags |= OCFS2_OSB_SOFT_RO;
				spin_unlock(&osb->osb_lock);
				goto out;
			}
		}
	}

	if (!ret) {
		/* Only save off the new mount options in case of a successful
		 * remount. */
		osb->s_mount_opt = parsed_options.mount_opt;
		osb->s_atime_quantum = parsed_options.atime_quantum;
		osb->preferred_slot = parsed_options.slot;
		if (parsed_options.commit_interval)
			osb->osb_commit_interval = parsed_options.commit_interval;

		if (!ocfs2_is_hard_readonly(osb))
			ocfs2_set_journal_params(osb);

		sb->s_flags = (sb->s_flags & ~MS_POSIXACL) |
			((osb->s_mount_opt & OCFS2_MOUNT_POSIX_ACL) ?
							MS_POSIXACL : 0);
	}
out:
	unlock_kernel();
	return ret;
}

static int ocfs2_sb_probe(struct super_block *sb,
			  struct buffer_head **bh,
			  int *sector_size,
			  struct ocfs2_blockcheck_stats *stats)
{
	int status, tmpstat;
	struct ocfs1_vol_disk_hdr *hdr;
	struct ocfs2_dinode *di;
	int blksize;

	*bh = NULL;

	/* may be > 512 */
	*sector_size = bdev_logical_block_size(sb->s_bdev);
	if (*sector_size > OCFS2_MAX_BLOCKSIZE) {
		mlog(ML_ERROR, "Hardware sector size too large: %d (max=%d)\n",
		     *sector_size, OCFS2_MAX_BLOCKSIZE);
		status = -EINVAL;
		goto bail;
	}

	/* Can this really happen? */
	if (*sector_size < OCFS2_MIN_BLOCKSIZE)
		*sector_size = OCFS2_MIN_BLOCKSIZE;

	/* check block zero for old format */
	status = ocfs2_get_sector(sb, bh, 0, *sector_size);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
	hdr = (struct ocfs1_vol_disk_hdr *) (*bh)->b_data;
	if (hdr->major_version == OCFS1_MAJOR_VERSION) {
		mlog(ML_ERROR, "incompatible version: %u.%u\n",
		     hdr->major_version, hdr->minor_version);
		status = -EINVAL;
	}
	if (memcmp(hdr->signature, OCFS1_VOLUME_SIGNATURE,
		   strlen(OCFS1_VOLUME_SIGNATURE)) == 0) {
		mlog(ML_ERROR, "incompatible volume signature: %8s\n",
		     hdr->signature);
		status = -EINVAL;
	}
	brelse(*bh);
	*bh = NULL;
	if (status < 0) {
		mlog(ML_ERROR, "This is an ocfs v1 filesystem which must be "
		     "upgraded before mounting with ocfs v2\n");
		goto bail;
	}

	/*
	 * Now check at magic offset for 512, 1024, 2048, 4096
	 * blocksizes.  4096 is the maximum blocksize because it is
	 * the minimum clustersize.
	 */
	status = -EINVAL;
	for (blksize = *sector_size;
	     blksize <= OCFS2_MAX_BLOCKSIZE;
	     blksize <<= 1) {
		tmpstat = ocfs2_get_sector(sb, bh,
					   OCFS2_SUPER_BLOCK_BLKNO,
					   blksize);
		if (tmpstat < 0) {
			status = tmpstat;
			mlog_errno(status);
			break;
		}
		di = (struct ocfs2_dinode *) (*bh)->b_data;
		memset(stats, 0, sizeof(struct ocfs2_blockcheck_stats));
		spin_lock_init(&stats->b_lock);
		tmpstat = ocfs2_verify_volume(di, *bh, blksize, stats);
		if (tmpstat < 0) {
			brelse(*bh);
			*bh = NULL;
		}
		if (tmpstat != -EAGAIN) {
			status = tmpstat;
			break;
		}
	}

bail:
	return status;
}

static int ocfs2_verify_heartbeat(struct ocfs2_super *osb)
{
	if (ocfs2_mount_local(osb)) {
		if (osb->s_mount_opt & OCFS2_MOUNT_HB_LOCAL) {
			mlog(ML_ERROR, "Cannot heartbeat on a locally "
			     "mounted device.\n");
			return -EINVAL;
		}
	}

	if (ocfs2_userspace_stack(osb)) {
		if (osb->s_mount_opt & OCFS2_MOUNT_HB_LOCAL) {
			mlog(ML_ERROR, "Userspace stack expected, but "
			     "o2cb heartbeat arguments passed to mount\n");
			return -EINVAL;
		}
	}

	if (!(osb->s_mount_opt & OCFS2_MOUNT_HB_LOCAL)) {
		if (!ocfs2_mount_local(osb) && !ocfs2_is_hard_readonly(osb) &&
		    !ocfs2_userspace_stack(osb)) {
			mlog(ML_ERROR, "Heartbeat has to be started to mount "
			     "a read-write clustered device.\n");
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * If we're using a userspace stack, mount should have passed
 * a name that matches the disk.  If not, mount should not
 * have passed a stack.
 */
static int ocfs2_verify_userspace_stack(struct ocfs2_super *osb,
					struct mount_options *mopt)
{
	if (!ocfs2_userspace_stack(osb) && mopt->cluster_stack[0]) {
		mlog(ML_ERROR,
		     "cluster stack passed to mount, but this filesystem "
		     "does not support it\n");
		return -EINVAL;
	}

	if (ocfs2_userspace_stack(osb) &&
	    strncmp(osb->osb_cluster_stack, mopt->cluster_stack,
		    OCFS2_STACK_LABEL_LEN)) {
		mlog(ML_ERROR,
		     "cluster stack passed to mount (\"%s\") does not "
		     "match the filesystem (\"%s\")\n",
		     mopt->cluster_stack,
		     osb->osb_cluster_stack);
		return -EINVAL;
	}

	return 0;
}

static int ocfs2_susp_quotas(struct ocfs2_super *osb, int unsuspend)
{
	int type;
	struct super_block *sb = osb->sb;
	unsigned int feature[MAXQUOTAS] = { OCFS2_FEATURE_RO_COMPAT_USRQUOTA,
					     OCFS2_FEATURE_RO_COMPAT_GRPQUOTA};
	int status = 0;

	for (type = 0; type < MAXQUOTAS; type++) {
		if (!OCFS2_HAS_RO_COMPAT_FEATURE(sb, feature[type]))
			continue;
		if (unsuspend)
			status = dquot_resume(sb, type);
		else {
			struct ocfs2_mem_dqinfo *oinfo;

			/* Cancel periodic syncing before suspending */
			oinfo = sb_dqinfo(sb, type)->dqi_priv;
			cancel_delayed_work_sync(&oinfo->dqi_sync_work);
			status = dquot_suspend(sb, type);
		}
		if (status < 0)
			break;
	}
	if (status < 0)
		mlog(ML_ERROR, "Failed to suspend/unsuspend quotas on "
		     "remount (error = %d).\n", status);
	return status;
}

static int ocfs2_enable_quotas(struct ocfs2_super *osb)
{
	struct inode *inode[MAXQUOTAS] = { NULL, NULL };
	struct super_block *sb = osb->sb;
	unsigned int feature[MAXQUOTAS] = { OCFS2_FEATURE_RO_COMPAT_USRQUOTA,
					     OCFS2_FEATURE_RO_COMPAT_GRPQUOTA};
	unsigned int ino[MAXQUOTAS] = { LOCAL_USER_QUOTA_SYSTEM_INODE,
					LOCAL_GROUP_QUOTA_SYSTEM_INODE };
	int status;
	int type;

	sb_dqopt(sb)->flags |= DQUOT_QUOTA_SYS_FILE | DQUOT_NEGATIVE_USAGE;
	for (type = 0; type < MAXQUOTAS; type++) {
		if (!OCFS2_HAS_RO_COMPAT_FEATURE(sb, feature[type]))
			continue;
		inode[type] = ocfs2_get_system_file_inode(osb, ino[type],
							osb->slot_num);
		if (!inode[type]) {
			status = -ENOENT;
			goto out_quota_off;
		}
		status = dquot_enable(inode[type], type, QFMT_OCFS2,
				      DQUOT_USAGE_ENABLED);
		if (status < 0)
			goto out_quota_off;
	}

	for (type = 0; type < MAXQUOTAS; type++)
		iput(inode[type]);
	return 0;
out_quota_off:
	ocfs2_disable_quotas(osb);
	for (type = 0; type < MAXQUOTAS; type++)
		iput(inode[type]);
	mlog_errno(status);
	return status;
}

static void ocfs2_disable_quotas(struct ocfs2_super *osb)
{
	int type;
	struct inode *inode;
	struct super_block *sb = osb->sb;
	struct ocfs2_mem_dqinfo *oinfo;

	/* We mostly ignore errors in this function because there's not much
	 * we can do when we see them */
	for (type = 0; type < MAXQUOTAS; type++) {
		if (!sb_has_quota_loaded(sb, type))
			continue;
		/* Cancel periodic syncing before we grab dqonoff_mutex */
		oinfo = sb_dqinfo(sb, type)->dqi_priv;
		cancel_delayed_work_sync(&oinfo->dqi_sync_work);
		inode = igrab(sb->s_dquot.files[type]);
		/* Turn off quotas. This will remove all dquot structures from
		 * memory and so they will be automatically synced to global
		 * quota files */
		dquot_disable(sb, type, DQUOT_USAGE_ENABLED |
					DQUOT_LIMITS_ENABLED);
		if (!inode)
			continue;
		iput(inode);
	}
}

/* Handle quota on quotactl */
static int ocfs2_quota_on(struct super_block *sb, int type, int format_id,
			  char *path)
{
	unsigned int feature[MAXQUOTAS] = { OCFS2_FEATURE_RO_COMPAT_USRQUOTA,
					     OCFS2_FEATURE_RO_COMPAT_GRPQUOTA};

	if (!OCFS2_HAS_RO_COMPAT_FEATURE(sb, feature[type]))
		return -EINVAL;

	return dquot_enable(sb_dqopt(sb)->files[type], type,
			    format_id, DQUOT_LIMITS_ENABLED);
}

/* Handle quota off quotactl */
static int ocfs2_quota_off(struct super_block *sb, int type)
{
	return dquot_disable(sb, type, DQUOT_LIMITS_ENABLED);
}

static const struct quotactl_ops ocfs2_quotactl_ops = {
	.quota_on	= ocfs2_quota_on,
	.quota_off	= ocfs2_quota_off,
	.quota_sync	= dquot_quota_sync,
	.get_info	= dquot_get_dqinfo,
	.set_info	= dquot_set_dqinfo,
	.get_dqblk	= dquot_get_dqblk,
	.set_dqblk	= dquot_set_dqblk,
};

static int ocfs2_fill_super(struct super_block *sb, void *data, int silent)
{
	struct dentry *root;
	int status, sector_size;
	struct mount_options parsed_options;
	struct inode *inode = NULL;
	struct ocfs2_super *osb = NULL;
	struct buffer_head *bh = NULL;
	char nodestr[8];
	struct ocfs2_blockcheck_stats stats;

	mlog_entry("%p, %p, %i", sb, data, silent);

	if (!ocfs2_parse_options(sb, data, &parsed_options, 0)) {
		status = -EINVAL;
		goto read_super_error;
	}

	/* probe for superblock */
	status = ocfs2_sb_probe(sb, &bh, &sector_size, &stats);
	if (status < 0) {
		mlog(ML_ERROR, "superblock probe failed!\n");
		goto read_super_error;
	}

	status = ocfs2_initialize_super(sb, bh, sector_size, &stats);
	osb = OCFS2_SB(sb);
	if (status < 0) {
		mlog_errno(status);
		goto read_super_error;
	}
	brelse(bh);
	bh = NULL;

	if (!ocfs2_check_set_options(sb, &parsed_options)) {
		status = -EINVAL;
		goto read_super_error;
	}
	osb->s_mount_opt = parsed_options.mount_opt;
	osb->s_atime_quantum = parsed_options.atime_quantum;
	osb->preferred_slot = parsed_options.slot;
	osb->osb_commit_interval = parsed_options.commit_interval;

	ocfs2_la_set_sizes(osb, parsed_options.localalloc_opt);
	osb->osb_resv_level = parsed_options.resv_level;
	osb->osb_dir_resv_level = parsed_options.resv_level;
	if (parsed_options.dir_resv_level == -1)
		osb->osb_dir_resv_level = parsed_options.resv_level;
	else
		osb->osb_dir_resv_level = parsed_options.dir_resv_level;

	status = ocfs2_verify_userspace_stack(osb, &parsed_options);
	if (status)
		goto read_super_error;

	sb->s_magic = OCFS2_SUPER_MAGIC;

	sb->s_flags = (sb->s_flags & ~MS_POSIXACL) |
		((osb->s_mount_opt & OCFS2_MOUNT_POSIX_ACL) ? MS_POSIXACL : 0);

	/* Hard readonly mode only if: bdev_read_only, MS_RDONLY,
	 * heartbeat=none */
	if (bdev_read_only(sb->s_bdev)) {
		if (!(sb->s_flags & MS_RDONLY)) {
			status = -EACCES;
			mlog(ML_ERROR, "Readonly device detected but readonly "
			     "mount was not specified.\n");
			goto read_super_error;
		}

		/* You should not be able to start a local heartbeat
		 * on a readonly device. */
		if (osb->s_mount_opt & OCFS2_MOUNT_HB_LOCAL) {
			status = -EROFS;
			mlog(ML_ERROR, "Local heartbeat specified on readonly "
			     "device.\n");
			goto read_super_error;
		}

		status = ocfs2_check_journals_nolocks(osb);
		if (status < 0) {
			if (status == -EROFS)
				mlog(ML_ERROR, "Recovery required on readonly "
				     "file system, but write access is "
				     "unavailable.\n");
			else
				mlog_errno(status);
			goto read_super_error;
		}

		ocfs2_set_ro_flag(osb, 1);

		printk(KERN_NOTICE "Readonly device detected. No cluster "
		       "services will be utilized for this mount. Recovery "
		       "will be skipped.\n");
	}

	if (!ocfs2_is_hard_readonly(osb)) {
		if (sb->s_flags & MS_RDONLY)
			ocfs2_set_ro_flag(osb, 0);
	}

	status = ocfs2_verify_heartbeat(osb);
	if (status < 0) {
		mlog_errno(status);
		goto read_super_error;
	}

	osb->osb_debug_root = debugfs_create_dir(osb->uuid_str,
						 ocfs2_debugfs_root);
	if (!osb->osb_debug_root) {
		status = -EINVAL;
		mlog(ML_ERROR, "Unable to create per-mount debugfs root.\n");
		goto read_super_error;
	}

	osb->osb_ctxt = debugfs_create_file("fs_state", S_IFREG|S_IRUSR,
					    osb->osb_debug_root,
					    osb,
					    &ocfs2_osb_debug_fops);
	if (!osb->osb_ctxt) {
		status = -EINVAL;
		mlog_errno(status);
		goto read_super_error;
	}

	if (ocfs2_meta_ecc(osb)) {
		status = ocfs2_blockcheck_stats_debugfs_install(
						&osb->osb_ecc_stats,
						osb->osb_debug_root);
		if (status) {
			mlog(ML_ERROR,
			     "Unable to create blockcheck statistics "
			     "files\n");
			goto read_super_error;
		}
	}

	status = ocfs2_mount_volume(sb);
	if (osb->root_inode)
		inode = igrab(osb->root_inode);

	if (status < 0)
		goto read_super_error;

	if (!inode) {
		status = -EIO;
		mlog_errno(status);
		goto read_super_error;
	}

	root = d_alloc_root(inode);
	if (!root) {
		status = -ENOMEM;
		mlog_errno(status);
		goto read_super_error;
	}

	sb->s_root = root;

	ocfs2_complete_mount_recovery(osb);

	if (ocfs2_mount_local(osb))
		snprintf(nodestr, sizeof(nodestr), "local");
	else
		snprintf(nodestr, sizeof(nodestr), "%u", osb->node_num);

	printk(KERN_INFO "ocfs2: Mounting device (%s) on (node %s, slot %d) "
	       "with %s data mode.\n",
	       osb->dev_str, nodestr, osb->slot_num,
	       osb->s_mount_opt & OCFS2_MOUNT_DATA_WRITEBACK ? "writeback" :
	       "ordered");

	atomic_set(&osb->vol_state, VOLUME_MOUNTED);
	wake_up(&osb->osb_mount_event);

	/* Now we can initialize quotas because we can afford to wait
	 * for cluster locks recovery now. That also means that truncation
	 * log recovery can happen but that waits for proper quota setup */
	if (!(sb->s_flags & MS_RDONLY)) {
		status = ocfs2_enable_quotas(osb);
		if (status < 0) {
			/* We have to err-out specially here because
			 * s_root is already set */
			mlog_errno(status);
			atomic_set(&osb->vol_state, VOLUME_DISABLED);
			wake_up(&osb->osb_mount_event);
			mlog_exit(status);
			return status;
		}
	}

	ocfs2_complete_quota_recovery(osb);

	/* Now we wake up again for processes waiting for quotas */
	atomic_set(&osb->vol_state, VOLUME_MOUNTED_QUOTAS);
	wake_up(&osb->osb_mount_event);

	/* Start this when the mount is almost sure of being successful */
	ocfs2_orphan_scan_start(osb);

	mlog_exit(status);
	return status;

read_super_error:
	brelse(bh);

	if (inode)
		iput(inode);

	if (osb) {
		atomic_set(&osb->vol_state, VOLUME_DISABLED);
		wake_up(&osb->osb_mount_event);
		ocfs2_dismount_volume(sb, 1);
	}

	mlog_exit(status);
	return status;
}

static int ocfs2_get_sb(struct file_system_type *fs_type,
			int flags,
			const char *dev_name,
			void *data,
			struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, ocfs2_fill_super,
			   mnt);
}

static void ocfs2_kill_sb(struct super_block *sb)
{
	struct ocfs2_super *osb = OCFS2_SB(sb);

	/* Failed mount? */
	if (!osb || atomic_read(&osb->vol_state) == VOLUME_DISABLED)
		goto out;

	/* Prevent further queueing of inode drop events */
	spin_lock(&dentry_list_lock);
	ocfs2_set_osb_flag(osb, OCFS2_OSB_DROP_DENTRY_LOCK_IMMED);
	spin_unlock(&dentry_list_lock);
	/* Wait for work to finish and/or remove it */
	cancel_work_sync(&osb->dentry_lock_work);
out:
	kill_block_super(sb);
}

static struct file_system_type ocfs2_fs_type = {
	.owner          = THIS_MODULE,
	.name           = "ocfs2",
	.get_sb         = ocfs2_get_sb, /* is this called when we mount
					* the fs? */
	.kill_sb        = ocfs2_kill_sb,

	.fs_flags       = FS_REQUIRES_DEV|FS_RENAME_DOES_D_MOVE,
	.next           = NULL
};

static int ocfs2_check_set_options(struct super_block *sb,
				   struct mount_options *options)
{
	if (options->mount_opt & OCFS2_MOUNT_USRQUOTA &&
	    !OCFS2_HAS_RO_COMPAT_FEATURE(sb,
					 OCFS2_FEATURE_RO_COMPAT_USRQUOTA)) {
		mlog(ML_ERROR, "User quotas were requested, but this "
		     "filesystem does not have the feature enabled.\n");
		return 0;
	}
	if (options->mount_opt & OCFS2_MOUNT_GRPQUOTA &&
	    !OCFS2_HAS_RO_COMPAT_FEATURE(sb,
					 OCFS2_FEATURE_RO_COMPAT_GRPQUOTA)) {
		mlog(ML_ERROR, "Group quotas were requested, but this "
		     "filesystem does not have the feature enabled.\n");
		return 0;
	}
	if (options->mount_opt & OCFS2_MOUNT_POSIX_ACL &&
	    !OCFS2_HAS_INCOMPAT_FEATURE(sb, OCFS2_FEATURE_INCOMPAT_XATTR)) {
		mlog(ML_ERROR, "ACL support requested but extended attributes "
		     "feature is not enabled\n");
		return 0;
	}
	/* No ACL setting specified? Use XATTR feature... */
	if (!(options->mount_opt & (OCFS2_MOUNT_POSIX_ACL |
				    OCFS2_MOUNT_NO_POSIX_ACL))) {
		if (OCFS2_HAS_INCOMPAT_FEATURE(sb, OCFS2_FEATURE_INCOMPAT_XATTR))
			options->mount_opt |= OCFS2_MOUNT_POSIX_ACL;
		else
			options->mount_opt |= OCFS2_MOUNT_NO_POSIX_ACL;
	}
	return 1;
}

static int ocfs2_parse_options(struct super_block *sb,
			       char *options,
			       struct mount_options *mopt,
			       int is_remount)
{
	int status;
	char *p;

	mlog_entry("remount: %d, options: \"%s\"\n", is_remount,
		   options ? options : "(none)");

	mopt->commit_interval = 0;
	mopt->mount_opt = OCFS2_MOUNT_NOINTR;
	mopt->atime_quantum = OCFS2_DEFAULT_ATIME_QUANTUM;
	mopt->slot = OCFS2_INVALID_SLOT;
	mopt->localalloc_opt = -1;
	mopt->cluster_stack[0] = '\0';
	mopt->resv_level = OCFS2_DEFAULT_RESV_LEVEL;
	mopt->dir_resv_level = -1;

	if (!options) {
		status = 1;
		goto bail;
	}

	while ((p = strsep(&options, ",")) != NULL) {
		int token, option;
		substring_t args[MAX_OPT_ARGS];

		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_hb_local:
			mopt->mount_opt |= OCFS2_MOUNT_HB_LOCAL;
			break;
		case Opt_hb_none:
			mopt->mount_opt &= ~OCFS2_MOUNT_HB_LOCAL;
			break;
		case Opt_barrier:
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option)
				mopt->mount_opt |= OCFS2_MOUNT_BARRIER;
			else
				mopt->mount_opt &= ~OCFS2_MOUNT_BARRIER;
			break;
		case Opt_intr:
			mopt->mount_opt &= ~OCFS2_MOUNT_NOINTR;
			break;
		case Opt_nointr:
			mopt->mount_opt |= OCFS2_MOUNT_NOINTR;
			break;
		case Opt_err_panic:
			mopt->mount_opt |= OCFS2_MOUNT_ERRORS_PANIC;
			break;
		case Opt_err_ro:
			mopt->mount_opt &= ~OCFS2_MOUNT_ERRORS_PANIC;
			break;
		case Opt_data_ordered:
			mopt->mount_opt &= ~OCFS2_MOUNT_DATA_WRITEBACK;
			break;
		case Opt_data_writeback:
			mopt->mount_opt |= OCFS2_MOUNT_DATA_WRITEBACK;
			break;
		case Opt_user_xattr:
			mopt->mount_opt &= ~OCFS2_MOUNT_NOUSERXATTR;
			break;
		case Opt_nouser_xattr:
			mopt->mount_opt |= OCFS2_MOUNT_NOUSERXATTR;
			break;
		case Opt_atime_quantum:
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option >= 0)
				mopt->atime_quantum = option;
			break;
		case Opt_slot:
			option = 0;
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option)
				mopt->slot = (s16)option;
			break;
		case Opt_commit:
			option = 0;
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option < 0)
				return 0;
			if (option == 0)
				option = JBD2_DEFAULT_MAX_COMMIT_AGE;
			mopt->commit_interval = HZ * option;
			break;
		case Opt_localalloc:
			option = 0;
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option >= 0)
				mopt->localalloc_opt = option;
			break;
		case Opt_localflocks:
			/*
			 * Changing this during remount could race
			 * flock() requests, or "unbalance" existing
			 * ones (e.g., a lock is taken in one mode but
			 * dropped in the other). If users care enough
			 * to flip locking modes during remount, we
			 * could add a "local" flag to individual
			 * flock structures for proper tracking of
			 * state.
			 */
			if (!is_remount)
				mopt->mount_opt |= OCFS2_MOUNT_LOCALFLOCKS;
			break;
		case Opt_stack:
			/* Check both that the option we were passed
			 * is of the right length and that it is a proper
			 * string of the right length.
			 */
			if (((args[0].to - args[0].from) !=
			     OCFS2_STACK_LABEL_LEN) ||
			    (strnlen(args[0].from,
				     OCFS2_STACK_LABEL_LEN) !=
			     OCFS2_STACK_LABEL_LEN)) {
				mlog(ML_ERROR,
				     "Invalid cluster_stack option\n");
				status = 0;
				goto bail;
			}
			memcpy(mopt->cluster_stack, args[0].from,
			       OCFS2_STACK_LABEL_LEN);
			mopt->cluster_stack[OCFS2_STACK_LABEL_LEN] = '\0';
			break;
		case Opt_inode64:
			mopt->mount_opt |= OCFS2_MOUNT_INODE64;
			break;
		case Opt_usrquota:
			mopt->mount_opt |= OCFS2_MOUNT_USRQUOTA;
			break;
		case Opt_grpquota:
			mopt->mount_opt |= OCFS2_MOUNT_GRPQUOTA;
			break;
		case Opt_acl:
			mopt->mount_opt |= OCFS2_MOUNT_POSIX_ACL;
			mopt->mount_opt &= ~OCFS2_MOUNT_NO_POSIX_ACL;
			break;
		case Opt_noacl:
			mopt->mount_opt |= OCFS2_MOUNT_NO_POSIX_ACL;
			mopt->mount_opt &= ~OCFS2_MOUNT_POSIX_ACL;
			break;
		case Opt_resv_level:
			if (is_remount)
				break;
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option >= OCFS2_MIN_RESV_LEVEL &&
			    option < OCFS2_MAX_RESV_LEVEL)
				mopt->resv_level = option;
			break;
		case Opt_dir_resv_level:
			if (is_remount)
				break;
			if (match_int(&args[0], &option)) {
				status = 0;
				goto bail;
			}
			if (option >= OCFS2_MIN_RESV_LEVEL &&
			    option < OCFS2_MAX_RESV_LEVEL)
				mopt->dir_resv_level = option;
			break;
		default:
			mlog(ML_ERROR,
			     "Unrecognized mount option \"%s\" "
			     "or missing value\n", p);
			status = 0;
			goto bail;
		}
	}

	status = 1;

bail:
	mlog_exit(status);
	return status;
}

static int ocfs2_show_options(struct seq_file *s, struct vfsmount *mnt)
{
	struct ocfs2_super *osb = OCFS2_SB(mnt->mnt_sb);
	unsigned long opts = osb->s_mount_opt;
	unsigned int local_alloc_megs;

	if (opts & OCFS2_MOUNT_HB_LOCAL)
		seq_printf(s, ",_netdev,heartbeat=local");
	else
		seq_printf(s, ",heartbeat=none");

	if (opts & OCFS2_MOUNT_NOINTR)
		seq_printf(s, ",nointr");

	if (opts & OCFS2_MOUNT_DATA_WRITEBACK)
		seq_printf(s, ",data=writeback");
	else
		seq_printf(s, ",data=ordered");

	if (opts & OCFS2_MOUNT_BARRIER)
		seq_printf(s, ",barrier=1");

	if (opts & OCFS2_MOUNT_ERRORS_PANIC)
		seq_printf(s, ",errors=panic");
	else
		seq_printf(s, ",errors=remount-ro");

	if (osb->preferred_slot != OCFS2_INVALID_SLOT)
		seq_printf(s, ",preferred_slot=%d", osb->preferred_slot);

	if (osb->s_atime_quantum != OCFS2_DEFAULT_ATIME_QUANTUM)
		seq_printf(s, ",atime_quantum=%u", osb->s_atime_quantum);

	if (osb->osb_commit_interval)
		seq_printf(s, ",commit=%u",
			   (unsigned) (osb->osb_commit_interval / HZ));

	local_alloc_megs = osb->local_alloc_bits >> (20 - osb->s_clustersize_bits);
	if (local_alloc_megs != ocfs2_la_default_mb(osb))
		seq_printf(s, ",localalloc=%d", local_alloc_megs);

	if (opts & OCFS2_MOUNT_LOCALFLOCKS)
		seq_printf(s, ",localflocks,");

	if (osb->osb_cluster_stack[0])
		seq_printf(s, ",cluster_stack=%.*s", OCFS2_STACK_LABEL_LEN,
			   osb->osb_cluster_stack);
	if (opts & OCFS2_MOUNT_USRQUOTA)
		seq_printf(s, ",usrquota");
	if (opts & OCFS2_MOUNT_GRPQUOTA)
		seq_printf(s, ",grpquota");

	if (opts & OCFS2_MOUNT_NOUSERXATTR)
		seq_printf(s, ",nouser_xattr");
	else
		seq_printf(s, ",user_xattr");

	if (opts & OCFS2_MOUNT_INODE64)
		seq_printf(s, ",inode64");

	if (opts & OCFS2_MOUNT_POSIX_ACL)
		seq_printf(s, ",acl");
	else
		seq_printf(s, ",noacl");

	if (osb->osb_resv_level != OCFS2_DEFAULT_RESV_LEVEL)
		seq_printf(s, ",resv_level=%d", osb->osb_resv_level);

	if (osb->osb_dir_resv_level != osb->osb_resv_level)
		seq_printf(s, ",dir_resv_level=%d", osb->osb_resv_level);

	return 0;
}

static int __init ocfs2_init(void)
{
	int status;

	mlog_entry_void();

	ocfs2_print_version();

	status = init_ocfs2_uptodate_cache();
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_initialize_mem_caches();
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	ocfs2_wq = create_singlethread_workqueue("ocfs2_wq");
	if (!ocfs2_wq) {
		status = -ENOMEM;
		goto leave;
	}

	ocfs2_debugfs_root = debugfs_create_dir("ocfs2", NULL);
	if (!ocfs2_debugfs_root) {
		status = -EFAULT;
		mlog(ML_ERROR, "Unable to create ocfs2 debugfs root.\n");
	}

	status = ocfs2_quota_setup();
	if (status)
		goto leave;

	ocfs2_set_locking_protocol();

	status = register_quota_format(&ocfs2_quota_format);
leave:
	if (status < 0) {
		ocfs2_quota_shutdown();
		ocfs2_free_mem_caches();
		exit_ocfs2_uptodate_cache();
	}

	mlog_exit(status);

	if (status >= 0) {
		return register_filesystem(&ocfs2_fs_type);
	} else
		return -1;
}

static void __exit ocfs2_exit(void)
{
	mlog_entry_void();

	ocfs2_quota_shutdown();

	if (ocfs2_wq) {
		flush_workqueue(ocfs2_wq);
		destroy_workqueue(ocfs2_wq);
	}

	unregister_quota_format(&ocfs2_quota_format);

	debugfs_remove(ocfs2_debugfs_root);

	ocfs2_free_mem_caches();

	unregister_filesystem(&ocfs2_fs_type);

	exit_ocfs2_uptodate_cache();

	mlog_exit_void();
}

static void ocfs2_put_super(struct super_block *sb)
{
	mlog_entry("(0x%p)\n", sb);

	lock_kernel();

	ocfs2_sync_blockdev(sb);
	ocfs2_dismount_volume(sb, 0);

	unlock_kernel();

	mlog_exit_void();
}

static int ocfs2_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct ocfs2_super *osb;
	u32 numbits, freebits;
	int status;
	struct ocfs2_dinode *bm_lock;
	struct buffer_head *bh = NULL;
	struct inode *inode = NULL;

	mlog_entry("(%p, %p)\n", dentry->d_sb, buf);

	osb = OCFS2_SB(dentry->d_sb);

	inode = ocfs2_get_system_file_inode(osb,
					    GLOBAL_BITMAP_SYSTEM_INODE,
					    OCFS2_INVALID_SLOT);
	if (!inode) {
		mlog(ML_ERROR, "failed to get bitmap inode\n");
		status = -EIO;
		goto bail;
	}

	status = ocfs2_inode_lock(inode, &bh, 0);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	bm_lock = (struct ocfs2_dinode *) bh->b_data;

	numbits = le32_to_cpu(bm_lock->id1.bitmap1.i_total);
	freebits = numbits - le32_to_cpu(bm_lock->id1.bitmap1.i_used);

	buf->f_type = OCFS2_SUPER_MAGIC;
	buf->f_bsize = dentry->d_sb->s_blocksize;
	buf->f_namelen = OCFS2_MAX_FILENAME_LEN;
	buf->f_blocks = ((sector_t) numbits) *
			(osb->s_clustersize >> osb->sb->s_blocksize_bits);
	buf->f_bfree = ((sector_t) freebits) *
		       (osb->s_clustersize >> osb->sb->s_blocksize_bits);
	buf->f_bavail = buf->f_bfree;
	buf->f_files = numbits;
	buf->f_ffree = freebits;
	buf->f_fsid.val[0] = crc32_le(0, osb->uuid_str, OCFS2_VOL_UUID_LEN)
				& 0xFFFFFFFFUL;
	buf->f_fsid.val[1] = crc32_le(0, osb->uuid_str + OCFS2_VOL_UUID_LEN,
				OCFS2_VOL_UUID_LEN) & 0xFFFFFFFFUL;

	brelse(bh);

	ocfs2_inode_unlock(inode, 0);
	status = 0;
bail:
	if (inode)
		iput(inode);

	mlog_exit(status);

	return status;
}

static void ocfs2_inode_init_once(void *data)
{
	struct ocfs2_inode_info *oi = data;

	oi->ip_flags = 0;
	oi->ip_open_count = 0;
	spin_lock_init(&oi->ip_lock);
	ocfs2_extent_map_init(&oi->vfs_inode);
	INIT_LIST_HEAD(&oi->ip_io_markers);
	oi->ip_dir_start_lookup = 0;

	init_rwsem(&oi->ip_alloc_sem);
	init_rwsem(&oi->ip_xattr_sem);
	mutex_init(&oi->ip_io_mutex);

	oi->ip_blkno = 0ULL;
	oi->ip_clusters = 0;

	ocfs2_resv_init_once(&oi->ip_la_data_resv);

	ocfs2_lock_res_init_once(&oi->ip_rw_lockres);
	ocfs2_lock_res_init_once(&oi->ip_inode_lockres);
	ocfs2_lock_res_init_once(&oi->ip_open_lockres);

	ocfs2_metadata_cache_init(INODE_CACHE(&oi->vfs_inode),
				  &ocfs2_inode_caching_ops);

	inode_init_once(&oi->vfs_inode);
}

static int ocfs2_initialize_mem_caches(void)
{
	ocfs2_inode_cachep = kmem_cache_create("ocfs2_inode_cache",
				       sizeof(struct ocfs2_inode_info),
				       0,
				       (SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD),
				       ocfs2_inode_init_once);
	ocfs2_dquot_cachep = kmem_cache_create("ocfs2_dquot_cache",
					sizeof(struct ocfs2_dquot),
					0,
					(SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD),
					NULL);
	ocfs2_qf_chunk_cachep = kmem_cache_create("ocfs2_qf_chunk_cache",
					sizeof(struct ocfs2_quota_chunk),
					0,
					(SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD),
					NULL);
	if (!ocfs2_inode_cachep || !ocfs2_dquot_cachep ||
	    !ocfs2_qf_chunk_cachep) {
		if (ocfs2_inode_cachep)
			kmem_cache_destroy(ocfs2_inode_cachep);
		if (ocfs2_dquot_cachep)
			kmem_cache_destroy(ocfs2_dquot_cachep);
		if (ocfs2_qf_chunk_cachep)
			kmem_cache_destroy(ocfs2_qf_chunk_cachep);
		return -ENOMEM;
	}

	return 0;
}

static void ocfs2_free_mem_caches(void)
{
	if (ocfs2_inode_cachep)
		kmem_cache_destroy(ocfs2_inode_cachep);
	ocfs2_inode_cachep = NULL;

	if (ocfs2_dquot_cachep)
		kmem_cache_destroy(ocfs2_dquot_cachep);
	ocfs2_dquot_cachep = NULL;

	if (ocfs2_qf_chunk_cachep)
		kmem_cache_destroy(ocfs2_qf_chunk_cachep);
	ocfs2_qf_chunk_cachep = NULL;
}

static int ocfs2_get_sector(struct super_block *sb,
			    struct buffer_head **bh,
			    int block,
			    int sect_size)
{
	if (!sb_set_blocksize(sb, sect_size)) {
		mlog(ML_ERROR, "unable to set blocksize\n");
		return -EIO;
	}

	*bh = sb_getblk(sb, block);
	if (!*bh) {
		mlog_errno(-EIO);
		return -EIO;
	}
	lock_buffer(*bh);
	if (!buffer_dirty(*bh))
		clear_buffer_uptodate(*bh);
	unlock_buffer(*bh);
	ll_rw_block(READ, 1, bh);
	wait_on_buffer(*bh);
	if (!buffer_uptodate(*bh)) {
		mlog_errno(-EIO);
		brelse(*bh);
		*bh = NULL;
		return -EIO;
	}

	return 0;
}

static int ocfs2_mount_volume(struct super_block *sb)
{
	int status = 0;
	int unlock_super = 0;
	struct ocfs2_super *osb = OCFS2_SB(sb);

	mlog_entry_void();

	if (ocfs2_is_hard_readonly(osb))
		goto leave;

	status = ocfs2_dlm_init(osb);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_super_lock(osb, 1);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}
	unlock_super = 1;

	/* This will load up the node map and add ourselves to it. */
	status = ocfs2_find_slot(osb);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	/* load all node-local system inodes */
	status = ocfs2_init_local_system_inodes(osb);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_check_volume(osb);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_truncate_log_init(osb);
	if (status < 0)
		mlog_errno(status);

leave:
	if (unlock_super)
		ocfs2_super_unlock(osb, 1);

	mlog_exit(status);
	return status;
}

static void ocfs2_dismount_volume(struct super_block *sb, int mnt_err)
{
	int tmp, hangup_needed = 0;
	struct ocfs2_super *osb = NULL;
	char nodestr[8];

	mlog_entry("(0x%p)\n", sb);

	BUG_ON(!sb);
	osb = OCFS2_SB(sb);
	BUG_ON(!osb);

	debugfs_remove(osb->osb_ctxt);

	/*
	 * Flush inode dropping work queue so that deletes are
	 * performed while the filesystem is still working
	 */
	ocfs2_drop_all_dl_inodes(osb);

	/* Orphan scan should be stopped as early as possible */
	ocfs2_orphan_scan_stop(osb);

	ocfs2_disable_quotas(osb);

	ocfs2_shutdown_local_alloc(osb);

	ocfs2_truncate_log_shutdown(osb);

	/* This will disable recovery and flush any recovery work. */
	ocfs2_recovery_exit(osb);

	ocfs2_journal_shutdown(osb);

	ocfs2_sync_blockdev(sb);

	ocfs2_purge_refcount_trees(osb);

	/* No cluster connection means we've failed during mount, so skip
	 * all the steps which depended on that to complete. */
	if (osb->cconn) {
		tmp = ocfs2_super_lock(osb, 1);
		if (tmp < 0) {
			mlog_errno(tmp);
			return;
		}
	}

	if (osb->slot_num != OCFS2_INVALID_SLOT)
		ocfs2_put_slot(osb);

	if (osb->cconn)
		ocfs2_super_unlock(osb, 1);

	ocfs2_release_system_inodes(osb);

	/*
	 * If we're dismounting due to mount error, mount.ocfs2 will clean
	 * up heartbeat.  If we're a local mount, there is no heartbeat.
	 * If we failed before we got a uuid_str yet, we can't stop
	 * heartbeat.  Otherwise, do it.
	 */
	if (!mnt_err && !ocfs2_mount_local(osb) && osb->uuid_str)
		hangup_needed = 1;

	if (osb->cconn)
		ocfs2_dlm_shutdown(osb, hangup_needed);

	ocfs2_blockcheck_stats_debugfs_remove(&osb->osb_ecc_stats);
	debugfs_remove(osb->osb_debug_root);

	if (hangup_needed)
		ocfs2_cluster_hangup(osb->uuid_str, strlen(osb->uuid_str));

	atomic_set(&osb->vol_state, VOLUME_DISMOUNTED);

	if (ocfs2_mount_local(osb))
		snprintf(nodestr, sizeof(nodestr), "local");
	else
		snprintf(nodestr, sizeof(nodestr), "%u", osb->node_num);

	printk(KERN_INFO "ocfs2: Unmounting device (%s) on (node %s)\n",
	       osb->dev_str, nodestr);

	ocfs2_delete_osb(osb);
	kfree(osb);
	sb->s_dev = 0;
	sb->s_fs_info = NULL;
}

static int ocfs2_setup_osb_uuid(struct ocfs2_super *osb, const unsigned char *uuid,
				unsigned uuid_bytes)
{
	int i, ret;
	char *ptr;

	BUG_ON(uuid_bytes != OCFS2_VOL_UUID_LEN);

	osb->uuid_str = kzalloc(OCFS2_VOL_UUID_LEN * 2 + 1, GFP_KERNEL);
	if (osb->uuid_str == NULL)
		return -ENOMEM;

	for (i = 0, ptr = osb->uuid_str; i < OCFS2_VOL_UUID_LEN; i++) {
		/* print with null */
		ret = snprintf(ptr, 3, "%02X", uuid[i]);
		if (ret != 2) /* drop super cleans up */
			return -EINVAL;
		/* then only advance past the last char */
		ptr += 2;
	}

	return 0;
}

static int ocfs2_initialize_super(struct super_block *sb,
				  struct buffer_head *bh,
				  int sector_size,
				  struct ocfs2_blockcheck_stats *stats)
{
	int status;
	int i, cbits, bbits;
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)bh->b_data;
	struct inode *inode = NULL;
	struct ocfs2_journal *journal;
	__le32 uuid_net_key;
	struct ocfs2_super *osb;

	mlog_entry_void();

	osb = kzalloc(sizeof(struct ocfs2_super), GFP_KERNEL);
	if (!osb) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	sb->s_fs_info = osb;
	sb->s_op = &ocfs2_sops;
	sb->s_export_op = &ocfs2_export_ops;
	sb->s_qcop = &ocfs2_quotactl_ops;
	sb->dq_op = &ocfs2_quota_operations;
	sb->s_xattr = ocfs2_xattr_handlers;
	sb->s_time_gran = 1;
	sb->s_flags |= MS_NOATIME;
	/* this is needed to support O_LARGEFILE */
	cbits = le32_to_cpu(di->id2.i_super.s_clustersize_bits);
	bbits = le32_to_cpu(di->id2.i_super.s_blocksize_bits);
	sb->s_maxbytes = ocfs2_max_file_offset(bbits, cbits);

	osb->osb_dx_mask = (1 << (cbits - bbits)) - 1;

	for (i = 0; i < 3; i++)
		osb->osb_dx_seed[i] = le32_to_cpu(di->id2.i_super.s_dx_seed[i]);
	osb->osb_dx_seed[3] = le32_to_cpu(di->id2.i_super.s_uuid_hash);

	osb->sb = sb;
	/* Save off for ocfs2_rw_direct */
	osb->s_sectsize_bits = blksize_bits(sector_size);
	BUG_ON(!osb->s_sectsize_bits);

	spin_lock_init(&osb->dc_task_lock);
	init_waitqueue_head(&osb->dc_event);
	osb->dc_work_sequence = 0;
	osb->dc_wake_sequence = 0;
	INIT_LIST_HEAD(&osb->blocked_lock_list);
	osb->blocked_lock_count = 0;
	spin_lock_init(&osb->osb_lock);
	spin_lock_init(&osb->osb_xattr_lock);
	ocfs2_init_steal_slots(osb);

	atomic_set(&osb->alloc_stats.moves, 0);
	atomic_set(&osb->alloc_stats.local_data, 0);
	atomic_set(&osb->alloc_stats.bitmap_data, 0);
	atomic_set(&osb->alloc_stats.bg_allocs, 0);
	atomic_set(&osb->alloc_stats.bg_extends, 0);

	/* Copy the blockcheck stats from the superblock probe */
	osb->osb_ecc_stats = *stats;

	ocfs2_init_node_maps(osb);

	snprintf(osb->dev_str, sizeof(osb->dev_str), "%u,%u",
		 MAJOR(osb->sb->s_dev), MINOR(osb->sb->s_dev));

	ocfs2_orphan_scan_init(osb);

	status = ocfs2_recovery_init(osb);
	if (status) {
		mlog(ML_ERROR, "Unable to initialize recovery state\n");
		mlog_errno(status);
		goto bail;
	}

	init_waitqueue_head(&osb->checkpoint_event);
	atomic_set(&osb->needs_checkpoint, 0);

	osb->s_atime_quantum = OCFS2_DEFAULT_ATIME_QUANTUM;

	osb->slot_num = OCFS2_INVALID_SLOT;

	osb->s_xattr_inline_size = le16_to_cpu(
					di->id2.i_super.s_xattr_inline_size);

	osb->local_alloc_state = OCFS2_LA_UNUSED;
	osb->local_alloc_bh = NULL;
	INIT_DELAYED_WORK(&osb->la_enable_wq, ocfs2_la_enable_worker);

	init_waitqueue_head(&osb->osb_mount_event);

	status = ocfs2_resmap_init(osb, &osb->osb_la_resmap);
	if (status) {
		mlog_errno(status);
		goto bail;
	}

	osb->vol_label = kmalloc(OCFS2_MAX_VOL_LABEL_LEN, GFP_KERNEL);
	if (!osb->vol_label) {
		mlog(ML_ERROR, "unable to alloc vol label\n");
		status = -ENOMEM;
		goto bail;
	}

	osb->max_slots = le16_to_cpu(di->id2.i_super.s_max_slots);
	if (osb->max_slots > OCFS2_MAX_SLOTS || osb->max_slots == 0) {
		mlog(ML_ERROR, "Invalid number of node slots (%u)\n",
		     osb->max_slots);
		status = -EINVAL;
		goto bail;
	}
	mlog(0, "max_slots for this device: %u\n", osb->max_slots);

	osb->slot_recovery_generations =
		kcalloc(osb->max_slots, sizeof(*osb->slot_recovery_generations),
			GFP_KERNEL);
	if (!osb->slot_recovery_generations) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	init_waitqueue_head(&osb->osb_wipe_event);
	osb->osb_orphan_wipes = kcalloc(osb->max_slots,
					sizeof(*osb->osb_orphan_wipes),
					GFP_KERNEL);
	if (!osb->osb_orphan_wipes) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	osb->osb_rf_lock_tree = RB_ROOT;

	osb->s_feature_compat =
		le32_to_cpu(OCFS2_RAW_SB(di)->s_feature_compat);
	osb->s_feature_ro_compat =
		le32_to_cpu(OCFS2_RAW_SB(di)->s_feature_ro_compat);
	osb->s_feature_incompat =
		le32_to_cpu(OCFS2_RAW_SB(di)->s_feature_incompat);

	if ((i = OCFS2_HAS_INCOMPAT_FEATURE(osb->sb, ~OCFS2_FEATURE_INCOMPAT_SUPP))) {
		mlog(ML_ERROR, "couldn't mount because of unsupported "
		     "optional features (%x).\n", i);
		status = -EINVAL;
		goto bail;
	}
	if (!(osb->sb->s_flags & MS_RDONLY) &&
	    (i = OCFS2_HAS_RO_COMPAT_FEATURE(osb->sb, ~OCFS2_FEATURE_RO_COMPAT_SUPP))) {
		mlog(ML_ERROR, "couldn't mount RDWR because of "
		     "unsupported optional features (%x).\n", i);
		status = -EINVAL;
		goto bail;
	}

	if (ocfs2_userspace_stack(osb)) {
		memcpy(osb->osb_cluster_stack,
		       OCFS2_RAW_SB(di)->s_cluster_info.ci_stack,
		       OCFS2_STACK_LABEL_LEN);
		osb->osb_cluster_stack[OCFS2_STACK_LABEL_LEN] = '\0';
		if (strlen(osb->osb_cluster_stack) != OCFS2_STACK_LABEL_LEN) {
			mlog(ML_ERROR,
			     "couldn't mount because of an invalid "
			     "cluster stack label (%s) \n",
			     osb->osb_cluster_stack);
			status = -EINVAL;
			goto bail;
		}
	} else {
		/* The empty string is identical with classic tools that
		 * don't know about s_cluster_info. */
		osb->osb_cluster_stack[0] = '\0';
	}

	get_random_bytes(&osb->s_next_generation, sizeof(u32));

	/* initialize our journal structure */

	journal = kzalloc(sizeof(struct ocfs2_journal), GFP_KERNEL);
	if (!journal) {
		mlog(ML_ERROR, "unable to alloc journal\n");
		status = -ENOMEM;
		goto bail;
	}
	osb->journal = journal;
	journal->j_osb = osb;

	atomic_set(&journal->j_num_trans, 0);
	init_rwsem(&journal->j_trans_barrier);
	init_waitqueue_head(&journal->j_checkpointed);
	spin_lock_init(&journal->j_lock);
	journal->j_trans_id = (unsigned long) 1;
	INIT_LIST_HEAD(&journal->j_la_cleanups);
	INIT_WORK(&journal->j_recovery_work, ocfs2_complete_recovery);
	journal->j_state = OCFS2_JOURNAL_FREE;

	INIT_WORK(&osb->dentry_lock_work, ocfs2_drop_dl_inodes);
	osb->dentry_lock_list = NULL;

	/* get some pseudo constants for clustersize bits */
	osb->s_clustersize_bits =
		le32_to_cpu(di->id2.i_super.s_clustersize_bits);
	osb->s_clustersize = 1 << osb->s_clustersize_bits;
	mlog(0, "clusterbits=%d\n", osb->s_clustersize_bits);

	if (osb->s_clustersize < OCFS2_MIN_CLUSTERSIZE ||
	    osb->s_clustersize > OCFS2_MAX_CLUSTERSIZE) {
		mlog(ML_ERROR, "Volume has invalid cluster size (%d)\n",
		     osb->s_clustersize);
		status = -EINVAL;
		goto bail;
	}

	if (ocfs2_clusters_to_blocks(osb->sb, le32_to_cpu(di->i_clusters) - 1)
	    > (u32)~0UL) {
		mlog(ML_ERROR, "Volume might try to write to blocks beyond "
		     "what jbd can address in 32 bits.\n");
		status = -EINVAL;
		goto bail;
	}

	if (ocfs2_setup_osb_uuid(osb, di->id2.i_super.s_uuid,
				 sizeof(di->id2.i_super.s_uuid))) {
		mlog(ML_ERROR, "Out of memory trying to setup our uuid.\n");
		status = -ENOMEM;
		goto bail;
	}

	memcpy(&uuid_net_key, di->id2.i_super.s_uuid, sizeof(uuid_net_key));

	strncpy(osb->vol_label, di->id2.i_super.s_label, 63);
	osb->vol_label[63] = '\0';
	osb->root_blkno = le64_to_cpu(di->id2.i_super.s_root_blkno);
	osb->system_dir_blkno = le64_to_cpu(di->id2.i_super.s_system_dir_blkno);
	osb->first_cluster_group_blkno =
		le64_to_cpu(di->id2.i_super.s_first_cluster_group);
	osb->fs_generation = le32_to_cpu(di->i_fs_generation);
	osb->uuid_hash = le32_to_cpu(di->id2.i_super.s_uuid_hash);
	mlog(0, "vol_label: %s\n", osb->vol_label);
	mlog(0, "uuid: %s\n", osb->uuid_str);
	mlog(0, "root_blkno=%llu, system_dir_blkno=%llu\n",
	     (unsigned long long)osb->root_blkno,
	     (unsigned long long)osb->system_dir_blkno);

	osb->osb_dlm_debug = ocfs2_new_dlm_debug();
	if (!osb->osb_dlm_debug) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	atomic_set(&osb->vol_state, VOLUME_INIT);

	/* load root, system_dir, and all global system inodes */
	status = ocfs2_init_global_system_inodes(osb);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	/*
	 * global bitmap
	 */
	inode = ocfs2_get_system_file_inode(osb, GLOBAL_BITMAP_SYSTEM_INODE,
					    OCFS2_INVALID_SLOT);
	if (!inode) {
		status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}

	osb->bitmap_blkno = OCFS2_I(inode)->ip_blkno;
	osb->osb_clusters_at_boot = OCFS2_I(inode)->ip_clusters;
	iput(inode);

	osb->bitmap_cpg = ocfs2_group_bitmap_size(sb, 0,
				 osb->s_feature_incompat) * 8;

	status = ocfs2_init_slot_info(osb);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

bail:
	mlog_exit(status);
	return status;
}

/*
 * will return: -EAGAIN if it is ok to keep searching for superblocks
 *              -EINVAL if there is a bad superblock
 *              0 on success
 */
static int ocfs2_verify_volume(struct ocfs2_dinode *di,
			       struct buffer_head *bh,
			       u32 blksz,
			       struct ocfs2_blockcheck_stats *stats)
{
	int status = -EAGAIN;

	mlog_entry_void();

	if (memcmp(di->i_signature, OCFS2_SUPER_BLOCK_SIGNATURE,
		   strlen(OCFS2_SUPER_BLOCK_SIGNATURE)) == 0) {
		/* We have to do a raw check of the feature here */
		if (le32_to_cpu(di->id2.i_super.s_feature_incompat) &
		    OCFS2_FEATURE_INCOMPAT_META_ECC) {
			status = ocfs2_block_check_validate(bh->b_data,
							    bh->b_size,
							    &di->i_check,
							    stats);
			if (status)
				goto out;
		}
		status = -EINVAL;
		if ((1 << le32_to_cpu(di->id2.i_super.s_blocksize_bits)) != blksz) {
			mlog(ML_ERROR, "found superblock with incorrect block "
			     "size: found %u, should be %u\n",
			     1 << le32_to_cpu(di->id2.i_super.s_blocksize_bits),
			       blksz);
		} else if (le16_to_cpu(di->id2.i_super.s_major_rev_level) !=
			   OCFS2_MAJOR_REV_LEVEL ||
			   le16_to_cpu(di->id2.i_super.s_minor_rev_level) !=
			   OCFS2_MINOR_REV_LEVEL) {
			mlog(ML_ERROR, "found superblock with bad version: "
			     "found %u.%u, should be %u.%u\n",
			     le16_to_cpu(di->id2.i_super.s_major_rev_level),
			     le16_to_cpu(di->id2.i_super.s_minor_rev_level),
			     OCFS2_MAJOR_REV_LEVEL,
			     OCFS2_MINOR_REV_LEVEL);
		} else if (bh->b_blocknr != le64_to_cpu(di->i_blkno)) {
			mlog(ML_ERROR, "bad block number on superblock: "
			     "found %llu, should be %llu\n",
			     (unsigned long long)le64_to_cpu(di->i_blkno),
			     (unsigned long long)bh->b_blocknr);
		} else if (le32_to_cpu(di->id2.i_super.s_clustersize_bits) < 12 ||
			    le32_to_cpu(di->id2.i_super.s_clustersize_bits) > 20) {
			mlog(ML_ERROR, "bad cluster size found: %u\n",
			     1 << le32_to_cpu(di->id2.i_super.s_clustersize_bits));
		} else if (!le64_to_cpu(di->id2.i_super.s_root_blkno)) {
			mlog(ML_ERROR, "bad root_blkno: 0\n");
		} else if (!le64_to_cpu(di->id2.i_super.s_system_dir_blkno)) {
			mlog(ML_ERROR, "bad system_dir_blkno: 0\n");
		} else if (le16_to_cpu(di->id2.i_super.s_max_slots) > OCFS2_MAX_SLOTS) {
			mlog(ML_ERROR,
			     "Superblock slots found greater than file system "
			     "maximum: found %u, max %u\n",
			     le16_to_cpu(di->id2.i_super.s_max_slots),
			     OCFS2_MAX_SLOTS);
		} else {
			/* found it! */
			status = 0;
		}
	}

out:
	mlog_exit(status);
	return status;
}

static int ocfs2_check_volume(struct ocfs2_super *osb)
{
	int status;
	int dirty;
	int local;
	struct ocfs2_dinode *local_alloc = NULL; /* only used if we
						  * recover
						  * ourselves. */

	mlog_entry_void();

	/* Init our journal object. */
	status = ocfs2_journal_init(osb->journal, &dirty);
	if (status < 0) {
		mlog(ML_ERROR, "Could not initialize journal!\n");
		goto finally;
	}

	/* If the journal was unmounted cleanly then we don't want to
	 * recover anything. Otherwise, journal_load will do that
	 * dirty work for us :) */
	if (!dirty) {
		status = ocfs2_journal_wipe(osb->journal, 0);
		if (status < 0) {
			mlog_errno(status);
			goto finally;
		}
	} else {
		mlog(ML_NOTICE, "File system was not unmounted cleanly, "
		     "recovering volume.\n");
	}

	local = ocfs2_mount_local(osb);

	/* will play back anything left in the journal. */
	status = ocfs2_journal_load(osb->journal, local, dirty);
	if (status < 0) {
		mlog(ML_ERROR, "ocfs2 journal load failed! %d\n", status);
		goto finally;
	}

	if (dirty) {
		/* recover my local alloc if we didn't unmount cleanly. */
		status = ocfs2_begin_local_alloc_recovery(osb,
							  osb->slot_num,
							  &local_alloc);
		if (status < 0) {
			mlog_errno(status);
			goto finally;
		}
		/* we complete the recovery process after we've marked
		 * ourselves as mounted. */
	}

	mlog(0, "Journal loaded.\n");

	status = ocfs2_load_local_alloc(osb);
	if (status < 0) {
		mlog_errno(status);
		goto finally;
	}

	if (dirty) {
		/* Recovery will be completed after we've mounted the
		 * rest of the volume. */
		osb->dirty = 1;
		osb->local_alloc_copy = local_alloc;
		local_alloc = NULL;
	}

	/* go through each journal, trylock it and if you get the
	 * lock, and it's marked as dirty, set the bit in the recover
	 * map and launch a recovery thread for it. */
	status = ocfs2_mark_dead_nodes(osb);
	if (status < 0) {
		mlog_errno(status);
		goto finally;
	}

	status = ocfs2_compute_replay_slots(osb);
	if (status < 0)
		mlog_errno(status);

finally:
	if (local_alloc)
		kfree(local_alloc);

	mlog_exit(status);
	return status;
}

/*
 * The routine gets called from dismount or close whenever a dismount on
 * volume is requested and the osb open count becomes 1.
 * It will remove the osb from the global list and also free up all the
 * initialized resources and fileobject.
 */
static void ocfs2_delete_osb(struct ocfs2_super *osb)
{
	mlog_entry_void();

	/* This function assumes that the caller has the main osb resource */

	ocfs2_free_slot_info(osb);

	kfree(osb->osb_orphan_wipes);
	kfree(osb->slot_recovery_generations);
	kfree(osb->journal);
	if (osb->local_alloc_copy)
		kfree(osb->local_alloc_copy);
	kfree(osb->uuid_str);
	ocfs2_put_dlm_debug(osb->osb_dlm_debug);
	memset(osb, 0, sizeof(struct ocfs2_super));

	mlog_exit_void();
}

/* Put OCFS2 into a readonly state, or (if the user specifies it),
 * panic(). We do not support continue-on-error operation. */
static void ocfs2_handle_error(struct super_block *sb)
{
	struct ocfs2_super *osb = OCFS2_SB(sb);

	if (osb->s_mount_opt & OCFS2_MOUNT_ERRORS_PANIC)
		panic("OCFS2: (device %s): panic forced after error\n",
		      sb->s_id);

	ocfs2_set_osb_flag(osb, OCFS2_OSB_ERROR_FS);

	if (sb->s_flags & MS_RDONLY &&
	    (ocfs2_is_soft_readonly(osb) ||
	     ocfs2_is_hard_readonly(osb)))
		return;

	printk(KERN_CRIT "File system is now read-only due to the potential "
	       "of on-disk corruption. Please run fsck.ocfs2 once the file "
	       "system is unmounted.\n");
	sb->s_flags |= MS_RDONLY;
	ocfs2_set_ro_flag(osb, 0);
}

static char error_buf[1024];

void __ocfs2_error(struct super_block *sb,
		   const char *function,
		   const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(error_buf, sizeof(error_buf), fmt, args);
	va_end(args);

	/* Not using mlog here because we want to show the actual
	 * function the error came from. */
	printk(KERN_CRIT "OCFS2: ERROR (device %s): %s: %s\n",
	       sb->s_id, function, error_buf);

	ocfs2_handle_error(sb);
}

/* Handle critical errors. This is intentionally more drastic than
 * ocfs2_handle_error, so we only use for things like journal errors,
 * etc. */
void __ocfs2_abort(struct super_block* sb,
		   const char *function,
		   const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(error_buf, sizeof(error_buf), fmt, args);
	va_end(args);

	printk(KERN_CRIT "OCFS2: abort (device %s): %s: %s\n",
	       sb->s_id, function, error_buf);

	/* We don't have the cluster support yet to go straight to
	 * hard readonly in here. Until then, we want to keep
	 * ocfs2_abort() so that we can at least mark critical
	 * errors.
	 *
	 * TODO: This should abort the journal and alert other nodes
	 * that our slot needs recovery. */

	/* Force a panic(). This stinks, but it's better than letting
	 * things continue without having a proper hard readonly
	 * here. */
	if (!ocfs2_mount_local(OCFS2_SB(sb)))
		OCFS2_SB(sb)->s_mount_opt |= OCFS2_MOUNT_ERRORS_PANIC;
	ocfs2_handle_error(sb);
}

/*
 * Void signal blockers, because in-kernel sigprocmask() only fails
 * when SIG_* is wrong.
 */
void ocfs2_block_signals(sigset_t *oldset)
{
	int rc;
	sigset_t blocked;

	sigfillset(&blocked);
	rc = sigprocmask(SIG_BLOCK, &blocked, oldset);
	BUG_ON(rc);
}

void ocfs2_unblock_signals(sigset_t *oldset)
{
	int rc = sigprocmask(SIG_SETMASK, oldset, NULL);
	BUG_ON(rc);
}

module_init(ocfs2_init);
module_exit(ocfs2_exit);

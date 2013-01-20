/*
 * mkjournal.c --- make a journal for a filesystem
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "ext2_fs.h"
#include "e2p/e2p.h"
#include "ext2fs.h"
#include "jfs_user.h"

/*
 * This function automatically sets up the journal superblock and
 * returns it as an allocated block.
 */
errcode_t ext2fs_create_journal_superblock(ext2_filsys fs,
					   __u32 size, int flags,
					   char  **ret_jsb)
{
	errcode_t		retval;
	journal_superblock_t	*jsb;

	if (size < 1024)
		return EXT2_ET_JOURNAL_TOO_SMALL;

	if ((retval = ext2fs_get_mem(fs->blocksize, &jsb)))
		return retval;

	memset (jsb, 0, fs->blocksize);

	jsb->s_header.h_magic = htonl(JFS_MAGIC_NUMBER);
	if (flags & EXT2_MKJOURNAL_V1_SUPER)
		jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V1);
	else
		jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V2);
	jsb->s_blocksize = htonl(fs->blocksize);
	jsb->s_maxlen = htonl(size);
	jsb->s_nr_users = htonl(1);
	jsb->s_first = htonl(1);
	jsb->s_sequence = htonl(1);
	memcpy(jsb->s_uuid, fs->super->s_uuid, sizeof(fs->super->s_uuid));
	/*
	 * If we're creating an external journal device, we need to
	 * adjust these fields.
	 */
	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		jsb->s_nr_users = 0;
		if (fs->blocksize == 1024)
			jsb->s_first = htonl(3);
		else
			jsb->s_first = htonl(2);
	}

	*ret_jsb = (char *) jsb;
	return 0;
}

/*
 * This function writes a journal using POSIX routines.  It is used
 * for creating external journals and creating journals on live
 * filesystems.
 */
static errcode_t write_journal_file(ext2_filsys fs, char *filename,
				    blk_t size, int flags)
{
	errcode_t	retval;
	char		*buf = 0;
	int		fd, ret_size;
	blk_t		i;

	if ((retval = ext2fs_create_journal_superblock(fs, size, flags, &buf)))
		return retval;

	/* Open the device or journal file */
	if ((fd = open(filename, O_WRONLY)) < 0) {
		retval = errno;
		goto errout;
	}

	/* Write the superblock out */
	retval = EXT2_ET_SHORT_WRITE;
	ret_size = write(fd, buf, fs->blocksize);
	if (ret_size < 0) {
		retval = errno;
		goto errout;
	}
	if (ret_size != (int) fs->blocksize)
		goto errout;
	memset(buf, 0, fs->blocksize);

	for (i = 1; i < size; i++) {
		ret_size = write(fd, buf, fs->blocksize);
		if (ret_size < 0) {
			retval = errno;
			goto errout;
		}
		if (ret_size != (int) fs->blocksize)
			goto errout;
	}
	close(fd);

	retval = 0;
errout:
	ext2fs_free_mem(&buf);
	return retval;
}

/*
 * Convenience function which zeros out _num_ blocks starting at
 * _blk_.  In case of an error, the details of the error is returned
 * via _ret_blk_ and _ret_count_ if they are non-NULL pointers.
 * Returns 0 on success, and an error code on an error.
 *
 * As a special case, if the first argument is NULL, then it will
 * attempt to free the static zeroizing buffer.  (This is to keep
 * programs that check for memory leaks happy.)
 */
#define STRIDE_LENGTH 8
errcode_t ext2fs_zero_blocks(ext2_filsys fs, blk_t blk, int num,
			     blk_t *ret_blk, int *ret_count)
{
	int		j, count;
	static char	*buf;
	errcode_t	retval;

	/* If fs is null, clean up the static buffer and return */
	if (!fs) {
		if (buf) {
			free(buf);
			buf = 0;
		}
		return 0;
	}
	/* Allocate the zeroizing buffer if necessary */
	if (!buf) {
		buf = malloc(fs->blocksize * STRIDE_LENGTH);
		if (!buf)
			return ENOMEM;
		memset(buf, 0, fs->blocksize * STRIDE_LENGTH);
	}
	/* OK, do the write loop */
	j=0;
	while (j < num) {
		if (blk % STRIDE_LENGTH) {
			count = STRIDE_LENGTH - (blk % STRIDE_LENGTH);
			if (count > (num - j))
				count = num - j;
		} else {
			count = num - j;
			if (count > STRIDE_LENGTH)
				count = STRIDE_LENGTH;
		}
		retval = io_channel_write_blk(fs->io, blk, count, buf);
		if (retval) {
			if (ret_count)
				*ret_count = count;
			if (ret_blk)
				*ret_blk = blk;
			return retval;
		}
		j += count; blk += count;
	}
	return 0;
}

/*
 * Helper function for creating the journal using direct I/O routines
 */
struct mkjournal_struct {
	int		num_blocks;
	int		newblocks;
	blk_t		goal;
	blk_t		blk_to_zero;
	int		zero_count;
	char		*buf;
	errcode_t	err;
};

static int mkjournal_proc(ext2_filsys	fs,
			   blk_t	*blocknr,
			   e2_blkcnt_t	blockcnt,
			   blk_t	ref_block EXT2FS_ATTR((unused)),
			   int		ref_offset EXT2FS_ATTR((unused)),
			   void		*priv_data)
{
	struct mkjournal_struct *es = (struct mkjournal_struct *) priv_data;
	blk_t	new_blk;
	errcode_t	retval;

	if (*blocknr) {
		es->goal = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, es->goal, 0, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt >= 0)
		es->num_blocks--;

	es->newblocks++;
	retval = 0;
	if (blockcnt <= 0)
		retval = io_channel_write_blk(fs->io, new_blk, 1, es->buf);
	else {
		if (es->zero_count) {
			if ((es->blk_to_zero + es->zero_count == new_blk) &&
			    (es->zero_count < 1024))
				es->zero_count++;
			else {
				retval = ext2fs_zero_blocks(fs,
							    es->blk_to_zero,
							    es->zero_count,
							    0, 0);
				es->zero_count = 0;
			}
		}
		if (es->zero_count == 0) {
			es->blk_to_zero = new_blk;
			es->zero_count = 1;
		}
	}

	if (blockcnt == 0)
		memset(es->buf, 0, fs->blocksize);

	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	*blocknr = es->goal = new_blk;
	ext2fs_block_alloc_stats(fs, new_blk, +1);

	if (es->num_blocks == 0)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;

}

/*
 * This function creates a journal using direct I/O routines.
 */
static errcode_t write_journal_inode(ext2_filsys fs, ext2_ino_t journal_ino,
				     blk_t size, int flags)
{
	char			*buf;
	dgrp_t			group, start, end, i, log_flex;
	errcode_t		retval;
	struct ext2_inode	inode;
	struct mkjournal_struct	es;

	if ((retval = ext2fs_create_journal_superblock(fs, size, flags, &buf)))
		return retval;

	if ((retval = ext2fs_read_bitmaps(fs)))
		return retval;

	if ((retval = ext2fs_read_inode(fs, journal_ino, &inode)))
		return retval;

	if (inode.i_blocks > 0)
		return EEXIST;

	es.num_blocks = size;
	es.newblocks = 0;
	es.buf = buf;
	es.err = 0;
	es.zero_count = 0;

	if (fs->super->s_feature_incompat & EXT3_FEATURE_INCOMPAT_EXTENTS) {
		inode.i_flags |= EXT4_EXTENTS_FL;
		if ((retval = ext2fs_write_inode(fs, journal_ino, &inode)))
			return retval;
	}

	/*
	 * Set the initial goal block to be roughly at the middle of
	 * the filesystem.  Pick a group that has the largest number
	 * of free blocks.
	 */
	group = ext2fs_group_of_blk(fs, (fs->super->s_blocks_count -
					 fs->super->s_first_data_block) / 2);
	log_flex = 1 << fs->super->s_log_groups_per_flex;
	if (fs->super->s_log_groups_per_flex && (group > log_flex)) {
		group = group & ~(log_flex - 1);
		while ((group < fs->group_desc_count) &&
		       fs->group_desc[group].bg_free_blocks_count == 0)
			group++;
		if (group == fs->group_desc_count)
			group = 0;
		start = group;
	} else
		start = (group > 0) ? group-1 : group;
	end = ((group+1) < fs->group_desc_count) ? group+1 : group;
	group = start;
	for (i=start+1; i <= end; i++)
		if (fs->group_desc[i].bg_free_blocks_count >
		    fs->group_desc[group].bg_free_blocks_count)
			group = i;

	es.goal = (fs->super->s_blocks_per_group * group) +
		fs->super->s_first_data_block;

	retval = ext2fs_block_iterate2(fs, journal_ino, BLOCK_FLAG_APPEND,
				       0, mkjournal_proc, &es);
	if (es.err) {
		retval = es.err;
		goto errout;
	}
	if (es.zero_count) {
		retval = ext2fs_zero_blocks(fs, es.blk_to_zero,
					    es.zero_count, 0, 0);
		if (retval)
			goto errout;
	}

	if ((retval = ext2fs_read_inode(fs, journal_ino, &inode)))
		goto errout;

 	inode.i_size += fs->blocksize * size;
	ext2fs_iblk_add_blocks(fs, &inode, es.newblocks);
	inode.i_mtime = inode.i_ctime = fs->now ? fs->now : time(0);
	inode.i_links_count = 1;
	inode.i_mode = LINUX_S_IFREG | 0600;

	if ((retval = ext2fs_write_new_inode(fs, journal_ino, &inode)))
		goto errout;
	retval = 0;

	memcpy(fs->super->s_jnl_blocks, inode.i_block, EXT2_N_BLOCKS*4);
	fs->super->s_jnl_blocks[16] = inode.i_size;
	fs->super->s_jnl_backup_type = EXT3_JNL_BACKUP_BLOCKS;
	ext2fs_mark_super_dirty(fs);

errout:
	ext2fs_free_mem(&buf);
	return retval;
}

/*
 * Find a reasonable journal file size (in blocks) given the number of blocks
 * in the filesystem.  For very small filesystems, it is not reasonable to
 * have a journal that fills more than half of the filesystem.
 */
int ext2fs_default_journal_size(__u64 blocks)
{
	if (blocks < 2048)
		return -1;
	if (blocks < 32768)
		return (1024);
	if (blocks < 256*1024)
		return (4096);
	if (blocks < 512*1024)
		return (8192);
	if (blocks < 1024*1024)
		return (16384);
	return 32768;
}

/*
 * This function adds a journal device to a filesystem
 */
errcode_t ext2fs_add_journal_device(ext2_filsys fs, ext2_filsys journal_dev)
{
	struct stat	st;
	errcode_t	retval;
	char		buf[1024];
	journal_superblock_t	*jsb;
	int		start;
	__u32		i, nr_users;

	/* Make sure the device exists and is a block device */
	if (stat(journal_dev->device_name, &st) < 0)
		return errno;

	if (!S_ISBLK(st.st_mode))
		return EXT2_ET_JOURNAL_NOT_BLOCK; /* Must be a block device */

	/* Get the journal superblock */
	start = 1;
	if (journal_dev->blocksize == 1024)
		start++;
	if ((retval = io_channel_read_blk(journal_dev->io, start, -1024, buf)))
		return retval;

	jsb = (journal_superblock_t *) buf;
	if ((jsb->s_header.h_magic != (unsigned) ntohl(JFS_MAGIC_NUMBER)) ||
	    (jsb->s_header.h_blocktype != (unsigned) ntohl(JFS_SUPERBLOCK_V2)))
		return EXT2_ET_NO_JOURNAL_SB;

	if (ntohl(jsb->s_blocksize) != (unsigned long) fs->blocksize)
		return EXT2_ET_UNEXPECTED_BLOCK_SIZE;

	/* Check and see if this filesystem has already been added */
	nr_users = ntohl(jsb->s_nr_users);
	for (i=0; i < nr_users; i++) {
		if (memcmp(fs->super->s_uuid,
			   &jsb->s_users[i*16], 16) == 0)
			break;
	}
	if (i >= nr_users) {
		memcpy(&jsb->s_users[nr_users*16],
		       fs->super->s_uuid, 16);
		jsb->s_nr_users = htonl(nr_users+1);
	}

	/* Writeback the journal superblock */
	if ((retval = io_channel_write_blk(journal_dev->io, start, -1024, buf)))
		return retval;

	fs->super->s_journal_inum = 0;
	fs->super->s_journal_dev = st.st_rdev;
	memcpy(fs->super->s_journal_uuid, jsb->s_uuid,
	       sizeof(fs->super->s_journal_uuid));
	fs->super->s_feature_compat |= EXT3_FEATURE_COMPAT_HAS_JOURNAL;
	ext2fs_mark_super_dirty(fs);
	return 0;
}

/*
 * This function adds a journal inode to a filesystem, using either
 * POSIX routines if the filesystem is mounted, or using direct I/O
 * functions if it is not.
 */
errcode_t ext2fs_add_journal_inode(ext2_filsys fs, blk_t size, int flags)
{
	errcode_t		retval;
	ext2_ino_t		journal_ino;
	struct stat		st;
	char			jfile[1024];
	int			mount_flags, f;
	int			fd = -1;

	if ((retval = ext2fs_check_mount_point(fs->device_name, &mount_flags,
					       jfile, sizeof(jfile)-10)))
		return retval;

	if (mount_flags & EXT2_MF_MOUNTED) {
		strcat(jfile, "/.journal");

		/*
		 * If .../.journal already exists, make sure any
		 * immutable or append-only flags are cleared.
		 */
#if defined(HAVE_CHFLAGS) && defined(UF_NODUMP)
		(void) chflags (jfile, 0);
#else
#if HAVE_EXT2_IOCTLS
		fd = open(jfile, O_RDONLY);
		if (fd >= 0) {
			f = 0;
			ioctl(fd, EXT2_IOC_SETFLAGS, &f);
			close(fd);
		}
#endif
#endif

		/* Create the journal file */
		if ((fd = open(jfile, O_CREAT|O_WRONLY, 0600)) < 0)
			return errno;

		if ((retval = write_journal_file(fs, jfile, size, flags)))
			goto errout;

		/* Get inode number of the journal file */
		if (fstat(fd, &st) < 0) {
			retval = errno;
			goto errout;
		}

#if defined(HAVE_CHFLAGS) && defined(UF_NODUMP)
		retval = fchflags (fd, UF_NODUMP|UF_IMMUTABLE);
#else
#if HAVE_EXT2_IOCTLS
		if (ioctl(fd, EXT2_IOC_GETFLAGS, &f) < 0) {
			retval = errno;
			goto errout;
		}
		f |= EXT2_NODUMP_FL | EXT2_IMMUTABLE_FL;
		retval = ioctl(fd, EXT2_IOC_SETFLAGS, &f);
#endif
#endif
		if (retval) {
			retval = errno;
			goto errout;
		}

		if (close(fd) < 0) {
			retval = errno;
			fd = -1;
			goto errout;
		}
		journal_ino = st.st_ino;
	} else {
		if ((mount_flags & EXT2_MF_BUSY) &&
		    !(fs->flags & EXT2_FLAG_EXCLUSIVE)) {
			retval = EBUSY;
			goto errout;
		}
		journal_ino = EXT2_JOURNAL_INO;
		if ((retval = write_journal_inode(fs, journal_ino,
						  size, flags)))
			return retval;
	}

	fs->super->s_journal_inum = journal_ino;
	fs->super->s_journal_dev = 0;
	memset(fs->super->s_journal_uuid, 0,
	       sizeof(fs->super->s_journal_uuid));
	fs->super->s_feature_compat |= EXT3_FEATURE_COMPAT_HAS_JOURNAL;

	ext2fs_mark_super_dirty(fs);
	return 0;
errout:
	if (fd > 0)
		close(fd);
	return retval;
}

#ifdef DEBUG
main(int argc, char **argv)
{
	errcode_t	retval;
	char		*device_name;
	ext2_filsys 	fs;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s filesystem\n", argv[0]);
		exit(1);
	}
	device_name = argv[1];

	retval = ext2fs_open (device_name, EXT2_FLAG_RW, 0, 0,
			      unix_io_manager, &fs);
	if (retval) {
		com_err(argv[0], retval, "while opening %s", device_name);
		exit(1);
	}

	retval = ext2fs_add_journal_inode(fs, 1024);
	if (retval) {
		com_err(argv[0], retval, "while adding journal to %s",
			device_name);
		exit(1);
	}
	retval = ext2fs_flush(fs);
	if (retval) {
		printf("Warning, had trouble writing out superblocks.\n");
	}
	ext2fs_close(fs);
	exit(0);

}
#endif

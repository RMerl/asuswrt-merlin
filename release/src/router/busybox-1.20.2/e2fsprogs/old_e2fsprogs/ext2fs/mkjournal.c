/* vi: set sw=4 ts=4: */
/*
 * mkjournal.c --- make a journal for a filesystem
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
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
#include "../e2p/e2p.h"
#include "../e2fsck.h"
#include "ext2fs.h"
#include "kernel-jbd.h"

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
	char		*buf = NULL;
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
 * Helper function for creating the journal using direct I/O routines
 */
struct mkjournal_struct {
	int		num_blocks;
	int		newblocks;
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
	static blk_t	last_blk = 0;
	errcode_t	retval;

	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, 0, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0)
		es->num_blocks--;

	es->newblocks++;
	retval = io_channel_write_blk(fs->io, new_blk, 1, es->buf);

	if (blockcnt == 0)
		memset(es->buf, 0, fs->blocksize);

	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	*blocknr = new_blk;
	last_blk = new_blk;
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

	retval = ext2fs_block_iterate2(fs, journal_ino, BLOCK_FLAG_APPEND,
				       0, mkjournal_proc, &es);
	if (es.err) {
		retval = es.err;
		goto errout;
	}

	if ((retval = ext2fs_read_inode(fs, journal_ino, &inode)))
		goto errout;

	inode.i_size += fs->blocksize * size;
	inode.i_blocks += (fs->blocksize / 512) * es.newblocks;
	inode.i_mtime = inode.i_ctime = time(NULL);
	inode.i_links_count = 1;
	inode.i_mode = LINUX_S_IFREG | 0600;

	if ((retval = ext2fs_write_inode(fs, journal_ino, &inode)))
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
	int			fd, mount_flags, f;

	retval = ext2fs_check_mount_point(fs->device_name, &mount_flags,
					       jfile, sizeof(jfile)-10);
	if (retval)
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
		if (fstat(fd, &st) < 0)
			goto errout;

#if defined(HAVE_CHFLAGS) && defined(UF_NODUMP)
		retval = fchflags (fd, UF_NODUMP|UF_IMMUTABLE);
#else
#if HAVE_EXT2_IOCTLS
		f = EXT2_NODUMP_FL | EXT2_IMMUTABLE_FL;
		retval = ioctl(fd, EXT2_IOC_SETFLAGS, &f);
#endif
#endif
		if (retval)
			goto errout;

		close(fd);
		journal_ino = st.st_ino;
	} else {
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
	close(fd);
	return retval;
}

#ifdef DEBUG
main(int argc, char **argv)
{
	errcode_t	retval;
	char		*device_name;
	ext2_filsys	fs;

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

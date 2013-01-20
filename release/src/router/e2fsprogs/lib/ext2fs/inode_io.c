/*
 * inode_io.c --- This is allows an inode in an ext2 filesystem image
 * 	to be accessed via the I/O manager interface.
 *
 * Copyright (C) 2002 Theodore Ts'o.
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
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)

struct inode_private_data {
	int				magic;
	char				name[32];
	ext2_file_t			file;
	ext2_filsys			fs;
	ext2_ino_t 			ino;
	struct ext2_inode		inode;
	int				flags;
	struct inode_private_data	*next;
};

#define CHANNEL_HAS_INODE	0x8000

static struct inode_private_data *top_intern;
static int ino_unique = 0;

static errcode_t inode_open(const char *name, int flags, io_channel *channel);
static errcode_t inode_close(io_channel channel);
static errcode_t inode_set_blksize(io_channel channel, int blksize);
static errcode_t inode_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t inode_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t inode_flush(io_channel channel);
static errcode_t inode_write_byte(io_channel channel, unsigned long offset,
				int size, const void *data);
static errcode_t inode_read_blk64(io_channel channel,
				unsigned long long block, int count, void *data);
static errcode_t inode_write_blk64(io_channel channel,
				unsigned long long block, int count, const void *data);

static struct struct_io_manager struct_inode_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Inode I/O Manager",
	inode_open,
	inode_close,
	inode_set_blksize,
	inode_read_blk,
	inode_write_blk,
	inode_flush,
	inode_write_byte,
	NULL,
	NULL,
	inode_read_blk64,
	inode_write_blk64
};

io_manager inode_io_manager = &struct_inode_manager;

errcode_t ext2fs_inode_io_intern2(ext2_filsys fs, ext2_ino_t ino,
				  struct ext2_inode *inode,
				  char **name)
{
	struct inode_private_data 	*data;
	errcode_t			retval;

	if ((retval = ext2fs_get_mem(sizeof(struct inode_private_data),
				     &data)))
		return retval;
	data->magic = EXT2_ET_MAGIC_INODE_IO_CHANNEL;
	sprintf(data->name, "%u:%d", ino, ino_unique++);
	data->file = 0;
	data->fs = fs;
	data->ino = ino;
	data->flags = 0;
	if (inode) {
		memcpy(&data->inode, inode, sizeof(struct ext2_inode));
		data->flags |= CHANNEL_HAS_INODE;
	}
	data->next = top_intern;
	top_intern = data;
	*name = data->name;
	return 0;
}

errcode_t ext2fs_inode_io_intern(ext2_filsys fs, ext2_ino_t ino,
				 char **name)
{
	return ext2fs_inode_io_intern2(fs, ino, NULL, name);
}


static errcode_t inode_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct inode_private_data *prev, *data = NULL;
	errcode_t	retval;
	int		open_flags;

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;

	for (data = top_intern, prev = NULL; data;
	     prev = data, data = data->next)
		if (strcmp(name, data->name) == 0)
			break;
	if (!data)
		return ENOENT;
	if (prev)
		prev->next = data->next;
	else
		top_intern = data->next;

	retval = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (retval)
		goto cleanup;
	memset(io, 0, sizeof(struct struct_io_channel));

	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	io->manager = inode_io_manager;
	retval = ext2fs_get_mem(strlen(name)+1, &io->name);
	if (retval)
		goto cleanup;

	strcpy(io->name, name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

	open_flags = (flags & IO_FLAG_RW) ? EXT2_FILE_WRITE : 0;
	retval = ext2fs_file_open2(data->fs, data->ino,
				   (data->flags & CHANNEL_HAS_INODE) ?
				   &data->inode : 0, open_flags,
				   &data->file);
	if (retval)
		goto cleanup;

	*channel = io;
	return 0;

cleanup:
	if (data) {
		ext2fs_free_mem(&data);
	}
	if (io)
		ext2fs_free_mem(&io);
	return retval;
}

static errcode_t inode_close(io_channel channel)
{
	struct inode_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;

	retval = ext2fs_file_close(data->file);

	ext2fs_free_mem(&channel->private_data);
	if (channel->name)
		ext2fs_free_mem(&channel->name);
	ext2fs_free_mem(&channel);
	return retval;
}

static errcode_t inode_set_blksize(io_channel channel, int blksize)
{
	struct inode_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	channel->block_size = blksize;
	return 0;
}


static errcode_t inode_read_blk64(io_channel channel,
				unsigned long long block, int count, void *buf)
{
	struct inode_private_data *data;
	errcode_t	retval;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	if ((retval = ext2fs_file_lseek(data->file,
					block * channel->block_size,
					EXT2_SEEK_SET, 0)))
		return retval;

	count = (count < 0) ? -count : (count * channel->block_size);

	return ext2fs_file_read(data->file, buf, count, 0);
}

static errcode_t inode_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	return inode_read_blk64(channel, block, count, buf);
}

static errcode_t inode_write_blk64(io_channel channel,
				unsigned long long block, int count, const void *buf)
{
	struct inode_private_data *data;
	errcode_t	retval;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	if ((retval = ext2fs_file_lseek(data->file,
					block * channel->block_size,
					EXT2_SEEK_SET, 0)))
		return retval;

	count = (count < 0) ? -count : (count * channel->block_size);

	return ext2fs_file_write(data->file, buf, count, 0);
}

static errcode_t inode_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	return inode_write_blk64(channel, block, count, buf);
}

static errcode_t inode_write_byte(io_channel channel, unsigned long offset,
				 int size, const void *buf)
{
	struct inode_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	if ((retval = ext2fs_file_lseek(data->file, offset,
					EXT2_SEEK_SET, 0)))
		return retval;

	return ext2fs_file_write(data->file, buf, size, 0);
}

/*
 * Flush data buffers to disk.
 */
static errcode_t inode_flush(io_channel channel)
{
	struct inode_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct inode_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_INODE_IO_CHANNEL);

	return ext2fs_file_flush(data->file);
}


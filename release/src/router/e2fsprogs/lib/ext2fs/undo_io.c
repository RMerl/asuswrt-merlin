/*
 * undo_io.c --- This is the undo io manager that copies the old data that
 * copies the old data being overwritten into a tdb database
 *
 * Copyright IBM Corporation, 2007
 * Author Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

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
#ifdef __linux__
#include <sys/utsname.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "tdb.h"

#include "ext2_fs.h"
#include "ext2fs.h"

#ifdef __GNUC__
#define ATTR(x) __attribute__(x)
#else
#define ATTR(x)
#endif

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)

struct undo_private_data {
	int	magic;
	TDB_CONTEXT *tdb;
	char *tdb_file;

	/* The backing io channel */
	io_channel real;

	int tdb_data_size;
	int tdb_written;

	/* to support offset in unix I/O manager */
	ext2_loff_t offset;
};

static errcode_t undo_open(const char *name, int flags, io_channel *channel);
static errcode_t undo_close(io_channel channel);
static errcode_t undo_set_blksize(io_channel channel, int blksize);
static errcode_t undo_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t undo_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t undo_flush(io_channel channel);
static errcode_t undo_write_byte(io_channel channel, unsigned long offset,
				int size, const void *data);
static errcode_t undo_set_option(io_channel channel, const char *option,
				 const char *arg);

static struct struct_io_manager struct_undo_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Undo I/O Manager",
	undo_open,
	undo_close,
	undo_set_blksize,
	undo_read_blk,
	undo_write_blk,
	undo_flush,
	undo_write_byte,
	undo_set_option
};

io_manager undo_io_manager = &struct_undo_manager;
static io_manager undo_io_backing_manager ;
static char *tdb_file;
static int actual_size;

static unsigned char mtime_key[] = "filesystem MTIME";
static unsigned char blksize_key[] = "filesystem BLKSIZE";
static unsigned char uuid_key[] = "filesystem UUID";

errcode_t set_undo_io_backing_manager(io_manager manager)
{
	/*
	 * We may want to do some validation later
	 */
	undo_io_backing_manager = manager;
	return 0;
}

errcode_t set_undo_io_backup_file(char *file_name)
{
	tdb_file = strdup(file_name);

	if (tdb_file == NULL) {
		return EXT2_ET_NO_MEMORY;
	}

	return 0;
}

static errcode_t write_file_system_identity(io_channel undo_channel,
							TDB_CONTEXT *tdb)
{
	errcode_t retval;
	struct ext2_super_block super;
	TDB_DATA tdb_key, tdb_data;
	struct undo_private_data *data;
	io_channel channel;
	int block_size ;

	data = (struct undo_private_data *) undo_channel->private_data;
	channel = data->real;
	block_size = channel->block_size;

	io_channel_set_blksize(channel, SUPERBLOCK_OFFSET);
	retval = io_channel_read_blk(channel, 1, -SUPERBLOCK_SIZE, &super);
	if (retval)
		goto err_out;

	/* Write to tdb file in the file system byte order */
	tdb_key.dptr = mtime_key;
	tdb_key.dsize = sizeof(mtime_key);
	tdb_data.dptr = (unsigned char *) &(super.s_mtime);
	tdb_data.dsize = sizeof(super.s_mtime);

	retval = tdb_store(tdb, tdb_key, tdb_data, TDB_INSERT);
	if (retval == -1) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
		goto err_out;
	}

	tdb_key.dptr = uuid_key;
	tdb_key.dsize = sizeof(uuid_key);
	tdb_data.dptr = (unsigned char *)&(super.s_uuid);
	tdb_data.dsize = sizeof(super.s_uuid);

	retval = tdb_store(tdb, tdb_key, tdb_data, TDB_INSERT);
	if (retval == -1) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
	}

err_out:
	io_channel_set_blksize(channel, block_size);
	return retval;
}

static errcode_t write_block_size(TDB_CONTEXT *tdb, int block_size)
{
	errcode_t retval;
	TDB_DATA tdb_key, tdb_data;

	tdb_key.dptr = blksize_key;
	tdb_key.dsize = sizeof(blksize_key);
	tdb_data.dptr = (unsigned char *)&(block_size);
	tdb_data.dsize = sizeof(block_size);

	retval = tdb_store(tdb, tdb_key, tdb_data, TDB_INSERT);
	if (retval == -1) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
	}

	return retval;
}

static errcode_t undo_write_tdb(io_channel channel,
				unsigned long block, int count)

{
	int size, sz;
	unsigned long block_num, backing_blk_num;
	errcode_t retval = 0;
	ext2_loff_t offset;
	struct undo_private_data *data;
	TDB_DATA tdb_key, tdb_data;
	unsigned char *read_ptr;
	unsigned long end_block;

	data = (struct undo_private_data *) channel->private_data;

	if (data->tdb == NULL) {
		/*
		 * Transaction database not initialized
		 */
		return 0;
	}

	if (count == 1)
		size = channel->block_size;
	else {
		if (count < 0)
			size = -count;
		else
			size = count * channel->block_size;
	}
	/*
	 * Data is stored in tdb database as blocks of tdb_data_size size
	 * This helps in efficient lookup further.
	 *
	 * We divide the disk to blocks of tdb_data_size.
	 */
	offset = (block * channel->block_size) + data->offset ;
	block_num = offset / data->tdb_data_size;
	end_block = (offset + size) / data->tdb_data_size;

	tdb_transaction_start(data->tdb);
	while (block_num <= end_block ) {

		tdb_key.dptr = (unsigned char *)&block_num;
		tdb_key.dsize = sizeof(block_num);
		/*
		 * Check if we have the record already
		 */
		if (tdb_exists(data->tdb, tdb_key)) {
			/* Try the next block */
			block_num++;
			continue;
		}
		/*
		 * Read one block using the backing I/O manager
		 * The backing I/O manager block size may be
		 * different from the tdb_data_size.
		 * Also we need to recalcuate the block number with respect
		 * to the backing I/O manager.
		 */
		offset = block_num * data->tdb_data_size;
		backing_blk_num = (offset - data->offset) / channel->block_size;

		count = data->tdb_data_size +
				((offset - data->offset) % channel->block_size);
		retval = ext2fs_get_mem(count, &read_ptr);
		if (retval) {
			tdb_transaction_cancel(data->tdb);
			return retval;
		}

		memset(read_ptr, 0, count);
		actual_size = 0;
		if ((count % channel->block_size) == 0)
			sz = count / channel->block_size;
		else
			sz = -count;
		retval = io_channel_read_blk(data->real, backing_blk_num,
					     sz, read_ptr);
		if (retval) {
			if (retval != EXT2_ET_SHORT_READ) {
				free(read_ptr);
				tdb_transaction_cancel(data->tdb);
				return retval;
			}
			/*
			 * short read so update the record size
			 * accordingly
			 */
			tdb_data.dsize = actual_size;
		} else {
			tdb_data.dsize = data->tdb_data_size;
		}
		tdb_data.dptr = read_ptr +
				((offset - data->offset) % channel->block_size);
#ifdef DEBUG
		printf("Printing with key %ld data %x and size %d\n",
		       block_num,
		       tdb_data.dptr,
		       tdb_data.dsize);
#endif
		if (!data->tdb_written) {
			data->tdb_written = 1;
			/* Write the blocksize to tdb file */
			retval = write_block_size(data->tdb,
						  data->tdb_data_size);
			if (retval) {
				tdb_transaction_cancel(data->tdb);
				retval = EXT2_ET_TDB_ERR_IO;
				free(read_ptr);
				return retval;
			}
		}
		retval = tdb_store(data->tdb, tdb_key, tdb_data, TDB_INSERT);
		if (retval == -1) {
			/*
			 * TDB_ERR_EXISTS cannot happen because we
			 * have already verified it doesn't exist
			 */
			tdb_transaction_cancel(data->tdb);
			retval = EXT2_ET_TDB_ERR_IO;
			free(read_ptr);
			return retval;
		}
		free(read_ptr);
		/* Next block */
		block_num++;
	}
	tdb_transaction_commit(data->tdb);

	return retval;
}

static errcode_t undo_io_read_error(io_channel channel ATTR((unused)),
				    unsigned long block ATTR((unused)),
				    int count ATTR((unused)),
				    void *data ATTR((unused)),
				    size_t size ATTR((unused)),
				    int actual,
				    errcode_t error ATTR((unused)))
{
	actual_size = actual;
	return error;
}

static void undo_err_handler_init(io_channel channel)
{
	channel->read_error = undo_io_read_error;
}

static errcode_t undo_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct undo_private_data *data = NULL;
	errcode_t	retval;

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;
	retval = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (retval)
		return retval;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	retval = ext2fs_get_mem(sizeof(struct undo_private_data), &data);
	if (retval)
		goto cleanup;

	io->manager = undo_io_manager;
	retval = ext2fs_get_mem(strlen(name)+1, &io->name);
	if (retval)
		goto cleanup;

	strcpy(io->name, name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

	memset(data, 0, sizeof(struct undo_private_data));
	data->magic = EXT2_ET_MAGIC_UNIX_IO_CHANNEL;

	if (undo_io_backing_manager) {
		retval = undo_io_backing_manager->open(name, flags,
						       &data->real);
		if (retval)
			goto cleanup;
	} else {
		data->real = 0;
	}

	/* setup the tdb file */
	data->tdb = tdb_open(tdb_file, 0, TDB_CLEAR_IF_FIRST,
			     O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
	if (!data->tdb) {
		retval = errno;
		goto cleanup;
	}

	/*
	 * setup err handler for read so that we know
	 * when the backing manager fails do short read
	 */
	undo_err_handler_init(data->real);

	*channel = io;
	return 0;

cleanup:
	if (data->real)
		io_channel_close(data->real);
	if (data)
		ext2fs_free_mem(&data);
	if (io)
		ext2fs_free_mem(&io);
	return retval;
}

static errcode_t undo_close(io_channel channel)
{
	struct undo_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;
	/* Before closing write the file system identity */
	retval = write_file_system_identity(channel, data->tdb);
	if (retval)
		return retval;
	if (data->real)
		retval = io_channel_close(data->real);
	if (data->tdb)
		tdb_close(data->tdb);
	ext2fs_free_mem(&channel->private_data);
	if (channel->name)
		ext2fs_free_mem(&channel->name);
	ext2fs_free_mem(&channel);

	return retval;
}

static errcode_t undo_set_blksize(io_channel channel, int blksize)
{
	struct undo_private_data *data;
	errcode_t		retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (data->real)
		retval = io_channel_set_blksize(data->real, blksize);
	/*
	 * Set the block size used for tdb
	 */
	if (!data->tdb_data_size) {
		data->tdb_data_size = blksize;
	}
	channel->block_size = blksize;
	return retval;
}

static errcode_t undo_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	errcode_t	retval = 0;
	struct undo_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (data->real)
		retval = io_channel_read_blk(data->real, block, count, buf);

	return retval;
}

static errcode_t undo_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	struct undo_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);
	/*
	 * First write the existing content into database
	 */
	retval = undo_write_tdb(channel, block, count);
	if (retval)
		 return retval;
	if (data->real)
		retval = io_channel_write_blk(data->real, block, count, buf);

	return retval;
}

static errcode_t undo_write_byte(io_channel channel, unsigned long offset,
				 int size, const void *buf)
{
	struct undo_private_data *data;
	errcode_t	retval = 0;
	ext2_loff_t	location;
	unsigned long blk_num, count;;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	location = offset + data->offset;
	blk_num = location/channel->block_size;
	/*
	 * the size specified may spread across multiple blocks
	 * also make sure we account for the fact that block start
	 * offset for tdb is different from the backing I/O manager
	 * due to possible different block size
	 */
	count = (size + (location % channel->block_size) +
			channel->block_size  -1)/channel->block_size;
	retval = undo_write_tdb(channel, blk_num, count);
	if (retval)
		return retval;
	if (data->real && data->real->manager->write_byte)
		retval = io_channel_write_byte(data->real, offset, size, buf);

	return retval;
}

/*
 * Flush data buffers to disk.
 */
static errcode_t undo_flush(io_channel channel)
{
	errcode_t	retval = 0;
	struct undo_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (data->real)
		retval = io_channel_flush(data->real);

	return retval;
}

static errcode_t undo_set_option(io_channel channel, const char *option,
				 const char *arg)
{
	errcode_t	retval = 0;
	struct undo_private_data *data;
	unsigned long tmp;
	char *end;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct undo_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (!strcmp(option, "tdb_data_size")) {
		if (!arg)
			return EXT2_ET_INVALID_ARGUMENT;

		tmp = strtoul(arg, &end, 0);
		if (*end)
			return EXT2_ET_INVALID_ARGUMENT;
		if (!data->tdb_data_size || !data->tdb_written) {
			data->tdb_data_size = tmp;
		}
		return 0;
	}
	/*
	 * Need to support offset option to work with
	 * Unix I/O manager
	 */
	if (data->real && data->real->manager->set_option) {
		retval = data->real->manager->set_option(data->real,
							option, arg);
	}
	if (!retval && !strcmp(option, "offset")) {
		if (!arg)
			return EXT2_ET_INVALID_ARGUMENT;

		tmp = strtoul(arg, &end, 0);
		if (*end)
			return EXT2_ET_INVALID_ARGUMENT;
		data->offset = tmp;
	}
	return retval;
}

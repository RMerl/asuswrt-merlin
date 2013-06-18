/*
 * unix_io.c --- This is the Unix (well, really POSIX) implementation
 * 	of the I/O manager.
 *
 * Implements a one-block write-through cache.
 *
 * Includes support for Windows NT support under Cygwin.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 * 	2002 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
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
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
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
#if HAVE_LINUX_FALLOC_H
#include <linux/falloc.h>
#endif

#if defined(__linux__) && defined(_IO) && !defined(BLKROGET)
#define BLKROGET   _IO(0x12, 94) /* Get read-only status (0 = read_write).  */
#endif

#undef ALIGN_DEBUG

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * For checking structure magic numbers...
 */

#define EXT2_CHECK_MAGIC(struct, code) \
	  if ((struct)->magic != (code)) return (code)

struct unix_cache {
	char			*buf;
	unsigned long long	block;
	int			access_time;
	unsigned		dirty:1;
	unsigned		in_use:1;
};

#define CACHE_SIZE 8
#define WRITE_DIRECT_SIZE 4	/* Must be smaller than CACHE_SIZE */
#define READ_DIRECT_SIZE 4	/* Should be smaller than CACHE_SIZE */

struct unix_private_data {
	int	magic;
	int	dev;
	int	flags;
	int	align;
	int	access_time;
	ext2_loff_t offset;
	struct unix_cache cache[CACHE_SIZE];
	void	*bounce;
	struct struct_io_stats io_stats;
};

#define IS_ALIGNED(n, align) ((((unsigned long) n) & \
			       ((unsigned long) ((align)-1))) == 0)

static errcode_t unix_open(const char *name, int flags, io_channel *channel);
static errcode_t unix_close(io_channel channel);
static errcode_t unix_set_blksize(io_channel channel, int blksize);
static errcode_t unix_read_blk(io_channel channel, unsigned long block,
			       int count, void *data);
static errcode_t unix_write_blk(io_channel channel, unsigned long block,
				int count, const void *data);
static errcode_t unix_flush(io_channel channel);
static errcode_t unix_write_byte(io_channel channel, unsigned long offset,
				int size, const void *data);
static errcode_t unix_set_option(io_channel channel, const char *option,
				 const char *arg);
static errcode_t unix_get_stats(io_channel channel, io_stats *stats)
;
static void reuse_cache(io_channel channel, struct unix_private_data *data,
		 struct unix_cache *cache, unsigned long long block);
static errcode_t unix_read_blk64(io_channel channel, unsigned long long block,
			       int count, void *data);
static errcode_t unix_write_blk64(io_channel channel, unsigned long long block,
				int count, const void *data);
static errcode_t unix_discard(io_channel channel, unsigned long long block,
			      unsigned long long count);

static struct struct_io_manager struct_unix_manager = {
	EXT2_ET_MAGIC_IO_MANAGER,
	"Unix I/O Manager",
	unix_open,
	unix_close,
	unix_set_blksize,
	unix_read_blk,
	unix_write_blk,
	unix_flush,
	unix_write_byte,
	unix_set_option,
	unix_get_stats,
	unix_read_blk64,
	unix_write_blk64,
	unix_discard,
};

io_manager unix_io_manager = &struct_unix_manager;

static errcode_t unix_get_stats(io_channel channel, io_stats *stats)
{
	errcode_t 	retval = 0;

	struct unix_private_data *data;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (stats)
		*stats = &data->io_stats;

	return retval;
}

/*
 * Here are the raw I/O functions
 */
static errcode_t raw_read_blk(io_channel channel,
			      struct unix_private_data *data,
			      unsigned long long block,
			      int count, void *bufv)
{
	errcode_t	retval;
	ssize_t		size;
	ext2_loff_t	location;
	int		actual = 0;
	unsigned char	*buf = bufv;

	size = (count < 0) ? -count : count * channel->block_size;
	data->io_stats.bytes_read += size;
	location = ((ext2_loff_t) block * channel->block_size) + data->offset;
	if (ext2fs_llseek(data->dev, location, SEEK_SET) != location) {
		retval = errno ? errno : EXT2_ET_LLSEEK_FAILED;
		goto error_out;
	}
	if ((channel->align == 0) ||
	    (IS_ALIGNED(buf, channel->align) &&
	     IS_ALIGNED(size, channel->align))) {
		actual = read(data->dev, buf, size);
		if (actual != size) {
		short_read:
			if (actual < 0)
				actual = 0;
			retval = EXT2_ET_SHORT_READ;
			goto error_out;
		}
		return 0;
	}

#ifdef ALIGN_DEBUG
	printf("raw_read_blk: O_DIRECT fallback: %p %lu\n", buf,
	       (unsigned long) size);
#endif

	/*
	 * The buffer or size which we're trying to read isn't aligned
	 * to the O_DIRECT rules, so we need to do this the hard way...
	 */
	while (size > 0) {
		actual = read(data->dev, data->bounce, channel->block_size);
		if (actual != channel->block_size)
			goto short_read;
		actual = size;
		if (size > channel->block_size)
			actual = channel->block_size;
		memcpy(buf, data->bounce, actual);
		size -= actual;
		buf += actual;
	}
	return 0;

error_out:
	memset((char *) buf+actual, 0, size-actual);
	if (channel->read_error)
		retval = (channel->read_error)(channel, block, count, buf,
					       size, actual, retval);
	return retval;
}

static errcode_t raw_write_blk(io_channel channel,
			       struct unix_private_data *data,
			       unsigned long long block,
			       int count, const void *bufv)
{
	ssize_t		size;
	ext2_loff_t	location;
	int		actual = 0;
	errcode_t	retval;
	const unsigned char *buf = bufv;

	if (count == 1)
		size = channel->block_size;
	else {
		if (count < 0)
			size = -count;
		else
			size = count * channel->block_size;
	}
	data->io_stats.bytes_written += size;

	location = ((ext2_loff_t) block * channel->block_size) + data->offset;
	if (ext2fs_llseek(data->dev, location, SEEK_SET) != location) {
		retval = errno ? errno : EXT2_ET_LLSEEK_FAILED;
		goto error_out;
	}

	if ((channel->align == 0) ||
	    (IS_ALIGNED(buf, channel->align) &&
	     IS_ALIGNED(size, channel->align))) {
		actual = write(data->dev, buf, size);
		if (actual != size) {
		short_write:
			retval = EXT2_ET_SHORT_WRITE;
			goto error_out;
		}
		return 0;
	}

#ifdef ALIGN_DEBUG
	printf("raw_write_blk: O_DIRECT fallback: %p %lu\n", buf,
	       (unsigned long) size);
#endif
	/*
	 * The buffer or size which we're trying to write isn't aligned
	 * to the O_DIRECT rules, so we need to do this the hard way...
	 */
	while (size > 0) {
		if (size < channel->block_size) {
			actual = read(data->dev, data->bounce,
				      channel->block_size);
			if (actual != channel->block_size) {
				retval = EXT2_ET_SHORT_READ;
				goto error_out;
			}
		}
		actual = size;
		if (size > channel->block_size)
			actual = channel->block_size;
		memcpy(data->bounce, buf, actual);
		actual = write(data->dev, data->bounce, channel->block_size);
		if (actual != channel->block_size)
			goto short_write;
		size -= actual;
		buf += actual;
	}
	return 0;

error_out:
	if (channel->write_error)
		retval = (channel->write_error)(channel, block, count, buf,
						size, actual, retval);
	return retval;
}


/*
 * Here we implement the cache functions
 */

/* Allocate the cache buffers */
static errcode_t alloc_cache(io_channel channel,
			     struct unix_private_data *data)
{
	errcode_t		retval;
	struct unix_cache	*cache;
	int			i;

	data->access_time = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		cache->block = 0;
		cache->access_time = 0;
		cache->dirty = 0;
		cache->in_use = 0;
		if (cache->buf)
			ext2fs_free_mem(&cache->buf);
		retval = io_channel_alloc_buf(channel, 0, &cache->buf);
		if (retval)
			return retval;
	}
	if (channel->align) {
		if (data->bounce)
			ext2fs_free_mem(&data->bounce);
		retval = io_channel_alloc_buf(channel, 0, &data->bounce);
	}
	return retval;
}

/* Free the cache buffers */
static void free_cache(struct unix_private_data *data)
{
	struct unix_cache	*cache;
	int			i;

	data->access_time = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		cache->block = 0;
		cache->access_time = 0;
		cache->dirty = 0;
		cache->in_use = 0;
		if (cache->buf)
			ext2fs_free_mem(&cache->buf);
	}
	if (data->bounce)
		ext2fs_free_mem(&data->bounce);
}

#ifndef NO_IO_CACHE
/*
 * Try to find a block in the cache.  If the block is not found, and
 * eldest is a non-zero pointer, then fill in eldest with the cache
 * entry to that should be reused.
 */
static struct unix_cache *find_cached_block(struct unix_private_data *data,
					    unsigned long long block,
					    struct unix_cache **eldest)
{
	struct unix_cache	*cache, *unused_cache, *oldest_cache;
	int			i;

	unused_cache = oldest_cache = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		if (!cache->in_use) {
			if (!unused_cache)
				unused_cache = cache;
			continue;
		}
		if (cache->block == block) {
			cache->access_time = ++data->access_time;
			return cache;
		}
		if (!oldest_cache ||
		    (cache->access_time < oldest_cache->access_time))
			oldest_cache = cache;
	}
	if (eldest)
		*eldest = (unused_cache) ? unused_cache : oldest_cache;
	return 0;
}

/*
 * Reuse a particular cache entry for another block.
 */
static void reuse_cache(io_channel channel, struct unix_private_data *data,
		 struct unix_cache *cache, unsigned long long block)
{
	if (cache->dirty && cache->in_use)
		raw_write_blk(channel, data, cache->block, 1, cache->buf);

	cache->in_use = 1;
	cache->dirty = 0;
	cache->block = block;
	cache->access_time = ++data->access_time;
}

/*
 * Flush all of the blocks in the cache
 */
static errcode_t flush_cached_blocks(io_channel channel,
				     struct unix_private_data *data,
				     int invalidate)

{
	struct unix_cache	*cache;
	errcode_t		retval, retval2;
	int			i;

	retval2 = 0;
	for (i=0, cache = data->cache; i < CACHE_SIZE; i++, cache++) {
		if (!cache->in_use)
			continue;

		if (invalidate)
			cache->in_use = 0;

		if (!cache->dirty)
			continue;

		retval = raw_write_blk(channel, data,
				       cache->block, 1, cache->buf);
		if (retval)
			retval2 = retval;
		else
			cache->dirty = 0;
	}
	return retval2;
}
#endif /* NO_IO_CACHE */

#ifdef __linux__
#ifndef BLKDISCARDZEROES
#define BLKDISCARDZEROES _IO(0x12,124)
#endif
#endif

int ext2fs_open_file(const char *pathname, int flags, mode_t mode)
{
	if (mode)
#if defined(HAVE_OPEN64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
		return open64(pathname, flags, mode);
	else
		return open64(pathname, flags);
#else
		return open(pathname, flags, mode);
	else
		return open(pathname, flags);
#endif
}

int ext2fs_stat(const char *path, ext2fs_struct_stat *buf)
{
#if defined(HAVE_FSTAT64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
	return stat64(path, buf);
#else
	return stat(path, buf);
#endif
}

int ext2fs_fstat(int fd, ext2fs_struct_stat *buf)
{
#if defined(HAVE_FSTAT64) && !defined(__OSX_AVAILABLE_BUT_DEPRECATED)
	return fstat64(fd, buf);
#else
	return fstat(fd, buf);
#endif
}

static errcode_t unix_open(const char *name, int flags, io_channel *channel)
{
	io_channel	io = NULL;
	struct unix_private_data *data = NULL;
	errcode_t	retval;
	int		open_flags;
	int		f_nocache = 0;
	ext2fs_struct_stat st;
#ifdef __linux__
	struct 		utsname ut;
#endif

	if (name == 0)
		return EXT2_ET_BAD_DEVICE_NAME;
	retval = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (retval)
		goto cleanup;
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	retval = ext2fs_get_mem(sizeof(struct unix_private_data), &data);
	if (retval)
		goto cleanup;

	io->manager = unix_io_manager;
	retval = ext2fs_get_mem(strlen(name)+1, &io->name);
	if (retval)
		goto cleanup;

	strcpy(io->name, name);
	io->private_data = data;
	io->block_size = 1024;
	io->read_error = 0;
	io->write_error = 0;
	io->refcount = 1;

	memset(data, 0, sizeof(struct unix_private_data));
	data->magic = EXT2_ET_MAGIC_UNIX_IO_CHANNEL;
	data->io_stats.num_fields = 2;
	data->dev = -1;

	open_flags = (flags & IO_FLAG_RW) ? O_RDWR : O_RDONLY;
	if (flags & IO_FLAG_EXCLUSIVE)
		open_flags |= O_EXCL;
#if defined(O_DIRECT)
	if (flags & IO_FLAG_DIRECT_IO) {
		open_flags |= O_DIRECT;
		io->align = ext2fs_get_dio_alignment(data->dev);
	}
#elif defined(F_NOCACHE)
	if (flags & IO_FLAG_DIRECT_IO) {
		f_nocache = F_NOCACHE;
		io->align = 4096;
	}
#endif
	data->flags = flags;

	data->dev = ext2fs_open_file(io->name, open_flags, 0);
	if (data->dev < 0) {
		retval = errno;
		goto cleanup;
	}
	if (f_nocache) {
		if (fcntl(data->dev, f_nocache, 1) < 0) {
			retval = errno;
			goto cleanup;
		}
	}

	/*
	 * If the device is really a block device, then set the
	 * appropriate flag, otherwise we can set DISCARD_ZEROES flag
	 * because we are going to use punch hole instead of discard
	 * and if it succeed, subsequent read from sparse area returns
	 * zero.
	 */
	if (ext2fs_stat(io->name, &st) == 0) {
		if (S_ISBLK(st.st_mode))
			io->flags |= CHANNEL_FLAGS_BLOCK_DEVICE;
		else
			io->flags |= CHANNEL_FLAGS_DISCARD_ZEROES;
	}

#ifdef BLKDISCARDZEROES
	{
		int zeroes = 0;
		if (ioctl(data->dev, BLKDISCARDZEROES, &zeroes) == 0 &&
		    zeroes)
			io->flags |= CHANNEL_FLAGS_DISCARD_ZEROES;
	}
#endif

#if defined(__CYGWIN__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	/*
	 * Some operating systems require that the buffers be aligned,
	 * regardless of O_DIRECT
	 */
	if (!io->align)
		io->align = 512;
#endif


	if ((retval = alloc_cache(io, data)))
		goto cleanup;

#ifdef BLKROGET
	if (flags & IO_FLAG_RW) {
		int error;
		int readonly = 0;

		/* Is the block device actually writable? */
		error = ioctl(data->dev, BLKROGET, &readonly);
		if (!error && readonly) {
			retval = EPERM;
			goto cleanup;
		}
	}
#endif

#ifdef __linux__
#undef RLIM_INFINITY
#if (defined(__alpha__) || ((defined(__sparc__) || defined(__mips__)) && (SIZEOF_LONG == 4)))
#define RLIM_INFINITY	((unsigned long)(~0UL>>1))
#else
#define RLIM_INFINITY  (~0UL)
#endif
	/*
	 * Work around a bug in 2.4.10-2.4.18 kernels where writes to
	 * block devices are wrongly getting hit by the filesize
	 * limit.  This workaround isn't perfect, since it won't work
	 * if glibc wasn't built against 2.2 header files.  (Sigh.)
	 *
	 */
	if ((flags & IO_FLAG_RW) &&
	    (uname(&ut) == 0) &&
	    ((ut.release[0] == '2') && (ut.release[1] == '.') &&
	     (ut.release[2] == '4') && (ut.release[3] == '.') &&
	     (ut.release[4] == '1') && (ut.release[5] >= '0') &&
	     (ut.release[5] < '8')) &&
	    (ext2fs_stat(io->name, &st) == 0) &&
	    (S_ISBLK(st.st_mode))) {
		struct rlimit	rlim;

		rlim.rlim_cur = rlim.rlim_max = (unsigned long) RLIM_INFINITY;
		setrlimit(RLIMIT_FSIZE, &rlim);
		getrlimit(RLIMIT_FSIZE, &rlim);
		if (((unsigned long) rlim.rlim_cur) <
		    ((unsigned long) rlim.rlim_max)) {
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_FSIZE, &rlim);
		}
	}
#endif
	*channel = io;
	return 0;

cleanup:
	if (data) {
		if (data->dev >= 0)
			close(data->dev);
		free_cache(data);
		ext2fs_free_mem(&data);
	}
	if (io) {
		if (io->name) {
			ext2fs_free_mem(&io->name);
		}
		ext2fs_free_mem(&io);
	}
	return retval;
}

static errcode_t unix_close(io_channel channel)
{
	struct unix_private_data *data;
	errcode_t	retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (--channel->refcount > 0)
		return 0;

#ifndef NO_IO_CACHE
	retval = flush_cached_blocks(channel, data, 0);
#endif

	if (close(data->dev) < 0)
		retval = errno;
	free_cache(data);

	ext2fs_free_mem(&channel->private_data);
	if (channel->name)
		ext2fs_free_mem(&channel->name);
	ext2fs_free_mem(&channel);
	return retval;
}

static errcode_t unix_set_blksize(io_channel channel, int blksize)
{
	struct unix_private_data *data;
	errcode_t		retval;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (channel->block_size != blksize) {
#ifndef NO_IO_CACHE
		if ((retval = flush_cached_blocks(channel, data, 0)))
			return retval;
#endif

		channel->block_size = blksize;
		free_cache(data);
		if ((retval = alloc_cache(channel, data)))
			return retval;
	}
	return 0;
}


static errcode_t unix_read_blk64(io_channel channel, unsigned long long block,
			       int count, void *buf)
{
	struct unix_private_data *data;
	struct unix_cache *cache, *reuse[READ_DIRECT_SIZE];
	errcode_t	retval;
	char		*cp;
	int		i, j;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifdef NO_IO_CACHE
	return raw_read_blk(channel, data, block, count, buf);
#else
	/*
	 * If we're doing an odd-sized read or a very large read,
	 * flush out the cache and then do a direct read.
	 */
	if (count < 0 || count > WRITE_DIRECT_SIZE) {
		if ((retval = flush_cached_blocks(channel, data, 0)))
			return retval;
		return raw_read_blk(channel, data, block, count, buf);
	}

	cp = buf;
	while (count > 0) {
		/* If it's in the cache, use it! */
		if ((cache = find_cached_block(data, block, &reuse[0]))) {
#ifdef DEBUG
			printf("Using cached block %lu\n", block);
#endif
			memcpy(cp, cache->buf, channel->block_size);
			count--;
			block++;
			cp += channel->block_size;
			continue;
		}
		if (count == 1) {
			/*
			 * Special case where we read directly into the
			 * cache buffer; important in the O_DIRECT case
			 */
			cache = reuse[0];
			reuse_cache(channel, data, cache, block);
			if ((retval = raw_read_blk(channel, data, block, 1,
						   cache->buf))) {
				cache->in_use = 0;
				return retval;
			}
			memcpy(cp, cache->buf, channel->block_size);
			return 0;
		}

		/*
		 * Find the number of uncached blocks so we can do a
		 * single read request
		 */
		for (i=1; i < count; i++)
			if (find_cached_block(data, block+i, &reuse[i]))
				break;
#ifdef DEBUG
		printf("Reading %d blocks starting at %lu\n", i, block);
#endif
		if ((retval = raw_read_blk(channel, data, block, i, cp)))
			return retval;

		/* Save the results in the cache */
		for (j=0; j < i; j++) {
			count--;
			cache = reuse[j];
			reuse_cache(channel, data, cache, block++);
			memcpy(cache->buf, cp, channel->block_size);
			cp += channel->block_size;
		}
	}
	return 0;
#endif /* NO_IO_CACHE */
}

static errcode_t unix_read_blk(io_channel channel, unsigned long block,
			       int count, void *buf)
{
	return unix_read_blk64(channel, block, count, buf);
}

static errcode_t unix_write_blk64(io_channel channel, unsigned long long block,
				int count, const void *buf)
{
	struct unix_private_data *data;
	struct unix_cache *cache, *reuse;
	errcode_t	retval = 0;
	const char	*cp;
	int		writethrough;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifdef NO_IO_CACHE
	return raw_write_blk(channel, data, block, count, buf);
#else
	/*
	 * If we're doing an odd-sized write or a very large write,
	 * flush out the cache completely and then do a direct write.
	 */
	if (count < 0 || count > WRITE_DIRECT_SIZE) {
		if ((retval = flush_cached_blocks(channel, data, 1)))
			return retval;
		return raw_write_blk(channel, data, block, count, buf);
	}

	/*
	 * For a moderate-sized multi-block write, first force a write
	 * if we're in write-through cache mode, and then fill the
	 * cache with the blocks.
	 */
	writethrough = channel->flags & CHANNEL_FLAGS_WRITETHROUGH;
	if (writethrough)
		retval = raw_write_blk(channel, data, block, count, buf);

	cp = buf;
	while (count > 0) {
		cache = find_cached_block(data, block, &reuse);
		if (!cache) {
			cache = reuse;
			reuse_cache(channel, data, cache, block);
		}
		memcpy(cache->buf, cp, channel->block_size);
		cache->dirty = !writethrough;
		count--;
		block++;
		cp += channel->block_size;
	}
	return retval;
#endif /* NO_IO_CACHE */
}

static errcode_t unix_write_blk(io_channel channel, unsigned long block,
				int count, const void *buf)
{
	return unix_write_blk64(channel, block, count, buf);
}

static errcode_t unix_write_byte(io_channel channel, unsigned long offset,
				 int size, const void *buf)
{
	struct unix_private_data *data;
	errcode_t	retval = 0;
	ssize_t		actual;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (channel->align != 0) {
#ifdef ALIGN_DEBUG
		printf("unix_write_byte: O_DIRECT fallback\n");
#endif
		return EXT2_ET_UNIMPLEMENTED;
	}

#ifndef NO_IO_CACHE
	/*
	 * Flush out the cache completely
	 */
	if ((retval = flush_cached_blocks(channel, data, 1)))
		return retval;
#endif

	if (lseek(data->dev, offset + data->offset, SEEK_SET) < 0)
		return errno;

	actual = write(data->dev, buf, size);
	if (actual != size)
		return EXT2_ET_SHORT_WRITE;

	return 0;
}

/*
 * Flush data buffers to disk.
 */
static errcode_t unix_flush(io_channel channel)
{
	struct unix_private_data *data;
	errcode_t retval = 0;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifndef NO_IO_CACHE
	retval = flush_cached_blocks(channel, data, 0);
#endif
	fsync(data->dev);
	return retval;
}

static errcode_t unix_set_option(io_channel channel, const char *option,
				 const char *arg)
{
	struct unix_private_data *data;
	unsigned long long tmp;
	char *end;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (!strcmp(option, "offset")) {
		if (!arg)
			return EXT2_ET_INVALID_ARGUMENT;

		tmp = strtoull(arg, &end, 0);
		if (*end)
			return EXT2_ET_INVALID_ARGUMENT;
		data->offset = tmp;
		if (data->offset < 0)
			return EXT2_ET_INVALID_ARGUMENT;
		return 0;
	}
	return EXT2_ET_INVALID_ARGUMENT;
}

#if defined(__linux__) && !defined(BLKDISCARD)
#define BLKDISCARD		_IO(0x12,119)
#endif

static errcode_t unix_discard(io_channel channel, unsigned long long block,
			      unsigned long long count)
{
	struct unix_private_data *data;
	int		ret;

	EXT2_CHECK_MAGIC(channel, EXT2_ET_MAGIC_IO_CHANNEL);
	data = (struct unix_private_data *) channel->private_data;
	EXT2_CHECK_MAGIC(data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

	if (channel->flags & CHANNEL_FLAGS_BLOCK_DEVICE) {
#ifdef BLKDISCARD
		__uint64_t range[2];

		range[0] = (__uint64_t)(block) * channel->block_size;
		range[1] = (__uint64_t)(count) * channel->block_size;

		ret = ioctl(data->dev, BLKDISCARD, &range);
#else
		goto unimplemented;
#endif
	} else {
#if defined(HAVE_FALLOCATE) && defined(FALLOC_FL_PUNCH_HOLE)
		/*
		 * If we are not on block device, try to use punch hole
		 * to reclaim free space.
		 */
		ret = fallocate(data->dev,
				FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				(off_t)(block) * channel->block_size,
				(off_t)(count) * channel->block_size);
#else
		goto unimplemented;
#endif
	}
	if (ret < 0) {
		if (errno == EOPNOTSUPP)
			goto unimplemented;
		return errno;
	}
	return 0;
unimplemented:
	return EXT2_ET_UNIMPLEMENTED;
}

/*
 * icount.c --- an efficient inode count abstraction
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "ext2_fs.h"
#include "ext2fs.h"
#include "tdb.h"

/*
 * The data storage strategy used by icount relies on the observation
 * that most inode counts are either zero (for non-allocated inodes),
 * one (for most files), and only a few that are two or more
 * (directories and files that are linked to more than one directory).
 *
 * Also, e2fsck tends to load the icount data sequentially.
 *
 * So, we use an inode bitmap to indicate which inodes have a count of
 * one, and then use a sorted list to store the counts for inodes
 * which are greater than one.
 *
 * We also use an optional bitmap to indicate which inodes are already
 * in the sorted list, to speed up the use of this abstraction by
 * e2fsck's pass 2.  Pass 2 increments inode counts as it finds them,
 * so this extra bitmap avoids searching the sorted list to see if a
 * particular inode is on the sorted list already.
 */

struct ext2_icount_el {
	ext2_ino_t	ino;
	__u32		count;
};

struct ext2_icount {
	errcode_t		magic;
	ext2fs_inode_bitmap	single;
	ext2fs_inode_bitmap	multiple;
	ext2_ino_t		count;
	ext2_ino_t		size;
	ext2_ino_t		num_inodes;
	ext2_ino_t		cursor;
	struct ext2_icount_el	*list;
	struct ext2_icount_el	*last_lookup;
	char			*tdb_fn;
	TDB_CONTEXT		*tdb;
};

/*
 * We now use a 32-bit counter field because it doesn't cost us
 * anything extra for the in-memory data structure, due to alignment
 * padding.  But there's no point changing the interface if most of
 * the time we only care if the number is bigger than 65,000 or not.
 * So use the following translation function to return a 16-bit count.
 */
#define icount_16_xlate(x) (((x) > 65500) ? 65500 : (x))

void ext2fs_free_icount(ext2_icount_t icount)
{
	if (!icount)
		return;

	icount->magic = 0;
	if (icount->list)
		ext2fs_free_mem(&icount->list);
	if (icount->single)
		ext2fs_free_inode_bitmap(icount->single);
	if (icount->multiple)
		ext2fs_free_inode_bitmap(icount->multiple);
	if (icount->tdb)
		tdb_close(icount->tdb);
	if (icount->tdb_fn) {
		unlink(icount->tdb_fn);
		free(icount->tdb_fn);
	}

	ext2fs_free_mem(&icount);
}

static errcode_t alloc_icount(ext2_filsys fs, int flags, ext2_icount_t *ret)
{
	ext2_icount_t	icount;
	errcode_t	retval;

	*ret = 0;

	retval = ext2fs_get_mem(sizeof(struct ext2_icount), &icount);
	if (retval)
		return retval;
	memset(icount, 0, sizeof(struct ext2_icount));

	retval = ext2fs_allocate_inode_bitmap(fs, 0, &icount->single);
	if (retval)
		goto errout;

	if (flags & EXT2_ICOUNT_OPT_INCREMENT) {
		retval = ext2fs_allocate_inode_bitmap(fs, 0,
						      &icount->multiple);
		if (retval)
			goto errout;
	} else
		icount->multiple = 0;

	icount->magic = EXT2_ET_MAGIC_ICOUNT;
	icount->num_inodes = fs->super->s_inodes_count;

	*ret = icount;
	return 0;

errout:
	ext2fs_free_icount(icount);
	return(retval);
}

struct uuid {
	__u32	time_low;
	__u16	time_mid;
	__u16	time_hi_and_version;
	__u16	clock_seq;
	__u8	node[6];
};

static void unpack_uuid(void *in, struct uuid *uu)
{
	__u8	*ptr = in;
	__u32	tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_low = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_mid = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->time_hi_and_version = tmp;

	tmp = *ptr++;
	tmp = (tmp << 8) | *ptr++;
	uu->clock_seq = tmp;

	memcpy(uu->node, ptr, 6);
}

static void uuid_unparse(void *uu, char *out)
{
	struct uuid uuid;

	unpack_uuid(uu, &uuid);
	sprintf(out,
		"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
		uuid.clock_seq >> 8, uuid.clock_seq & 0xFF,
		uuid.node[0], uuid.node[1], uuid.node[2],
		uuid.node[3], uuid.node[4], uuid.node[5]);
}

errcode_t ext2fs_create_icount_tdb(ext2_filsys fs, char *tdb_dir,
				   int flags, ext2_icount_t *ret)
{
	ext2_icount_t	icount;
	errcode_t	retval;
	char 		*fn, uuid[40];
	int		fd;

	retval = alloc_icount(fs, flags,  &icount);
	if (retval)
		return retval;

	retval = ext2fs_get_mem(strlen(tdb_dir) + 64, &fn);
	if (retval)
		goto errout;
	uuid_unparse(fs->super->s_uuid, uuid);
	sprintf(fn, "%s/%s-icount-XXXXXX", tdb_dir, uuid);
	fd = mkstemp(fn);

	icount->tdb_fn = fn;
	icount->tdb = tdb_open(fn, 0, TDB_CLEAR_IF_FIRST,
			       O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (icount->tdb) {
		close(fd);
		*ret = icount;
		return 0;
	}

	retval = errno;
	close(fd);

errout:
	ext2fs_free_icount(icount);
	return(retval);
}

errcode_t ext2fs_create_icount2(ext2_filsys fs, int flags, unsigned int size,
				ext2_icount_t hint, ext2_icount_t *ret)
{
	ext2_icount_t	icount;
	errcode_t	retval;
	size_t		bytes;
	ext2_ino_t	i;

	if (hint) {
		EXT2_CHECK_MAGIC(hint, EXT2_ET_MAGIC_ICOUNT);
		if (hint->size > size)
			size = (size_t) hint->size;
	}

	retval = alloc_icount(fs, flags, &icount);
	if (retval)
		return retval;

	if (size) {
		icount->size = size;
	} else {
		/*
		 * Figure out how many special case inode counts we will
		 * have.  We know we will need one for each directory;
		 * we also need to reserve some extra room for file links
		 */
		retval = ext2fs_get_num_dirs(fs, &icount->size);
		if (retval)
			goto errout;
		icount->size += fs->super->s_inodes_count / 50;
	}

	bytes = (size_t) (icount->size * sizeof(struct ext2_icount_el));
#if 0
	printf("Icount allocated %u entries, %d bytes.\n",
	       icount->size, bytes);
#endif
	retval = ext2fs_get_array(icount->size, sizeof(struct ext2_icount_el),
			 &icount->list);
	if (retval)
		goto errout;
	memset(icount->list, 0, bytes);

	icount->count = 0;
	icount->cursor = 0;

	/*
	 * Populate the sorted list with those entries which were
	 * found in the hint icount (since those are ones which will
	 * likely need to be in the sorted list this time around).
	 */
	if (hint) {
		for (i=0; i < hint->count; i++)
			icount->list[i].ino = hint->list[i].ino;
		icount->count = hint->count;
	}

	*ret = icount;
	return 0;

errout:
	ext2fs_free_icount(icount);
	return(retval);
}

errcode_t ext2fs_create_icount(ext2_filsys fs, int flags,
			       unsigned int size,
			       ext2_icount_t *ret)
{
	return ext2fs_create_icount2(fs, flags, size, 0, ret);
}

/*
 * insert_icount_el() --- Insert a new entry into the sorted list at a
 * 	specified position.
 */
static struct ext2_icount_el *insert_icount_el(ext2_icount_t icount,
					    ext2_ino_t ino, int pos)
{
	struct ext2_icount_el 	*el;
	errcode_t		retval;
	ext2_ino_t		new_size = 0;
	int			num;

	if (icount->last_lookup && icount->last_lookup->ino == ino)
		return icount->last_lookup;

	if (icount->count >= icount->size) {
		if (icount->count) {
			new_size = icount->list[(unsigned)icount->count-1].ino;
			new_size = (ext2_ino_t) (icount->count *
				((float) icount->num_inodes / new_size));
		}
		if (new_size < (icount->size + 100))
			new_size = icount->size + 100;
#if 0
		printf("Reallocating icount %u entries...\n", new_size);
#endif
		retval = ext2fs_resize_mem((size_t) icount->size *
					   sizeof(struct ext2_icount_el),
					   (size_t) new_size *
					   sizeof(struct ext2_icount_el),
					   &icount->list);
		if (retval)
			return 0;
		icount->size = new_size;
	}
	num = (int) icount->count - pos;
	if (num < 0)
		return 0;	/* should never happen */
	if (num) {
		memmove(&icount->list[pos+1], &icount->list[pos],
			sizeof(struct ext2_icount_el) * num);
	}
	icount->count++;
	el = &icount->list[pos];
	el->count = 0;
	el->ino = ino;
	icount->last_lookup = el;
	return el;
}

/*
 * get_icount_el() --- given an inode number, try to find icount
 * 	information in the sorted list.  If the create flag is set,
 * 	and we can't find an entry, create one in the sorted list.
 */
static struct ext2_icount_el *get_icount_el(ext2_icount_t icount,
					    ext2_ino_t ino, int create)
{
	float	range;
	int	low, high, mid;
	ext2_ino_t	lowval, highval;

	if (!icount || !icount->list)
		return 0;

	if (create && ((icount->count == 0) ||
		       (ino > icount->list[(unsigned)icount->count-1].ino))) {
		return insert_icount_el(icount, ino, (unsigned) icount->count);
	}
	if (icount->count == 0)
		return 0;

	if (icount->cursor >= icount->count)
		icount->cursor = 0;
	if (ino == icount->list[icount->cursor].ino)
		return &icount->list[icount->cursor++];
#if 0
	printf("Non-cursor get_icount_el: %u\n", ino);
#endif
	low = 0;
	high = (int) icount->count-1;
	while (low <= high) {
#if 0
		mid = (low+high)/2;
#else
		if (low == high)
			mid = low;
		else {
			/* Interpolate for efficiency */
			lowval = icount->list[low].ino;
			highval = icount->list[high].ino;

			if (ino < lowval)
				range = 0;
			else if (ino > highval)
				range = 1;
			else {
				range = ((float) (ino - lowval)) /
					(highval - lowval);
				if (range > 0.9)
					range = 0.9;
				if (range < 0.1)
					range = 0.1;
			}
			mid = low + ((int) (range * (high-low)));
		}
#endif
		if (ino == icount->list[mid].ino) {
			icount->cursor = mid+1;
			return &icount->list[mid];
		}
		if (ino < icount->list[mid].ino)
			high = mid-1;
		else
			low = mid+1;
	}
	/*
	 * If we need to create a new entry, it should be right at
	 * low (where high will be left at low-1).
	 */
	if (create)
		return insert_icount_el(icount, ino, low);
	return 0;
}

static errcode_t set_inode_count(ext2_icount_t icount, ext2_ino_t ino,
				 __u32 count)
{
	struct ext2_icount_el 	*el;
	TDB_DATA key, data;

	if (icount->tdb) {
		key.dptr = (unsigned char *) &ino;
		key.dsize = sizeof(ext2_ino_t);
		data.dptr = (unsigned char *) &count;
		data.dsize = sizeof(__u32);
		if (count) {
			if (tdb_store(icount->tdb, key, data, TDB_REPLACE))
				return tdb_error(icount->tdb) +
					EXT2_ET_TDB_SUCCESS;
		} else {
			if (tdb_delete(icount->tdb, key))
				return tdb_error(icount->tdb) +
					EXT2_ET_TDB_SUCCESS;
		}
		return 0;
	}

	el = get_icount_el(icount, ino, 1);
	if (!el)
		return EXT2_ET_NO_MEMORY;

	el->count = count;
	return 0;
}

static errcode_t get_inode_count(ext2_icount_t icount, ext2_ino_t ino,
				 __u32 *count)
{
	struct ext2_icount_el 	*el;
	TDB_DATA key, data;

	if (icount->tdb) {
		key.dptr = (unsigned char *) &ino;
		key.dsize = sizeof(ext2_ino_t);

		data = tdb_fetch(icount->tdb, key);
		if (data.dptr == NULL) {
			*count = 0;
			return tdb_error(icount->tdb) + EXT2_ET_TDB_SUCCESS;
		}

		*count = *((__u32 *) data.dptr);
		free(data.dptr);
		return 0;
	}
	el = get_icount_el(icount, ino, 0);
	if (!el) {
		*count = 0;
		return ENOENT;
	}

	*count = el->count;
	return 0;
}

errcode_t ext2fs_icount_validate(ext2_icount_t icount, FILE *out)
{
	errcode_t	ret = 0;
	unsigned int	i;
	const char *bad = "bad icount";

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (icount->count > icount->size) {
		fprintf(out, "%s: count > size\n", bad);
		return EXT2_ET_INVALID_ARGUMENT;
	}
	for (i=1; i < icount->count; i++) {
		if (icount->list[i-1].ino >= icount->list[i].ino) {
			fprintf(out, "%s: list[%d].ino=%u, list[%d].ino=%u\n",
				bad, i-1, icount->list[i-1].ino,
				i, icount->list[i].ino);
			ret = EXT2_ET_INVALID_ARGUMENT;
		}
	}
	return ret;
}

errcode_t ext2fs_icount_fetch(ext2_icount_t icount, ext2_ino_t ino, __u16 *ret)
{
	__u32	val;
	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		*ret = 1;
		return 0;
	}
	if (icount->multiple &&
	    !ext2fs_test_inode_bitmap(icount->multiple, ino)) {
		*ret = 0;
		return 0;
	}
	get_inode_count(icount, ino, &val);
	*ret = icount_16_xlate(val);
	return 0;
}

errcode_t ext2fs_icount_increment(ext2_icount_t icount, ext2_ino_t ino,
				  __u16 *ret)
{
	__u32			curr_value;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		/*
		 * If the existing count is 1, then we know there is
		 * no entry in the list.
		 */
		if (set_inode_count(icount, ino, 2))
			return EXT2_ET_NO_MEMORY;
		curr_value = 2;
		ext2fs_unmark_inode_bitmap(icount->single, ino);
	} else if (icount->multiple) {
		/*
		 * The count is either zero or greater than 1; if the
		 * inode is set in icount->multiple, then there should
		 * be an entry in the list, so we need to fix it.
		 */
		if (ext2fs_test_inode_bitmap(icount->multiple, ino)) {
			get_inode_count(icount, ino, &curr_value);
			curr_value++;
			if (set_inode_count(icount, ino, curr_value))
				return EXT2_ET_NO_MEMORY;
		} else {
			/*
			 * The count was zero; mark the single bitmap
			 * and return.
			 */
			ext2fs_mark_inode_bitmap(icount->single, ino);
			if (ret)
				*ret = 1;
			return 0;
		}
	} else {
		/*
		 * The count is either zero or greater than 1; try to
		 * find an entry in the list to determine which.
		 */
		get_inode_count(icount, ino, &curr_value);
		curr_value++;
		if (set_inode_count(icount, ino, curr_value))
			return EXT2_ET_NO_MEMORY;
	}
	if (icount->multiple)
		ext2fs_mark_inode_bitmap(icount->multiple, ino);
	if (ret)
		*ret = icount_16_xlate(curr_value);
	return 0;
}

errcode_t ext2fs_icount_decrement(ext2_icount_t icount, ext2_ino_t ino,
				  __u16 *ret)
{
	__u32			curr_value;

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		ext2fs_unmark_inode_bitmap(icount->single, ino);
		if (icount->multiple)
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		else {
			set_inode_count(icount, ino, 0);
		}
		if (ret)
			*ret = 0;
		return 0;
	}

	if (icount->multiple &&
	    !ext2fs_test_inode_bitmap(icount->multiple, ino))
		return EXT2_ET_INVALID_ARGUMENT;

	get_inode_count(icount, ino, &curr_value);
	if (!curr_value)
		return EXT2_ET_INVALID_ARGUMENT;
	curr_value--;
	if (set_inode_count(icount, ino, curr_value))
		return EXT2_ET_NO_MEMORY;

	if (curr_value == 1)
		ext2fs_mark_inode_bitmap(icount->single, ino);
	if ((curr_value == 0) && icount->multiple)
		ext2fs_unmark_inode_bitmap(icount->multiple, ino);

	if (ret)
		*ret = icount_16_xlate(curr_value);
	return 0;
}

errcode_t ext2fs_icount_store(ext2_icount_t icount, ext2_ino_t ino,
			      __u16 count)
{
	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (count == 1) {
		ext2fs_mark_inode_bitmap(icount->single, ino);
		if (icount->multiple)
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		return 0;
	}
	if (count == 0) {
		ext2fs_unmark_inode_bitmap(icount->single, ino);
		if (icount->multiple) {
			/*
			 * If the icount->multiple bitmap is enabled,
			 * we can just clear both bitmaps and we're done
			 */
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		} else
			set_inode_count(icount, ino, 0);
		return 0;
	}

	if (set_inode_count(icount, ino, count))
		return EXT2_ET_NO_MEMORY;
	ext2fs_unmark_inode_bitmap(icount->single, ino);
	if (icount->multiple)
		ext2fs_mark_inode_bitmap(icount->multiple, ino);
	return 0;
}

ext2_ino_t ext2fs_get_icount_size(ext2_icount_t icount)
{
	if (!icount || icount->magic != EXT2_ET_MAGIC_ICOUNT)
		return 0;

	return icount->size;
}

#ifdef DEBUG

ext2_filsys	test_fs;
ext2_icount_t	icount;

#define EXIT		0x00
#define FETCH		0x01
#define STORE		0x02
#define INCREMENT	0x03
#define DECREMENT	0x04

struct test_program {
	int		cmd;
	ext2_ino_t	ino;
	__u16		arg;
	__u16		expected;
};

struct test_program prog[] = {
	{ STORE, 42, 42, 42 },
	{ STORE, 1,  1, 1 },
	{ STORE, 2,  2, 2 },
	{ STORE, 3,  3, 3 },
	{ STORE, 10, 1, 1 },
	{ STORE, 42, 0, 0 },
	{ INCREMENT, 5, 0, 1 },
	{ INCREMENT, 5, 0, 2 },
	{ INCREMENT, 5, 0, 3 },
	{ INCREMENT, 5, 0, 4 },
	{ DECREMENT, 5, 0, 3 },
	{ DECREMENT, 5, 0, 2 },
	{ DECREMENT, 5, 0, 1 },
	{ DECREMENT, 5, 0, 0 },
	{ FETCH, 10, 0, 1 },
	{ FETCH, 1, 0, 1 },
	{ FETCH, 2, 0, 2 },
	{ FETCH, 3, 0, 3 },
	{ INCREMENT, 1, 0, 2 },
	{ DECREMENT, 2, 0, 1 },
	{ DECREMENT, 2, 0, 0 },
	{ FETCH, 12, 0, 0 },
	{ EXIT, 0, 0, 0 }
};

struct test_program extended[] = {
	{ STORE, 1,  1, 1 },
	{ STORE, 2,  2, 2 },
	{ STORE, 3,  3, 3 },
	{ STORE, 4,  4, 4 },
	{ STORE, 5,  5, 5 },
	{ STORE, 6,  1, 1 },
	{ STORE, 7,  2, 2 },
	{ STORE, 8,  3, 3 },
	{ STORE, 9,  4, 4 },
	{ STORE, 10, 5, 5 },
	{ STORE, 11, 1, 1 },
	{ STORE, 12, 2, 2 },
	{ STORE, 13, 3, 3 },
	{ STORE, 14, 4, 4 },
	{ STORE, 15, 5, 5 },
	{ STORE, 16, 1, 1 },
	{ STORE, 17, 2, 2 },
	{ STORE, 18, 3, 3 },
	{ STORE, 19, 4, 4 },
	{ STORE, 20, 5, 5 },
	{ STORE, 21, 1, 1 },
	{ STORE, 22, 2, 2 },
	{ STORE, 23, 3, 3 },
	{ STORE, 24, 4, 4 },
	{ STORE, 25, 5, 5 },
	{ STORE, 26, 1, 1 },
	{ STORE, 27, 2, 2 },
	{ STORE, 28, 3, 3 },
	{ STORE, 29, 4, 4 },
	{ STORE, 30, 5, 5 },
	{ EXIT, 0, 0, 0 }
};

/*
 * Setup the variables for doing the inode scan test.
 */
static void setup(void)
{
	errcode_t	retval;
	struct ext2_super_block param;

	initialize_ext2_error_table();

	memset(&param, 0, sizeof(param));
	param.s_blocks_count = 12000;

	retval = ext2fs_initialize("test fs", 0, &param,
				   test_io_manager, &test_fs);
	if (retval) {
		com_err("setup", retval,
			"while initializing filesystem");
		exit(1);
	}
	retval = ext2fs_allocate_tables(test_fs);
	if (retval) {
		com_err("setup", retval,
			"while allocating tables for test filesystem");
		exit(1);
	}
}

int run_test(int flags, int size, char *dir, struct test_program *prog)
{
	errcode_t	retval;
	ext2_icount_t	icount;
	struct test_program *pc;
	__u16		result;
	int		problem = 0;

	if (dir) {
		retval = ext2fs_create_icount_tdb(test_fs, dir,
						  flags, &icount);
		if (retval) {
			com_err("run_test", retval,
				"while creating icount using tdb");
			exit(1);
		}
	} else {
		retval = ext2fs_create_icount2(test_fs, flags, size, 0,
					       &icount);
		if (retval) {
			com_err("run_test", retval, "while creating icount");
			exit(1);
		}
	}
	for (pc = prog; pc->cmd != EXIT; pc++) {
		switch (pc->cmd) {
		case FETCH:
			printf("icount_fetch(%u) = ", pc->ino);
			break;
		case STORE:
			retval = ext2fs_icount_store(icount, pc->ino, pc->arg);
			if (retval) {
				com_err("run_test", retval,
					"while calling icount_store");
				exit(1);
			}
			printf("icount_store(%u, %u) = ", pc->ino, pc->arg);
			break;
		case INCREMENT:
			retval = ext2fs_icount_increment(icount, pc->ino, 0);
			if (retval) {
				com_err("run_test", retval,
					"while calling icount_increment");
				exit(1);
			}
			printf("icount_increment(%u) = ", pc->ino);
			break;
		case DECREMENT:
			retval = ext2fs_icount_decrement(icount, pc->ino, 0);
			if (retval) {
				com_err("run_test", retval,
					"while calling icount_decrement");
				exit(1);
			}
			printf("icount_decrement(%u) = ", pc->ino);
			break;
		}
		retval = ext2fs_icount_fetch(icount, pc->ino, &result);
		if (retval) {
			com_err("run_test", retval,
				"while calling icount_fetch");
			exit(1);
		}
		printf("%u (%s)\n", result, (result == pc->expected) ?
		       "OK" : "NOT OK");
		if (result != pc->expected)
			problem++;
	}
	printf("icount size is %u\n", ext2fs_get_icount_size(icount));
	retval = ext2fs_icount_validate(icount, stdout);
	if (retval) {
		com_err("run_test", retval, "while calling icount_validate");
		exit(1);
	}
	ext2fs_free_icount(icount);
	return problem;
}


int main(int argc, char **argv)
{
	int failed = 0;

	setup();
	printf("Standard icount run:\n");
	failed += run_test(0, 0, 0, prog);
	printf("\nMultiple bitmap test:\n");
	failed += run_test(EXT2_ICOUNT_OPT_INCREMENT, 0, 0, prog);
	printf("\nResizing icount:\n");
	failed += run_test(0, 3, 0, extended);
	printf("\nStandard icount run with tdb:\n");
	failed += run_test(0, 0, ".", prog);
	printf("\nMultiple bitmap test with tdb:\n");
	failed += run_test(EXT2_ICOUNT_OPT_INCREMENT, 0, ".", prog);
	if (failed)
		printf("FAILED!\n");
	return failed;
}
#endif

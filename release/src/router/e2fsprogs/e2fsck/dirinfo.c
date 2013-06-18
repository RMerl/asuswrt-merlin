/*
 * dirinfo.c --- maintains the directory information table for e2fsck.
 *
 * Copyright (C) 1993 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#undef DIRINFO_DEBUG

#include "config.h"
#include "e2fsck.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "uuid/uuid.h"

#include <ext2fs/tdb.h>

struct dir_info_db {
	int		count;
	int		size;
	struct dir_info *array;
	struct dir_info *last_lookup;
	char		*tdb_fn;
	TDB_CONTEXT	*tdb;
};

struct dir_info_iter {
	int	i;
	TDB_DATA	tdb_iter;
};

struct dir_info_ent {
	ext2_ino_t		dotdot;	/* Parent according to '..' */
	ext2_ino_t		parent; /* Parent according to treewalk */
};


static void e2fsck_put_dir_info(e2fsck_t ctx, struct dir_info *dir);

static void setup_tdb(e2fsck_t ctx, ext2_ino_t num_dirs)
{
	struct dir_info_db	*db = ctx->dir_info;
	unsigned int		threshold;
	errcode_t		retval;
	char			*tdb_dir, uuid[40];
	int			fd, enable;

	profile_get_string(ctx->profile, "scratch_files", "directory", 0, 0,
			   &tdb_dir);
	profile_get_uint(ctx->profile, "scratch_files",
			 "numdirs_threshold", 0, 0, &threshold);
	profile_get_boolean(ctx->profile, "scratch_files",
			    "dirinfo", 0, 1, &enable);

	if (!enable || !tdb_dir || access(tdb_dir, W_OK) ||
	    (threshold && num_dirs <= threshold))
		return;

	retval = ext2fs_get_mem(strlen(tdb_dir) + 64, &db->tdb_fn);
	if (retval)
		return;

	uuid_unparse(ctx->fs->super->s_uuid, uuid);
	sprintf(db->tdb_fn, "%s/%s-dirinfo-XXXXXX", tdb_dir, uuid);
	fd = mkstemp(db->tdb_fn);
	if (fd < 0) {
		db->tdb = NULL;
		return;
	}

	if (num_dirs < 99991)
		num_dirs = 99991; /* largest 5 digit prime */

	db->tdb = tdb_open(db->tdb_fn, num_dirs, TDB_NOLOCK | TDB_NOSYNC,
			   O_RDWR | O_CREAT | O_TRUNC, 0600);
	close(fd);
}

static void setup_db(e2fsck_t ctx)
{
	struct dir_info_db	*db;
	ext2_ino_t		num_dirs;
	errcode_t		retval;

	db = (struct dir_info_db *)
		e2fsck_allocate_memory(ctx, sizeof(struct dir_info_db),
				       "directory map db");
	db->count = db->size = 0;
	db->array = 0;

	ctx->dir_info = db;

	retval = ext2fs_get_num_dirs(ctx->fs, &num_dirs);
	if (retval)
		num_dirs = 1024;	/* Guess */

	setup_tdb(ctx, num_dirs);

	if (db->tdb) {
#ifdef DIRINFO_DEBUG
		printf("Note: using tdb!\n");
#endif
		return;
	}

	db->size = num_dirs + 10;
	db->array  = (struct dir_info *)
		e2fsck_allocate_memory(ctx, db->size
				       * sizeof (struct dir_info),
				       "directory map");
}

/*
 * This subroutine is called during pass1 to create a directory info
 * entry.  During pass1, the passed-in parent is 0; it will get filled
 * in during pass2.
 */
void e2fsck_add_dir_info(e2fsck_t ctx, ext2_ino_t ino, ext2_ino_t parent)
{
	struct dir_info_db 	*db;
	struct dir_info 	*dir, ent;
	int			i, j;
	errcode_t		retval;
	unsigned long		old_size;

#ifdef DIRINFO_DEBUG
	printf("add_dir_info for inode (%lu, %lu)...\n", ino, parent);
#endif
	if (!ctx->dir_info)
		setup_db(ctx);
	db = ctx->dir_info;

	if (ctx->dir_info->count >= ctx->dir_info->size) {
		old_size = ctx->dir_info->size * sizeof(struct dir_info);
		ctx->dir_info->size += 10;
		retval = ext2fs_resize_mem(old_size, ctx->dir_info->size *
					   sizeof(struct dir_info),
					   &ctx->dir_info->array);
		if (retval) {
			ctx->dir_info->size -= 10;
			return;
		}
	}

	ent.ino = ino;
	ent.parent = parent;
	ent.dotdot = parent;

	if (db->tdb) {
		e2fsck_put_dir_info(ctx, &ent);
		return;
	}

	/*
	 * Normally, add_dir_info is called with each inode in
	 * sequential order; but once in a while (like when pass 3
	 * needs to recreate the root directory or lost+found
	 * directory) it is called out of order.  In those cases, we
	 * need to move the dir_info entries down to make room, since
	 * the dir_info array needs to be sorted by inode number for
	 * get_dir_info()'s sake.
	 */
	if (ctx->dir_info->count &&
	    ctx->dir_info->array[ctx->dir_info->count-1].ino >= ino) {
		for (i = ctx->dir_info->count-1; i > 0; i--)
			if (ctx->dir_info->array[i-1].ino < ino)
				break;
		dir = &ctx->dir_info->array[i];
		if (dir->ino != ino)
			for (j = ctx->dir_info->count++; j > i; j--)
				ctx->dir_info->array[j] = ctx->dir_info->array[j-1];
	} else
		dir = &ctx->dir_info->array[ctx->dir_info->count++];

	dir->ino = ino;
	dir->dotdot = parent;
	dir->parent = parent;
}

/*
 * get_dir_info() --- given an inode number, try to find the directory
 * information entry for it.
 */
static struct dir_info *e2fsck_get_dir_info(e2fsck_t ctx, ext2_ino_t ino)
{
	struct dir_info_db	*db = ctx->dir_info;
	int			low, high, mid;
	struct dir_info_ent	*buf;
	static struct dir_info	ret_dir_info;

	if (!db)
		return 0;

#ifdef DIRINFO_DEBUG
	printf("e2fsck_get_dir_info %d...", ino);
#endif

	if (db->tdb) {
		TDB_DATA key, data;

		key.dptr = (unsigned char *) &ino;
		key.dsize = sizeof(ext2_ino_t);

		data = tdb_fetch(db->tdb, key);
		if (!data.dptr) {
			if (tdb_error(db->tdb) != TDB_ERR_NOEXIST)
				printf("fetch failed: %s\n",
				       tdb_errorstr(db->tdb));
			return 0;
		}

		buf = (struct dir_info_ent *) data.dptr;
		ret_dir_info.ino = ino;
		ret_dir_info.dotdot = buf->dotdot;
		ret_dir_info.parent = buf->parent;
#ifdef DIRINFO_DEBUG
		printf("(%d,%d,%d)\n", ino, buf->dotdot, buf->parent);
#endif
		free(data.dptr);
		return &ret_dir_info;
	}

	if (db->last_lookup && db->last_lookup->ino == ino)
		return db->last_lookup;

	low = 0;
	high = ctx->dir_info->count-1;
	if (ino == ctx->dir_info->array[low].ino) {
#ifdef DIRINFO_DEBUG
		printf("(%d,%d,%d)\n", ino,
		       ctx->dir_info->array[low].dotdot,
		       ctx->dir_info->array[low].parent);
#endif
		return &ctx->dir_info->array[low];
	}
	if (ino == ctx->dir_info->array[high].ino) {
#ifdef DIRINFO_DEBUG
		printf("(%d,%d,%d)\n", ino,
		       ctx->dir_info->array[high].dotdot,
		       ctx->dir_info->array[high].parent);
#endif
		return &ctx->dir_info->array[high];
	}

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (ino == ctx->dir_info->array[mid].ino) {
#ifdef DIRINFO_DEBUG
			printf("(%d,%d,%d)\n", ino,
			       ctx->dir_info->array[mid].dotdot,
			       ctx->dir_info->array[mid].parent);
#endif
			return &ctx->dir_info->array[mid];
		}
		if (ino < ctx->dir_info->array[mid].ino)
			high = mid;
		else
			low = mid;
	}
	return 0;
}

static void e2fsck_put_dir_info(e2fsck_t ctx, struct dir_info *dir)
{
	struct dir_info_db	*db = ctx->dir_info;
	struct dir_info_ent	buf;
	TDB_DATA		key, data;

#ifdef DIRINFO_DEBUG
	printf("e2fsck_put_dir_info (%d, %d, %d)...", dir->ino, dir->dotdot,
	       dir->parent);
#endif

	if (!db->tdb)
		return;

	buf.parent = dir->parent;
	buf.dotdot = dir->dotdot;

	key.dptr = (unsigned char *) &dir->ino;
	key.dsize = sizeof(ext2_ino_t);
	data.dptr = (unsigned char *) &buf;
	data.dsize = sizeof(buf);

	if (tdb_store(db->tdb, key, data, TDB_REPLACE) == -1) {
		printf("store failed: %s\n", tdb_errorstr(db->tdb));
	}
	return;
}

/*
 * Free the dir_info structure when it isn't needed any more.
 */
void e2fsck_free_dir_info(e2fsck_t ctx)
{
	if (ctx->dir_info) {
		if (ctx->dir_info->tdb)
			tdb_close(ctx->dir_info->tdb);
		if (ctx->dir_info->tdb_fn) {
			unlink(ctx->dir_info->tdb_fn);
			free(ctx->dir_info->tdb_fn);
		}
		if (ctx->dir_info->array)
			ext2fs_free_mem(&ctx->dir_info->array);
		ctx->dir_info->array = 0;
		ctx->dir_info->size = 0;
		ctx->dir_info->count = 0;
		ext2fs_free_mem(&ctx->dir_info);
		ctx->dir_info = 0;
	}
}

/*
 * Return the count of number of directories in the dir_info structure
 */
int e2fsck_get_num_dirinfo(e2fsck_t ctx)
{
	return ctx->dir_info ? ctx->dir_info->count : 0;
}

extern struct dir_info_iter *e2fsck_dir_info_iter_begin(e2fsck_t ctx)
{
	struct dir_info_iter *iter;
	struct dir_info_db *db = ctx->dir_info;

	iter = e2fsck_allocate_memory(ctx, sizeof(struct dir_info_iter),
				      "dir_info iterator");

	if (db->tdb)
		iter->tdb_iter = tdb_firstkey(db->tdb);

	return iter;
}

extern void e2fsck_dir_info_iter_end(e2fsck_t ctx EXT2FS_ATTR((unused)),
				     struct dir_info_iter *iter)
{
	free(iter->tdb_iter.dptr);
	ext2fs_free_mem(&iter);
}

/*
 * A simple interator function
 */
struct dir_info *e2fsck_dir_info_iter(e2fsck_t ctx, struct dir_info_iter *iter)
{
	TDB_DATA data, key;
	struct dir_info_db *db = ctx->dir_info;
	struct dir_info_ent *buf;
	static struct dir_info ret_dir_info;

	if (!ctx->dir_info || !iter)
		return 0;

	if (db->tdb) {
		if (iter->tdb_iter.dptr == 0)
			return 0;
		key = iter->tdb_iter;
		data = tdb_fetch(db->tdb, key);
		if (!data.dptr) {
			printf("iter fetch failed: %s\n",
			       tdb_errorstr(db->tdb));
			return 0;
		}
		buf = (struct dir_info_ent *) data.dptr;
		ret_dir_info.ino = *((ext2_ino_t *) iter->tdb_iter.dptr);
		ret_dir_info.dotdot = buf->dotdot;
		ret_dir_info.parent = buf->parent;
		iter->tdb_iter = tdb_nextkey(db->tdb, key);
		free(key.dptr);
		free(data.dptr);
		return &ret_dir_info;
	}

	if (iter->i >= ctx->dir_info->count)
		return 0;

#ifdef DIRINFO_DEBUG
	printf("iter(%d, %d, %d)...", ctx->dir_info->array[iter->i].ino,
	       ctx->dir_info->array[iter->i].dotdot,
	       ctx->dir_info->array[iter->i].parent);
#endif
	ctx->dir_info->last_lookup = ctx->dir_info->array + iter->i++;
	return(ctx->dir_info->last_lookup);
}

/*
 * This function only sets the parent pointer, and requires that
 * dirinfo structure has already been created.
 */
int e2fsck_dir_info_set_parent(e2fsck_t ctx, ext2_ino_t ino,
			       ext2_ino_t parent)
{
	struct dir_info *p;

	p = e2fsck_get_dir_info(ctx, ino);
	if (!p)
		return 1;
	p->parent = parent;
	e2fsck_put_dir_info(ctx, p);
	return 0;
}

/*
 * This function only sets the dot dot pointer, and requires that
 * dirinfo structure has already been created.
 */
int e2fsck_dir_info_set_dotdot(e2fsck_t ctx, ext2_ino_t ino,
			       ext2_ino_t dotdot)
{
	struct dir_info *p;

	p = e2fsck_get_dir_info(ctx, ino);
	if (!p)
		return 1;
	p->dotdot = dotdot;
	e2fsck_put_dir_info(ctx, p);
	return 0;
}

/*
 * This function only sets the parent pointer, and requires that
 * dirinfo structure has already been created.
 */
int e2fsck_dir_info_get_parent(e2fsck_t ctx, ext2_ino_t ino,
			       ext2_ino_t *parent)
{
	struct dir_info *p;

	p = e2fsck_get_dir_info(ctx, ino);
	if (!p)
		return 1;
	*parent = p->parent;
	return 0;
}

/*
 * This function only sets the dot dot pointer, and requires that
 * dirinfo structure has already been created.
 */
int e2fsck_dir_info_get_dotdot(e2fsck_t ctx, ext2_ino_t ino,
			       ext2_ino_t *dotdot)
{
	struct dir_info *p;

	p = e2fsck_get_dir_info(ctx, ino);
	if (!p)
		return 1;
	*dotdot = p->dotdot;
	return 0;
}


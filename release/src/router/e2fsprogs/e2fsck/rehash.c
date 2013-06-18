/*
 * rehash.c --- rebuild hash tree directories
 *
 * Copyright (C) 2002 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * This algorithm is designed for simplicity of implementation and to
 * pack the directory as much as possible.  It however requires twice
 * as much memory as the size of the directory.  The maximum size
 * directory supported using a 4k blocksize is roughly a gigabyte, and
 * so there may very well be problems with machines that don't have
 * virtual memory, and obscenely large directories.
 *
 * An alternate algorithm which is much more disk intensive could be
 * written, and probably will need to be written in the future.  The
 * design goals of such an algorithm are: (a) use (roughly) constant
 * amounts of memory, no matter how large the directory, (b) the
 * directory must be safe at all times, even if e2fsck is interrupted
 * in the middle, (c) we must use minimal amounts of extra disk
 * blocks.  This pretty much requires an incremental approach, where
 * we are reading from one part of the directory, and inserting into
 * the front half.  So the algorithm will have to keep track of a
 * moving block boundary between the new tree and the old tree, and
 * files will need to be moved from the old directory and inserted
 * into the new tree.  If the new directory requires space which isn't
 * yet available, blocks from the beginning part of the old directory
 * may need to be moved to the end of the directory to make room for
 * the new tree:
 *
 *    --------------------------------------------------------
 *    |  new tree   |        | old tree                      |
 *    --------------------------------------------------------
 *                  ^ ptr    ^ptr
 *                tail new   head old
 *
 * This is going to be a pain in the tuckus to implement, and will
 * require a lot more disk accesses.  So I'm going to skip it for now;
 * it's only really going to be an issue for really, really big
 * filesystems (when we reach the level of tens of millions of files
 * in a single directory).  It will probably be easier to simply
 * require that e2fsck use VM first.
 */

#include "config.h"
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "e2fsck.h"
#include "problem.h"

struct fill_dir_struct {
	char *buf;
	struct ext2_inode *inode;
	int err;
	e2fsck_t ctx;
	struct hash_entry *harray;
	int max_array, num_array;
	int dir_size;
	int compress;
	ino_t parent;
};

struct hash_entry {
	ext2_dirhash_t	hash;
	ext2_dirhash_t	minor_hash;
	ino_t		ino;
	struct ext2_dir_entry	*dir;
};

struct out_dir {
	int		num;
	int		max;
	char		*buf;
	ext2_dirhash_t	*hashes;
};

static int fill_dir_block(ext2_filsys fs,
			  blk64_t *block_nr,
			  e2_blkcnt_t blockcnt,
			  blk64_t ref_block EXT2FS_ATTR((unused)),
			  int ref_offset EXT2FS_ATTR((unused)),
			  void *priv_data)
{
	struct fill_dir_struct	*fd = (struct fill_dir_struct *) priv_data;
	struct hash_entry 	*new_array, *ent;
	struct ext2_dir_entry 	*dirent;
	char			*dir;
	unsigned int		offset, dir_offset, rec_len;
	int			hash_alg;

	if (blockcnt < 0)
		return 0;

	offset = blockcnt * fs->blocksize;
	if (offset + fs->blocksize > fd->inode->i_size) {
		fd->err = EXT2_ET_DIR_CORRUPTED;
		return BLOCK_ABORT;
	}
	dir = (fd->buf+offset);
	if (HOLE_BLKADDR(*block_nr)) {
		memset(dir, 0, fs->blocksize);
		dirent = (struct ext2_dir_entry *) dir;
		(void) ext2fs_set_rec_len(fs, fs->blocksize, dirent);
	} else {
		fd->err = ext2fs_read_dir_block3(fs, *block_nr, dir, 0);
		if (fd->err)
			return BLOCK_ABORT;
	}
	hash_alg = fs->super->s_def_hash_version;
	if ((hash_alg <= EXT2_HASH_TEA) &&
	    (fs->super->s_flags & EXT2_FLAGS_UNSIGNED_HASH))
		hash_alg += 3;
	/* While the directory block is "hot", index it. */
	dir_offset = 0;
	while (dir_offset < fs->blocksize) {
		dirent = (struct ext2_dir_entry *) (dir + dir_offset);
		(void) ext2fs_get_rec_len(fs, dirent, &rec_len);
		if (((dir_offset + rec_len) > fs->blocksize) ||
		    (rec_len < 8) ||
		    ((rec_len % 4) != 0) ||
		    (((dirent->name_len & 0xFF)+8) > rec_len)) {
			fd->err = EXT2_ET_DIR_CORRUPTED;
			return BLOCK_ABORT;
		}
		dir_offset += rec_len;
		if (dirent->inode == 0)
			continue;
		if (!fd->compress && ((dirent->name_len&0xFF) == 1) &&
		    (dirent->name[0] == '.'))
			continue;
		if (!fd->compress && ((dirent->name_len&0xFF) == 2) &&
		    (dirent->name[0] == '.') && (dirent->name[1] == '.')) {
			fd->parent = dirent->inode;
			continue;
		}
		if (fd->num_array >= fd->max_array) {
			new_array = realloc(fd->harray,
			    sizeof(struct hash_entry) * (fd->max_array+500));
			if (!new_array) {
				fd->err = ENOMEM;
				return BLOCK_ABORT;
			}
			fd->harray = new_array;
			fd->max_array += 500;
		}
		ent = fd->harray + fd->num_array++;
		ent->dir = dirent;
		fd->dir_size += EXT2_DIR_REC_LEN(dirent->name_len & 0xFF);
		ent->ino = dirent->inode;
		if (fd->compress)
			ent->hash = ent->minor_hash = 0;
		else {
			fd->err = ext2fs_dirhash(hash_alg, dirent->name,
						 dirent->name_len & 0xFF,
						 fs->super->s_hash_seed,
						 &ent->hash, &ent->minor_hash);
			if (fd->err)
				return BLOCK_ABORT;
		}
	}

	return 0;
}

/* Used for sorting the hash entry */
static EXT2_QSORT_TYPE ino_cmp(const void *a, const void *b)
{
	const struct hash_entry *he_a = (const struct hash_entry *) a;
	const struct hash_entry *he_b = (const struct hash_entry *) b;

	return (he_a->ino - he_b->ino);
}

/* Used for sorting the hash entry */
static EXT2_QSORT_TYPE name_cmp(const void *a, const void *b)
{
	const struct hash_entry *he_a = (const struct hash_entry *) a;
	const struct hash_entry *he_b = (const struct hash_entry *) b;
	int	ret;
	int	min_len;

	min_len = he_a->dir->name_len;
	if (min_len > he_b->dir->name_len)
		min_len = he_b->dir->name_len;

	ret = strncmp(he_a->dir->name, he_b->dir->name, min_len);
	if (ret == 0) {
		if (he_a->dir->name_len > he_b->dir->name_len)
			ret = 1;
		else if (he_a->dir->name_len < he_b->dir->name_len)
			ret = -1;
		else
			ret = he_b->dir->inode - he_a->dir->inode;
	}
	return ret;
}

/* Used for sorting the hash entry */
static EXT2_QSORT_TYPE hash_cmp(const void *a, const void *b)
{
	const struct hash_entry *he_a = (const struct hash_entry *) a;
	const struct hash_entry *he_b = (const struct hash_entry *) b;
	int	ret;

	if (he_a->hash > he_b->hash)
		ret = 1;
	else if (he_a->hash < he_b->hash)
		ret = -1;
	else {
		if (he_a->minor_hash > he_b->minor_hash)
			ret = 1;
		else if (he_a->minor_hash < he_b->minor_hash)
			ret = -1;
		else
			ret = name_cmp(a, b);
	}
	return ret;
}

static errcode_t alloc_size_dir(ext2_filsys fs, struct out_dir *outdir,
				int blocks)
{
	void			*new_mem;

	if (outdir->max) {
		new_mem = realloc(outdir->buf, blocks * fs->blocksize);
		if (!new_mem)
			return ENOMEM;
		outdir->buf = new_mem;
		new_mem = realloc(outdir->hashes,
				  blocks * sizeof(ext2_dirhash_t));
		if (!new_mem)
			return ENOMEM;
		outdir->hashes = new_mem;
	} else {
		outdir->buf = malloc(blocks * fs->blocksize);
		outdir->hashes = malloc(blocks * sizeof(ext2_dirhash_t));
		outdir->num = 0;
	}
	outdir->max = blocks;
	return 0;
}

static void free_out_dir(struct out_dir *outdir)
{
	free(outdir->buf);
	free(outdir->hashes);
	outdir->max = 0;
	outdir->num =0;
}

static errcode_t get_next_block(ext2_filsys fs, struct out_dir *outdir,
			 char ** ret)
{
	errcode_t	retval;

	if (outdir->num >= outdir->max) {
		retval = alloc_size_dir(fs, outdir, outdir->max + 50);
		if (retval)
			return retval;
	}
	*ret = outdir->buf + (outdir->num++ * fs->blocksize);
	memset(*ret, 0, fs->blocksize);
	return 0;
}

/*
 * This function is used to make a unique filename.  We do this by
 * appending ~0, and then incrementing the number.  However, we cannot
 * expand the length of the filename beyond the padding available in
 * the directory entry.
 */
static void mutate_name(char *str, __u16 *len)
{
	int	i;
	__u16	l = *len & 0xFF, h = *len & 0xff00;

	/*
	 * First check to see if it looks the name has been mutated
	 * already
	 */
	for (i = l-1; i > 0; i--) {
		if (!isdigit(str[i]))
			break;
	}
	if ((i == l-1) || (str[i] != '~')) {
		if (((l-1) & 3) < 2)
			l += 2;
		else
			l = (l+3) & ~3;
		str[l-2] = '~';
		str[l-1] = '0';
		*len = l | h;
		return;
	}
	for (i = l-1; i >= 0; i--) {
		if (isdigit(str[i])) {
			if (str[i] == '9')
				str[i] = '0';
			else {
				str[i]++;
				return;
			}
			continue;
		}
		if (i == 1) {
			if (str[0] == 'z')
				str[0] = 'A';
			else if (str[0] == 'Z') {
				str[0] = '~';
				str[1] = '0';
			} else
				str[0]++;
		} else if (i > 0) {
			str[i] = '1';
			str[i-1] = '~';
		} else {
			if (str[0] == '~')
				str[0] = 'a';
			else
				str[0]++;
		}
		break;
	}
}

static int duplicate_search_and_fix(e2fsck_t ctx, ext2_filsys fs,
				    ext2_ino_t ino,
				    struct fill_dir_struct *fd)
{
	struct problem_context	pctx;
	struct hash_entry 	*ent, *prev;
	int			i, j;
	int			fixed = 0;
	char			new_name[256];
	__u16			new_len;
	int			hash_alg;

	clear_problem_context(&pctx);
	pctx.ino = ino;

	hash_alg = fs->super->s_def_hash_version;
	if ((hash_alg <= EXT2_HASH_TEA) &&
	    (fs->super->s_flags & EXT2_FLAGS_UNSIGNED_HASH))
		hash_alg += 3;

	for (i=1; i < fd->num_array; i++) {
		ent = fd->harray + i;
		prev = ent - 1;
		if (!ent->dir->inode ||
		    ((ent->dir->name_len & 0xFF) !=
		     (prev->dir->name_len & 0xFF)) ||
		    (strncmp(ent->dir->name, prev->dir->name,
			     ent->dir->name_len & 0xFF)))
			continue;
		pctx.dirent = ent->dir;
		if ((ent->dir->inode == prev->dir->inode) &&
		    fix_problem(ctx, PR_2_DUPLICATE_DIRENT, &pctx)) {
			e2fsck_adjust_inode_count(ctx, ent->dir->inode, -1);
			ent->dir->inode = 0;
			fixed++;
			continue;
		}
		memcpy(new_name, ent->dir->name, ent->dir->name_len & 0xFF);
		new_len = ent->dir->name_len;
		mutate_name(new_name, &new_len);
		for (j=0; j < fd->num_array; j++) {
			if ((i==j) ||
			    ((new_len & 0xFF) !=
			     (fd->harray[j].dir->name_len & 0xFF)) ||
			    (strncmp(new_name, fd->harray[j].dir->name,
				     new_len & 0xFF)))
				continue;
			mutate_name(new_name, &new_len);

			j = -1;
		}
		new_name[new_len & 0xFF] = 0;
		pctx.str = new_name;
		if (fix_problem(ctx, PR_2_NON_UNIQUE_FILE, &pctx)) {
			memcpy(ent->dir->name, new_name, new_len & 0xFF);
			ent->dir->name_len = new_len;
			ext2fs_dirhash(hash_alg, ent->dir->name,
				       ent->dir->name_len & 0xFF,
				       fs->super->s_hash_seed,
				       &ent->hash, &ent->minor_hash);
			fixed++;
		}
	}
	return fixed;
}


static errcode_t copy_dir_entries(e2fsck_t ctx,
				  struct fill_dir_struct *fd,
				  struct out_dir *outdir)
{
	ext2_filsys 		fs = ctx->fs;
	errcode_t		retval;
	char			*block_start;
	struct hash_entry 	*ent;
	struct ext2_dir_entry	*dirent;
	unsigned int		rec_len, prev_rec_len;
	int			i, left;
	ext2_dirhash_t		prev_hash;
	int			offset, slack;

	if (ctx->htree_slack_percentage == 255) {
		profile_get_uint(ctx->profile, "options",
				 "indexed_dir_slack_percentage",
				 0, 20,
				 &ctx->htree_slack_percentage);
		if (ctx->htree_slack_percentage > 100)
			ctx->htree_slack_percentage = 20;
	}

	outdir->max = 0;
	retval = alloc_size_dir(fs, outdir,
				(fd->dir_size / fs->blocksize) + 2);
	if (retval)
		return retval;
	outdir->num = fd->compress ? 0 : 1;
	offset = 0;
	outdir->hashes[0] = 0;
	prev_hash = 1;
	if ((retval = get_next_block(fs, outdir, &block_start)))
		return retval;
	dirent = (struct ext2_dir_entry *) block_start;
	prev_rec_len = 0;
	rec_len = 0;
	left = fs->blocksize;
	slack = fd->compress ? 12 :
		(fs->blocksize * ctx->htree_slack_percentage)/100;
	if (slack < 12)
		slack = 12;
	for (i = 0; i < fd->num_array; i++) {
		ent = fd->harray + i;
		if (ent->dir->inode == 0)
			continue;
		rec_len = EXT2_DIR_REC_LEN(ent->dir->name_len & 0xFF);
		if (rec_len > left) {
			if (left) {
				left += prev_rec_len;
				retval = ext2fs_set_rec_len(fs, left, dirent);
				if (retval)
					return retval;
			}
			if ((retval = get_next_block(fs, outdir,
						      &block_start)))
				return retval;
			offset = 0;
		}
		left = fs->blocksize - offset;
		dirent = (struct ext2_dir_entry *) (block_start + offset);
		if (offset == 0) {
			if (ent->hash == prev_hash)
				outdir->hashes[outdir->num-1] = ent->hash | 1;
			else
				outdir->hashes[outdir->num-1] = ent->hash;
		}
		dirent->inode = ent->dir->inode;
		dirent->name_len = ent->dir->name_len;
		retval = ext2fs_set_rec_len(fs, rec_len, dirent);
		if (retval)
			return retval;
		prev_rec_len = rec_len;
		memcpy(dirent->name, ent->dir->name, dirent->name_len & 0xFF);
		offset += rec_len;
		left -= rec_len;
		if (left < slack) {
			prev_rec_len += left;
			retval = ext2fs_set_rec_len(fs, prev_rec_len, dirent);
			if (retval)
				return retval;
			offset += left;
			left = 0;
		}
		prev_hash = ent->hash;
	}
	if (left)
		retval = ext2fs_set_rec_len(fs, rec_len + left, dirent);

	return retval;
}


static struct ext2_dx_root_info *set_root_node(ext2_filsys fs, char *buf,
				    ext2_ino_t ino, ext2_ino_t parent)
{
	struct ext2_dir_entry 		*dir;
	struct ext2_dx_root_info  	*root;
	struct ext2_dx_countlimit	*limits;
	int				filetype = 0;

	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE)
		filetype = EXT2_FT_DIR << 8;

	memset(buf, 0, fs->blocksize);
	dir = (struct ext2_dir_entry *) buf;
	dir->inode = ino;
	dir->name[0] = '.';
	dir->name_len = 1 | filetype;
	dir->rec_len = 12;
	dir = (struct ext2_dir_entry *) (buf + 12);
	dir->inode = parent;
	dir->name[0] = '.';
	dir->name[1] = '.';
	dir->name_len = 2 | filetype;
	dir->rec_len = fs->blocksize - 12;

	root = (struct ext2_dx_root_info *) (buf+24);
	root->reserved_zero = 0;
	root->hash_version = fs->super->s_def_hash_version;
	root->info_length = 8;
	root->indirect_levels = 0;
	root->unused_flags = 0;

	limits = (struct ext2_dx_countlimit *) (buf+32);
	limits->limit = (fs->blocksize - 32) / sizeof(struct ext2_dx_entry);
	limits->count = 0;

	return root;
}


static struct ext2_dx_entry *set_int_node(ext2_filsys fs, char *buf)
{
	struct ext2_dir_entry 		*dir;
	struct ext2_dx_countlimit	*limits;

	memset(buf, 0, fs->blocksize);
	dir = (struct ext2_dir_entry *) buf;
	dir->inode = 0;
	(void) ext2fs_set_rec_len(fs, fs->blocksize, dir);

	limits = (struct ext2_dx_countlimit *) (buf+8);
	limits->limit = (fs->blocksize - 8) / sizeof(struct ext2_dx_entry);
	limits->count = 0;

	return (struct ext2_dx_entry *) limits;
}

/*
 * This function takes the leaf nodes which have been written in
 * outdir, and populates the root node and any necessary interior nodes.
 */
static errcode_t calculate_tree(ext2_filsys fs,
				struct out_dir *outdir,
				ext2_ino_t ino,
				ext2_ino_t parent)
{
	struct ext2_dx_root_info  	*root_info;
	struct ext2_dx_entry 		*root, *dx_ent = 0;
	struct ext2_dx_countlimit	*root_limit, *limit;
	errcode_t			retval;
	char				* block_start;
	int				i, c1, c2, nblks;
	int				limit_offset, root_offset;

	root_info = set_root_node(fs, outdir->buf, ino, parent);
	root_offset = limit_offset = ((char *) root_info - outdir->buf) +
		root_info->info_length;
	root_limit = (struct ext2_dx_countlimit *) (outdir->buf + limit_offset);
	c1 = root_limit->limit;
	nblks = outdir->num;

	/* Write out the pointer blocks */
	if (nblks-1 <= c1) {
		/* Just write out the root block, and we're done */
		root = (struct ext2_dx_entry *) (outdir->buf + root_offset);
		for (i=1; i < nblks; i++) {
			root->block = ext2fs_cpu_to_le32(i);
			if (i != 1)
				root->hash =
					ext2fs_cpu_to_le32(outdir->hashes[i]);
			root++;
			c1--;
		}
	} else {
		c2 = 0;
		limit = 0;
		root_info->indirect_levels = 1;
		for (i=1; i < nblks; i++) {
			if (c1 == 0)
				return ENOSPC;
			if (c2 == 0) {
				if (limit)
					limit->limit = limit->count =
		ext2fs_cpu_to_le16(limit->limit);
				root = (struct ext2_dx_entry *)
					(outdir->buf + root_offset);
				root->block = ext2fs_cpu_to_le32(outdir->num);
				if (i != 1)
					root->hash =
			ext2fs_cpu_to_le32(outdir->hashes[i]);
				if ((retval =  get_next_block(fs, outdir,
							      &block_start)))
					return retval;
				dx_ent = set_int_node(fs, block_start);
				limit = (struct ext2_dx_countlimit *) dx_ent;
				c2 = limit->limit;
				root_offset += sizeof(struct ext2_dx_entry);
				c1--;
			}
			dx_ent->block = ext2fs_cpu_to_le32(i);
			if (c2 != limit->limit)
				dx_ent->hash =
					ext2fs_cpu_to_le32(outdir->hashes[i]);
			dx_ent++;
			c2--;
		}
		limit->count = ext2fs_cpu_to_le16(limit->limit - c2);
		limit->limit = ext2fs_cpu_to_le16(limit->limit);
	}
	root_limit = (struct ext2_dx_countlimit *) (outdir->buf + limit_offset);
	root_limit->count = ext2fs_cpu_to_le16(root_limit->limit - c1);
	root_limit->limit = ext2fs_cpu_to_le16(root_limit->limit);

	return 0;
}

struct write_dir_struct {
	struct out_dir *outdir;
	errcode_t	err;
	e2fsck_t	ctx;
	int		cleared;
};

/*
 * Helper function which writes out a directory block.
 */
static int write_dir_block(ext2_filsys fs,
			   blk64_t *block_nr,
			   e2_blkcnt_t blockcnt,
			   blk64_t ref_block EXT2FS_ATTR((unused)),
			   int ref_offset EXT2FS_ATTR((unused)),
			   void *priv_data)
{
	struct write_dir_struct	*wd = (struct write_dir_struct *) priv_data;
	blk64_t	blk;
	char	*dir;

	if (*block_nr == 0)
		return 0;
	if (blockcnt >= wd->outdir->num) {
		e2fsck_read_bitmaps(wd->ctx);
		blk = *block_nr;
		ext2fs_unmark_block_bitmap2(wd->ctx->block_found_map, blk);
		ext2fs_block_alloc_stats2(fs, blk, -1);
		*block_nr = 0;
		wd->cleared++;
		return BLOCK_CHANGED;
	}
	if (blockcnt < 0)
		return 0;

	dir = wd->outdir->buf + (blockcnt * fs->blocksize);
	wd->err = ext2fs_write_dir_block3(fs, *block_nr, dir, 0);
	if (wd->err)
		return BLOCK_ABORT;
	return 0;
}

static errcode_t write_directory(e2fsck_t ctx, ext2_filsys fs,
				 struct out_dir *outdir,
				 ext2_ino_t ino, int compress)
{
	struct write_dir_struct wd;
	errcode_t	retval;
	struct ext2_inode 	inode;

	retval = e2fsck_expand_directory(ctx, ino, -1, outdir->num);
	if (retval)
		return retval;

	wd.outdir = outdir;
	wd.err = 0;
	wd.ctx = ctx;
	wd.cleared = 0;

	retval = ext2fs_block_iterate3(fs, ino, 0, 0,
				       write_dir_block, &wd);
	if (retval)
		return retval;
	if (wd.err)
		return wd.err;

	e2fsck_read_inode(ctx, ino, &inode, "rehash_dir");
	if (compress)
		inode.i_flags &= ~EXT2_INDEX_FL;
	else
		inode.i_flags |= EXT2_INDEX_FL;
	inode.i_size = outdir->num * fs->blocksize;
	ext2fs_iblk_sub_blocks(fs, &inode, wd.cleared);
	e2fsck_write_inode(ctx, ino, &inode, "rehash_dir");

	return 0;
}

errcode_t e2fsck_rehash_dir(e2fsck_t ctx, ext2_ino_t ino)
{
	ext2_filsys 		fs = ctx->fs;
	errcode_t		retval;
	struct ext2_inode 	inode;
	char			*dir_buf = 0;
	struct fill_dir_struct	fd;
	struct out_dir		outdir;

	outdir.max = outdir.num = 0;
	outdir.buf = 0;
	outdir.hashes = 0;
	e2fsck_read_inode(ctx, ino, &inode, "rehash_dir");

	retval = ENOMEM;
	fd.harray = 0;
	dir_buf = malloc(inode.i_size);
	if (!dir_buf)
		goto errout;

	fd.max_array = inode.i_size / 32;
	fd.num_array = 0;
	fd.harray = malloc(fd.max_array * sizeof(struct hash_entry));
	if (!fd.harray)
		goto errout;

	fd.ctx = ctx;
	fd.buf = dir_buf;
	fd.inode = &inode;
	fd.err = 0;
	fd.dir_size = 0;
	fd.compress = 0;
	if (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) ||
	    (inode.i_size / fs->blocksize) < 2)
		fd.compress = 1;
	fd.parent = 0;

retry_nohash:
	/* Read in the entire directory into memory */
	retval = ext2fs_block_iterate3(fs, ino, 0, 0,
				       fill_dir_block, &fd);
	if (fd.err) {
		retval = fd.err;
		goto errout;
	}

	/* 
	 * If the entries read are less than a block, then don't index
	 * the directory
	 */
	if (!fd.compress && (fd.dir_size < (fs->blocksize - 24))) {
		fd.compress = 1;
		fd.dir_size = 0;
		fd.num_array = 0;
		goto retry_nohash;
	}

#if 0
	printf("%d entries (%d bytes) found in inode %d\n",
	       fd.num_array, fd.dir_size, ino);
#endif

	/* Sort the list */
resort:
	if (fd.compress)
		qsort(fd.harray+2, fd.num_array-2, sizeof(struct hash_entry),
		      hash_cmp);
	else
		qsort(fd.harray, fd.num_array, sizeof(struct hash_entry),
		      hash_cmp);

	/*
	 * Look for duplicates
	 */
	if (duplicate_search_and_fix(ctx, fs, ino, &fd))
		goto resort;

	if (ctx->options & E2F_OPT_NO) {
		retval = 0;
		goto errout;
	}

	/* Sort non-hashed directories by inode number */
	if (fd.compress)
		qsort(fd.harray+2, fd.num_array-2,
		      sizeof(struct hash_entry), ino_cmp);

	/*
	 * Copy the directory entries.  In a htree directory these
	 * will become the leaf nodes.
	 */
	retval = copy_dir_entries(ctx, &fd, &outdir);
	if (retval)
		goto errout;

	free(dir_buf); dir_buf = 0;

	if (!fd.compress) {
		/* Calculate the interior nodes */
		retval = calculate_tree(fs, &outdir, ino, fd.parent);
		if (retval)
			goto errout;
	}

	retval = write_directory(ctx, fs, &outdir, ino, fd.compress);
	if (retval)
		goto errout;

errout:
	free(dir_buf);
	free(fd.harray);

	free_out_dir(&outdir);
	return retval;
}

void e2fsck_rehash_directories(e2fsck_t ctx)
{
	struct problem_context	pctx;
#ifdef RESOURCE_TRACK
	struct resource_track	rtrack;
#endif
	struct dir_info		*dir;
	ext2_u32_iterate 	iter;
	struct dir_info_iter *	dirinfo_iter = 0;
	ext2_ino_t		ino;
	errcode_t		retval;
	int			cur, max, all_dirs, first = 1;

	init_resource_track(&rtrack, ctx->fs->io);
	all_dirs = ctx->options & E2F_OPT_COMPRESS_DIRS;

	if (!ctx->dirs_to_hash && !all_dirs)
		return;

	e2fsck_get_lost_and_found(ctx, 0);

	clear_problem_context(&pctx);

	cur = 0;
	if (all_dirs) {
		dirinfo_iter = e2fsck_dir_info_iter_begin(ctx);
		max = e2fsck_get_num_dirinfo(ctx);
	} else {
		retval = ext2fs_u32_list_iterate_begin(ctx->dirs_to_hash,
						       &iter);
		if (retval) {
			pctx.errcode = retval;
			fix_problem(ctx, PR_3A_OPTIMIZE_ITER, &pctx);
			return;
		}
		max = ext2fs_u32_list_count(ctx->dirs_to_hash);
	}
	while (1) {
		if (all_dirs) {
			if ((dir = e2fsck_dir_info_iter(ctx,
							dirinfo_iter)) == 0)
				break;
			ino = dir->ino;
		} else {
			if (!ext2fs_u32_list_iterate(iter, &ino))
				break;
		}
		if (ino == ctx->lost_and_found)
			continue;
		pctx.dir = ino;
		if (first) {
			fix_problem(ctx, PR_3A_PASS_HEADER, &pctx);
			first = 0;
		}
#if 0
		fix_problem(ctx, PR_3A_OPTIMIZE_DIR, &pctx);
#endif
		pctx.errcode = e2fsck_rehash_dir(ctx, ino);
		if (pctx.errcode) {
			end_problem_latch(ctx, PR_LATCH_OPTIMIZE_DIR);
			fix_problem(ctx, PR_3A_OPTIMIZE_DIR_ERR, &pctx);
		}
		if (ctx->progress && !ctx->progress_fd)
			e2fsck_simple_progress(ctx, "Rebuilding directory",
			       100.0 * (float) (++cur) / (float) max, ino);
	}
	end_problem_latch(ctx, PR_LATCH_OPTIMIZE_DIR);
	if (all_dirs)
		e2fsck_dir_info_iter_end(ctx, dirinfo_iter);
	else
		ext2fs_u32_list_iterate_end(iter);

	if (ctx->dirs_to_hash)
		ext2fs_u32_list_free(ctx->dirs_to_hash);
	ctx->dirs_to_hash = 0;

	print_resource_track(ctx, "Pass 3A", &rtrack, ctx->fs->io);
}

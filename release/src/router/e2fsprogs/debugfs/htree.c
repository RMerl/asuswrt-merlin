/*
 * htree.c --- hash tree routines
 *
 * Copyright (C) 2002 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"

static FILE *pager;

static void htree_dump_leaf_node(ext2_filsys fs, ext2_ino_t ino,
				 struct ext2_inode *inode,
				 struct ext2_dx_root_info * rootnode,
				 blk_t blk, char *buf)
{
	errcode_t	errcode;
	struct ext2_dir_entry *dirent;
	int		thislen, col = 0;
	unsigned int	offset = 0;
	char		name[EXT2_NAME_LEN + 1];
	char		tmp[EXT2_NAME_LEN + 16];
	blk_t		pblk;
	ext2_dirhash_t 	hash, minor_hash;
	unsigned int	rec_len;
	int		hash_alg;

	errcode = ext2fs_bmap(fs, ino, inode, buf, 0, blk, &pblk);
	if (errcode) {
		com_err("htree_dump_leaf_node", errcode,
			"while mapping logical block %u\n", blk);
		return;
	}

	printf("Reading directory block %lu, phys %lu\n",
	       (unsigned long) blk, (unsigned long) pblk);
	errcode = ext2fs_read_dir_block2(current_fs, pblk, buf, 0);
	if (errcode) {
		com_err("htree_dump_leaf_node", errcode,
			"while reading block %lu (%lu)\n",
			(unsigned long) blk, (unsigned long) pblk);
		return;
	}
	hash_alg = rootnode->hash_version;
	if ((hash_alg <= EXT2_HASH_TEA) &&
	    (fs->super->s_flags & EXT2_FLAGS_UNSIGNED_HASH))
		hash_alg += 3;

	while (offset < fs->blocksize) {
		dirent = (struct ext2_dir_entry *) (buf + offset);
		errcode = ext2fs_get_rec_len(fs, dirent, &rec_len);
		if (errcode) {
			com_err("htree_dump_leaf_inode", errcode,
				"while getting rec_len for block %lu",
				(unsigned long) blk);
			return;
		}
		if (((offset + rec_len) > fs->blocksize) ||
		    (rec_len < 8) ||
		    ((rec_len % 4) != 0) ||
		    ((((unsigned) dirent->name_len & 0xFF)+8) > rec_len)) {
			fprintf(pager, "Corrupted directory block (%u)!\n", blk);
			break;
		}
		thislen = ((dirent->name_len & 0xFF) < EXT2_NAME_LEN) ?
			(dirent->name_len & 0xFF) : EXT2_NAME_LEN;
		strncpy(name, dirent->name, thislen);
		name[thislen] = '\0';
		errcode = ext2fs_dirhash(hash_alg, name,
					 thislen, fs->super->s_hash_seed,
					 &hash, &minor_hash);
		if (errcode)
			com_err("htree_dump_leaf_node", errcode,
				"while calculating hash");
		sprintf(tmp, "%u 0x%08x-%08x (%d) %s   ", dirent->inode,
			hash, minor_hash, rec_len, name);
		thislen = strlen(tmp);
		if (col + thislen > 80) {
			fprintf(pager, "\n");
			col = 0;
		}
		fprintf(pager, "%s", tmp);
		col += thislen;
		offset += rec_len;
	}
	fprintf(pager, "\n");
}


static void htree_dump_int_block(ext2_filsys fs, ext2_ino_t ino,
				 struct ext2_inode *inode,
				 struct ext2_dx_root_info * rootnode,
				 blk_t blk, char *buf, int level);


static void htree_dump_int_node(ext2_filsys fs, ext2_ino_t ino,
				struct ext2_inode *inode,
				struct ext2_dx_root_info * rootnode,
				struct ext2_dx_entry *ent,
				char *buf, int level)
{
	struct ext2_dx_countlimit	limit;
	struct ext2_dx_entry		e;
	int				hash, i;


	limit = *((struct ext2_dx_countlimit *) ent);
	limit.count = ext2fs_le16_to_cpu(limit.count);
	limit.limit = ext2fs_le16_to_cpu(limit.limit);

	fprintf(pager, "Number of entries (count): %d\n", limit.count);
	fprintf(pager, "Number of entries (limit): %d\n", limit.limit);

	for (i=0; i < limit.count; i++) {
		hash = i ? ext2fs_le32_to_cpu(ent[i].hash) : 0;
		fprintf(pager, "Entry #%d: Hash 0x%08x%s, block %u\n", i,
			hash, (hash & 1) ? " (**)" : "",
			ext2fs_le32_to_cpu(ent[i].block));
		}

	fprintf(pager, "\n");

	for (i=0; i < limit.count; i++) {
		e.hash = ext2fs_le32_to_cpu(ent[i].hash);
		e.block = ext2fs_le32_to_cpu(ent[i].block);
		fprintf(pager, "Entry #%d: Hash 0x%08x, block %u\n", i,
		       i ? e.hash : 0, e.block);
		if (level)
			htree_dump_int_block(fs, ino, inode, rootnode,
					     e.block, buf, level-1);
		else
			htree_dump_leaf_node(fs, ino, inode, rootnode,
					     e.block, buf);
	}

	fprintf(pager, "---------------------\n");
}

static void htree_dump_int_block(ext2_filsys fs, ext2_ino_t ino,
				 struct ext2_inode *inode,
				 struct ext2_dx_root_info * rootnode,
				 blk_t blk, char *buf, int level)
{
	char		*cbuf;
	errcode_t	errcode;
	blk_t		pblk;

	cbuf = malloc(fs->blocksize);
	if (!cbuf) {
		fprintf(pager, "Couldn't allocate child block.\n");
		return;
	}

	errcode = ext2fs_bmap(fs, ino, inode, buf, 0, blk, &pblk);
	if (errcode) {
		com_err("htree_dump_int_block", errcode,
			"while mapping logical block %u\n", blk);
		goto errout;
	}

	errcode = io_channel_read_blk(current_fs->io, pblk, 1, buf);
	if (errcode) {
		com_err("htree_dump_int_block", errcode,
			"while 	reading block %u\n", blk);
		goto errout;
	}

	htree_dump_int_node(fs, ino, inode, rootnode,
			    (struct ext2_dx_entry *) (buf+8),
			    cbuf, level);
errout:
	free(cbuf);
}



void do_htree_dump(int argc, char *argv[])
{
	ext2_ino_t	ino;
	struct ext2_inode inode;
	int		c;
	int		long_opt = 0;
	blk_t		blk;
	char		*buf = NULL;
	struct 		ext2_dx_root_info  *rootnode;
	struct 		ext2_dx_entry *ent;
	struct		ext2_dx_countlimit *limit;
	errcode_t	errcode;

	if (check_fs_open(argv[0]))
		return;

	pager = open_pager();

	reset_getopt();
	while ((c = getopt (argc, argv, "l")) != EOF) {
		switch (c) {
		case 'l':
			long_opt++;
			break;
		default:
			goto print_usage;
		}
	}

	if (argc > optind+1) {
	print_usage:
		com_err(0, 0, "Usage: htree_dump [-l] file");
		goto errout;
	}

	if (argc == optind)
		ino = cwd;
	else
		ino = string_to_inode(argv[optind]);
	if (!ino)
		goto errout;

	if (debugfs_read_inode(ino, &inode, argv[1]))
		goto errout;

	if (!LINUX_S_ISDIR(inode.i_mode)) {
		com_err(argv[0], 0, "Not a directory");
		goto errout;
	}

	if ((inode.i_flags & EXT2_BTREE_FL) == 0) {
		com_err(argv[0], 0, "Not a hash-indexed directory");
		goto errout;
	}

	buf = malloc(2*current_fs->blocksize);
	if (!buf) {
		com_err(argv[0], 0, "Couldn't allocate htree buffer");
		goto errout;
	}

	errcode = ext2fs_bmap(current_fs, ino, &inode, buf, 0, 0, &blk);
	if (errcode) {
		com_err("do_htree_block", errcode,
			"while mapping logical block 0\n");
		goto errout;
	}

	errcode = io_channel_read_blk(current_fs->io, blk,
				      1, buf);
	if (errcode) {
		com_err(argv[0], errcode, "Error reading root node");
		goto errout;
	}

	rootnode = (struct ext2_dx_root_info *) (buf + 24);

	fprintf(pager, "Root node dump:\n");
	fprintf(pager, "\t Reserved zero: %u\n", rootnode->reserved_zero);
	fprintf(pager, "\t Hash Version: %d\n", rootnode->hash_version);
	fprintf(pager, "\t Info length: %d\n", rootnode->info_length);
	fprintf(pager, "\t Indirect levels: %d\n", rootnode->indirect_levels);
	fprintf(pager, "\t Flags: %d\n", rootnode->unused_flags);

	ent = (struct ext2_dx_entry *) (buf + 24 + rootnode->info_length);
	limit = (struct ext2_dx_countlimit *) ent;

	htree_dump_int_node(current_fs, ino, &inode, rootnode, ent,
			    buf + current_fs->blocksize,
			    rootnode->indirect_levels);

errout:
	free(buf);
	close_pager(pager);
}

/*
 * This function prints the hash of a given file.
 */
void do_dx_hash(int argc, char *argv[])
{
	ext2_dirhash_t hash, minor_hash;
	errcode_t	err;
	int		c;
	int		hash_version = 0;
	__u32		hash_seed[4];

	hash_seed[0] = hash_seed[1] = hash_seed[2] = hash_seed[3] = 0;

	reset_getopt();
	while ((c = getopt (argc, argv, "h:s:")) != EOF) {
		switch (c) {
		case 'h':
			hash_version = e2p_string2hash(optarg);
			if (hash_version < 0)
				hash_version = atoi(optarg);
			break;
		case 's':
			if (uuid_parse(optarg, (unsigned char *) hash_seed)) {
				fprintf(stderr, "Invalid UUID format: %s\n",
					optarg);
				return;
			}
			break;
		default:
			goto print_usage;
		}
	}
	if (optind != argc-1) {
	print_usage:
		com_err(argv[0], 0, "usage: dx_hash [-h hash_alg] "
			"[-s hash_seed] filename");
		return;
	}
	err = ext2fs_dirhash(hash_version, argv[optind], strlen(argv[optind]),
			     hash_seed, &hash, &minor_hash);
	if (err) {
		com_err(argv[0], err, "while caclulating hash");
		return;
	}
	printf("Hash of %s is 0x%0x (minor 0x%0x)\n", argv[optind],
	       hash, minor_hash);
}

/*
 * Search for particular directory entry (useful for debugging very
 * large hash tree directories that have lost some blocks from the
 * btree index).
 */
struct process_block_struct {
	char	*search_name;
	char	*buf;
	int	len;
};

static int search_dir_block(ext2_filsys fs, blk_t *blocknr,
			    e2_blkcnt_t blockcnt, blk_t ref_blk,
			    int ref_offset, void *priv_data);

void do_dirsearch(int argc, char *argv[])
{
	ext2_ino_t	inode;
	struct process_block_struct pb;

	if (check_fs_open(argv[0]))
		return;

	if (argc != 3) {
		com_err(0, 0, "Usage: dirsearch dir filename");
		return;
	}

	inode = string_to_inode(argv[1]);
	if (!inode)
		return;

	pb.buf = malloc(current_fs->blocksize);
	if (!pb.buf) {
		com_err("dirsearch", 0, "Couldn't allocate buffer");
		return;
	}
	pb.search_name = argv[2];
	pb.len = strlen(pb.search_name);

	ext2fs_block_iterate2(current_fs, inode, BLOCK_FLAG_READ_ONLY, 0,
			      search_dir_block, &pb);

	free(pb.buf);
}


static int search_dir_block(ext2_filsys fs, blk_t *blocknr,
			    e2_blkcnt_t blockcnt,
			    blk_t ref_blk EXT2FS_ATTR((unused)),
			    int ref_offset EXT2FS_ATTR((unused)),
			    void *priv_data)
{
	struct process_block_struct *p;
	struct ext2_dir_entry *dirent;
	errcode_t	       	errcode;
	unsigned int		offset = 0;
	unsigned int		rec_len;

	if (blockcnt < 0)
		return 0;

	p = (struct process_block_struct *) priv_data;

	errcode = io_channel_read_blk(current_fs->io, *blocknr, 1, p->buf);
	if (errcode) {
		com_err("search_dir_block", errcode,
			"while reading block %lu", (unsigned long) *blocknr);
		return BLOCK_ABORT;
	}

	while (offset < fs->blocksize) {
		dirent = (struct ext2_dir_entry *) (p->buf + offset);
		errcode = ext2fs_get_rec_len(fs, dirent, &rec_len);
		if (errcode) {
			com_err("htree_dump_leaf_inode", errcode,
				"while getting rec_len for block %lu",
				(unsigned long) *blocknr);
			return BLOCK_ABORT;
		}
		if (dirent->inode &&
		    p->len == (dirent->name_len & 0xFF) &&
		    strncmp(p->search_name, dirent->name,
			    p->len) == 0) {
			printf("Entry found at logical block %lld, "
			       "phys %u, offset %u\n", (long long)blockcnt,
			       *blocknr, offset);
			printf("offset %u\n", offset);
			return BLOCK_ABORT;
		}
		offset += rec_len;
	}
	return 0;
}


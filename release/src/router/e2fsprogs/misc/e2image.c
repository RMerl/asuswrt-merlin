/*
 * e2image.c --- Program which writes an image file backing up
 * critical metadata for the filesystem.
 *
 * Copyright 2000, 2001 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <fcntl.h>
#include <grp.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <pwd.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "et/com_err.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"
#include "ext2fs/e2image.h"

#include "../version.h"
#include "nls-enable.h"

const char * program_name = "e2image";
char * device_name = NULL;

static void usage(void)
{
	fprintf(stderr, _("Usage: %s [-rsI] device image_file\n"),
		program_name);
	exit (1);
}

static void write_header(int fd, struct ext2_image_hdr *hdr, int blocksize)
{
	char *header_buf;
	int actual;

	header_buf = malloc(blocksize);
	if (!header_buf) {
		fputs(_("Couldn't allocate header buffer\n"), stderr);
		exit(1);
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		perror("lseek while writing header");
		exit(1);
	}
	memset(header_buf, 0, blocksize);

	if (hdr)
		memcpy(header_buf, hdr, sizeof(struct ext2_image_hdr));

	actual = write(fd, header_buf, blocksize);
	if (actual < 0) {
		perror("write header");
		exit(1);
	}
	if (actual != blocksize) {
		fprintf(stderr, _("short write (only %d bytes) for "
				  "writing image header"), actual);
		exit(1);
	}
	free(header_buf);
}

static void write_image_file(ext2_filsys fs, int fd)
{
	struct ext2_image_hdr	hdr;
	struct stat		st;
	errcode_t		retval;

	write_header(fd, NULL, fs->blocksize);
	memset(&hdr, 0, sizeof(struct ext2_image_hdr));

	hdr.offset_super = lseek(fd, 0, SEEK_CUR);
	retval = ext2fs_image_super_write(fs, fd, 0);
	if (retval) {
		com_err(program_name, retval, _("while writing superblock"));
		exit(1);
	}

	hdr.offset_inode = lseek(fd, 0, SEEK_CUR);
	retval = ext2fs_image_inode_write(fs, fd,
				  (fd != 1) ? IMAGER_FLAG_SPARSEWRITE : 0);
	if (retval) {
		com_err(program_name, retval, _("while writing inode table"));
		exit(1);
	}

	hdr.offset_blockmap = lseek(fd, 0, SEEK_CUR);
	retval = ext2fs_image_bitmap_write(fs, fd, 0);
	if (retval) {
		com_err(program_name, retval, _("while writing block bitmap"));
		exit(1);
	}

	hdr.offset_inodemap = lseek(fd, 0, SEEK_CUR);
	retval = ext2fs_image_bitmap_write(fs, fd, IMAGER_FLAG_INODEMAP);
	if (retval) {
		com_err(program_name, retval, _("while writing inode bitmap"));
		exit(1);
	}

	hdr.magic_number = EXT2_ET_MAGIC_E2IMAGE;
	strcpy(hdr.magic_descriptor, "Ext2 Image 1.0");
	gethostname(hdr.fs_hostname, sizeof(hdr.fs_hostname));
	strncpy(hdr.fs_device_name, device_name, sizeof(hdr.fs_device_name)-1);
	hdr.fs_device_name[sizeof(hdr.fs_device_name) - 1] = 0;
	hdr.fs_blocksize = fs->blocksize;

	if (stat(device_name, &st) == 0)
		hdr.fs_device = st.st_rdev;

	if (fstat(fd, &st) == 0) {
		hdr.image_device = st.st_dev;
		hdr.image_inode = st.st_ino;
	}
	memcpy(hdr.fs_uuid, fs->super->s_uuid, sizeof(hdr.fs_uuid));

	hdr.image_time = time(0);
	write_header(fd, &hdr, fs->blocksize);
}

/*
 * These set of functions are used to write a RAW image file.
 */
ext2fs_block_bitmap meta_block_map;
ext2fs_block_bitmap scramble_block_map;	/* Directory blocks to be scrambled */

struct process_block_struct {
	ext2_ino_t	ino;
	int		is_dir;
};

/*
 * These subroutines short circuits ext2fs_get_blocks and
 * ext2fs_check_directory; we use them since we already have the inode
 * structure, so there's no point in letting the ext2fs library read
 * the inode again.
 */
static ino_t stashed_ino = 0;
static struct ext2_inode *stashed_inode;

static errcode_t meta_get_blocks(ext2_filsys fs EXT2FS_ATTR((unused)),
				 ext2_ino_t ino,
				 blk_t *blocks)
{
	int	i;

	if ((ino != stashed_ino) || !stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	for (i=0; i < EXT2_N_BLOCKS; i++)
		blocks[i] = stashed_inode->i_block[i];
	return 0;
}

static errcode_t meta_check_directory(ext2_filsys fs EXT2FS_ATTR((unused)),
				      ext2_ino_t ino)
{
	if ((ino != stashed_ino) || !stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	if (!LINUX_S_ISDIR(stashed_inode->i_mode))
		return EXT2_ET_NO_DIRECTORY;
	return 0;
}

static errcode_t meta_read_inode(ext2_filsys fs EXT2FS_ATTR((unused)),
				 ext2_ino_t ino,
				 struct ext2_inode *inode)
{
	if ((ino != stashed_ino) || !stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;
	*inode = *stashed_inode;
	return 0;
}

static void use_inode_shortcuts(ext2_filsys fs, int bool)
{
	if (bool) {
		fs->get_blocks = meta_get_blocks;
		fs->check_directory = meta_check_directory;
		fs->read_inode = meta_read_inode;
		stashed_ino = 0;
	} else {
		fs->get_blocks = 0;
		fs->check_directory = 0;
		fs->read_inode = 0;
	}
}

static int process_dir_block(ext2_filsys fs EXT2FS_ATTR((unused)),
			     blk_t *block_nr,
			     e2_blkcnt_t blockcnt EXT2FS_ATTR((unused)),
			     blk_t ref_block EXT2FS_ATTR((unused)),
			     int ref_offset EXT2FS_ATTR((unused)),
			     void *priv_data EXT2FS_ATTR((unused)))
{
	struct process_block_struct *p;

	p = (struct process_block_struct *) priv_data;

	ext2fs_mark_block_bitmap(meta_block_map, *block_nr);
	if (scramble_block_map && p->is_dir && blockcnt >= 0)
		ext2fs_mark_block_bitmap(scramble_block_map, *block_nr);
	return 0;
}

static int process_file_block(ext2_filsys fs EXT2FS_ATTR((unused)),
			      blk_t *block_nr,
			      e2_blkcnt_t blockcnt,
			      blk_t ref_block EXT2FS_ATTR((unused)),
			      int ref_offset EXT2FS_ATTR((unused)),
			      void *priv_data EXT2FS_ATTR((unused)))
{
	if (blockcnt < 0) {
		ext2fs_mark_block_bitmap(meta_block_map, *block_nr);
	}
	return 0;
}

static void mark_table_blocks(ext2_filsys fs)
{
	blk_t	first_block, b;
	unsigned int	i,j;

	first_block = fs->super->s_first_data_block;
	/*
	 * Mark primary superblock
	 */
	ext2fs_mark_block_bitmap(meta_block_map, first_block);

	/*
	 * Mark the primary superblock descriptors
	 */
	for (j = 0; j < fs->desc_blocks; j++) {
		ext2fs_mark_block_bitmap(meta_block_map,
			 ext2fs_descriptor_block_loc(fs, first_block, j));
	}

	for (i = 0; i < fs->group_desc_count; i++) {
		/*
		 * Mark the blocks used for the inode table
		 */
		if (fs->group_desc[i].bg_inode_table) {
			for (j = 0, b = fs->group_desc[i].bg_inode_table;
			     j < (unsigned) fs->inode_blocks_per_group;
			     j++, b++)
				ext2fs_mark_block_bitmap(meta_block_map, b);
		}

		/*
		 * Mark block used for the block bitmap
		 */
		if (fs->group_desc[i].bg_block_bitmap) {
			ext2fs_mark_block_bitmap(meta_block_map,
				     fs->group_desc[i].bg_block_bitmap);
		}

		/*
		 * Mark block used for the inode bitmap
		 */
		if (fs->group_desc[i].bg_inode_bitmap) {
			ext2fs_mark_block_bitmap(meta_block_map,
				 fs->group_desc[i].bg_inode_bitmap);
		}
	}
}

/*
 * This function returns 1 if the specified block is all zeros
 */
static int check_zero_block(char *buf, int blocksize)
{
	char	*cp = buf;
	int	left = blocksize;

	while (left > 0) {
		if (*cp++)
			return 0;
		left--;
	}
	return 1;
}

static void write_block(int fd, char *buf, int sparse_offset,
			int blocksize, blk_t block)
{
	int		count;
	errcode_t	err;

	if (sparse_offset) {
#ifdef HAVE_LSEEK64
		if (lseek64(fd, sparse_offset, SEEK_CUR) < 0)
			perror("lseek");
#else
		if (lseek(fd, sparse_offset, SEEK_CUR) < 0)
			perror("lseek");
#endif
	}
	if (blocksize) {
		count = write(fd, buf, blocksize);
		if (count != blocksize) {
			if (count == -1)
				err = errno;
			else
				err = 0;
			com_err(program_name, err, "error writing block %u",
				block);
			exit(1);
		}
	}
}

int name_id[256];

#define EXT4_MAX_REC_LEN		((1<<16)-1)

static void scramble_dir_block(ext2_filsys fs, blk_t blk, char *buf)
{
	char *p, *end, *cp;
	struct ext2_dir_entry_2 *dirent;
	unsigned int rec_len;
	int id, len;

	end = buf + fs->blocksize;
	for (p = buf; p < end-8; p += rec_len) {
		dirent = (struct ext2_dir_entry_2 *) p;
		rec_len = dirent->rec_len;
#ifdef WORDS_BIGENDIAN
		rec_len = ext2fs_swab16(rec_len);
#endif
		if (rec_len == EXT4_MAX_REC_LEN || rec_len == 0)
			rec_len = fs->blocksize;
		else 
			rec_len = (rec_len & 65532) | ((rec_len & 3) << 16);
#if 0
		printf("rec_len = %d, name_len = %d\n", rec_len, dirent->name_len);
#endif
		if (rec_len < 8 || (rec_len % 4) ||
		    (p+rec_len > end)) {
			printf("Corrupt directory block %lu: "
			       "bad rec_len (%d)\n", (unsigned long) blk,
			       rec_len);
			rec_len = end - p;
			(void) ext2fs_set_rec_len(fs, rec_len,
					(struct ext2_dir_entry *) dirent);
#ifdef WORDS_BIGENDIAN
			dirent->rec_len = ext2fs_swab16(dirent->rec_len);
#endif
			continue;
		}
		if (dirent->name_len + 8 > rec_len) {
			printf("Corrupt directory block %lu: "
			       "bad name_len (%d)\n", (unsigned long) blk,
			       dirent->name_len);
			dirent->name_len = rec_len - 8;
			continue;
		}
		cp = p+8;
		len = rec_len - dirent->name_len - 8;
		if (len > 0)
			memset(cp+dirent->name_len, 0, len);
		if (dirent->name_len==1 && cp[0] == '.')
			continue;
		if (dirent->name_len==2 && cp[0] == '.' && cp[1] == '.')
			continue;

		memset(cp, 'A', dirent->name_len);
		len = dirent->name_len;
		id = name_id[len]++;
		while ((len > 0) && (id > 0)) {
			*cp += id % 26;
			id = id / 26;
			cp++;
			len--;
		}
	}
}

static void output_meta_data_blocks(ext2_filsys fs, int fd)
{
	errcode_t	retval;
	blk_t		blk;
	char		*buf, *zero_buf;
	int		sparse = 0;

	buf = malloc(fs->blocksize);
	if (!buf) {
		com_err(program_name, ENOMEM, "while allocating buffer");
		exit(1);
	}
	zero_buf = malloc(fs->blocksize);
	if (!zero_buf) {
		com_err(program_name, ENOMEM, "while allocating buffer");
		exit(1);
	}
	memset(zero_buf, 0, fs->blocksize);
	for (blk = 0; blk < fs->super->s_blocks_count; blk++) {
		if ((blk >= fs->super->s_first_data_block) &&
		    ext2fs_test_block_bitmap(meta_block_map, blk)) {
			retval = io_channel_read_blk(fs->io, blk, 1, buf);
			if (retval) {
				com_err(program_name, retval,
					"error reading block %u", blk);
			}
			if (scramble_block_map &&
			    ext2fs_test_block_bitmap(scramble_block_map, blk))
				scramble_dir_block(fs, blk, buf);
			if ((fd != 1) && check_zero_block(buf, fs->blocksize))
				goto sparse_write;
			write_block(fd, buf, sparse, fs->blocksize, blk);
			sparse = 0;
		} else {
		sparse_write:
			if (fd == 1) {
				write_block(fd, zero_buf, 0,
					    fs->blocksize, blk);
				continue;
			}
			sparse += fs->blocksize;
			if (sparse >= 1024*1024) {
				write_block(fd, 0, sparse, 0, 0);
				sparse = 0;
			}
		}
	}
	if (sparse)
		write_block(fd, zero_buf, sparse-1, 1, -1);
	free(zero_buf);
	free(buf);
}

static void write_raw_image_file(ext2_filsys fs, int fd, int scramble_flag)
{
	struct process_block_struct	pb;
	struct ext2_inode		inode;
	ext2_inode_scan			scan;
	ext2_ino_t			ino;
	errcode_t			retval;
	char *				block_buf;

	retval = ext2fs_allocate_block_bitmap(fs, "in-use block map",
					      &meta_block_map);
	if (retval) {
		com_err(program_name, retval, "while allocating block bitmap");
		exit(1);
	}

	if (scramble_flag) {
		retval = ext2fs_allocate_block_bitmap(fs, "scramble block map",
						      &scramble_block_map);
		if (retval) {
			com_err(program_name, retval,
				"while allocating scramble block bitmap");
			exit(1);
		}
	}

	mark_table_blocks(fs);

	retval = ext2fs_open_inode_scan(fs, 0, &scan);
	if (retval) {
		com_err(program_name, retval, _("while opening inode scan"));
		exit(1);
	}

	block_buf = malloc(fs->blocksize * 3);
	if (!block_buf) {
		com_err(program_name, 0, "Can't allocate block buffer");
		exit(1);
	}

	use_inode_shortcuts(fs, 1);
	stashed_inode = &inode;
	while (1) {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE)
			continue;
		if (retval) {
			com_err(program_name, retval,
				_("while getting next inode"));
			exit(1);
		}
		if (ino == 0)
			break;
		if (!inode.i_links_count)
			continue;
		if (inode.i_file_acl) {
			ext2fs_mark_block_bitmap(meta_block_map,
						 inode.i_file_acl);
		}
		if (!ext2fs_inode_has_valid_blocks(&inode))
			continue;

		stashed_ino = ino;
		pb.ino = ino;
		pb.is_dir = LINUX_S_ISDIR(inode.i_mode);
		if (LINUX_S_ISDIR(inode.i_mode) ||
		    (LINUX_S_ISLNK(inode.i_mode) &&
		     ext2fs_inode_has_valid_blocks(&inode)) ||
		    ino == fs->super->s_journal_inum) {
			retval = ext2fs_block_iterate2(fs, ino,
					BLOCK_FLAG_READ_ONLY, block_buf,
					process_dir_block, &pb);
			if (retval) {
				com_err(program_name, retval,
					"while iterating over inode %u",
					ino);
				exit(1);
			}
		} else {
			if ((inode.i_flags & EXT4_EXTENTS_FL) ||
			    inode.i_block[EXT2_IND_BLOCK] ||
			    inode.i_block[EXT2_DIND_BLOCK] ||
			    inode.i_block[EXT2_TIND_BLOCK]) {
				retval = ext2fs_block_iterate2(fs,
				       ino, BLOCK_FLAG_READ_ONLY, block_buf,
				       process_file_block, &pb);
				if (retval) {
					com_err(program_name, retval,
					"while iterating over inode %u", ino);
					exit(1);
				}
			}
		}
	}
	use_inode_shortcuts(fs, 0);
	output_meta_data_blocks(fs, fd);
	free(block_buf);
}

static void install_image(char *device, char *image_fn, int raw_flag)
{
	errcode_t retval;
	ext2_filsys fs;
	int open_flag = EXT2_FLAG_IMAGE_FILE;
	int fd = 0;
	io_manager	io_ptr;
	io_channel	io, image_io;

	if (raw_flag) {
		com_err(program_name, 0, "Raw images cannot be installed");
		exit(1);
	}

#ifdef CONFIG_TESTIO_DEBUG
	if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
	} else
#endif
		io_ptr = unix_io_manager;

	retval = ext2fs_open (image_fn, open_flag, 0, 0,
			      io_ptr, &fs);
        if (retval) {
		com_err (program_name, retval, _("while trying to open %s"),
			 image_fn);
		exit(1);
	}

	retval = ext2fs_read_bitmaps (fs);
	if (retval) {
		com_err(program_name, retval, "error reading bitmaps");
		exit(1);
	}

#ifdef HAVE_OPEN64
	fd = open64(image_fn, O_RDONLY);
#else
	fd = open(image_fn, O_RDONLY);
#endif
	if (fd < 0) {
		perror(image_fn);
		exit(1);
	}

	retval = io_ptr->open(device, IO_FLAG_RW, &io);
	if (retval) {
		com_err(device, 0, "while opening device file");
		exit(1);
	}

	image_io = fs->io;

	ext2fs_rewrite_to_io(fs, io);

	if (lseek(fd, fs->image_header->offset_inode, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}

	retval = ext2fs_image_inode_read(fs, fd, 0);
	if (retval) {
		com_err(image_fn, 0, "while restoring the image table");
		exit(1);
	}

	ext2fs_close (fs);
	exit (0);
}

int main (int argc, char ** argv)
{
	int c;
	errcode_t retval;
	ext2_filsys fs;
	char *image_fn;
	int open_flag = 0;
	int raw_flag = 0;
	int install_flag = 0;
	int scramble_flag = 0;
	int fd = 0;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	fprintf (stderr, "e2image %s (%s)\n", E2FSPROGS_VERSION,
		 E2FSPROGS_DATE);
	if (argc && *argv)
		program_name = *argv;
	add_error_table(&et_ext2_error_table);
	while ((c = getopt (argc, argv, "rsI")) != EOF)
		switch (c) {
		case 'r':
			raw_flag++;
			break;
		case 's':
			scramble_flag++;
			break;
		case 'I':
			install_flag++;
			break;
		default:
			usage();
		}
	if (optind != argc - 2 )
		usage();
	device_name = argv[optind];
	image_fn = argv[optind+1];

	if (install_flag) {
		install_image(device_name, image_fn, raw_flag);
		exit (0);
	}

	retval = ext2fs_open (device_name, open_flag, 0, 0,
			      unix_io_manager, &fs);
        if (retval) {
		com_err (program_name, retval, _("while trying to open %s"),
			 device_name);
		fputs(_("Couldn't find valid filesystem superblock.\n"), stdout);
		exit(1);
	}

	if (strcmp(image_fn, "-") == 0)
		fd = 1;
	else {
#ifdef HAVE_OPEN64
		fd = open64(image_fn, O_CREAT|O_TRUNC|O_WRONLY, 0600);
#else
		fd = open(image_fn, O_CREAT|O_TRUNC|O_WRONLY, 0600);
#endif
		if (fd < 0) {
			com_err(program_name, errno,
				_("while trying to open %s"), argv[optind+1]);
			exit(1);
		}
	}

	if (raw_flag)
		write_raw_image_file(fs, fd, scramble_flag);
	else
		write_image_file(fs, fd);

	ext2fs_close (fs);
	remove_error_table(&et_ext2_error_table);
	exit (0);
}

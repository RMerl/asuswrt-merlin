/*
 * Create a squashfs filesystem.  This is a highly compressed read only filesystem.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006
 * Phillip Lougher <phillip@lougher.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * mksquashfs.c
 */

#define FALSE 0
#define TRUE 1

#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <zlib.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include <squashfs_fs.h>
#include "mksquashfs.h"
#include "global.h"
#include "sort.h"

#ifdef SQUASHFS_TRACE
#define TRACE(s, args...)	do { \
					printf("mksquashfs: "s, ## args); \
				} while(0)
#else
#define TRACE(s, args...)
#endif

#define INFO(s, args...)	do {\
					 if(!silent)\
						printf("mksquashfs: "s, ## args);\
				} while(0)
#define ERROR(s, args...)	do {\
					fprintf(stderr, s, ## args);\
				} while(0)
#define EXIT_MKSQUASHFS()	do {\
					if(restore)\
						restorefs();\
					if(delete && destination_file && !block_device)\
						unlink(destination_file);\
					exit(1);\
				} while(0)
#define BAD_ERROR(s, args...)	do {\
					fprintf(stderr, "FATAL ERROR:" s, ##args);\
					EXIT_MKSQUASHFS();\
				} while(0)

int delete = FALSE;
long long total_compressed = 0, total_uncompressed = 0;
int fd;

/* filesystem flags for building */
int duplicate_checking = 1, noF = 0, no_fragments = 0, always_use_fragments = 0;
int noI = 0, noD = 0, check_data = 0;
int swap, silent = TRUE;
long long global_uid = -1, global_gid = -1;

/* superblock attributes */
int block_size = SQUASHFS_FILE_SIZE, block_log;
unsigned short uid_count = 0, guid_count = 0;
squashfs_uid uids[SQUASHFS_UIDS], guids[SQUASHFS_GUIDS];
int block_offset;
int file_count = 0, sym_count = 0, dev_count = 0, dir_count = 0, fifo_count = 0, sock_count = 0;

/* write position within data section */
long long bytes = 0, total_bytes = 0;

/* in memory directory table - possibly compressed */
char *directory_table = NULL;
unsigned int directory_bytes = 0, directory_size = 0, total_directory_bytes = 0;

/* cached directory table */
char *directory_data_cache = NULL;
unsigned int directory_cache_bytes = 0, directory_cache_size = 0;

/* in memory inode table - possibly compressed */
char *inode_table = NULL;
unsigned int inode_bytes = 0, inode_size = 0, total_inode_bytes = 0;

/* cached inode table */
char *data_cache = NULL;
unsigned int cache_bytes = 0, cache_size = 0, inode_count = 0;

/* in memory directory data */
#define I_COUNT_SIZE		128
#define DIR_ENTRIES		32
#define INODE_HASH_SIZE		65536
#define INODE_HASH_MASK		(INODE_HASH_SIZE - 1)
#define INODE_HASH(dev, ino)	(ino & INODE_HASH_MASK)

struct cached_dir_index {
	squashfs_dir_index	index;
	char			*name;
};

struct directory {
	unsigned int		start_block;
	unsigned int		size;
	unsigned char		*buff;
	unsigned char		*p;
	unsigned int		entry_count;
	unsigned char		*entry_count_p;
	unsigned int		i_count;
	unsigned int		i_size;
	struct cached_dir_index	*index;
	unsigned char		*index_count_p;
	unsigned int		inode_number;
};

struct inode_info *inode_info[INODE_HASH_SIZE];

/* hash tables used to do fast duplicate searches in duplicate check */
struct file_info *dupl[65536], *frag_dups[65536];
int dup_files = 0;

/* list of exclude dirs/files */
struct exclude_info {
	dev_t			st_dev;
	ino_t			st_ino;
};

#define EXCLUDE_SIZE 8192
int exclude = 0;
struct exclude_info *exclude_paths = NULL;
int excluded(char *filename, struct stat *buf);

/* fragment block data structures */
int fragments = 0;
char fragment_data[SQUASHFS_FILE_SIZE];
int fragment_size = 0;
struct fragment {
	unsigned int		index;
	int			offset;
	int			size;
};
#define FRAG_SIZE 32768
squashfs_fragment_entry *fragment_table = NULL;


/* current inode number for directories and non directories */
unsigned int dir_inode_no = 1;
unsigned int inode_no = 0;
unsigned int root_inode_number = 0;

/* list of source dirs/files */
int source = 0;
char **source_path;

/* list of root directory entries read from original filesystem */
int old_root_entries = 0;
struct old_root_entry_info {
	char			name[SQUASHFS_NAME_LEN + 1];
	squashfs_inode		inode;
	int			type;
	int			inode_number;
};
struct old_root_entry_info *old_root_entry;

/* in memory file info */
struct file_info {
	long long		bytes;
	unsigned short		checksum;
	long long		start;
	unsigned int		*block_list;
	struct file_info	*next;
	struct fragment		*fragment;
	unsigned short		fragment_checksum;
};

/* count of how many times SIGINT or SIGQUIT has been sent */
int interrupted = 0;

/* restore orignal filesystem state if appending to existing filesystem is cancelled */
jmp_buf env;
char *sdata_cache, *sdirectory_data_cache;

long long sbytes, stotal_bytes;

unsigned int sinode_bytes, scache_bytes, sdirectory_bytes,
	sdirectory_cache_bytes, suid_count, sguid_count,
	stotal_inode_bytes, stotal_directory_bytes,
	sinode_count, sfile_count, ssym_count, sdev_count,
	sdir_count, sfifo_count, ssock_count, sdup_files;
int sfragments;
int restore = 0;

/* flag whether destination file is a block device */
int block_device = 0;

/* flag indicating whether files are sorted using sort list(s) */
int sorted = 0;

/* save destination file name for deleting on error */
char *destination_file = NULL;

/* structure to used to pass in a pointer or an integer
 * to duplicate buffer read helper functions.
 */
struct duplicate_buffer_handle {
	char	*ptr;
	long long	start;
};

void add_old_root_entry(char *name, squashfs_inode inode, int inode_number, int type);
extern int read_super(int fd, squashfs_super_block *sBlk, int *be, char *source);
extern long long read_filesystem(char *root_name, int fd, squashfs_super_block *sBlk, char **cinode_table,
		char **data_cache, char **cdirectory_table, char **directory_data_cache,
		unsigned int *last_directory_block, unsigned int *inode_dir_offset, unsigned int *inode_dir_file_size,
		unsigned int *root_inode_size, unsigned int *inode_dir_start_block, int *file_count, int *sym_count,
		int *dev_count, int *dir_count, int *fifo_count, int *sock_count, squashfs_uid *uids,
		unsigned short *uid_count, squashfs_uid *guids, unsigned short *guid_count,
		long long *uncompressed_file, unsigned int *uncompressed_inode, unsigned int *uncompressed_directory,
		unsigned int *inode_dir_inode_number, unsigned int *inode_dir_parent_inode,
		void (push_directory_entry)(char *, squashfs_inode, int, int),
		squashfs_fragment_entry **fragment_table);
int get_sorted_inode(squashfs_inode *inode, struct stat *buf);
int read_sort_file(char *filename, int source, char *source_path[]);
void sort_files_and_write(struct dir_info *dir);
struct file_info *duplicate(char *(get_next_file_block)(struct duplicate_buffer_handle *, unsigned int), struct duplicate_buffer_handle *file_start, long long bytes, unsigned int **block_list, long long *start, int blocks, struct fragment **fragment, char *frag_data, int frag_bytes);
struct dir_info *dir_scan1(char *, int (_readdir)(char *, char *, struct dir_info *));

#define MKINODE(A)	((squashfs_inode)(((squashfs_inode) inode_bytes << 16) + (((char *)A) - data_cache)))


void restorefs()
{
	ERROR("Exiting - restoring original filesystem!\n\n");
	bytes = sbytes;
	memcpy(data_cache, sdata_cache, cache_bytes = scache_bytes);
	memcpy(directory_data_cache, sdirectory_data_cache, directory_cache_bytes = sdirectory_cache_bytes);
	inode_bytes = sinode_bytes;
	directory_bytes = sdirectory_bytes;
	uid_count = suid_count;
	guid_count = sguid_count;
	total_bytes = stotal_bytes;
	total_inode_bytes = stotal_inode_bytes;
	total_directory_bytes = stotal_directory_bytes;
	inode_count = sinode_count;
	file_count = sfile_count;
	sym_count = ssym_count;
	dev_count = sdev_count;
	dir_count = sdir_count;
	fifo_count = sfifo_count;
	sock_count = ssock_count;
	dup_files = sdup_files;
	fragments = sfragments;
	fragment_size = 0;
	longjmp(env, 1);
}


void sighandler()
{
	if(interrupted == 1)
		restorefs();
	else {
		ERROR("Interrupting will restore original filesystem!\n");
		ERROR("Interrupt again to quit\n");
		interrupted ++;
	}
}


void sighandler2()
{
	EXIT_MKSQUASHFS();
}


unsigned int mangle(char *d, char *s, int size, int block_size, int uncompressed, int data_block)
{
	unsigned long c_byte = block_size << 1;
	unsigned int res;

	if(!uncompressed && (res = compress2((unsigned char *) d, &c_byte, (unsigned char *) s, size, 9)) != Z_OK) {
		if(res == Z_MEM_ERROR)
			BAD_ERROR("zlib::compress failed, not enough memory\n");
		else if(res == Z_BUF_ERROR)
			BAD_ERROR("zlib::compress failed, not enough room in output buffer\n");
		else
			BAD_ERROR("zlib::compress failed, unknown error %d\n", res);
		return 0;
	}

	if(uncompressed || c_byte >= size) {
		memcpy(d, s, size);
		return size | (data_block ? SQUASHFS_COMPRESSED_BIT_BLOCK : SQUASHFS_COMPRESSED_BIT);
	}

	return (unsigned int) c_byte;
}


squashfs_base_inode_header *get_inode(int req_size)
{
	int data_space;
	unsigned short c_byte;

	while(cache_bytes >= SQUASHFS_METADATA_SIZE) {
		if((inode_size - inode_bytes) < ((SQUASHFS_METADATA_SIZE << 1)) + 2) {
			if((inode_table = (char *) realloc(inode_table, inode_size + (SQUASHFS_METADATA_SIZE << 1) + 2))
					== NULL) {
				goto failed;
			}
			inode_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}

		c_byte = mangle(inode_table + inode_bytes + block_offset, data_cache,
								SQUASHFS_METADATA_SIZE, SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Inode block @ %x, size %d\n", inode_bytes, c_byte);
		if(!swap)
			memcpy(inode_table + inode_bytes, &c_byte, sizeof(unsigned short));
		else
			SQUASHFS_SWAP_SHORTS((&c_byte), (inode_table + inode_bytes), 1);
		if(check_data)
			*((unsigned char *)(inode_table + inode_bytes + block_offset - 1)) = SQUASHFS_MARKER_BYTE;
		inode_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + block_offset;
		total_inode_bytes += SQUASHFS_METADATA_SIZE + block_offset;
		memcpy(data_cache, data_cache + SQUASHFS_METADATA_SIZE, cache_bytes - SQUASHFS_METADATA_SIZE);
		cache_bytes -= SQUASHFS_METADATA_SIZE;
	}

	data_space = (cache_size - cache_bytes);
	if(data_space < req_size) {
			int realloc_size = cache_size == 0 ? ((req_size + SQUASHFS_METADATA_SIZE) & ~(SQUASHFS_METADATA_SIZE - 1)) : req_size - data_space;

			if((data_cache = (char *) realloc(data_cache, cache_size + realloc_size)) == NULL) {
				goto failed;
			}
			cache_size += realloc_size;
	}

	cache_bytes += req_size;

	return (squashfs_base_inode_header *)(data_cache + (cache_bytes - req_size));

failed:
	BAD_ERROR("Out of memory in inode table reallocation!\n");
}


void read_bytes(int fd, long long byte, int bytes, char *buff)
{
	off_t off = byte;

	if(lseek(fd, off, SEEK_SET) == -1) {
		perror("Lseek on destination failed");
		EXIT_MKSQUASHFS();
	}

	if(read(fd, buff, bytes) == -1) {
		perror("Read on destination failed");
		EXIT_MKSQUASHFS();
	}
}


void write_bytes(int fd, long long byte, int bytes, char *buff)
{
	off_t off = byte;

	if(lseek(fd, off, SEEK_SET) == -1) {
		perror("Lseek on destination failed");
		EXIT_MKSQUASHFS();
	}

	if(write(fd, buff, bytes) == -1) {
		perror("Write on destination failed");
		EXIT_MKSQUASHFS();
	}
}


long long write_inodes()
{
	unsigned short c_byte;
	int avail_bytes;
	char *datap = data_cache;
	long long start_bytes = bytes;

	while(cache_bytes) {
		if(inode_size - inode_bytes < ((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			if((inode_table = (char *) realloc(inode_table, inode_size + ((SQUASHFS_METADATA_SIZE << 1) + 2))) == NULL) {
				BAD_ERROR("Out of memory in inode table reallocation!\n");
			}
			inode_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}
		avail_bytes = cache_bytes > SQUASHFS_METADATA_SIZE ? SQUASHFS_METADATA_SIZE : cache_bytes;
		c_byte = mangle(inode_table + inode_bytes + block_offset, datap, avail_bytes, SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Inode block @ %x, size %d\n", inode_bytes, c_byte);
		if(!swap)
			memcpy(inode_table + inode_bytes, &c_byte, sizeof(unsigned short));
		else
			SQUASHFS_SWAP_SHORTS((&c_byte), (inode_table + inode_bytes), 1); 
		if(check_data)
			*((unsigned char *)(inode_table + inode_bytes + block_offset - 1)) = SQUASHFS_MARKER_BYTE;
		inode_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + block_offset;
		total_inode_bytes += avail_bytes + block_offset;
		datap += avail_bytes;
		cache_bytes -= avail_bytes;
	}

	write_bytes(fd, bytes, inode_bytes, (char *) inode_table);
	bytes += inode_bytes;

	return start_bytes;
}


long long write_directories()
{
	unsigned short c_byte;
	int avail_bytes;
	char *directoryp = directory_data_cache;
	long long start_bytes = bytes;

	while(directory_cache_bytes) {
		if(directory_size - directory_bytes < ((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			if((directory_table = (char *) realloc(directory_table, directory_size +
					((SQUASHFS_METADATA_SIZE << 1) + 2))) == NULL) {
				BAD_ERROR("Out of memory in directory table reallocation!\n");
			}
			directory_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}
		avail_bytes = directory_cache_bytes > SQUASHFS_METADATA_SIZE ? SQUASHFS_METADATA_SIZE : directory_cache_bytes;
		c_byte = mangle(directory_table + directory_bytes + block_offset, directoryp, avail_bytes, SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Directory block @ %x, size %d\n", directory_bytes, c_byte);
		if(!swap)
			memcpy(directory_table + directory_bytes, &c_byte, sizeof(unsigned short));
		else
			SQUASHFS_SWAP_SHORTS((&c_byte), (directory_table + directory_bytes), 1);
		if(check_data)
			*((unsigned char *)(directory_table + directory_bytes + block_offset - 1)) = SQUASHFS_MARKER_BYTE;
		directory_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + block_offset;
		total_directory_bytes += avail_bytes + block_offset;
		directoryp += avail_bytes;
		directory_cache_bytes -= avail_bytes;
	}
	write_bytes(fd, bytes, directory_bytes, (char *) directory_table);
	bytes += directory_bytes;

	return start_bytes;
}


unsigned int get_uid(squashfs_uid uid)
{
	int i;

	for(i = 0; (i < uid_count) && uids[i] != uid; i++);
	if(i == uid_count) {
		if(uid_count == SQUASHFS_UIDS) {
			ERROR("Out of uids! - using uid 0 - probably not what's wanted!\n");
			i = 0;
		} else
			uids[uid_count++] = uid;
	}

	return i;
}


unsigned int get_guid(squashfs_uid uid, squashfs_uid guid)
{
	int i;

	if(uid == guid)
		return SQUASHFS_GUIDS;

	for(i = 0; (i < guid_count) && guids[i] != guid; i++);
	if(i == guid_count) {
		if(guid_count == SQUASHFS_GUIDS) {
			ERROR("Out of gids! - using gid 0 - probably not what's wanted!\n");
			return SQUASHFS_GUIDS;
		} else
			guids[guid_count++] = guid;
	}

	return i;
}


int create_inode(squashfs_inode *i_no, struct dir_ent *dir_ent, int type, long long byte_size, long long start_block, unsigned int offset, unsigned int *block_list, struct fragment *fragment, struct directory *dir_in)
{
	struct stat *buf = &dir_ent->inode->buf;
	squashfs_inode_header inode_header;
	squashfs_base_inode_header *inode, *base = &inode_header.base;
	char *filename = dir_ent->pathname;
	int nlink = dir_ent->inode->nlink;
	int inode_number = (type == SQUASHFS_LDIR_TYPE || type == SQUASHFS_DIR_TYPE) ? dir_ent->inode->inode_number : dir_ent->inode->inode_number + dir_inode_no;

	base->mode = SQUASHFS_MODE(buf->st_mode);
	base->uid = get_uid((squashfs_uid) global_uid == -1 ? buf->st_uid : global_uid);
	base->inode_type = type;
	base->guid = get_guid((squashfs_uid) global_uid == -1 ? buf->st_uid : global_uid, (squashfs_uid) global_gid == -1 ? buf->st_gid : global_gid);
	base->mtime = buf->st_mtime;
	base->inode_number = inode_number;

	if(type == SQUASHFS_FILE_TYPE) {
		int i;
		squashfs_reg_inode_header *reg = &inode_header.reg, *inodep;

		inode = get_inode(sizeof(*reg) + offset * sizeof(unsigned int));
		inodep = (squashfs_reg_inode_header *) inode;
		reg->file_size = byte_size;
		reg->start_block = start_block;
		reg->fragment = fragment->index;
		reg->offset = fragment->offset;
		if(!swap) {
			memcpy(inodep, reg, sizeof(*reg));
			memcpy(inodep->block_list, block_list, offset * sizeof(unsigned int));
		} else {
			SQUASHFS_SWAP_REG_INODE_HEADER(reg, inodep);
			SQUASHFS_SWAP_INTS(block_list, inodep->block_list, offset);
		}
		TRACE("File inode, file_size %d, start_block %llx, blocks %d, fragment %d, offset %d, size %d\n", (int) byte_size,
			start_block, offset, fragment->index, fragment->offset, fragment->size);
		for(i = 0; i < offset; i++)
			TRACE("Block %d, size %d\n", i, block_list[i]);
	}
	else if(type == SQUASHFS_LREG_TYPE) {
		int i;
		squashfs_lreg_inode_header *reg = &inode_header.lreg, *inodep;

		inode = get_inode(sizeof(*reg) + offset * sizeof(unsigned int));
		inodep = (squashfs_lreg_inode_header *) inode;
		reg->nlink = nlink;
		reg->file_size = byte_size;
		reg->start_block = start_block;
		reg->fragment = fragment->index;
		reg->offset = fragment->offset;
		if(!swap) {
			memcpy(inodep, reg, sizeof(*reg));
			memcpy(inodep->block_list, block_list, offset * sizeof(unsigned int));
		} else {
			SQUASHFS_SWAP_LREG_INODE_HEADER(reg, inodep);
			SQUASHFS_SWAP_INTS(block_list, inodep->block_list, offset);
		}
		TRACE("Long file inode, file_size %lld, start_block %llx, blocks %d, fragment %d, offset %d, size %d, nlink %d\n", byte_size,
			start_block, offset, fragment->index, fragment->offset, fragment->size, nlink);
		for(i = 0; i < offset; i++)
			TRACE("Block %d, size %d\n", i, block_list[i]);
	}
	else if(type == SQUASHFS_LDIR_TYPE) {
		int i;
		unsigned char *p;
		squashfs_ldir_inode_header *dir = &inode_header.ldir, *inodep;
		struct cached_dir_index *index = dir_in->index;
		unsigned int i_count = dir_in->i_count;
		unsigned int i_size = dir_in->i_size;

		if(byte_size >= 1 << 27)
			BAD_ERROR("directory greater than 2^27-1 bytes!\n");

		inode = get_inode(sizeof(*dir) + i_size);
		inodep = (squashfs_ldir_inode_header *) inode;
		dir->inode_type = SQUASHFS_LDIR_TYPE;
		dir->nlink = dir_ent->dir->directory_count + 2;
		dir->file_size = byte_size;
		dir->offset = offset;
		dir->start_block = start_block;
		dir->i_count = i_count;
		dir->parent_inode = dir_ent->our_dir ? dir_ent->our_dir->dir_ent->inode->inode_number : dir_inode_no + inode_no;

		if(!swap)
			memcpy(inode, dir, sizeof(*dir));
		else
			SQUASHFS_SWAP_LDIR_INODE_HEADER(dir, inode);
		p = (unsigned char *) inodep->index;
		for(i = 0; i < i_count; i++) {
			if(!swap)
				memcpy(p, &index[i].index, sizeof(squashfs_dir_index));
			else
				SQUASHFS_SWAP_DIR_INDEX(&index[i].index, p);
			memcpy(((squashfs_dir_index *)p)->name, index[i].name, index[i].index.size + 1);
			p += sizeof(squashfs_dir_index) + index[i].index.size + 1;
		}
		TRACE("Long directory inode, file_size %d, start_block %llx, offset %x, nlink %d\n", (int) byte_size,
			start_block, offset, dir_ent->dir->directory_count + 2);
	}
	else if(type == SQUASHFS_DIR_TYPE) {
		squashfs_dir_inode_header *dir = &inode_header.dir;

		inode = get_inode(sizeof(*dir));
		dir->nlink = dir_ent->dir->directory_count + 2;
		dir->file_size = byte_size;
		dir->offset = offset;
		dir->start_block = start_block;
		dir->parent_inode = dir_ent->our_dir ? dir_ent->our_dir->dir_ent->inode->inode_number : dir_inode_no + inode_no;
		if(!swap)
			memcpy(inode, dir, sizeof(*dir));
		else
			SQUASHFS_SWAP_DIR_INODE_HEADER(dir, inode);
		TRACE("Directory inode, file_size %d, start_block %llx, offset %x, nlink %d\n", (int) byte_size,
			start_block, offset, dir_ent->dir->directory_count + 2);
	}
	else if(type == SQUASHFS_CHRDEV_TYPE || type == SQUASHFS_BLKDEV_TYPE) {
		squashfs_dev_inode_header *dev = &inode_header.dev;

		inode = get_inode(sizeof(*dev));
		dev->nlink = nlink;
		dev->rdev = (unsigned short) ((major(buf->st_rdev) << 8) |
			(minor(buf->st_rdev) & 0xff));
		if(!swap)
			memcpy(inode, dev, sizeof(*dev));
		else
			SQUASHFS_SWAP_DEV_INODE_HEADER(dev, inode);
		TRACE("Device inode, rdev %x, nlink %d\n", dev->rdev, nlink);
	}
	else if(type == SQUASHFS_SYMLINK_TYPE) {
		squashfs_symlink_inode_header *symlink = &inode_header.symlink, *inodep;
		int byte;
		char buff[65536];

		if((byte = readlink(filename, buff, 65536)) == -1) {
			perror("Error in reading symbolic link, skipping...");
			return FALSE;
		}

		if(byte == 65536) {
			ERROR("Symlink is greater than 65536 bytes! skipping...");
			return FALSE;
		}

		inode = get_inode(sizeof(*symlink) + byte);
		symlink->nlink = nlink;
		inodep = (squashfs_symlink_inode_header *) inode;
		symlink->symlink_size = byte;
		if(!swap)
			memcpy(inode, symlink, sizeof(*symlink));
		else
			SQUASHFS_SWAP_SYMLINK_INODE_HEADER(symlink, inode);
		strncpy(inodep->symlink, buff, byte);
		TRACE("Symbolic link inode, symlink_size %d, nlink %d\n", byte, nlink);
	}
	else if(type == SQUASHFS_FIFO_TYPE || type == SQUASHFS_SOCKET_TYPE) {
		squashfs_ipc_inode_header *ipc = &inode_header.ipc;

		inode = get_inode(sizeof(*ipc));
		ipc->nlink = nlink;
		if(!swap)
			memcpy(inode, ipc, sizeof(*ipc));
		else
			SQUASHFS_SWAP_IPC_INODE_HEADER(ipc, inode);
		TRACE("ipc inode, type %s, nlink %d\n", type == SQUASHFS_FIFO_TYPE ? "fifo" : "socket", nlink);
	} else
		return FALSE;

	*i_no = MKINODE(inode);
	inode_count ++;

	TRACE("Created inode 0x%llx, type %d, uid %d, guid %d\n", *i_no, type, base->uid, base->guid);

	return TRUE;
}


void scan2_init_dir(struct directory *dir)
{
	if((dir->buff = malloc(SQUASHFS_METADATA_SIZE)) == NULL) {
		BAD_ERROR("Out of memory allocating directory buffer\n");
	}

	dir->size = SQUASHFS_METADATA_SIZE;
	dir->p = dir->index_count_p = dir->buff;
	dir->entry_count = 256;
	dir->entry_count_p = NULL;
	dir->index = NULL;
	dir->i_count = dir->i_size = 0;
}


void add_dir(squashfs_inode inode, unsigned int inode_number, char *name, int type, struct directory *dir)
{
	unsigned char *buff;
	squashfs_dir_entry idir, *idirp;
	unsigned int start_block = inode >> 16;
	unsigned int offset = inode & 0xffff;
	unsigned int size;

	if((size = strlen(name)) > SQUASHFS_NAME_LEN) {
		size = SQUASHFS_NAME_LEN;
		ERROR("Filename is greater than %d characters, truncating! ...\n", SQUASHFS_NAME_LEN);
	}

	if(dir->p + sizeof(squashfs_dir_entry) + size + sizeof(squashfs_dir_header) >= dir->buff + dir->size) {
		if((buff = realloc(dir->buff, dir->size += SQUASHFS_METADATA_SIZE)) == NULL)  {
			BAD_ERROR("Out of memory reallocating directory buffer\n");
		}

		dir->p = (dir->p - dir->buff) + buff;
		if(dir->entry_count_p) 
			dir->entry_count_p = (dir->entry_count_p - dir->buff + buff);
		dir->index_count_p = dir->index_count_p - dir->buff + buff;
		dir->buff = buff;
	}

	if(dir->entry_count == 256 || start_block != dir->start_block || ((dir->entry_count_p != NULL) && ((dir->p + sizeof(squashfs_dir_entry) + size - dir->index_count_p) > SQUASHFS_METADATA_SIZE)) || ((long long) inode_number - dir->inode_number) > 32767 || ((long long) inode_number - dir->inode_number) < - 32768) {
		if(dir->entry_count_p) {
			squashfs_dir_header dir_header;

			if((dir->p + sizeof(squashfs_dir_entry) + size - dir->index_count_p) > SQUASHFS_METADATA_SIZE) {
				if(dir->i_count % I_COUNT_SIZE == 0)
					if((dir->index = realloc(dir->index, (dir->i_count + I_COUNT_SIZE) * sizeof(struct cached_dir_index))) == NULL)
						BAD_ERROR("Out of memory in directory index table reallocation!\n");
				dir->index[dir->i_count].index.index = dir->p - dir->buff;
				dir->index[dir->i_count].index.size = size - 1;
				dir->index[dir->i_count++].name = name;
				dir->i_size += sizeof(squashfs_dir_index) + size;
				dir->index_count_p = dir->p;
			}

			dir_header.count = dir->entry_count - 1;
			dir_header.start_block = dir->start_block;
			dir_header.inode_number = dir->inode_number;
			if(!swap)
				memcpy(dir->entry_count_p, &dir_header, sizeof(dir_header));
			else
				SQUASHFS_SWAP_DIR_HEADER((&dir_header), (squashfs_dir_header *) dir->entry_count_p);

		}


		dir->entry_count_p = dir->p;
		dir->start_block = start_block;
		dir->entry_count = 0;
		dir->inode_number = inode_number;
		dir->p += sizeof(squashfs_dir_header);
	}

	idirp = (squashfs_dir_entry *) dir->p;
	idir.offset = offset;
	idir.type = type;
	idir.size = size - 1;
	idir.inode_number = ((long long) inode_number - dir->inode_number);
	if(!swap)
		memcpy(idirp, &idir, sizeof(idir));
	else
		SQUASHFS_SWAP_DIR_ENTRY((&idir), idirp);
	strncpy(idirp->name, name, size);
	dir->p += sizeof(squashfs_dir_entry) + size;
	dir->entry_count ++;
}


int write_dir(squashfs_inode *inode, struct dir_info *dir_info, struct directory *dir)
{
	unsigned int dir_size = dir->p - dir->buff;
	int data_space = (directory_cache_size - directory_cache_bytes);
	unsigned int directory_block, directory_offset, i_count, index;
	unsigned short c_byte;

	if(data_space < dir_size) {
		int realloc_size = directory_cache_size == 0 ? ((dir_size + SQUASHFS_METADATA_SIZE) & ~(SQUASHFS_METADATA_SIZE - 1)) : dir_size - data_space;

		if((directory_data_cache = (char *) realloc(directory_data_cache, directory_cache_size + realloc_size)) == NULL) {
			goto failed;
		}
		directory_cache_size += realloc_size;
	}

	if(dir_size) {
		squashfs_dir_header dir_header;

		dir_header.count = dir->entry_count - 1;
		dir_header.start_block = dir->start_block;
		dir_header.inode_number = dir->inode_number;
		if(!swap)
			memcpy(dir->entry_count_p, &dir_header, sizeof(dir_header));
		else
			SQUASHFS_SWAP_DIR_HEADER((&dir_header), (squashfs_dir_header *) dir->entry_count_p);
		memcpy(directory_data_cache + directory_cache_bytes, dir->buff, dir_size);
	}
	directory_offset = directory_cache_bytes;
	directory_block = directory_bytes;
	directory_cache_bytes += dir_size;
	i_count = 0;
	index = SQUASHFS_METADATA_SIZE - directory_offset;

	while(1) {
		while(i_count < dir->i_count && dir->index[i_count].index.index < index)
			dir->index[i_count++].index.start_block = directory_bytes;
		index += SQUASHFS_METADATA_SIZE;

		if(directory_cache_bytes < SQUASHFS_METADATA_SIZE)
			break;

		if((directory_size - directory_bytes) < ((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			if((directory_table = (char *) realloc(directory_table,
							directory_size + (SQUASHFS_METADATA_SIZE << 1) + 2)) == NULL) {
				goto failed;
			}
			directory_size += SQUASHFS_METADATA_SIZE << 1;
		}

		c_byte = mangle(directory_table + directory_bytes + block_offset, directory_data_cache,
				SQUASHFS_METADATA_SIZE, SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Directory block @ %x, size %d\n", directory_bytes, c_byte);
		if(!swap)
			memcpy(directory_table + directory_bytes, &c_byte, sizeof(unsigned short));
		else
			SQUASHFS_SWAP_SHORTS((&c_byte), (directory_table + directory_bytes), 1);
		if(check_data)
			*((unsigned char *)(directory_table + directory_bytes + block_offset - 1)) = SQUASHFS_MARKER_BYTE;
		directory_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + block_offset;
		total_directory_bytes += SQUASHFS_METADATA_SIZE + block_offset;
		memcpy(directory_data_cache, directory_data_cache + SQUASHFS_METADATA_SIZE, directory_cache_bytes - SQUASHFS_METADATA_SIZE);
		directory_cache_bytes -= SQUASHFS_METADATA_SIZE;
	}

	if(dir_info->dir_is_ldir) {
		if(create_inode(inode, dir_info->dir_ent, SQUASHFS_LDIR_TYPE, dir_size + 3, directory_block, directory_offset, NULL, NULL, dir) == FALSE)
			return FALSE;
	} else {
		if(create_inode(inode, dir_info->dir_ent, SQUASHFS_DIR_TYPE, dir_size + 3, directory_block, directory_offset, NULL, NULL, NULL) == FALSE)
			return FALSE;
	}

#ifdef SQUASHFS_TRACE
	if(!swap) {
		unsigned char *dirp;
		int count;

		TRACE("Directory contents of inode 0x%llx\n", *inode);
		dirp = dir->buff;
		while(dirp < dir->p) {
			char buffer[SQUASHFS_NAME_LEN + 1];
			squashfs_dir_entry idir, *idirp;
			squashfs_dir_header *dirh = (squashfs_dir_header *) dirp;
			count = dirh->count + 1;
			dirp += sizeof(squashfs_dir_header);

			TRACE("\tStart block 0x%x, count %d\n", dirh->start_block, count);

			while(count--) {
				idirp = (squashfs_dir_entry *) dirp;
				memcpy((char *) &idir, (char *) idirp, sizeof(idir));
				strncpy(buffer, idirp->name, idir.size + 1);
				buffer[idir.size + 1] = '\0';
				TRACE("\t\tname %s, inode offset 0x%x, type %d\n", buffer,
						  idir.offset, idir.type);
				dirp += sizeof(squashfs_dir_entry) + idir.size + 1;
			}
		}
	}
#endif
	dir_count ++;

	return TRUE;

failed:
	BAD_ERROR("Out of memory in directory table reallocation!\n");
}


char *get_fragment(char *buffer, struct fragment *fragment)
{
	squashfs_fragment_entry *disk_fragment = &fragment_table[fragment->index];
	int size = SQUASHFS_COMPRESSED_SIZE_BLOCK(disk_fragment->size);

	if(SQUASHFS_COMPRESSED_BLOCK(disk_fragment->size)) {
		int res;
		unsigned long bytes = block_size;
		char cbuffer[block_size];

		read_bytes(fd, disk_fragment->start_block, size, cbuffer);

		if((res = uncompress((unsigned char *) buffer, &bytes, (const unsigned char *) cbuffer, size)) != Z_OK) {
			if(res == Z_MEM_ERROR)
				BAD_ERROR("zlib::uncompress failed, not enough memory\n");
			else if(res == Z_BUF_ERROR)
				BAD_ERROR("zlib::uncompress failed, not enough room in output buffer\n");
			else
				BAD_ERROR("zlib::uncompress failed, unknown error %d\n", res);
		}
	} else
		read_bytes(fd, disk_fragment->start_block, size, buffer);

	return buffer + fragment->offset;
}

	
void write_fragment()
{
	int compressed_size;
	char buffer[block_size << 1];

	if(fragment_size == 0)
		return;

	if(fragments % FRAG_SIZE == 0)
		if((fragment_table = (squashfs_fragment_entry *) realloc(fragment_table, (fragments + FRAG_SIZE) * sizeof(squashfs_fragment_entry))) == NULL)
			BAD_ERROR("Out of memory in fragment table\n");
	fragment_table[fragments].size = mangle(buffer, fragment_data, fragment_size, block_size, noF, 1);
	fragment_table[fragments].start_block = bytes;
	compressed_size = SQUASHFS_COMPRESSED_SIZE_BLOCK(fragment_table[fragments].size);
	write_bytes(fd, bytes, compressed_size, buffer);
	bytes += compressed_size;
	total_uncompressed += fragment_size;
	total_compressed += compressed_size;
	TRACE("Writing fragment %d, uncompressed size %d, compressed size %d\n",fragments, fragment_size, compressed_size);
	fragments ++;
	fragment_size = 0;
}


static struct fragment empty_fragment = {SQUASHFS_INVALID_FRAG, 0, 0};
struct fragment *get_and_fill_fragment(char *buff, int size)
{
	struct fragment *ffrg;

	if(size == 0)
		return &empty_fragment;

	if(fragment_size + size > block_size)
		write_fragment();

	if((ffrg = (struct fragment *) malloc(sizeof(struct fragment))) == NULL)
		BAD_ERROR("Out of memory in fragment block allocation!\n");

	ffrg->index = fragments;
	ffrg->offset = fragment_size;
	ffrg->size = size;
	memcpy(fragment_data + fragment_size, buff, size);
	fragment_size += size;

	return ffrg;
}


long long write_fragment_table()
{
	long long start_bytes;
	unsigned int frag_bytes = SQUASHFS_FRAGMENT_BYTES(fragments),
		meta_blocks = SQUASHFS_FRAGMENT_INDEXES(fragments);
	char cbuffer[(SQUASHFS_METADATA_SIZE << 2) + 2], buffer[frag_bytes];
	squashfs_fragment_entry *p = (squashfs_fragment_entry *) buffer;
	unsigned short c_byte;
	int i, compressed_size;
	squashfs_fragment_index list[meta_blocks];

	TRACE("write_fragment_table: fragments %d, frag_bytes %d, meta_blocks %d\n", fragments, frag_bytes, meta_blocks);
	for(i = 0; i < fragments; i++, p++) {
		TRACE("write_fragment_table: fragment %d, start_block %llx, size %d\n", i, fragment_table[i].start_block, fragment_table[i].size);
		if(!swap)
			memcpy(p, &fragment_table[i], sizeof(squashfs_fragment_entry));
		else
			SQUASHFS_SWAP_FRAGMENT_ENTRY(&fragment_table[i], p);
	}

	for(i = 0; i < meta_blocks; i++) {
		int avail_bytes = i == meta_blocks - 1 ? frag_bytes % SQUASHFS_METADATA_SIZE : SQUASHFS_METADATA_SIZE;
		c_byte = mangle(cbuffer + block_offset, buffer + i * SQUASHFS_METADATA_SIZE , avail_bytes, SQUASHFS_METADATA_SIZE, noF, 0);
		if(!swap)
			memcpy(cbuffer, &c_byte, sizeof(unsigned short));
		else
			SQUASHFS_SWAP_SHORTS((&c_byte), cbuffer, 1);
		if(check_data)
			*((unsigned char *)(cbuffer + block_offset - 1)) = SQUASHFS_MARKER_BYTE;
		list[i] = bytes;
		compressed_size = SQUASHFS_COMPRESSED_SIZE(c_byte) + block_offset;
		write_bytes(fd, bytes, compressed_size, cbuffer);
		bytes += compressed_size;
	}

	if(!swap)
		write_bytes(fd, bytes, sizeof(list), (char *) list);
	else {
		squashfs_fragment_index slist[meta_blocks];
		SQUASHFS_SWAP_FRAGMENT_INDEXES(list, slist, meta_blocks);
		write_bytes(fd, bytes, sizeof(list), (char *) slist);
	}

	start_bytes = bytes;
	bytes += sizeof(list);

	return start_bytes;
}


char *read_from_buffer(struct duplicate_buffer_handle *handle, unsigned int avail_bytes)
{
	char *v = handle->ptr;
	handle->ptr += avail_bytes;	
	return v;
}


char read_from_file_buffer[SQUASHFS_FILE_MAX_SIZE];
char *read_from_file(struct duplicate_buffer_handle *handle, unsigned int avail_bytes)
{
	read_bytes(fd, handle->start, avail_bytes, read_from_file_buffer);
	handle->start += avail_bytes;
	return read_from_file_buffer;
}


/*
 * Compute 16 bit BSD checksum over the data
 */
unsigned short get_checksum(char *(get_next_file_block)(struct duplicate_buffer_handle *, unsigned int), struct duplicate_buffer_handle *handle, long long l)
{
	unsigned short chksum = 0;
	unsigned int bytes = 0;
	unsigned char *b;
	struct duplicate_buffer_handle position = *handle;

	while(l) {
		bytes = l > SQUASHFS_FILE_MAX_SIZE ? SQUASHFS_FILE_MAX_SIZE : l;
		l -= bytes;
		b = (unsigned char *) get_next_file_block(&position, bytes);
		while(bytes--) {
			chksum = (chksum & 1) ? (chksum >> 1) | 0x8000 : chksum >> 1;
			chksum += *b++;
		}
	}

	return chksum;
}


int cached_frag = -1;
void add_file(long long start, long long file_bytes, unsigned int *block_listp, int blocks, unsigned int fragment, int offset, int bytes)
{
	struct fragment *frg;
	struct file_info *dupl_ptr;
	char *datap;
	struct duplicate_buffer_handle handle;
	unsigned int *block_list = block_listp;

	if(!duplicate_checking)
		return;

	if((frg = (struct fragment *) malloc(sizeof(struct fragment))) == NULL)
		BAD_ERROR("Out of memory in fragment block allocation!\n");

	frg->index = fragment;
	frg->offset = offset;
	frg->size = bytes;
	if(fragment == cached_frag || fragment == SQUASHFS_INVALID_FRAG)
		datap = fragment_data + offset;
	else
		datap = get_fragment(fragment_data, frg);
	handle.start = start;
	if((dupl_ptr = duplicate(read_from_file, &handle, file_bytes, &block_listp, &start, blocks, &frg, datap, bytes)) != NULL)
		dupl_ptr->fragment = frg;
	else
		free(block_list);
	cached_frag = fragment;
}


char cached_fragment[SQUASHFS_FILE_SIZE];
int cached_frag1 = -1;

struct file_info *duplicate(char *(get_next_file_block)(struct duplicate_buffer_handle *, unsigned int), struct duplicate_buffer_handle *file_start, long long bytes, unsigned int **block_list, long long *start, int blocks, struct fragment **fragment, char *frag_data, int frag_bytes)
{
	unsigned short checksum = get_checksum(get_next_file_block, file_start, bytes);
	struct duplicate_buffer_handle handle = { frag_data, 0 };
	unsigned short fragment_checksum = get_checksum(read_from_buffer, &handle, frag_bytes);
	struct file_info *dupl_ptr = bytes ? dupl[checksum] : frag_dups[fragment_checksum];


	for(; dupl_ptr; dupl_ptr = dupl_ptr->next)
		if(bytes == dupl_ptr->bytes && frag_bytes == dupl_ptr->fragment->size && fragment_checksum == dupl_ptr->fragment_checksum) {
			char buffer1[SQUASHFS_FILE_MAX_SIZE];
			long long dup_bytes = dupl_ptr->bytes;
			long long dup_start = dupl_ptr->start;
			struct duplicate_buffer_handle position = *file_start;
			char *buffer;
			while(dup_bytes) {
				int avail_bytes = dup_bytes > SQUASHFS_FILE_MAX_SIZE ? SQUASHFS_FILE_MAX_SIZE : dup_bytes;

				buffer = get_next_file_block(&position, avail_bytes);
				read_bytes(fd, dup_start, avail_bytes, buffer1);
				if(memcmp(buffer, buffer1, avail_bytes) != 0)
					break;
				dup_bytes -= avail_bytes;
				dup_start += avail_bytes;
			}
			if(dup_bytes == 0) {
				char *fragment_buffer1;
				
				if(dupl_ptr->fragment->index == fragments || dupl_ptr->fragment->index == SQUASHFS_INVALID_FRAG)
					fragment_buffer1 = fragment_data + dupl_ptr->fragment->offset;
				else if(dupl_ptr->fragment->index == cached_frag1)
					fragment_buffer1 = cached_fragment + dupl_ptr->fragment->offset;
				else {
					fragment_buffer1 = get_fragment(cached_fragment, dupl_ptr->fragment);
					cached_frag1 = dupl_ptr->fragment->index;
				}

				if(frag_bytes == 0 || memcmp(frag_data, fragment_buffer1, frag_bytes) == 0) {
					TRACE("Found duplicate file, start 0x%llx, size %lld, checksum 0x%x, fragment %d, size %d, offset %d, checksum 0x%x\n", dupl_ptr->start,
						dupl_ptr->bytes, dupl_ptr->checksum, dupl_ptr->fragment->index, frag_bytes, dupl_ptr->fragment->offset, fragment_checksum);
					*block_list = dupl_ptr->block_list;
					*start = dupl_ptr->start;
					*fragment = dupl_ptr->fragment;
					return 0;
				}
			}
		}


	if((dupl_ptr = (struct file_info *) malloc(sizeof(struct file_info))) == NULL) {
		BAD_ERROR("Out of memory in dup_files allocation!\n");
	}

	dupl_ptr->bytes = bytes;
	dupl_ptr->checksum = checksum;
	dupl_ptr->start = *start;
	dupl_ptr->fragment_checksum = fragment_checksum;
	dupl_ptr->block_list = *block_list;

	dup_files ++;
	if(bytes) {
		dupl_ptr->next = dupl[checksum];
		dupl[checksum] = dupl_ptr;
	} else {
		dupl_ptr->next = frag_dups[fragment_checksum];
		frag_dups[fragment_checksum] = dupl_ptr;
	}

	return dupl_ptr;
}


#define MINALLOCBYTES (1024 * 1024)
int write_file(squashfs_inode *inode, struct dir_ent *dir_ent, long long size, int *duplicate_file)
{
	int block = 0, i, file, whole_file = 1, status;
	unsigned int c_byte, frag_bytes;
	long long bbytes, file_bytes = 0, start;
	char buff[block_size], *c_buffer = NULL, *filename = dir_ent->pathname;
	struct fragment *fragment;
	struct file_info *dupl_ptr = NULL;
	struct duplicate_buffer_handle handle;
	long long read_size = (size > SQUASHFS_MAX_FILE_SIZE) ? SQUASHFS_MAX_FILE_SIZE : size;
	int blocks = (read_size + block_size - 1) >> block_log, allocated_blocks = blocks;
	unsigned int *block_list, *block_listp;

	if((block_list = malloc(blocks * sizeof(unsigned int))) == NULL)
		BAD_ERROR("Out of memory allocating block_list\n");
	block_listp = block_list;

	if(!no_fragments && (read_size < block_size || always_use_fragments)) {
		allocated_blocks = blocks = read_size >> block_log;
		frag_bytes = read_size % block_size;
	} else
		frag_bytes = 0;

	if(size > read_size)
		ERROR("file %s truncated to %lld bytes\n", filename, SQUASHFS_MAX_FILE_SIZE);

	total_bytes += read_size;
	if((file = open(filename, O_RDONLY)) == -1)
		goto read_err;

	do {
		long long bytes = (((long long) allocated_blocks) + 1) << block_log;
		if(bytes != ((size_t) bytes) || (c_buffer = (char *) malloc(bytes)) == NULL) {
			TRACE("Out of memory allocating write_file buffer, allocated_blocks %ld, blocks %d\n", allocated_blocks, blocks);
			whole_file = 0;
			if(bytes < MINALLOCBYTES)
				BAD_ERROR("Out of memory allocating write_file buffer, could not allocate %ld blocks (%d Kbytes)\n", allocated_blocks, allocated_blocks << (block_log - 10));
			allocated_blocks >>= 1;
		}
	} while(!c_buffer);

	for(start = bytes; block < blocks; file_bytes += bbytes) {
		for(i = 0, bbytes = 0; (i < allocated_blocks) && (block < blocks); i++) {
			int available_bytes = read_size - (block * block_size) > block_size ? block_size : read_size - (block * block_size);
			if(read(file, buff, available_bytes) == -1)
				goto read_err;
			c_byte = mangle(c_buffer + bbytes, buff, available_bytes, block_size, noD, 1);
			block_list[block ++] = c_byte;
			bbytes += SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte);
		}
		if(!whole_file) {
			write_bytes(fd, bytes, bbytes, c_buffer);
			bytes += bbytes;
		}
	}

	if(frag_bytes != 0)
		if(read(file, buff, frag_bytes) == -1)
			goto read_err;

	close(file);
	if(whole_file) {
		handle.ptr = c_buffer;
		if(duplicate_checking && (dupl_ptr = duplicate(read_from_buffer, &handle, file_bytes, &block_listp, &start, blocks, &fragment, buff, frag_bytes)) == NULL) {
			*duplicate_file = TRUE;
			goto wr_inode;
		}
		write_bytes(fd, bytes, file_bytes, c_buffer);
		bytes += file_bytes;
	} else {
		handle.start = start;
		if(duplicate_checking && (dupl_ptr = duplicate(read_from_file, &handle, file_bytes, &block_listp, &start, blocks, &fragment, buff, frag_bytes)) == NULL) {
			bytes = start;
			if(!block_device)
				ftruncate(fd, bytes);
			*duplicate_file = TRUE;
			goto wr_inode;
		}
	}

	fragment = get_and_fill_fragment(buff, frag_bytes);
	if(duplicate_checking)
		dupl_ptr->fragment = fragment;

	*duplicate_file = FALSE;

wr_inode:
	free(c_buffer);
	file_count ++;
	if(dir_ent->inode->nlink == 1 && read_size < ((long long) (1<<30) - 1))
		status = create_inode(inode, dir_ent, SQUASHFS_FILE_TYPE, read_size, start, blocks, block_listp, fragment, NULL);
	else
		status = create_inode(inode, dir_ent, SQUASHFS_LREG_TYPE, read_size, start, blocks, block_listp, fragment, NULL);
	if(duplicate_checking == FALSE || *duplicate_file == TRUE)
		free(block_list);
	return status;

read_err:
	perror("Error in reading file, skipping...");
	free(c_buffer);
	free(block_list);
	return FALSE;
}


char b_buffer[8192];
char *name;
char *basename_r();

char *getbase(char *pathname)
{
	char *result;

	if(*pathname != '/') {
		result = getenv("PWD");
		strcat(strcat(strcpy(b_buffer, result), "/"), pathname);
	} else
		strcpy(b_buffer, pathname);
	name = b_buffer;
	if(((result = basename_r()) == NULL) || (strcmp(result, "..") == 0))
		return NULL;
	else
		return result;
}


char *basename_r()
{
	char *s;
	char *p;
	int n = 1;

	for(;;) {
		s = name;
		if(*name == '\0')
			return NULL;
		if(*name != '/') {
			while(*name != '\0' && *name != '/') name++;
			n = name - s;
		}
		while(*name == '/') name++;
		if(strncmp(s, ".", n) == 0)
			continue;
		if((*name == '\0') || (strncmp(s, "..", n) == 0) || ((p = basename_r()) == NULL)) {
			s[n] = '\0';
			return s;
		}
		if(strcmp(p, "..") == 0)
			continue;
		return p;
	}
}


struct inode_info *lookup_inode(struct stat *buf)
{
	int inode_hash = INODE_HASH(buf->st_dev, buf->st_ino);
	struct inode_info *inode = inode_info[inode_hash];

	while(inode != NULL) {
		if(memcmp(buf, &inode->buf, sizeof(struct stat)) == 0) {
			inode->nlink ++;
			return inode;
		}
		inode = inode->next;
	}

	if((inode = malloc(sizeof(struct inode_info))) == NULL)
		BAD_ERROR("Out of memory in inode hash table entry allocation\n");

	memcpy(&inode->buf, buf, sizeof(struct stat));
	inode->inode = SQUASHFS_INVALID_BLK;
	inode->nlink = 1;
	if((buf->st_mode & S_IFMT) == S_IFDIR)
		inode->inode_number = dir_inode_no ++;
	else
		inode->inode_number = inode_no ++;

	inode->next = inode_info[inode_hash];
	inode_info[inode_hash] = inode;

	return inode;
}


inline void add_dir_entry(char *name, char *pathname, struct dir_info *sub_dir, struct inode_info *inode_info, void *data, struct dir_info *dir)
{
	if((dir->count % DIR_ENTRIES) == 0)
		if((dir->list = realloc(dir->list, (dir->count + DIR_ENTRIES) * sizeof(struct dir_ent *))) == NULL)
			BAD_ERROR("Out of memory in add_dir_entry\n");

	if((dir->list[dir->count] = malloc(sizeof(struct dir_ent))) == NULL)
		BAD_ERROR("Out of memory in linux_opendir\n");

	if(sub_dir)
		sub_dir->dir_ent = dir->list[dir->count];
	dir->list[dir->count]->name = strdup(name);
	dir->list[dir->count]->pathname = pathname != NULL ? strdup(pathname) : NULL;
	dir->list[dir->count]->inode = inode_info;
	dir->list[dir->count]->dir = sub_dir;
	dir->list[dir->count]->our_dir = dir;
	dir->list[dir->count++]->data = data;
	dir->byte_count += strlen(name) + sizeof(squashfs_dir_entry);
}


int compare_name(const void *ent1_ptr, const void *ent2_ptr)
{
	struct dir_ent *ent1 = *((struct dir_ent **) ent1_ptr);
	struct dir_ent *ent2 = *((struct dir_ent **) ent2_ptr);

	return strcmp(ent1->name, ent2->name);
}


void sort_directory(struct dir_info *dir)
{
	qsort(dir->list, dir->count, sizeof(struct dir_ent *), compare_name);

	if((dir->count < 257 && dir->byte_count < SQUASHFS_METADATA_SIZE))
		dir->dir_is_ldir = FALSE;
}


struct dir_info *scan1_opendir(char *pathname)
{
	DIR	*linuxdir;
	struct dirent *d_name;
	struct dir_info *dir;

	if((dir = malloc(sizeof(struct dir_info))) == NULL)
		return NULL;

	if(pathname[0] != '\0' && (dir->linuxdir = opendir(pathname)) == NULL) {
		free(dir);
		return NULL;
	}
	dir->pathname = strdup(pathname);
	dir->count = dir->directory_count = dir->current_count = dir->byte_count = 0;
	dir->dir_is_ldir = TRUE;
	dir->list = NULL;

	return dir;
}


int scan1_encomp_readdir(char *pathname, char *dir_name, struct dir_info *dir)
{
	int i, n, pass;
	char *basename;
	static int index = 0;

	if(dir->count < old_root_entries)
		for(i = 0; i < old_root_entries; i++) {
			if(old_root_entry[i].type == SQUASHFS_DIR_TYPE)
				dir->directory_count ++;
			add_dir_entry(old_root_entry[i].name, "", NULL, NULL, &old_root_entry[i], dir);
		}

	while(index < source) {
		if((basename = getbase(source_path[index])) == NULL) {
			ERROR("Bad source directory %s - skipping ...\n", source_path[index]);
			index ++;
			continue;
		}
		strcpy(dir_name, basename);
		pass = 1;
		for(;;) {
			for(n = 0; n < dir->count && strcmp(dir->list[n]->name, dir_name) != 0; n++);
			if(n == dir->count)
				break;
			ERROR("Source directory entry %s already used! - trying ", dir_name);
			sprintf(dir_name, "%s_%d", basename, pass++);
			ERROR("%s\n", dir_name);
		}
		strcpy(pathname, source_path[index ++]);
		return 1;
	}
	return 0;
}


int scan1_single_readdir(char *pathname, char *dir_name, struct dir_info *dir)
{
	struct dirent *d_name;
	int i, pass;

	if(dir->count < old_root_entries)
		for(i = 0; i < old_root_entries; i++) {
			if(old_root_entry[i].type == SQUASHFS_DIR_TYPE)
				dir->directory_count ++;
			add_dir_entry(old_root_entry[i].name, "", NULL, NULL, &old_root_entry[i], dir);
		}

	if((d_name = readdir(dir->linuxdir)) != NULL) {
		strcpy(dir_name, d_name->d_name);
		pass = 1;
		for(;;) {
			for(i = 0; i < dir->count && strcmp(dir->list[i]->name, dir_name) != 0; i++);
			if(i == dir->count)
				break;
			ERROR("Source directory entry %s already used! - trying ", dir_name);
			sprintf(dir_name, "%s_%d", d_name->d_name, pass++);
			ERROR("%s\n", dir_name);
		}
		strcat(strcat(strcpy(pathname, dir->pathname), "/"), d_name->d_name);
		return 1;
	}

	return 0;
}


int scan1_readdir(char *pathname, char *dir_name, struct dir_info *dir)
{
	struct dirent *d_name;

	if((d_name = readdir(dir->linuxdir)) != NULL) {
		strcpy(dir_name, d_name->d_name);
		strcat(strcat(strcpy(pathname, dir->pathname), "/"), d_name->d_name);
		return 1;
	}

	return 0;
}


struct dir_ent *scan2_readdir(struct directory *dir, struct dir_info *dir_info)
{
	int current_count;

	while((current_count = dir_info->current_count++) < dir_info->count)
		if(dir_info->list[current_count]->data)
			add_dir(dir_info->list[current_count]->data->inode, dir_info->list[current_count]->data->inode_number,
				dir_info->list[current_count]->name, dir_info->list[current_count]->data->type, dir);
		else 
			return dir_info->list[current_count];
	return FALSE;	
}


void scan1_freedir(struct dir_info *dir)
{
	if(dir->pathname[0] != '\0')
		closedir(dir->linuxdir);
}


void scan2_freedir(struct directory *dir)
{
	if(dir->index)
		free(dir->index);
	free(dir->buff);
}


void dir_scan(squashfs_inode *inode, char *pathname, int (_readdir)(char *, char *, struct dir_info *))
{
	struct dir_info *dir_info = dir_scan1(pathname, _readdir);
	struct dir_ent *dir_ent;
	struct inode_info *inode_info;
	
	if(dir_info == NULL)
		return;

	if((dir_ent = malloc(sizeof(struct dir_ent))) == NULL)
		BAD_ERROR("Out of memory in dir_scan\n");

	if((inode_info = malloc(sizeof(struct inode_info))) == NULL)
		BAD_ERROR("Out of memory in dir_scan\n");

	dir_ent->name = dir_ent->pathname = strdup(pathname);
	dir_ent->dir = dir_info;
	dir_ent->inode = inode_info;
	dir_ent->our_dir = NULL;
	dir_ent->data = NULL;
	inode_info->nlink = 1;
	inode_info->inode_number = root_inode_number ? root_inode_number : dir_inode_no++;
	dir_info->dir_ent = dir_ent;

	if(pathname[0] == '\0') {
		/* dummy top level directory, if multiple sources specified on command line */
		inode_info->buf.st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
		inode_info->buf.st_uid = getuid();
		inode_info->buf.st_gid = getgid();
		inode_info->buf.st_mtime = time(NULL);
	} else if(lstat(pathname, &inode_info->buf) == -1) {
		char buffer[8192];
		sprintf(buffer, "Cannot stat dir/file %s, ignoring", pathname);
		perror(buffer);
		return;
	}
	if(sorted)
		sort_files_and_write(dir_info);
	dir_scan2(inode, dir_info);
}


struct dir_info *dir_scan1(char *pathname, int (_readdir)(char *, char *, struct dir_info *))
{
	struct dir_info *dir, *sub_dir;
	struct stat buf;
	char filename[8192], dir_name[8192];

	if((dir = scan1_opendir(pathname)) == NULL) {
		ERROR("Could not open %s, skipping...\n", pathname);
		goto error;
	}

	while(_readdir(filename, dir_name, dir) != FALSE) {

		if(strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)
			continue;

		if(lstat(filename, &buf) == -1) {
			char buffer[8192];
			sprintf(buffer, "Cannot stat dir/file %s, ignoring", filename);
			perror(buffer);
			continue;
		}
		if(excluded(filename, &buf))
			continue;

		if((buf.st_mode & S_IFMT) == S_IFDIR) {
			if((sub_dir = dir_scan1(filename, scan1_readdir)) == NULL)
				continue;
			dir->directory_count ++;
		} else
			sub_dir = NULL;

		add_dir_entry(dir_name, filename, sub_dir, lookup_inode(&buf), NULL, dir);
	}

	scan1_freedir(dir);
	sort_directory(dir);

error:
	return dir;
}


int dir_scan2(squashfs_inode *inode, struct dir_info *dir_info)
{
	int squashfs_type;
	int result = FALSE;
	int duplicate_file;
	char *pathname = dir_info->pathname;
	struct directory dir;
	struct dir_ent *dir_ent;
	
	scan2_init_dir(&dir);
	
	while((dir_ent = scan2_readdir(&dir, dir_info)) != NULL) {
		struct inode_info *inode_info = dir_ent->inode;
		struct stat *buf = &inode_info->buf;
		char *filename = dir_ent->pathname;
		char *dir_name = dir_ent->name;
		unsigned int inode_number = ((buf->st_mode & S_IFMT) == S_IFDIR) ? dir_ent->inode->inode_number : dir_ent->inode->inode_number + dir_inode_no;

		if(dir_ent->inode->inode == SQUASHFS_INVALID_BLK) {
			switch(buf->st_mode & S_IFMT) {
				case S_IFREG:
					squashfs_type = SQUASHFS_FILE_TYPE;
					result = write_file(inode, dir_ent, buf->st_size, &duplicate_file);
					INFO("file %s, uncompressed size %lld bytes %s\n", filename, buf->st_size, duplicate_file ? "DUPLICATE" : "");
					break;

				case S_IFDIR:
					squashfs_type = SQUASHFS_DIR_TYPE;
					result = dir_scan2(inode, dir_ent->dir);
					break;

				case S_IFLNK:
					squashfs_type = SQUASHFS_SYMLINK_TYPE;
					result = create_inode(inode, dir_ent, squashfs_type, 0, 0, 0, NULL, NULL, NULL);
					INFO("symbolic link %s inode 0x%llx\n", dir_name, *inode);
					sym_count ++;
					break;

				case S_IFCHR:
					squashfs_type = SQUASHFS_CHRDEV_TYPE;
					result = create_inode(inode, dir_ent, squashfs_type, 0, 0, 0, NULL, NULL, NULL);
					INFO("character device %s inode 0x%llx\n", dir_name, *inode);
					dev_count ++;
					break;

				case S_IFBLK:
					squashfs_type = SQUASHFS_BLKDEV_TYPE;
					result = create_inode(inode, dir_ent, squashfs_type, 0, 0, 0, NULL, NULL, NULL);
					INFO("block device %s inode 0x%llx\n", dir_name, *inode);
					dev_count ++;
					break;

				case S_IFIFO:
					squashfs_type = SQUASHFS_FIFO_TYPE;
					result = create_inode(inode, dir_ent, squashfs_type, 0, 0, 0, NULL, NULL, NULL);
					INFO("fifo %s inode 0x%llx\n", dir_name, *inode);
					fifo_count ++;
					break;

				case S_IFSOCK:
					squashfs_type = SQUASHFS_SOCKET_TYPE;
					result = create_inode(inode, dir_ent, squashfs_type, 0, 0, 0, NULL, NULL, NULL);
					INFO("unix domain socket %s inode 0x%llx\n", dir_name, *inode);
					sock_count ++;
					break;

			 	default:
					ERROR("%s unrecognised file type, mode is %x\n", filename, buf->st_mode);
					result = FALSE;
				}
			if(result)
				dir_ent->inode->inode = *inode;
			dir_ent->inode->type = squashfs_type;
		 } else {
			*inode = dir_ent->inode->inode;
			squashfs_type = dir_ent->inode->type;
			switch(squashfs_type) {
				case SQUASHFS_FILE_TYPE:
					if(!sorted)
						INFO("file %s, uncompressed size %lld bytes LINK\n", filename, buf->st_size);
					break;
				case SQUASHFS_SYMLINK_TYPE:
					INFO("symbolic link %s inode 0x%llx LINK\n", dir_name, *inode);
					break;
				case SQUASHFS_CHRDEV_TYPE:
					INFO("character device %s inode 0x%llx LINK\n", dir_name, *inode);
					break;
				caseSQUASHFS_BLKDEV_TYPE:
					INFO("block device %s inode 0x%llx LINK\n", dir_name, *inode);
					break;
				case SQUASHFS_FIFO_TYPE:
					INFO("fifo %s inode 0x%llx LINK\n", dir_name, *inode);
					break;
				case SQUASHFS_SOCKET_TYPE:
					INFO("unix domain socket %s inode 0x%llx LINK\n", dir_name, *inode);
					break;
			}
			result = TRUE;
		}
		

		if(result)
			add_dir(*inode, inode_number, dir_name, squashfs_type, &dir);
	}

	result = write_dir(inode, dir_info, &dir);
	INFO("directory %s inode 0x%llx\n", pathname, *inode);

	scan2_freedir(&dir);

	return result;
}


unsigned int slog(unsigned int block)
{
	int i;

	for(i = 12; i <= 16; i++)
		if(block == (1 << i))
			return i;
	return 0;
}


int excluded(char *filename, struct stat *buf)
{
	int i;

	for(i = 0; i < exclude; i++)
		if((exclude_paths[i].st_dev == buf->st_dev) && (exclude_paths[i].st_ino == buf->st_ino))
			return TRUE;
	return FALSE;
}


#define ADD_ENTRY(buf) \
	if(exclude % EXCLUDE_SIZE == 0) {\
		if((exclude_paths = (struct exclude_info *) realloc(exclude_paths, (exclude + EXCLUDE_SIZE) * sizeof(struct exclude_info))) == NULL)\
			BAD_ERROR("Out of memory in exclude dir/file table\n");\
	}\
	exclude_paths[exclude].st_dev = buf.st_dev;\
	exclude_paths[exclude++].st_ino = buf.st_ino;
int add_exclude(char *path)
{
	int i;
	char buffer[4096], filename[4096];
	struct stat buf;

	if(path[0] == '/' || strncmp(path, "./", 2) == 0 || strncmp(path, "../", 3) == 0) {
		if(lstat(path, &buf) == -1) {
			sprintf(buffer, "Cannot stat exclude dir/file %s, ignoring", path);
			perror(buffer);
			return TRUE;
		}
		ADD_ENTRY(buf);
		return TRUE;
	}

	for(i = 0; i < source; i++) {
		strcat(strcat(strcpy(filename, source_path[i]), "/"), path);
		if(lstat(filename, &buf) == -1) {
			if(!(errno == ENOENT || errno == ENOTDIR)) {
				sprintf(buffer, "Cannot stat exclude dir/file %s, ignoring", filename);
				perror(buffer);
			}
			continue;
		}
		ADD_ENTRY(buf);
	}
	return TRUE;
}


void add_old_root_entry(char *name, squashfs_inode inode, int inode_number, int type)
{
	if((old_root_entry = (struct old_root_entry_info *) realloc(old_root_entry, sizeof(struct old_root_entry_info)
				* (old_root_entries + 1))) == NULL)
		BAD_ERROR("Out of memory in old root directory entries reallocation\n");

	strcpy(old_root_entry[old_root_entries].name, name);
	old_root_entry[old_root_entries].inode = inode;
	old_root_entry[old_root_entries].inode_number = inode_number;
	old_root_entry[old_root_entries++].type = type;
}


#define VERSION() \
	printf("mksquashfs version 3.0 (2006/03/15)\n");\
	printf("copyright (C) 2006 Phillip Lougher <phillip@lougher.org.uk>\n\n"); \
    	printf("This program is free software; you can redistribute it and/or\n");\
	printf("modify it under the terms of the GNU General Public License\n");\
	printf("as published by the Free Software Foundation; either version 2,\n");\
	printf("or (at your option) any later version.\n\n");\
	printf("This program is distributed in the hope that it will be useful,\n");\
	printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");\
	printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");\
	printf("GNU General Public License for more details.\n");
int main(int argc, char *argv[])
{
	struct stat buf, source_buf;
	int i;
	squashfs_super_block sBlk;
	char *b, *root_name = NULL;
	int be, nopad = FALSE, keep_as_directory = FALSE, orig_be;
	squashfs_inode inode;

#if __BYTE_ORDER == __BIG_ENDIAN
	be = TRUE;
#else
	be = FALSE;
#endif

	block_log = slog(block_size);
	if(argc > 1 && strcmp(argv[1], "-version") == 0) {
		VERSION();
		exit(0);
	}
        for(i = 1; i < argc && argv[i][0] != '-'; i++);
	if(i < 3)
		goto printOptions;
	source_path = argv + 1;
	source = i - 2;
	for(; i < argc; i++) {
		if(strcmp(argv[i], "-b") == 0) {
			if((++i == argc) || (block_size = strtol(argv[i], &b, 10), *b !='\0')) {
				ERROR("%s: -b missing or invalid block size\n", argv[0]);
				exit(1);
			}

			if((block_log = slog(block_size)) == 0) {
				ERROR("%s: -b block size not power of two or not between 4096 and 64K\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-ef") == 0) {
			if(++i == argc) {
				ERROR("%s: -ef missing filename\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-no-duplicates") == 0)
			duplicate_checking = FALSE;

		else if(strcmp(argv[i], "-no-fragments") == 0)
			no_fragments = TRUE;

		 else if(strcmp(argv[i], "-always-use-fragments") == 0)
			always_use_fragments = TRUE;

		 else if(strcmp(argv[i], "-sort") == 0) {
			if(++i == argc) {
				ERROR("%s: -sort missing filename\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-all-root") == 0 ||
				strcmp(argv[i], "-root-owned") == 0)
			global_uid = global_gid = 0;

		else if(strcmp(argv[i], "-force-uid") == 0) {
			if(++i == argc) {
				ERROR("%s: -force-uid missing uid or user\n", argv[0]);
				exit(1);
			}
			if((global_uid = strtoll(argv[i], &b, 10)), *b =='\0') {
				if(global_uid < 0 || global_uid > (((long long) 1 << 32) - 1)) {
					ERROR("%s: -force-uid uid out of range\n", argv[0]);
					exit(1);
				}
			} else {
				struct passwd *uid = getpwnam(argv[i]);
				if(uid)
					global_uid = uid->pw_uid;
				else {
					ERROR("%s: -force-uid invalid uid or unknown user\n", argv[0]);
					exit(1);
				}
			}
		} else if(strcmp(argv[i], "-force-gid") == 0) {
			if(++i == argc) {
				ERROR("%s: -force-gid missing gid or group\n", argv[0]);
				exit(1);
			}
			if((global_gid = strtoll(argv[i], &b, 10)), *b =='\0') {
				if(global_gid < 0 || global_gid > (((long long) 1 << 32) - 1)) {
					ERROR("%s: -force-gid gid out of range\n", argv[0]);
					exit(1);
				}
			} else {
				struct group *gid = getgrnam(argv[i]);
				if(gid)
					global_gid = gid->gr_gid;
				else {
					ERROR("%s: -force-gid invalid gid or unknown group\n", argv[0]);
					exit(1);
				}
			}
		} else if(strcmp(argv[i], "-noI") == 0 ||
				strcmp(argv[i], "-noInodeCompression") == 0)
			noI = TRUE;

		else if(strcmp(argv[i], "-noD") == 0 ||
				strcmp(argv[i], "-noDataCompression") == 0)
			noD = TRUE;

		else if(strcmp(argv[i], "-noF") == 0 ||
				strcmp(argv[i], "-noFragmentCompression") == 0)
			noF = TRUE;

		else if(strcmp(argv[i], "-nopad") == 0)
			nopad = TRUE;

		else if(strcmp(argv[i], "-check_data") == 0)
			check_data = TRUE;

		else if(strcmp(argv[i], "-info") == 0)
			silent = 0;

		else if(strcmp(argv[i], "-be") == 0)
			be = TRUE;

		else if(strcmp(argv[i], "-le") == 0)
			be = FALSE;

		else if(strcmp(argv[i], "-e") == 0)
			break;

		else if(strcmp(argv[i], "-noappend") == 0)
			delete = TRUE;

		else if(strcmp(argv[i], "-keep-as-directory") == 0)
			keep_as_directory = TRUE;

		else if(strcmp(argv[i], "-root-becomes") == 0) {
			if(++i == argc) {
				ERROR("%s: -root-becomes: missing name\n", argv[0]);
				exit(1);
			}	
			root_name = argv[i];
		} else if(strcmp(argv[i], "-version") == 0) {
			VERSION();
		} else {
			ERROR("%s: invalid option\n\n", argv[0]);
printOptions:
			ERROR("SYNTAX:%s source1 source2 ...  dest [options] [-e list of exclude\ndirs/files]\n", argv[0]);
			ERROR("\nOptions are\n");
			ERROR("-version\t\tprint version, licence and copyright message\n");
			ERROR("-info\t\t\tprint files written to filesystem\n");
			ERROR("-b <block_size>\t\tset data block to <block_size>.  Default %d bytes\n", SQUASHFS_FILE_SIZE);
			ERROR("-noI\t\t\tdo not compress inode table\n");
			ERROR("-noD\t\t\tdo not compress data blocks\n");
			ERROR("-noF\t\t\tdo not compress fragment blocks\n");
			ERROR("-no-fragments\t\tdo not use fragments\n");
			ERROR("-always-use-fragments\tuse fragment blocks for files larger than block size\n");
			ERROR("-no-duplicates\t\tdo not perform duplicate checking\n");
			ERROR("-noappend\t\tdo not append to existing filesystem\n");
			ERROR("-keep-as-directory\tif one source directory is specified, create a root\n");
			ERROR("\t\t\tdirectory containing that directory, rather than the\n");
			ERROR("\t\t\tcontents of the directory\n");
			ERROR("-root-becomes <name>\twhen appending source files/directories, make the\n");
			ERROR("\t\t\toriginal root become a subdirectory in the new root\n");
			ERROR("\t\t\tcalled <name>, rather than adding the new source items\n");
			ERROR("\t\t\tto the original root\n");
			ERROR("-all-root\t\tmake all files owned by root\n");
			ERROR("-force-uid uid\t\tset all file uids to uid\n");
			ERROR("-force-gid gid\t\tset all file gids to gid\n");
			ERROR("-le\t\t\tcreate a little endian filesystem\n");
			ERROR("-be\t\t\tcreate a big endian filesystem\n");
			ERROR("-nopad\t\t\tdo not pad filesystem to a multiple of 4K\n");
			ERROR("-check_data\t\tadd checkdata for greater filesystem checks\n");
			ERROR("-root-owned\t\talternative name for -all-root\n");
			ERROR("-noInodeCompression\talternative name for -noI\n");
			ERROR("-noDataCompression\talternative name for -noD\n");
			ERROR("-noFragmentCompression\talternative name for -noF\n");
			ERROR("-sort <sort_file>\tsort files according to priorities in <sort_file>.  One\n");
			ERROR("\t\t\tfile or dir with priority per line.  Priority -32768 to\n");
			ERROR("\t\t\t32767, default priority 0\n");
			ERROR("-ef <exclude_file>\tlist of exclude dirs/files.  One per line\n");
			exit(1);
		}
	}

        for(i = 0; i < source; i++)
                if(stat(source_path[i], &source_buf) == -1) {
                        fprintf(stderr, "Cannot stat source directory \"%s\" because %s\n", source_path[i], strerror(errno));
                        EXIT_MKSQUASHFS();
                }

	destination_file = argv[source + 1];
	if(stat(argv[source + 1], &buf) == -1) {
		if(errno == ENOENT) { /* Does not exist */
			if((fd = open(argv[source + 1], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1) {
				perror("Could not create destination file");
				exit(1);
			}
			delete = TRUE;
		} else {
			perror("Could not stat destination file");
			exit(1);
		}

	} else {
		if(S_ISBLK(buf.st_mode)) {
			if((fd = open(argv[source + 1], O_RDWR)) == -1) {
				perror("Could not open block device as destination");
				exit(1);
			}
			block_device = 1;

		} else if(S_ISREG(buf.st_mode))	 {
			if((fd = open(argv[source + 1], (delete ? O_TRUNC : 0) | O_RDWR)) == -1) {
				perror("Could not open regular file for writing as destination");
				exit(1);
			}
		}
		else {
			ERROR("Destination not block device or regular file\n");
			exit(1);
		}

	}

	if(!delete) {
	        if(read_super(fd, &sBlk, &orig_be, argv[source + 1]) == 0) {
			ERROR("Failed to read existing filesystem - will not overwrite - ABORTING!\n");
			EXIT_MKSQUASHFS();
		}
	} else  {
		signal(SIGTERM, sighandler2);
		signal(SIGINT, sighandler2);
	}

	/* process the exclude files - must be done afer destination file has been possibly created */
	for(i = source + 2; i < argc; i++)
		if(strcmp(argv[i], "-ef") == 0) {
			FILE *fd;
			char filename[16385];
			if((fd = fopen(argv[++i], "r")) == NULL) {
				perror("Could not open exclude file...");
				EXIT_MKSQUASHFS();
			}
			while(fscanf(fd, "%16384[^\n]\n", filename) != EOF)
				add_exclude(filename);
			fclose(fd);
		} else if(strcmp(argv[i], "-e") == 0)
			break;
		else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-root-becomes") == 0 || strcmp(argv[i], "-sort") == 0)
			i++;

	if(i != argc) {
		if(++i == argc) {
			ERROR("%s: -e missing arguments\n", argv[0]);
			EXIT_MKSQUASHFS();
		}
		while(i < argc && add_exclude(argv[i++]));
	}

	/* process the sort files - must be done afer the exclude files  */
	for(i = source + 2; i < argc; i++)
		if(strcmp(argv[i], "-sort") == 0) {
			read_sort_file(argv[++i], source, source_path);
			sorted ++;
		} else if(strcmp(argv[i], "-e") == 0)
			break;
		else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-root-becomes") == 0 || strcmp(argv[i], "-ef") == 0)
			i++;

	if(delete) {
		printf("Creating %s %d.%d filesystem on %s, block size %d.\n",
				be ? "big endian" : "little endian", SQUASHFS_MAJOR, SQUASHFS_MINOR, argv[source + 1], block_size);
		bytes = sizeof(squashfs_super_block);
	} else {
		unsigned int last_directory_block, inode_dir_offset, inode_dir_file_size, root_inode_size,
		inode_dir_start_block, uncompressed_data, compressed_data, inode_dir_inode_number,
		inode_dir_parent_inode;
		unsigned int root_inode_start = SQUASHFS_INODE_BLK(sBlk.root_inode), root_inode_offset =
		SQUASHFS_INODE_OFFSET(sBlk.root_inode);

		be = orig_be;
		block_log = slog(block_size = sBlk.block_size);
		noI = SQUASHFS_UNCOMPRESSED_INODES(sBlk.flags);
		noD = SQUASHFS_UNCOMPRESSED_DATA(sBlk.flags);
		noF = SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk.flags);
		check_data = SQUASHFS_CHECK_DATA(sBlk.flags);
		no_fragments = SQUASHFS_NO_FRAGMENTS(sBlk.flags);
		always_use_fragments = SQUASHFS_ALWAYS_FRAGMENTS(sBlk.flags);
		duplicate_checking = SQUASHFS_DUPLICATES(sBlk.flags);
		
		if((bytes = read_filesystem(root_name, fd, &sBlk, &inode_table, &data_cache,
				&directory_table, &directory_data_cache, &last_directory_block, &inode_dir_offset,
				&inode_dir_file_size, &root_inode_size, &inode_dir_start_block,
				&file_count, &sym_count, &dev_count, &dir_count, &fifo_count, &sock_count,
				(squashfs_uid *) uids, &uid_count, (squashfs_uid *) guids, &guid_count,
				&total_bytes, &total_inode_bytes, &total_directory_bytes, &inode_dir_inode_number,
				&inode_dir_parent_inode, add_old_root_entry, &fragment_table)) == 0) {
			ERROR("Failed to read existing filesystem - will not overwrite - ABORTING!\n");
			EXIT_MKSQUASHFS();
		}
		if((fragments = sBlk.fragments))
			fragment_table = (squashfs_fragment_entry *) realloc((char *) fragment_table, ((fragments + FRAG_SIZE - 1) & ~(FRAG_SIZE - 1)) * sizeof(squashfs_fragment_entry)); 

		printf("Appending to existing %s %d.%d filesystem on %s, block size %d\n", be ? "big endian" :
			"little endian", SQUASHFS_MAJOR, SQUASHFS_MINOR, argv[source + 1], block_size);
		printf("All -be, -le, -b, -noI, -noD, -noF, -check_data, no-duplicates, no-fragments, -always-use-fragments and -2.0 options ignored\n");
		printf("\nIf appending is not wanted, please re-run with -noappend specified!\n\n");

		compressed_data = inode_dir_offset + inode_dir_file_size & ~(SQUASHFS_METADATA_SIZE - 1);
		uncompressed_data = inode_dir_offset + inode_dir_file_size & (SQUASHFS_METADATA_SIZE - 1);
		
		/* save original filesystem state for restoring ... */
		sfragments = fragments;
		sbytes = bytes;
		sinode_count = sBlk.inodes;
		sdata_cache = (char *)malloc(scache_bytes = root_inode_offset + root_inode_size);
		sdirectory_data_cache = (char *)malloc(sdirectory_cache_bytes = uncompressed_data);
		memcpy(sdata_cache, data_cache, scache_bytes);
		memcpy(sdirectory_data_cache, directory_data_cache + compressed_data, sdirectory_cache_bytes);
		sinode_bytes = root_inode_start;
		sdirectory_bytes = last_directory_block;
		suid_count = uid_count;
		sguid_count = guid_count;
		stotal_bytes = total_bytes;
		stotal_inode_bytes = total_inode_bytes;
		stotal_directory_bytes = total_directory_bytes + compressed_data;
		sfile_count = file_count;
		ssym_count = sym_count;
		sdev_count = dev_count;
		sdir_count = dir_count + 1;
		sfifo_count = fifo_count;
		ssock_count = sock_count;
		sdup_files = dup_files;
		restore = TRUE;
		if(setjmp(env))
			goto restore_filesystem;
		signal(SIGTERM, sighandler);
		signal(SIGINT, sighandler);
		write_bytes(fd, SQUASHFS_START, 4, "\0\0\0\0");

		/* set the filesystem state up to be able to append to the original filesystem.  The filesystem state
		 * differs depending on whether we're appending to the original root directory, or if the original
		 * root directory becomes a sub-directory (root-becomes specified on command line, here root_name != NULL)
		 */
		inode_bytes = inode_size = root_inode_start;
		directory_size = last_directory_block;
		cache_size = root_inode_offset + root_inode_size;
		directory_cache_size = inode_dir_offset + inode_dir_file_size;
		if(root_name) {
			root_inode_number = inode_dir_parent_inode;
			dir_inode_no = sBlk.inodes + 2;
			directory_bytes = last_directory_block;
			directory_cache_bytes = uncompressed_data;
			memmove(directory_data_cache, directory_data_cache + compressed_data, uncompressed_data);
			cache_bytes = root_inode_offset + root_inode_size;
			add_old_root_entry(root_name, sBlk.root_inode, inode_dir_inode_number, SQUASHFS_DIR_TYPE);
			total_directory_bytes += compressed_data;
			dir_count ++;
		} else {
			root_inode_number = inode_dir_inode_number;
			dir_inode_no = sBlk.inodes + 1;
			directory_bytes = inode_dir_start_block;
			directory_cache_bytes = inode_dir_offset;
			cache_bytes = root_inode_offset;
		}

		inode_count = file_count + dir_count + sym_count + dev_count + fifo_count + sock_count;
	}

#if __BYTE_ORDER == __BIG_ENDIAN
	swap = !be;
#else
	swap = be;
#endif

	block_offset = check_data ? 3 : 2;

	if(delete && !keep_as_directory && source == 1 && S_ISDIR(source_buf.st_mode))
		dir_scan(&inode, source_path[0], scan1_readdir);
	else if(!keep_as_directory && source == 1 && S_ISDIR(source_buf.st_mode))
		dir_scan(&inode, source_path[0], scan1_single_readdir);
	else
		dir_scan(&inode, "", scan1_encomp_readdir);
	sBlk.root_inode = inode;
	sBlk.inodes = inode_count;
	sBlk.s_magic = SQUASHFS_MAGIC;
	sBlk.s_major = SQUASHFS_MAJOR;
	sBlk.s_minor = SQUASHFS_MINOR;
	sBlk.block_size = block_size;
	sBlk.block_log = block_log;
	sBlk.flags = SQUASHFS_MKFLAGS(noI, noD, check_data, noF, no_fragments, always_use_fragments, duplicate_checking);
	sBlk.mkfs_time = time(NULL);

restore_filesystem:
	write_fragment();
	sBlk.fragments = fragments;
	sBlk.inode_table_start = write_inodes();
	sBlk.directory_table_start = write_directories();
	sBlk.fragment_table_start = write_fragment_table();

	TRACE("sBlk->inode_table_start 0x%x\n", sBlk.inode_table_start);
	TRACE("sBlk->directory_table_start 0x%x\n", sBlk.directory_table_start);
	TRACE("sBlk->fragment_table_start 0x%x\n", sBlk.fragment_table_start);

	if(sBlk.no_uids = uid_count) {
		if(!swap)
			write_bytes(fd, bytes, uid_count * sizeof(squashfs_uid), (char *) uids);
		else {
			squashfs_uid uids_copy[uid_count];

			SQUASHFS_SWAP_DATA(uids, uids_copy, uid_count, sizeof(squashfs_uid) * 8);
			write_bytes(fd, bytes, uid_count * sizeof(squashfs_uid), (char *) uids_copy);
		}
		sBlk.uid_start = bytes;
		bytes += uid_count * sizeof(squashfs_uid);
	} else
		sBlk.uid_start = 0;

	if(sBlk.no_guids = guid_count) {
		if(!swap)
			write_bytes(fd, bytes, guid_count * sizeof(squashfs_uid), (char *) guids);
		else {
			squashfs_uid guids_copy[guid_count];

			SQUASHFS_SWAP_DATA(guids, guids_copy, guid_count, sizeof(squashfs_uid) * 8);
			write_bytes(fd, bytes, guid_count * sizeof(squashfs_uid), (char *) guids_copy);
		}
		sBlk.guid_start = bytes;
		bytes += guid_count * sizeof(squashfs_uid);
	} else
		sBlk.guid_start = 0;

	sBlk.bytes_used = bytes;

	sBlk.unused = SQUASHFS_INVALID_BLK;

	if(!swap)
		write_bytes(fd, SQUASHFS_START, sizeof(squashfs_super_block), (char *) &sBlk);
	else {
		squashfs_super_block sBlk_copy;

		SQUASHFS_SWAP_SUPER_BLOCK((&sBlk), &sBlk_copy); 
		write_bytes(fd, SQUASHFS_START, sizeof(squashfs_super_block), (char *) &sBlk_copy);
	}

	if(!nopad && (i = bytes & (4096 - 1))) {
		char temp[4096] = {0};
		write_bytes(fd, bytes, 4096 - i, temp);
	}

	total_bytes += total_inode_bytes + total_directory_bytes + uid_count
		* sizeof(unsigned short) + guid_count * sizeof(unsigned short) +
		sizeof(squashfs_super_block);

	printf("\n%s filesystem, data block size %d, %s data, %s metadata, %s fragments\n", be ?
		"Big endian" : "Little endian", block_size, noD ? "uncompressed" : "compressed", noI ?
	"uncompressed" : "compressed", no_fragments ? "no" : noF ? "uncompressed" : "compressed");
	printf("Filesystem size %.2f Kbytes (%.2f Mbytes)\n", bytes / 1024.0, bytes / (1024.0 * 1024.0));
	printf("\t%.2f%% of uncompressed filesystem size (%.2f Kbytes)\n",
		((float) bytes / total_bytes) * 100.0, total_bytes / 1024.0);
	printf("Inode table size %d bytes (%.2f Kbytes)\n",
		inode_bytes, inode_bytes / 1024.0);
	printf("\t%.2f%% of uncompressed inode table size (%d bytes)\n",
		((float) inode_bytes / total_inode_bytes) * 100.0, total_inode_bytes);
	printf("Directory table size %d bytes (%.2f Kbytes)\n",
		directory_bytes, directory_bytes / 1024.0);
	printf("\t%.2f%% of uncompressed directory table size (%d bytes)\n",
		((float) directory_bytes / total_directory_bytes) * 100.0, total_directory_bytes);
	if(duplicate_checking)
		printf("Number of duplicate files found %d\n", file_count - dup_files);
	else
		printf("No duplicate files removed\n");
	printf("Number of inodes %d\n", inode_count);
	printf("Number of files %d\n", file_count);
	if(!no_fragments)
		printf("Number of fragments %d\n", fragments);
	printf("Number of symbolic links  %d\n", sym_count);
	printf("Number of device nodes %d\n", dev_count);
	printf("Number of fifo nodes %d\n", fifo_count);
	printf("Number of socket nodes %d\n", sock_count);
	printf("Number of directories %d\n", dir_count);
	printf("Number of uids %d\n", uid_count);

	for(i = 0; i < uid_count; i++) {
		struct passwd *user = getpwuid(uids[i]);
		printf("\t%s (%d)\n", user == NULL ? "unknown" : user->pw_name, uids[i]);
	}

	printf("Number of gids %d\n", guid_count);

	for(i = 0; i < guid_count; i++) {
		struct group *group = getgrgid(guids[i]);
		printf("\t%s (%d)\n", group == NULL ? "unknown" : group->gr_name, guids[i]);
	}
	close(fd);
	return 0;
}

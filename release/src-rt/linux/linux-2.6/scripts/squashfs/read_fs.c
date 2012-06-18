/*
 * Read a squashfs filesystem.  This is a highly compressed read only filesystem.
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
 * read_fs.c
 */

extern void read_bytes(int, long long, int, char *);
extern int add_file(long long, long long, unsigned int *, int, unsigned int, int, int);

#define TRUE 1
#define FALSE 0
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include <sys/mman.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include <squashfs_fs.h>
#include "read_fs.h"
#include "global.h"

#include <stdlib.h>

#ifdef SQUASHFS_TRACE
#define TRACE(s, args...)		do { \
						printf("mksquashfs: "s, ## args); \
					} while(0)
#else
#define TRACE(s, args...)
#endif

#define ERROR(s, args...)		do { \
						fprintf(stderr, s, ## args); \
					} while(0)

int swap;

int read_block(int fd, long long start, long long *next, unsigned char *block, squashfs_super_block *sBlk)
{
	unsigned short c_byte;
	int offset = 2;
	
	if(swap) {
		read_bytes(fd, start, 2, (char *) block);
		((unsigned char *) &c_byte)[1] = block[0];
		((unsigned char *) &c_byte)[0] = block[1]; 
	} else 
		read_bytes(fd, start, 2, (char *)&c_byte);

	if(SQUASHFS_CHECK_DATA(sBlk->flags))
		offset = 3;
	if(SQUASHFS_COMPRESSED(c_byte)) {
		char buffer[SQUASHFS_METADATA_SIZE];
		int res;
		unsigned long bytes = SQUASHFS_METADATA_SIZE;

		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);
		read_bytes(fd, start + offset, c_byte, buffer);

		if((res = uncompress(block, &bytes, (const unsigned char *) buffer, c_byte)) != Z_OK) {
			if(res == Z_MEM_ERROR)
				ERROR("zlib::uncompress failed, not enough memory\n");
			else if(res == Z_BUF_ERROR)
				ERROR("zlib::uncompress failed, not enough room in output buffer\n");
			else
				ERROR("zlib::uncompress failed, unknown error %d\n", res);
			return 0;
		}
		if(next)
			*next = start + offset + c_byte;
		return bytes;
	} else {
		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);
		read_bytes(fd, start + offset, c_byte, (char *) block);
		if(next)
			*next = start + offset + c_byte;
		return c_byte;
	}
}


int scan_inode_table(int fd, long long start, long long end, long long root_inode_start, int root_inode_offset,
		squashfs_super_block *sBlk,
		squashfs_inode_header *dir_inode, unsigned char **inode_table, unsigned int *root_inode_block,
		unsigned int *root_inode_size, long long *uncompressed_file, unsigned int *uncompressed_directory,
		int *file_count, int *sym_count, int *dev_count, int *dir_count, int *fifo_count, int *sock_count)
{
	unsigned char *cur_ptr;
	int byte, bytes = 0, size = 0, files = 0;
	squashfs_reg_inode_header inode;
	unsigned int directory_start_block;

	TRACE("scan_inode_table: start 0x%llx, end 0x%llx, root_inode_start 0x%llx\n", start, end, root_inode_start);
	while(start < end) {
		if(start == root_inode_start) {
			TRACE("scan_inode_table: read compressed block 0x%llx containing root inode\n", start);
			*root_inode_block = bytes;
		}
		if((size - bytes < SQUASHFS_METADATA_SIZE) &&
				((*inode_table = realloc(*inode_table, size += SQUASHFS_METADATA_SIZE)) == NULL))
			return FALSE;
		TRACE("scan_inode_table: reading block 0x%llx\n", start);
		if((byte = read_block(fd, start, &start, *inode_table + bytes, sBlk)) == 0) {
			free(*inode_table);
			return FALSE;
		}
		bytes += byte;
	}

	/*
	 * Read last inode entry which is the root directory inode, and obtain the last
	 * directory start block index.  This is used when calculating the total uncompressed
	 * directory size.  The directory bytes in the last block will be counted as normal.
	 *
	 * The root inode is ignored in the inode scan.  This ensures there is
	 * always enough bytes left to read a regular file inode entry
	 */
	*root_inode_size = bytes - (*root_inode_block + root_inode_offset);
	bytes = *root_inode_block + root_inode_offset;
	if(swap) {
		squashfs_base_inode_header sinode;
		memcpy(&sinode, *inode_table + bytes, sizeof(dir_inode->base));
		SQUASHFS_SWAP_BASE_INODE_HEADER(&dir_inode->base, &sinode, sizeof(squashfs_base_inode_header));
	} else
		memcpy(&dir_inode->base, *inode_table + bytes, sizeof(dir_inode->base));
	if(dir_inode->base.inode_type == SQUASHFS_DIR_TYPE) {
		if(swap) {
			squashfs_dir_inode_header sinode;
			memcpy(&sinode, *inode_table + bytes, sizeof(dir_inode->dir));
			SQUASHFS_SWAP_DIR_INODE_HEADER(&dir_inode->dir, &sinode);
		} else
			memcpy(&dir_inode->dir, *inode_table + bytes, sizeof(dir_inode->dir));
		directory_start_block = dir_inode->dir.start_block;
	} else {
		if(swap) {
			squashfs_ldir_inode_header sinode;
			memcpy(&sinode, *inode_table + bytes, sizeof(dir_inode->ldir));
			SQUASHFS_SWAP_LDIR_INODE_HEADER(&dir_inode->ldir, &sinode);
		} else
			memcpy(&dir_inode->ldir, *inode_table + bytes, sizeof(dir_inode->ldir));
		directory_start_block = dir_inode->ldir.start_block;
	}

	for(cur_ptr = *inode_table; cur_ptr < *inode_table + bytes; files ++) {
		if(swap) {
			squashfs_reg_inode_header sinode;
			memcpy(&sinode, cur_ptr, sizeof(inode));
			SQUASHFS_SWAP_REG_INODE_HEADER(&inode, &sinode);
		} else
			memcpy(&inode, cur_ptr, sizeof(inode));

		TRACE("scan_inode_table: processing inode @ byte position 0x%x, type 0x%x\n", cur_ptr - *inode_table,
				inode.inode_type);
		switch(inode.inode_type) {
			case SQUASHFS_FILE_TYPE: {
				int frag_bytes = inode.fragment == SQUASHFS_INVALID_FRAG ? 0 : inode.file_size % sBlk->block_size;
				int blocks = inode.fragment == SQUASHFS_INVALID_FRAG ? (inode.file_size
					+ sBlk->block_size - 1) >> sBlk->block_log : inode.file_size >>
					sBlk->block_log;
				long long file_bytes = 0;
				int i, start = inode.start_block;
				unsigned int *block_list;

				TRACE("scan_inode_table: regular file, file_size %lld, blocks %d\n", inode.file_size, blocks);

				if((block_list = malloc(blocks * sizeof(unsigned int))) == NULL) {
					ERROR("Out of memory in block list malloc\n");
					goto failed;
				}

				cur_ptr += sizeof(inode);
				if(swap) {
					unsigned int sblock_list[blocks];
					memcpy(sblock_list, cur_ptr, blocks * sizeof(unsigned int));
					SQUASHFS_SWAP_INTS(block_list, sblock_list, blocks);
				} else
					memcpy(block_list, cur_ptr, blocks * sizeof(unsigned int));

				*uncompressed_file += inode.file_size;
				(*file_count) ++;

				for(i = 0; i < blocks; i++)
					file_bytes += SQUASHFS_COMPRESSED_SIZE_BLOCK(block_list[i]);

	                        add_file(start, file_bytes, block_list, blocks, inode.fragment, inode.offset, frag_bytes);
				cur_ptr += blocks * sizeof(unsigned int);
				break;
			}	
			case SQUASHFS_LREG_TYPE: {
				squashfs_lreg_inode_header inode;
				int frag_bytes;
				int blocks;
				long long file_bytes = 0;
				int i, start;
				unsigned int *block_list;

				if(swap) {
					squashfs_lreg_inode_header sinodep;
					memcpy(&sinodep, cur_ptr, sizeof(sinodep));
					SQUASHFS_SWAP_LREG_INODE_HEADER(&inode, &sinodep);
				} else
					memcpy(&inode, cur_ptr, sizeof(inode));

				TRACE("scan_inode_table: extended regular file, file_size %lld, blocks %d\n", inode.file_size, blocks);

				cur_ptr += sizeof(inode);
				frag_bytes = inode.fragment == SQUASHFS_INVALID_FRAG ? 0 : inode.file_size % sBlk->block_size;
				blocks = inode.fragment == SQUASHFS_INVALID_FRAG ? (inode.file_size
					+ sBlk->block_size - 1) >> sBlk->block_log : inode.file_size >>
					sBlk->block_log;
				start = inode.start_block;

				if((block_list = malloc(blocks * sizeof(unsigned int))) == NULL) {
					ERROR("Out of memory in block list malloc\n");
					goto failed;
				}

				if(swap) {
					unsigned int sblock_list[blocks];
					memcpy(sblock_list, cur_ptr, blocks * sizeof(unsigned int));
					SQUASHFS_SWAP_INTS(block_list, sblock_list, blocks);
				} else
					memcpy(block_list, cur_ptr, blocks * sizeof(unsigned int));

				*uncompressed_file += inode.file_size;
				(*file_count) ++;

				for(i = 0; i < blocks; i++)
					file_bytes += SQUASHFS_COMPRESSED_SIZE_BLOCK(block_list[i]);

	                        add_file(start, file_bytes, block_list, blocks, inode.fragment, inode.offset, frag_bytes);
				cur_ptr += blocks * sizeof(unsigned int);
				break;
			}	
			case SQUASHFS_SYMLINK_TYPE: {
				squashfs_symlink_inode_header inodep;
	
				if(swap) {
					squashfs_symlink_inode_header sinodep;
					memcpy(&sinodep, cur_ptr, sizeof(sinodep));
					SQUASHFS_SWAP_SYMLINK_INODE_HEADER(&inodep, &sinodep);
				} else
					memcpy(&inodep, cur_ptr, sizeof(inodep));
				(*sym_count) ++;
				cur_ptr += sizeof(inodep) + inodep.symlink_size;
				break;
			}
			case SQUASHFS_DIR_TYPE: {
				squashfs_dir_inode_header dir_inode;

				if(swap) {
					squashfs_dir_inode_header sinode;
					memcpy(&sinode, cur_ptr, sizeof(dir_inode));
					SQUASHFS_SWAP_DIR_INODE_HEADER(&dir_inode, &sinode);
				} else
					memcpy(&dir_inode, cur_ptr, sizeof(dir_inode));
				if(dir_inode.start_block < directory_start_block)
					*uncompressed_directory += dir_inode.file_size;
				(*dir_count) ++;
				cur_ptr += sizeof(squashfs_dir_inode_header);
				break;
			}
			case SQUASHFS_LDIR_TYPE: {
				squashfs_ldir_inode_header dir_inode;
				int i;

				if(swap) {
					squashfs_ldir_inode_header sinode;
					memcpy(&sinode, cur_ptr, sizeof(dir_inode));
					SQUASHFS_SWAP_LDIR_INODE_HEADER(&dir_inode, &sinode);
				} else
					memcpy(&dir_inode, cur_ptr, sizeof(dir_inode));
				if(dir_inode.start_block < directory_start_block)
					*uncompressed_directory += dir_inode.file_size;
				(*dir_count) ++;
				cur_ptr += sizeof(squashfs_ldir_inode_header);
				for(i = 0; i < dir_inode.i_count; i++) {
					squashfs_dir_index index;
					if(swap) {
						squashfs_dir_index sindex;
						memcpy(&sindex, cur_ptr, sizeof(squashfs_dir_index));
						SQUASHFS_SWAP_DIR_INDEX(&index, &sindex);
					} else
						memcpy(&index, cur_ptr, sizeof(squashfs_dir_index));
					cur_ptr += sizeof(squashfs_dir_index) + index.size + 1;
				}
				break;
			}
		 	case SQUASHFS_BLKDEV_TYPE:
		 	case SQUASHFS_CHRDEV_TYPE:
				(*dev_count) ++;
				cur_ptr += sizeof(squashfs_dev_inode_header);
				break;

			case SQUASHFS_FIFO_TYPE:
				(*fifo_count) ++;
				cur_ptr += sizeof(squashfs_ipc_inode_header);
				break;
			case SQUASHFS_SOCKET_TYPE:
				(*sock_count) ++;
				cur_ptr += sizeof(squashfs_ipc_inode_header);
				break;
		 	default:
				ERROR("Unknown inode type %d in scan_inode_table!\n", inode.inode_type);
				goto failed;
		}
	}
	
	return files;


failed:
	free(*inode_table);
	return FALSE;
}


int read_super(int fd, squashfs_super_block *sBlk, int *be, char *source)
{
	read_bytes(fd, SQUASHFS_START, sizeof(squashfs_super_block), (char *) sBlk);

	/* Check it is a SQUASHFS superblock */
	swap = 0;
	if(sBlk->s_magic != SQUASHFS_MAGIC) {
		if(sBlk->s_magic == SQUASHFS_MAGIC_SWAP) {
			squashfs_super_block sblk;
			ERROR("Reading a different endian SQUASHFS filesystem on %s - ignoring -le/-be options\n", source);
			SQUASHFS_SWAP_SUPER_BLOCK(&sblk, sBlk);
			memcpy(sBlk, &sblk, sizeof(squashfs_super_block));
			swap = 1;
		} else  {
			ERROR("Can't find a SQUASHFS superblock on %s\n", source);
			goto failed_mount;
		}
	}

	/* Check the MAJOR & MINOR versions */
	if(sBlk->s_major != SQUASHFS_MAJOR || sBlk->s_minor > SQUASHFS_MINOR) {
		if(sBlk->s_major < 3)
			ERROR("Filesystem on %s is a SQUASHFS %d.%d filesystem.  Appending\nto SQUASHFS %d.%d filesystems is not supported.  Please convert it to a SQUASHFS 3.0 filesystem\n", source, sBlk->s_major, sBlk->s_minor, sBlk->s_major, sBlk->s_minor);
		else
			ERROR("Major/Minor mismatch, filesystem on %s is %d.%d, I support 3.0\n",
				source, sBlk->s_major, sBlk->s_minor);
		goto failed_mount;
	}

#if __BYTE_ORDER == __BIG_ENDIAN
	*be = !swap;
#else
	*be = swap;
#endif

	printf("Found a valid SQUASHFS superblock on %s.\n", source);
	printf("\tInodes are %scompressed\n", SQUASHFS_UNCOMPRESSED_INODES(sBlk->flags) ? "un" : "");
	printf("\tData is %scompressed\n", SQUASHFS_UNCOMPRESSED_DATA(sBlk->flags) ? "un" : "");
	printf("\tFragments are %scompressed\n", SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk->flags) ? "un" : "");
	printf("\tCheck data is %s present in the filesystem\n", SQUASHFS_CHECK_DATA(sBlk->flags) ? "" : "not");
	printf("\tFragments are %s present in the filesystem\n", SQUASHFS_NO_FRAGMENTS(sBlk->flags) ? "not" : "");
	printf("\tAlways_use_fragments option is %s specified\n", SQUASHFS_ALWAYS_FRAGMENTS(sBlk->flags) ? "" : "not");
	printf("\tDuplicates are %s removed\n", SQUASHFS_DUPLICATES(sBlk->flags) ? "" : "not");
	printf("\tFilesystem size %.2f Kbytes (%.2f Mbytes)\n", sBlk->bytes_used / 1024.0, sBlk->bytes_used / (1024.0 * 1024.0));
	printf("\tBlock size %d\n", sBlk->block_size);
	printf("\tNumber of fragments %d\n", sBlk->fragments);
	printf("\tNumber of inodes %d\n", sBlk->inodes);
	printf("\tNumber of uids %d\n", sBlk->no_uids);
	printf("\tNumber of gids %d\n", sBlk->no_guids);
	TRACE("sBlk->inode_table_start %llx\n", sBlk->inode_table_start);
	TRACE("sBlk->directory_table_start %llx\n", sBlk->directory_table_start);
	TRACE("sBlk->uid_start %llx\n", sBlk->uid_start);
	TRACE("sBlk->fragment_table_start %llx\n", sBlk->fragment_table_start);
	printf("\n");

	return TRUE;

failed_mount:
	return FALSE;
}


unsigned char *squashfs_readdir(int fd, int root_entries, unsigned int directory_start_block, int offset, int size,
		unsigned int *last_directory_block, squashfs_super_block *sBlk, void (push_directory_entry)(char *, squashfs_inode, int, int))
{
	squashfs_dir_header dirh;
	char buffer[sizeof(squashfs_dir_entry) + SQUASHFS_NAME_LEN + 1];
	squashfs_dir_entry *dire = (squashfs_dir_entry *) buffer;
	unsigned char *directory_table = NULL;
	int byte, bytes = 0, dir_count;
	long long start = sBlk->directory_table_start + directory_start_block, last_start_block; 

	size += offset;
	if((directory_table = malloc((size + SQUASHFS_METADATA_SIZE * 2 - 1) & ~(SQUASHFS_METADATA_SIZE - 1))) == NULL)
		return NULL;
	while(bytes < size) {
		TRACE("squashfs_readdir: reading block 0x%llx, bytes read so far %d\n", start, bytes);
		last_start_block = start;
		if((byte = read_block(fd, start, &start, directory_table + bytes, sBlk)) == 0) {
			free(directory_table);
			return NULL;
		}
		bytes += byte;
	}

	if(!root_entries)
		goto all_done;

	bytes = offset;
 	while(bytes < size) {			
		if(swap) {
			squashfs_dir_header sdirh;
			memcpy(&sdirh, directory_table + bytes, sizeof(sdirh));
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
		} else
			memcpy(&dirh, directory_table + bytes, sizeof(dirh));

		dir_count = dirh.count + 1;
		TRACE("squashfs_readdir: Read directory header @ byte position 0x%x, 0x%x directory entries\n", bytes, dir_count);
		bytes += sizeof(dirh);

		while(dir_count--) {
			if(swap) {
				squashfs_dir_entry sdire;
				memcpy(&sdire, directory_table + bytes, sizeof(sdire));
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
			} else
				memcpy(dire, directory_table + bytes, sizeof(dire));
			bytes += sizeof(*dire);

			memcpy(dire->name, directory_table + bytes, dire->size + 1);
			dire->name[dire->size + 1] = '\0';
			TRACE("squashfs_readdir: pushing directory entry %s, inode %x:%x, type 0x%x\n", dire->name, dirh.start_block, dire->offset, dire->type);
			push_directory_entry(dire->name, SQUASHFS_MKINODE(dirh.start_block, dire->offset), dire->inode_number, dire->type);
			bytes += dire->size + 1;
		}
	}

all_done:
	*last_directory_block = (unsigned int) last_start_block - sBlk->directory_table_start;
	return directory_table;
}


int read_fragment_table(int fd, squashfs_super_block *sBlk, squashfs_fragment_entry **fragment_table)
{
	int i, indexes = SQUASHFS_FRAGMENT_INDEXES(sBlk->fragments);
	squashfs_fragment_index fragment_table_index[indexes];

	TRACE("read_fragment_table: %d fragments, reading %d fragment indexes from 0x%llx\n", sBlk->fragments, indexes, sBlk->fragment_table_start);
	if(sBlk->fragments == 0)
		return 1;

	if((*fragment_table = (squashfs_fragment_entry *) malloc(sBlk->fragments * sizeof(squashfs_fragment_entry))) == NULL) {
		ERROR("Failed to allocate fragment table\n");
		return 0;
	}

	if(swap) {
		squashfs_fragment_index sfragment_table_index[indexes];

		read_bytes(fd, sBlk->fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk->fragments), (char *) sfragment_table_index);
		SQUASHFS_SWAP_FRAGMENT_INDEXES(fragment_table_index, sfragment_table_index, indexes);
	} else
		read_bytes(fd, sBlk->fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk->fragments), (char *) fragment_table_index);

	for(i = 0; i < indexes; i++) {
		int length = read_block(fd, fragment_table_index[i], NULL, ((unsigned char *) *fragment_table) + (i * SQUASHFS_METADATA_SIZE), sBlk);
		TRACE("Read fragment table block %d, from 0x%llx, length %d\n", i, fragment_table_index[i], length);
	}

	if(swap) {
		squashfs_fragment_entry sfragment;
		for(i = 0; i < sBlk->fragments; i++) {
			SQUASHFS_SWAP_FRAGMENT_ENTRY((&sfragment), (&(*fragment_table)[i]));
			memcpy((char *) &(*fragment_table)[i], (char *) &sfragment, sizeof(squashfs_fragment_entry));
		}
	}

	return 1;
}


long long read_filesystem(char *root_name, int fd, squashfs_super_block *sBlk, char **cinode_table,
		char **data_cache, char **cdirectory_table, char **directory_data_cache,
		unsigned int *last_directory_block, unsigned int *inode_dir_offset, unsigned int *inode_dir_file_size,
		unsigned int *root_inode_size, unsigned int *inode_dir_start_block, int *file_count, int *sym_count,
		int *dev_count, int *dir_count, int *fifo_count, int *sock_count, squashfs_uid *uids,
		unsigned short *uid_count, squashfs_uid *guids, unsigned short *guid_count,
		long long *uncompressed_file, unsigned int *uncompressed_inode, unsigned int *uncompressed_directory,
		unsigned int *inode_dir_inode_number, unsigned int *inode_dir_parent_inode,
		void (push_directory_entry)(char *, squashfs_inode, int, int), squashfs_fragment_entry **fragment_table)
{
	unsigned char *inode_table = NULL, *directory_table;
	long long start = sBlk->inode_table_start, end = sBlk->directory_table_start, root_inode_start = start +
		SQUASHFS_INODE_BLK(sBlk->root_inode);
	unsigned int root_inode_offset = SQUASHFS_INODE_OFFSET(sBlk->root_inode), root_inode_block, files;
	squashfs_inode_header inode;

	printf("Scanning existing filesystem...\n");

	if(read_fragment_table(fd, sBlk, fragment_table) == 0)
		goto error;

	if((files = scan_inode_table(fd, start, end, root_inode_start, root_inode_offset, sBlk, &inode, &inode_table,
			&root_inode_block, root_inode_size, uncompressed_file, uncompressed_directory, file_count, sym_count,
			dev_count, dir_count, fifo_count, sock_count)) == 0) {
		ERROR("read_filesystem: inode table read failed\n");
		goto error;
	}

	*uncompressed_inode = root_inode_block;

	printf("Read existing filesystem, %d inodes scanned\n", files);

	if(inode.base.inode_type == SQUASHFS_DIR_TYPE || inode.base.inode_type == SQUASHFS_LDIR_TYPE) {
		if(inode.base.inode_type == SQUASHFS_DIR_TYPE) {
			*inode_dir_start_block = inode.dir.start_block;
			*inode_dir_offset = inode.dir.offset;
			*inode_dir_file_size = inode.dir.file_size - 3;
			*inode_dir_inode_number = inode.dir.inode_number;
			*inode_dir_parent_inode = inode.dir.parent_inode;
		} else {
			*inode_dir_start_block = inode.ldir.start_block;
			*inode_dir_offset = inode.ldir.offset;
			*inode_dir_file_size = inode.ldir.file_size - 3;
			*inode_dir_inode_number = inode.ldir.inode_number;
			*inode_dir_parent_inode = inode.ldir.parent_inode;
		}

		if((directory_table = squashfs_readdir(fd, !root_name, *inode_dir_start_block, *inode_dir_offset,
				*inode_dir_file_size, last_directory_block, sBlk, push_directory_entry)) == NULL) {
			ERROR("read_filesystem: Could not read root directory\n");
			goto error;
		}

		root_inode_start -= start;
		if((*cinode_table = (char *) malloc(root_inode_start)) == NULL) {
			ERROR("read_filesystem: failed to alloc space for existing filesystem inode table\n");
			goto error;
		}
	       	read_bytes(fd, start, root_inode_start, *cinode_table);

		if((*cdirectory_table = (char *) malloc(*last_directory_block)) == NULL) {
			ERROR("read_filesystem: failed to alloc space for existing filesystem directory table\n");
			goto error;
		}
		read_bytes(fd, sBlk->directory_table_start, *last_directory_block, *cdirectory_table);

		if((*data_cache = (char *) malloc(root_inode_offset + *root_inode_size)) == NULL) {
			ERROR("read_filesystem: failed to alloc inode cache\n");
			goto error;
		}
		memcpy(*data_cache, inode_table + root_inode_block, root_inode_offset + *root_inode_size);

		if((*directory_data_cache = (char *) malloc(*inode_dir_offset + *inode_dir_file_size)) == NULL) {
			ERROR("read_filesystem: failed to alloc directory cache\n");
			goto error;
		}
		memcpy(*directory_data_cache, directory_table, *inode_dir_offset + *inode_dir_file_size);

		if(!swap)
			read_bytes(fd, sBlk->uid_start, sBlk->no_uids * sizeof(squashfs_uid), (char *) uids);
		else {
			squashfs_uid uids_copy[sBlk->no_uids];

			read_bytes(fd, sBlk->uid_start, sBlk->no_uids * sizeof(squashfs_uid), (char *) uids_copy);
			SQUASHFS_SWAP_DATA(uids, uids_copy, sBlk->no_uids, sizeof(squashfs_uid) * 8);
		}

		if(!swap)
			read_bytes(fd, sBlk->guid_start, sBlk->no_guids * sizeof(squashfs_uid), (char *) guids);
		else {
			squashfs_uid guids_copy[sBlk->no_guids];

			read_bytes(fd, sBlk->guid_start, sBlk->no_guids * sizeof(squashfs_uid), (char *) guids_copy);
			SQUASHFS_SWAP_DATA(guids, guids_copy, sBlk->no_guids, sizeof(squashfs_uid) * 8);
		}
		*uid_count = sBlk->no_uids;
		*guid_count = sBlk->no_guids;

		free(inode_table);
		free(directory_table);
		return sBlk->inode_table_start;
	}

error:
	return 0;
}
